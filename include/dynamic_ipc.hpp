/* 
Copyright (C) 2014 Jonathon Ogden   < jeog.dev@gmail.com >

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

#ifndef JO_TOSDB_DYNAMIC_IPC
#define JO_TOSDB_DYNAMIC_IPC

#include <map>
#include <set>
#include <algorithm>
#include <iterator>
#include <vector>
#include <memory>
#include <future>
#include <windows.h>

/* 
   DynamicIPCMaster / Slave are objects that provide a flexible
   means of IPC.  They combine a shared mem mapping and a duplexed
   named-pipe for external use. On construction the Slave creates a 
   shared memory segment, managed by an ExplicitHeap, accepting allocation 
   requests through an internal named-pipe on its own thread. It waits on 
   the master through a call to wait_for_master(). 

   To support multiple Masters(clients) from different processes and to 
   preempt blocking by the slave the pipe instances are acquired and then 
   released manually via grab_pipe() and release_pipe(), respecitively.
   (The concept is similar to the CallNamedPipe() system call.)  

   shem_chunks are structs that have an offset and byte-size used between
   different address spaces to convey the beginning and end of data inserted 
   into the shared memory segment.

   The sequence of calls by the master are:
   1) grab_pipe()
   2) insert()  : pass raw buffers, sets or vectors of data
   3) send()  : pass the shem_chunks returned by insert or a scallar
   4) recv()  : populate a shem_chunk or scalar with what's passed to send
   5) remove()  : 'deallocate' the shem_chunks created by insert
   6) release_pipe()

   The memory management is handled internally after calls to insert and 
   remove but its important that anything passed to insert also be removed 
   AFTER the slave sends back its cofirmation.  If it's not eventually 
   the heap managed by the slave will become full resulting in undefined
   behvarior. (Currently there isn't a means for dealing with this because
   the objects are only being used for trivial RPC.)

   shem_chunk { x, 0 } is reserved for special signals, opcodes etc.
   shem_chunk { 0, 0 } signals the end of a particular set of transmissions.
*/

#define IPC_TIMED_WAIT(cond,log_msg,err_code) do{ \
  unsigned int lcount = 0; \
  while(cond){ \
    Sleep(TOSDB_DEF_TIMEOUT / 10); \
    lcount += (TOSDB_DEF_TIMEOUT / 10); \
    if(lcount >= TOSDB_DEF_TIMEOUT){ \
      TOSDB_LogH("IPC",log_msg); \
      return err_code; \
}}}while(0)

class ExplicitHeap{
  typedef unsigned long _head_type;
  typedef unsigned char *_ptr_type;  

  typedef std::multimap<size_type, _ptr_type> _heap_type;
  typedef std::set<_ptr_type>                 _set_type;
  typedef std::pair<size_type, _ptr_type>     _heap_pair_type;  

  static const unsigned short HEAD_SZ = sizeof(_head_type);

  _heap_type _free_heap;
  _set_type  _allocated_set;

  inline bool _valid_start(void* start)
  {   
    return (_allocated_set.find((_ptr_type)start) != _allocated_set.cend());
  }

public:   
  ExplicitHeap(void* beg_addr, size_type sz)
    {
      this->_free_heap.insert(_heap_pair_type(sz, (_ptr_type)beg_addr)); 
    }    

  ~ExplicitHeap() { }
  
  void* allocate(size_type size);
  bool  deallocate(void* start);
  size_type   size(void* start);

  void clear()
  {
    _free_heap.clear();
    _allocated_set.clear();
  }
};

class DynamicIPCBase {
public:
  struct shem_chunk{
    size_type offset;
    int sz;
    shem_chunk() 
      : offset(0), sz(0) {}
    shem_chunk(shem_chunk&& sc)
      : offset(sc.offset), sz(sc.sz)
      { sc.offset = sc.sz = 0; }
    shem_chunk(size_type offset, size_type sz)
      : offset(offset), sz(sz) {}
    ~shem_chunk(){}
  };

private:  
  bool  _send(const shem_chunk& item) const;  
  bool  _recv(shem_chunk& item) const;  
  void* _allocate(size_type sz) const;
  bool  _deallocate(void* start) const;

  template<typename C>
  shem_chunk _insert(C cont) const
  {
    void* blk = nullptr;    
    C::const_iterator iter_b = cont.cbegin();
    C::const_iterator iter_e = cont.cend();     
    size_type sz = sizeof(C::value_type) * cont.size();  
    
    if(!(blk = _allocate(sz)))
      return shem_chunk(0,0);

    for(int i = 0; iter_b != iter_e; ++i, iter_b++)
      ((C::value_type*)blk)[i] = *iter_b; 

    return shem_chunk((size_type)blk - (size_type)_mmap_addr, sz);
  }

protected:
  static const int   PAUSE = 1000;  
  static const int   ACL_SIZE = 144;
  static const char* KMUTEX_NAME;

  static const unsigned int  ALLOC = 1;
  static const unsigned int  DEALLOC = 2;
  static const unsigned int  PING = 4;

  std::string _shem_str;
  std::string _xtrnl_pipe_str;
  std::string _intrnl_pipe_str;

