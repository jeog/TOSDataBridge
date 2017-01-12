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

#ifndef JO_TOSDB_IPC
#define JO_TOSDB_IPC

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
   shared memory segment, managed by an SlowHeapManager, accepting allocation 
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
   2) insert()  : pass raw buffers
   3) send()  : pass the shem_chunks returned by insert or a long scalar
   4) recv()  : populate a shem_chunk, or long scalar, with what's passed to send
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

/* a simple timed loop that waits until 'cond' is false */
#define IPC_TIMED_WAIT(cond,log_msg,err_code) do{ \
    unsigned int lcount = 0; \
    while(cond){ \
        Sleep(TOSDB_DEF_TIMEOUT / 10); \
        lcount += (TOSDB_DEF_TIMEOUT / 10); \
        if(lcount >= TOSDB_DEF_TIMEOUT){ \
            TOSDB_LogH("IPC",log_msg); \
            return err_code; \
        } \
    } \
}while(0)


/* NOT NECESSARY TO BE OPTIMAL - VERY SMALL HEAP, VERY LOW USAGE */
class SlowHeapManager{ 
    typedef unsigned long  _head_ty;
    typedef unsigned char* _ptr_ty;    
    typedef std::pair<size_t, _ptr_ty> _heap_pair_ty;    

    static const unsigned int HEAD_SZ = sizeof(_head_ty);

    std::multimap<size_t, _ptr_ty> _free_heap;
    std::set<_ptr_ty> _allocated_set;

    inline bool 
    _valid_start(void* start)
    {     
        return (_allocated_set.find((_ptr_ty)start) != _allocated_set.cend());
    }

public:       
    /* make sure offset can fit in an unsigned 4 bytes 
       AND won't overflow during addition of a 'width' */
    static const unsigned long MAX_SZ = ((65536LL * 65536 / 2) - 1); /* 4 BYTE SIGNED MAX */

    SlowHeapManager(void* beg_addr, size_t sz)
        {
            if(sz > MAX_SZ)
                throw std::invalid_argument("sz > SlowHeapManager::MAX_SZ");

            _free_heap.insert( _heap_pair_ty(sz, (_ptr_ty)beg_addr) ); 
        }        

    ~SlowHeapManager() 
        { 
        }
    
    void* 
    allocate(size_t size);

    bool    
    deallocate(void* start);

    size_t     
    size(void* start);

    void 
    clear()
    {
        _free_heap.clear();
        _allocated_set.clear();
    }
};


/* NOTE: size_t for msg passing creates all types of problems if someone mistakenly 
         tries to use 32 bit and 64 builds simultanesouly

         for instance, the master->connected() call from 32 bit will define a 4-byte 
         size_t PING msg diffrently than _listen_for_alloc thread will an 8-byte one 
         and generate bad op-codes; this will result in connected (rightly) returning 
         false but it's not what we really want.
   
         TODO: protect IPC from build issues by either:
               1) don't let client even get to IPC w/ different builds, or
               2) make some cross-build guarantees: uint32_t args etc.              */
class DynamicIPCBase {
public:
    class InterProcessMutex{
        HANDLE _mtx;  

        InterProcessMutex(const InterProcessMutex&);
        InterProcessMutex(InterProcessMutex&&);
        InterProcessMutex& operator=(InterProcessMutex& ipm);

    public:
        InterProcessMutex(std::string name, std::string func_name, unsigned long wait_for)                                     
            {
                  _mtx = OpenMutex(SYNCHRONIZE, FALSE, name.c_str());
                  if(!_mtx){
                      errno_t e = GetLastError();
                      if(e != ERROR_FILE_NOT_FOUND)
                          TOSDB_LogEx("IPC", ("OpenMutex failed in " + func_name).c_str(), e);                      
                      return;
                  }
                  
                  errno_t err = 0;
                  DWORD wstate = WaitForSingleObject(_mtx, wait_for);
                  switch(wstate){
                  case WAIT_TIMEOUT:
                      TOSDB_LogH("IPC", ("WaitForSingleObject timed out in InterProcessMutex, in:" + func_name).c_str()); 
                      break;
                  case WAIT_FAILED:                  
                      err = GetLastError();
                      TOSDB_LogEx("IPC", ("WaitForSingleObject failed in InterProcessMutex, in:" + func_name).c_str(), err);                  
                      break;
                  default:
                      return;
                  }

                  CloseHandle(_mtx);   
                  _mtx = NULL;
            }

        inline bool
        locked()
        {
            return (_mtx != NULL);
        }

        ~InterProcessMutex()
        {
            if(locked()){
                ReleaseMutex(_mtx);
                CloseHandle(_mtx);           
            }
        }

    };

    struct shem_chunk{
        long offset; /* should only be unsigned for an actual 'chunk' */                               
        size_t sz;
        
        shem_chunk() 
            : 
                offset(0), 
                sz(0) 
            {
            } 
     
        shem_chunk(long offset, size_t sz)
            : 
                offset(offset), 
                sz(sz) 
            {
            }

        ~shem_chunk()
            {
            }

        inline bool
        operator==(const shem_chunk& right) const
        {
            return (offset == right.offset) && (sz == right.sz);
        }

        inline std::string
        as_string() const
        {
            return "{" + std::to_string(offset) + "," + std::to_string(sz) + "}";
        }
    };

    static const shem_chunk NULL_SHEM_CHUNK;

private:    
    bool    
    _send(const shem_chunk& item) const;    

    bool    
    _recv(shem_chunk& item) const;    

    void* 
    _allocate(size_t sz) const;

    bool   
    _deallocate(void* start) const;

protected:           
    static const unsigned int ALLOC = 1;
    static const unsigned int DEALLOC = 2;
    static const unsigned int PING = 4;      
    static const int ACL_SIZE = 144;

