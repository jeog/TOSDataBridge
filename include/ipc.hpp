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
   2) insert()  : pass raw buffers, sets or vectors of data
   3) send()  : pass the shem_chunks returned by insert or, a long 
   4) recv()  : populate a shem_chunk, or long, with what's passed to send
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


class SlowHeapManager{ 
    /* NOT NECESSARY TO BE OPTIMAL - VERY SMALL HEAP, VERY LOW USAGE */
    typedef unsigned long  _head_ty;
    typedef unsigned char* _ptr_ty;    
    typedef std::pair<size_t, _ptr_ty> _heap_pair_ty;    

    static const unsigned int HEAD_SZ = sizeof(_head_ty);
    /* make sure offset can fit in an unsigned 4 bytes 
       AND won't overflow during addition of a 'width' */
    static const unsigned long MAX_SZ = LONG_MAX;

    std::multimap<size_t, _ptr_ty> _free_heap;
    std::set<_ptr_ty> _allocated_set;

    inline bool 
    _valid_start(void* start)
    {     
        return (this->_allocated_set.find((_ptr_ty)start) != this->_allocated_set.cend());
    }

public:     
    SlowHeapManager(void* beg_addr, size_t sz)
        {
            if(sz > MAX_SZ)
                throw std::invalid_argument("sz > SlowHeapManager::MAX_SZ");

            this->_free_heap.insert(_heap_pair_ty(sz, (_ptr_ty)beg_addr)); 
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

    inline void 
    clear()
    {
        _free_heap.clear();
        _allocated_set.clear();
    }
};

class DynamicIPCBase {
public:
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
    };

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
    static const unsigned long MAX_SZ = LONG_MAX;
    static const unsigned int ALLOC = 1;
    static const unsigned int DEALLOC = 2;
    static const unsigned int PING = 4;
    static const int PAUSE = 1000;    
    static const int ACL_SIZE = 144;
    static const char* KMUTEX_NAME;
    
    std::string _shem_str;
    std::string _xtrnl_pipe_str;
    std::string _intrnl_pipe_str;
    void *_fmap_hndl;
    void *_mmap_addr;
    void *_xtrnl_pipe_hndl;
    void *_intrnl_pipe_hndl;
    void *_mtx;    
    size_t _mmap_sz; /* should be unsinged */

    DynamicIPCBase(std::string name, size_t sz = 0)
        :
#ifdef NO_KGBLNS
            _shem_str(std::string(name).append("_shem")),
#else
            _shem_str(std::string("Global\\").append(name).append("_shem")),            
#endif
            _xtrnl_pipe_str(std::string("\\\\.\\pipe\\").append(name).append("_pipe")),
            _intrnl_pipe_str(std::string("\\\\.\\pipe\\").append(name).append("_pipe_intrnl")), 
            _fmap_hndl(NULL),
            _mmap_addr(NULL),
            _xtrnl_pipe_hndl(INVALID_HANDLE_VALUE),
            _intrnl_pipe_hndl(INVALID_HANDLE_VALUE),
            _mmap_sz(sz),
            _mtx(NULL)
        {            
            if(sz > MAX_SZ)
                throw std::invalid_argument("sz > DynamicIPCBase::MAX_SZ");
        }

    virtual ~DynamicIPCBase() 
        {
        }

public:
    /* insert/extract take pointers to raw data insead of refs like the rest of
       the interface; it just seems more intuitive for what's actually happening */
    template<typename T>
    shem_chunk 
    insert(T* data, size_t data_len = 1)
    {        
        size_t sz = sizeof(T) * data_len;

        void* blk = _allocate(sz); 
        if(!blk || memcpy_s(blk, sz, data, sz))
            return shem_chunk(0,0);

		 /* cast to long OK; blk can't be > LONG_MAX from _mmap_addr */
        return shem_chunk((long)((size_t)blk - (size_t)_mmap_addr), sz);        
    }

    template<typename T>
    void 
    extract(shem_chunk& chunk, T* dest, size_t buf_len) const
    {
        void* ptr = this->shem_ptr(chunk);
        if(!ptr){
            dest = nullptr;
            return; 
        }    
        memcpy_s(dest, sizeof(T) * buf_len, ptr, chunk.sz);        
    }
    
    const DynamicIPCBase& 
    remove(const shem_chunk&& chunk) const;
    
    void* 
    shem_ptr(shem_chunk& chunk) const;

    void* 
    ptr(size_t offset) const;

    size_t 
    offset(void* start) const;

    inline bool 
    send(const long val) const 
    { 
        return this->_send(shem_chunk(val,0)); 
    }

    bool 
    recv(long& val) const;

    inline bool 
    send(const shem_chunk& chunk) const 
    { 
        return this->_send(chunk); 
    }

    inline bool 
    recv(shem_chunk& chunk) const 
    { 
        return this->_recv(chunk); 
    }

    const DynamicIPCBase& 
    operator<<(const shem_chunk& chunk) const;

    const DynamicIPCBase& 
    operator>>(shem_chunk& chunk) const; 
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

    bool 
    try_for_slave();

    void 
    disconnect(int level = 3);

    long
    grab_pipe();

    void 
    release_pipe();

    bool 
    connected() const;

    inline bool 
    pipe_held() const 
    { 
        return this->_pipe_held; 
    }    
};


class DynamicIPCSlave
        : public DynamicIPCBase {      
    static const int NSECURABLE = 3;

    std::unordered_map<Securable, SECURITY_ATTRIBUTES> _sec_attr;
    std::unordered_map<Securable, SECURITY_DESCRIPTOR> _sec_desc;     
    std::unordered_map<Securable, SmartBuffer<void>> _sids; 
    std::unordered_map<Securable, SmartBuffer<ACL>> _acls;        
    
    typedef std::unique_ptr<SlowHeapManager> _uptr_heap_ty;
    
    _uptr_heap_ty _uptr_heap;    
    volatile bool _alloc_flag;

    int    
    _set_security();

    void 
    _listen_for_alloc();

public:         
    DynamicIPCSlave(std::string name, size_t sz);
    virtual ~DynamicIPCSlave();
    
    bool    
    wait_for_master();    
};


#endif