  void* _fmap_hndl;  
  void* _mmap_addr;
  void* _xtrnl_pipe_hndl;
  void* _intrnl_pipe_hndl;
  void* _mtx;
  int   _mmap_sz;

  DynamicIPCBase(std::string name, int sz = 0)
    :
#ifdef KGBLNS_
      _shem_str(std::string("Global\\").append(std::string(name)
                                       .append("_shem"))),
#else
      _shem_str(std::string(name).append("_shem")),
#endif
      _xtrnl_pipe_str(std::string("\\\\.\\pipe\\").append(std::string(name)
                                                  .append("_pipe"))),
      _intrnl_pipe_str(std::string("\\\\.\\pipe\\").append(std::string(name)
                                                   .append("pipe_intrnl"))), 
      _fmap_hndl(NULL),
      _mmap_addr(NULL),
      _xtrnl_pipe_hndl(INVALID_HANDLE_VALUE),
      _intrnl_pipe_hndl(INVALID_HANDLE_VALUE),
      _mmap_sz(sz),
      _mtx(NULL)
    {      
    }

  virtual ~DynamicIPCBase() {}

public:
  template<typename T>
  void extract(shem_chunk& chunk, T* dest, size_type buf_len) const
  {
    void* ptr = shem_ptr(chunk);
    if(!ptr){
      dest = nullptr;
      return; 
    }  
    memcpy_s(dest, sizeof(T) * buf_len, ptr, chunk.sz);    
  }
  
  template<typename T>
  shem_chunk insert(T* data, size_type data_len = 1)
  {    
    size_type sz = sizeof(T) * data_len;

    void* blk = _allocate(sz); 
    if(!blk || memcpy_s(blk, sz, data, sz))
      return shem_chunk(0,0);

    return shem_chunk((size_type)blk - (size_type)_mmap_addr, sz);    
  }

  template<typename T, typename A>
  inline shem_chunk insert(std::vector<T,A>& vec) { return _insert(vec); }

  template<typename T, typename E, typename A>
  inline shem_chunk insert(std::set<T,E,A>& set) { return _insert(set); }

  const DynamicIPCBase& remove(const shem_chunk chunk) const // removed rval
  {    
    void* start = (void*)((size_type)_mmap_addr + chunk.offset);
    _deallocate(start);
    return *this;
  }
  
  void* shem_ptr(shem_chunk& chunk) const
  {/* 
    * we check for overflow but NO GUARANTEE shem_chunk points at valid data 
    */        
    if(chunk.sz <= 0 
       || !_mmap_addr 
       || !((chunk.offset + chunk.sz) <= (size_type)_mmap_sz))
      { /* if blck is 'abnormal', no mem-map, or a read will overflow it */
        return nullptr;    
      }  
    return (void*)((size_type)_mmap_addr + chunk.offset);
  }

  void* ptr(size_type offset) const
  {
    if(offset > (size_type)_mmap_sz || !_mmap_addr)
      return nullptr;

    return (void*)((size_type)_mmap_addr + offset);
  }

  size_type offset(void* start) const  
  {
    if(start < _mmap_addr 
       || (size_type)start >= ((size_type)_mmap_addr + (size_type)_mmap_sz)) 
      {
        return 0;      
      }

    return ((size_type)start - (size_type)_mmap_addr); 
  }

  inline bool send(const int val) const { return _send(shem_chunk(val,0)); }

  bool recv(int& val) const
  {
    shem_chunk tmp;

    bool res = _recv(tmp);
    val = tmp.offset;
    return res;
  }

  inline bool send(const shem_chunk& chunk) const { return _send(chunk); }
  inline bool recv(shem_chunk& chunk) const { return _recv(chunk); }

  const DynamicIPCBase& operator<<(const shem_chunk& chunk) const
  {
    this->_send(chunk);
    return *this;
  }

  const DynamicIPCBase& operator>>(shem_chunk& chunk) const 
  {
    this->_recv(chunk);      
    return *this;
  }  
};

class DynamicIPCMaster
    : public DynamicIPCBase {  
  volatile bool _pipe_held;

public:  
  DynamicIPCMaster(std::string name)
    : 
      DynamicIPCBase(name),
      _pipe_held(false)
    {  
    }

  virtual ~DynamicIPCMaster()
    {
      disconnect();
    }

  bool try_for_slave();
  void disconnect(int level = 3);
  int  grab_pipe();
  void release_pipe();
  bool connected() const;
  inline bool pipe_held() const { return _pipe_held; }  
};

class DynamicIPCSlave
    : public DynamicIPCBase {      
  std::unordered_map<Securable, SECURITY_ATTRIBUTES> _sec_attr;
  std::unordered_map<Securable, SECURITY_DESCRIPTOR> _sec_desc;   
  std::unordered_map<Securable, SmartBuffer<void>>   _sids; 
  std::unordered_map<Securable, SmartBuffer<ACL>>    _acls;    

  std::unique_ptr<ExplicitHeap> _uptr_heap;  
  volatile bool _alloc_flag;

  int  _set_security();
  void _listen_for_alloc();

public:     
  DynamicIPCSlave(std::string name, int sz);
  virtual ~DynamicIPCSlave();
  bool  wait_for_master();  
};


#endif