    std::string _shem_name;
    std::string _xtrnl_mtx_name;
    std::string _xtrnl_pipe_name;
    std::string _intrnl_mtx_name;
    std::string _intrnl_pipe_name;

    void *_fmap_hndl;
    void *_mmap_addr;
    void *_xtrnl_pipe_hndl;
    void *_intrnl_pipe_hndl;
    void *_xtrnl_mtx;        
    void *_intrnl_mtx;

    size_t _mmap_sz; 

    DynamicIPCBase(std::string name, size_t sz = 0)
        :
#ifdef NO_KGBLNS
            _shem_name(std::string(name).append("_shem")),
            _xtrnl_mtx_name(std::string(name).append("_mtx_xtrnl")),  
            _intrnl_mtx_name(std::string(name).append("_mtx_intrnl")),
#else
            _shem_name(std::string("Global\\").append(name).append("_shem")),            
            _xtrnl_mtx_name(std::string("Global\\").append(name).append("_mtx_xtrnl")),
            _intrnl_mtx_name(std::string("Global\\").append(name).append("_mtx_intrnl")),
#endif
            _xtrnl_pipe_name(std::string("\\\\.\\pipe\\").append(name).append("_pipe")),
            _intrnl_pipe_name(std::string("\\\\.\\pipe\\").append(name).append("_pipe_intrnl")), 
            _fmap_hndl(NULL),
            _mmap_addr(NULL),
            _xtrnl_pipe_hndl(INVALID_HANDLE_VALUE),
            _intrnl_pipe_hndl(INVALID_HANDLE_VALUE),
            _mmap_sz(sz),            
            _xtrnl_mtx(NULL),
            _intrnl_mtx(NULL) /* we arent instantiated this in the master to maintaina const this ptr */
        {            
            if(sz > SlowHeapManager::MAX_SZ)
                throw std::invalid_argument("sz > SlowHeapManager::MAX_SZ");
        }

    virtual 
    ~DynamicIPCBase() 
        {            
        }

public:
    /* insert/extract take pointers to raw data insead of refs like the rest of
       the interface; it just seems more intuitive for what's actually happening */
    template<typename T>
    shem_chunk 
    insert(T* data, size_t data_len = 1) const
    {       
        size_t sz = sizeof(T) * data_len;

        void* blk = _allocate(sz); 
        if(!blk){
            TOSDB_LogH("IPC", "_allocate failed in insert");
            return NULL_SHEM_CHUNK;
        }

        errno_t err = memcpy_s(blk, sz, data, sz);
        if(err){
            std::string err_str = "memcpy_s failed in insert (" + std::to_string(err) + ")";
            TOSDB_LogH("IPC", err_str.c_str());
            return NULL_SHEM_CHUNK;
        }

		    /* cast to long OK; blk can't be > (2**31 -1) from _mmap_addr */
        return shem_chunk((long)((size_t)blk - (size_t)_mmap_addr), sz);        
    }

    template<typename T>
    bool 
    extract(const shem_chunk& chunk, T* dest, size_t buf_len = 1) const
    {      
        void* ptr = shem_ptr(chunk);
        if(!ptr){  
            TOSDB_LogH("IPC", "bad chunk passed to extract");
            return false; 
        }
        
        errno_t err = memcpy_s(dest, sizeof(T) * buf_len, ptr, chunk.sz);  
        if(err){
            std::string err_str = "memcpy_s failed in extract (" + std::to_string(err) + ")";
            TOSDB_LogH("IPC", err_str.c_str());
            return false;
        }

        return true;
    }
    
    /* move is a unecessary but it signals the client */
    const DynamicIPCBase& 
    remove(shem_chunk&& chunk) const;
    
    void* 
    shem_ptr(const shem_chunk& chunk) const;

    void* 
    ptr(size_t offset) const;

    size_t 
    offset(void* start) const;

    inline bool 
    send(const long val) const 
    { 
        return _send(shem_chunk(val,0)); 
    }

    bool 
    recv(long& val) const;

    inline bool 
    send(const shem_chunk& chunk) const 
    { 
        return _send(chunk); 
    }

    inline bool 
    recv(shem_chunk& chunk) const 
    { 
        return _recv(chunk); 
    }

    const DynamicIPCBase& 
    operator<<(const shem_chunk& chunk) const
    {
        _send(chunk);
        return *this;
    }

    const DynamicIPCBase& 
    operator>>(shem_chunk& chunk) const
    {
        _recv(chunk);            
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

    virtual 
    ~DynamicIPCMaster()
        {
            disconnect();
        }

    bool 
    try_for_slave(unsigned long timeout);

    void 
    disconnect(int level = 3);

    long
    grab_pipe(unsigned long timeout);

    void 
    release_pipe();

    bool 
    connected();

    inline bool 
    pipe_held() const 
    { 
        return _pipe_held; 
    }    
};


class DynamicIPCSlave
        : public DynamicIPCBase {      
    static const int NSECURABLE = 3;

    std::unordered_map<Securable, SECURITY_ATTRIBUTES> _sec_attr;
    std::unordered_map<Securable, SECURITY_DESCRIPTOR> _sec_desc;     
    std::unordered_map<Securable, SmartBuffer<void>> _sids; 
    std::unordered_map<Securable, SmartBuffer<ACL>> _acls;        
    
    std::unique_ptr<SlowHeapManager> _uptr_heap;    
    volatile bool _alloc_flag;

    int    
    _set_security();

    void 
    _listen_for_alloc();

public:         
    DynamicIPCSlave(std::string name, size_t sz);

    virtual 
    ~DynamicIPCSlave();
    
    bool    
    wait_for_master();    
};


#endif
