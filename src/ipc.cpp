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

#include "tos_databridge.h"
#include "ipc.hpp"
#include <fstream>
#include <iomanip>

void* 
SlowHeapManager::allocate(size_t size)
{  
    _ptr_ty blck;  
    long long rmndr;

    if(size > MAX_SZ)
        throw std::invalid_argument("size > SlowHeapManager::MAX_SZ");

    size_t width = HEAD_SZ + size;
    size_t snglton[1] = {width};

    auto free_heap_end = _free_heap.cend();

    auto ffind = [&](_heap_pair_ty elem, size_t val){ return (elem.first >= val); };

    /* check for space on free heap */
    auto found = std::find_first_of(_free_heap.cbegin(), free_heap_end,
                                    snglton, snglton + 1, ffind);  

    if(found == free_heap_end)
        throw std::bad_alloc();
    
    blck = found->second;      
    rmndr = found->first - width;
    if(rmndr < 0 || rmndr > MAX_SZ) /* probably uncessary */
        throw std::out_of_range("invalid allocation remainder");
    else if(rmndr > 0) /* if we are not taking the whole block */
        _free_heap.insert(_heap_pair_ty((size_t)rmndr, found->second + width));  
    
    /* remove old block */
    _free_heap.erase(found); 

    /* add header; OK for size_t -> unsigned int head because we check for MAX_SZ */
    *(_head_ty*)blck = (unsigned long)size;   

    /* add to allocated set */
    _allocated_set.insert(blck + HEAD_SZ); 

    /* return header-adjusted block addr */
    return (void*)(blck + HEAD_SZ);    
}  

bool 
SlowHeapManager::deallocate(void* start)
{   
    size_t sz;

    if( !_valid_start(start) )
        return false;

    _ptr_ty phead = (_ptr_ty)start - HEAD_SZ; /* back up to the header */
    _head_ty head = *(_head_ty*)(phead);  /* get the value in the header */  

    auto free_heap_end = _free_heap.cend();  

    /* look for contiguous free blocks to right */
    auto rfind = [&](_heap_pair_ty elem){ return (elem.second == ((_ptr_ty)start + head)); };
    auto rght = std::find_if(_free_heap.cbegin(), free_heap_end, rfind);  

    /* look for contiguous free blocks to left */
    auto lfind = [&](_heap_pair_ty elem){ return (((_ptr_ty)elem.second + elem.first) == phead); };
    auto left = std::find_if(_free_heap.cbegin(), free_heap_end, lfind);

    if( (rght != free_heap_end) && (left != free_heap_end) ){
        sz = left->first + HEAD_SZ + head + rght->first;
        _free_heap.insert(_heap_pair_ty(sz,left->second));
        _free_heap.erase(rght);
        _free_heap.erase(left);
    }else if(rght != free_heap_end){
        sz = head + HEAD_SZ + rght->first;
        _free_heap.insert(_heap_pair_ty(sz, phead));
        _free_heap.erase(rght);
    }else if(left != free_heap_end){
        sz = left->first + HEAD_SZ + head;
        _free_heap.insert(_heap_pair_ty(sz,left->second));
        _free_heap.erase(left);
    }else{
        _free_heap.insert(_heap_pair_ty(head + HEAD_SZ, phead));
    }
  
    _allocated_set.erase((_ptr_ty)start); /* remove from allocated set */
    return true; 
}

size_t 
SlowHeapManager::size(void* start)
{
    if( !_valid_start(start) )
        return 0;

    /* backup to the header */
    _ptr_ty p = (_ptr_ty)start - HEAD_SZ; 

    /* get the value in the header */
    return *(_head_ty*)(p); 
}


const char* DynamicIPCBase::KMUTEX_NAME =   
#ifdef NO_KGBLNS
  "DynamicIPC_Master_MUTEX1";
#else
  "Global\\DynamicIPC_Master_MUTEX1";
#endif

const DynamicIPCBase::shem_chunk DynamicIPCBase::NULL_SHEM_CHUNK;

void* 
DynamicIPCBase::_allocate(size_t sz) const
{
    BOOL ret; 
    DWORD read = 0;
    size_t off = 0;
    size_t obuf[2] = {ALLOC, sz};     
  
    ret = CallNamedPipe(this->_intrnl_pipe_str.c_str(), 
                        (void*)obuf, sizeof(obuf),
                        (void*)&off, sizeof(off), 
                        &read, TOSDB_DEF_TIMEOUT);
    
    return ret ? ptr(off) : nullptr;    
}


bool 
DynamicIPCBase::_deallocate(void* start) const
{ 
    BOOL ret;
    DWORD read = 0;    
    size_t val = 0;
    size_t obuf[2] = {DEALLOC, offset(start)};   

    ret = CallNamedPipe(_intrnl_pipe_str.c_str(), 
                        (void*)obuf, sizeof(obuf),
                        (void*)&val, sizeof(val), 
                        &read, TOSDB_DEF_TIMEOUT);

    /* we are only comparing to '1' becase the listening loop casts
       the boolean return value of deallocate to an int */
    return ret ? val == 1 : false;                    
}


bool 
DynamicIPCBase::_send(const shem_chunk& item) const
{
    unsigned long d;  
    return WriteFile(_xtrnl_pipe_hndl, (void*)&item, sizeof(shem_chunk), &d, NULL);
}
 

bool 
DynamicIPCBase::_recv(shem_chunk& item) const 
{
    unsigned long d;   
    return ReadFile(_xtrnl_pipe_hndl, (void*)&item, sizeof(shem_chunk), &d, NULL);
}


const DynamicIPCBase& 
DynamicIPCBase::remove(shem_chunk&& chunk) const 
{        
    void* start = (void*)((size_t)_mmap_addr + chunk.offset);
    _deallocate(start);
    return *this;
}
    

/* 
    ***WARNING***

         The following ptr/offset operations are poorly implemented.
          
         BE VERY CAREFUL CHANGING how fields are cast, compared etc.
         You can create very subtle bugs in the IPC mechanisms.
         
    ***WARNING***                                                   
*/

void* 
DynamicIPCBase::shem_ptr(const shem_chunk& chunk) const
{  /* we check for overflow but NO GUARANTEE shem_chunk points at valid data */                
    if( chunk.sz <= 0 
        || !_mmap_addr
        || !((chunk.offset + chunk.sz) <= (size_t)_mmap_sz))
    { /* if blck is 'abnormal', no mem-map, or a read will overflow it */
        return nullptr;        
    }    
    return (void*)((size_t)_mmap_addr + chunk.offset);
}


void* 
DynamicIPCBase::ptr(size_t offset) const
{
    if(offset > (size_t)_mmap_sz || !_mmap_addr)
        return nullptr;

    return (void*)((size_t)_mmap_addr + offset);
}


/* even though chunk.offset is a long a 'logical' offset
   can only be positive; so we return size_t */
size_t 
DynamicIPCBase::offset(void* start) const    
{
    size_t ub = (size_t)_mmap_addr + (size_t)_mmap_sz;
    if(start < _mmap_addr || (size_t)start >= ub)
        return 0;                

    return ((size_t)start - (size_t)_mmap_addr); 
}


bool 
DynamicIPCBase::recv(long& val) const
{
    shem_chunk tmp(0,0);

    bool res = _recv(tmp);
    val = tmp.offset;
    return res;
}



#define IPC_MASTER_ERROR(msg) TOSDB_LogEx("IPC-Master",msg,GetLastError())

long 
DynamicIPCMaster::grab_pipe()
{
    long ret;
      
    _mtx = OpenMutex(SYNCHRONIZE, FALSE, KMUTEX_NAME);
    if(!_mtx){
        IPC_MASTER_ERROR("OpenMutex failed in grab_pipe"); 
        return -1; 
    }    
    
    if( WaitForSingleObject(_mtx,TOSDB_DEF_TIMEOUT) == WAIT_TIMEOUT )
    {
        IPC_MASTER_ERROR("WaitForSingleObject timed out in grab_pipe"); 
        CloseHandle(_mtx);
        return -1;    
    }
    
    /* does this need timeout ?? */
    if( !WaitNamedPipe(_xtrnl_pipe_str.c_str(),0) )
    {    
        IPC_MASTER_ERROR("WaitNamedPipe failed in grab_pipe");
        ReleaseMutex(_mtx);
        CloseHandle(_mtx);
        return -1;    
    }    
    
    _xtrnl_pipe_hndl = CreateFile( _xtrnl_pipe_str.c_str(), 
                                   GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                   NULL, OPEN_EXISTING, 
                                   FILE_ATTRIBUTE_NORMAL, NULL );    

    if(!_xtrnl_pipe_hndl || (_xtrnl_pipe_hndl == INVALID_HANDLE_VALUE))
    {
        IPC_MASTER_ERROR("No pipe handle(or CreateFile failed) in grab_pipe");
        ReleaseMutex(_mtx);
        CloseHandle(_mtx);
        return -1;
    }

    if( !recv(ret) )
        IPC_MASTER_ERROR("recv failed in grab_pipe");
    
    _pipe_held = true;

    return ret;
}


void 
DynamicIPCMaster::release_pipe()
{
    if(_pipe_held){
        CloseHandle(_xtrnl_pipe_hndl);
        _xtrnl_pipe_hndl = INVALID_HANDLE_VALUE;
        ReleaseMutex(_mtx);
        CloseHandle(_mtx);
    }

    _pipe_held = false;
}


bool 
DynamicIPCMaster::try_for_slave()
{    
    long pipe_ret;

    disconnect(); 
     
    pipe_ret = grab_pipe();
    if(pipe_ret <= 0){             
        IPC_MASTER_ERROR("grab_pipe in try_for_slave failed");
        disconnect(0);        
        return false;
    }else{ /* grab_pipe will return slaves _mmap_sz on sucess */
        _mmap_sz = pipe_ret;
    }
   
    _fmap_hndl = OpenFileMapping(FILE_MAP_WRITE, 0, _shem_str.c_str()); 
    if(!_fmap_hndl){
        IPC_MASTER_ERROR("OpenFileMapping in try_for_slave failed");
        release_pipe();
        disconnect(1);         
        return false;
    }

    _mmap_addr = MapViewOfFile(_fmap_hndl, FILE_MAP_WRITE, 0, 0, 0);
    if(!_mmap_addr){
        IPC_MASTER_ERROR("MapViewOfFile in try_for_slave failed");
        release_pipe();
        disconnect(2);
        return false;
    }    

    /* release connection AFTER FileMapping calls to avoid race with slave */
    release_pipe();    

    return true;
}

bool 
DynamicIPCMaster::connected() const 
{ 
    size_t obuf[2] = {PING, 999};
    size_t off = 0;
    DWORD read = 0;
    BOOL ret;

    ret = CallNamedPipe(_intrnl_pipe_str.c_str(), 
                        (void*)obuf, sizeof(obuf),
                        (void*)&off, sizeof(off), 
                        &read, TOSDB_DEF_TIMEOUT);
                
    return (_mmap_addr && _fmap_hndl && ret && (off == obuf[1]));    
}


void 
DynamicIPCMaster::disconnect(int level)
{    
    switch(level){    
    case 3:
    {
        if(_mmap_addr){                     
            if( !UnmapViewOfFile(_mmap_addr) )
                IPC_MASTER_ERROR("UnmapViewOfFile in disconnect failed"); 
        }                
        _mmap_addr = NULL;     
    }
    /* NO BREAK */
    case 2: 
        CloseHandle(_fmap_hndl);        
        _fmap_hndl = NULL;  
    /* NO BREAK */
    case 1:
        release_pipe();   
    }            
}


#define IPC_SLAVE_ERROR(msg) TOSDB_LogEx("IPC-Slave",msg,GetLastError())

#define IPC_SLAVE_GOOD_RET_OR_THROW(r, msg) do{ \
    if(!(r)){ \
        IPC_SLAVE_ERROR(msg); \
        throw std::runtime_error(msg); \
    } \
}while(0)

DynamicIPCSlave::DynamicIPCSlave(std::string name, size_t sz)
    :     
        _alloc_flag(true),
        DynamicIPCBase(name, sz)
    { 
        /* allocate underlying objects */
        for(int i = 0; i < NSECURABLE; ++i){ 
            _sec_attr[(Securable)i] = SECURITY_ATTRIBUTES();
            _sec_desc[(Securable)i] = SECURITY_DESCRIPTOR();    
            _sids[(Securable)i] = SmartBuffer<void>(SECURITY_MAX_SID_SIZE);
            _acls[(Securable)i] = SmartBuffer<ACL>(ACL_SIZE);        
        }         

        /* initialize object security */
        int sec_ret = _set_security(); 
        IPC_SLAVE_GOOD_RET_OR_THROW(sec_ret == 0, "DynamicIPCSlave failed to initialize security");
             
        _mtx = CreateMutex(&(_sec_attr[MUTEX1]), FALSE, KMUTEX_NAME);
        IPC_SLAVE_GOOD_RET_OR_THROW(_mtx, "DynamicIPCSlave failed to create mutex");
               
        _intrnl_pipe_hndl = CreateNamedPipe(_intrnl_pipe_str.c_str(), 
                                            PIPE_ACCESS_DUPLEX,
                                            PIPE_TYPE_MESSAGE 
                                                | PIPE_READMODE_BYTE 
                                                | PIPE_WAIT,
                                            PIPE_UNLIMITED_INSTANCES, 0, 0,
                                            INFINITE, &_sec_attr[PIPE1]);    
     
        IPC_SLAVE_GOOD_RET_OR_THROW(_intrnl_pipe_hndl,"DynamicIPCSlave failed to create named pipe");        
                
        /* get handle to page file */
        _fmap_hndl = CreateFileMapping( INVALID_HANDLE_VALUE, 
                                        &(_sec_attr[SHEM1]), 
                                        PAGE_READWRITE, 
                                        (unsigned long long)_mmap_sz >> 32,
                                        ((unsigned long long)_mmap_sz << 32) >>32,
                                        _shem_str.c_str() );    

        IPC_SLAVE_GOOD_RET_OR_THROW(_fmap_hndl, "DynamicIPCSlave failed to create file mapping");
             
        _mmap_addr = MapViewOfFile(_fmap_hndl, FILE_MAP_WRITE, 0, 0, 0);
        IPC_SLAVE_GOOD_RET_OR_THROW(_mmap_addr, "DynamicIPCSlave failed to map view of file");        
    
        /* consturct our heap for managing the mappings allocatable space */
        SlowHeapManager* ehp = new SlowHeapManager(_mmap_addr, _mmap_sz);
        IPC_SLAVE_GOOD_RET_OR_THROW(ehp, "DynamicIPCSlave failed to allocate SlowHeapManager");
        
        _uptr_heap = std::unique_ptr<SlowHeapManager>(ehp);
        
        /* create a listener thread for allocs/deallocs */
        std::async(std::launch::async, [this]{ _listen_for_alloc(); }); 
    }    


DynamicIPCSlave::~DynamicIPCSlave()
    {   
        HANDLE tmp;

        _alloc_flag = false;   

        /* can't remember why we are doing it this way ?? */
        tmp = CreateFile(_intrnl_pipe_str.c_str(), 
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        CloseHandle(tmp);  
        CloseHandle(_mtx);     
    
        if( !UnmapViewOfFile(_mmap_addr) )        
            IPC_SLAVE_ERROR("UnmapViewOfFile in slave destructor failed");          
        
        CloseHandle(_fmap_hndl);            
        DisconnectNamedPipe(_xtrnl_pipe_hndl);
        CloseHandle(_xtrnl_pipe_hndl);            
    }


void 
DynamicIPCSlave::_listen_for_alloc()
{
    size_t args[2];
    DWORD done = 0;    
    void* start = nullptr;                    

    while(_alloc_flag){  

        if( !ConnectNamedPipe(_intrnl_pipe_hndl, NULL) )
            TOSDB_LogH("IPC", "ConnectNamedPipe failed in _listen_for_alloc");

        if( !ReadFile(_intrnl_pipe_hndl, (void*)&args, sizeof(args), &done, NULL) )
            TOSDB_LogH("IPC", "ReadFile failed in _listen_for_alloc");

        switch(args[0]){
        case ALLOC: /* ALLOCATE */                        
            start = _uptr_heap->allocate(args[1]);
            args[1] = offset(start);
            WriteFile(_intrnl_pipe_hndl, (void*)&args[1], sizeof(args[1]), &done, NULL);   
            break;        

        case DEALLOC: /* DEALLOCATE */                        
            args[1] = (size_t)(_uptr_heap->deallocate(ptr(args[1])));
            WriteFile(_intrnl_pipe_hndl, (void*)&args[1], sizeof(args[1]), &done, NULL);
            break;         

        case PING:          
            /* no mem map, send back (any) different val */
            if(!_mmap_addr || !_fmap_hndl)               
                --(args[1]); 
            
            WriteFile(_intrnl_pipe_hndl, (void*)&args[1], sizeof(args[1]), &done, NULL);
            break;        
          
        default:
            TOSDB_LogH("IPC-Slave", "invalid opcode passed to _listen_for_alloc()");            
            break;
        } 

        if( !DisconnectNamedPipe(_intrnl_pipe_hndl) )
            TOSDB_LogH("IPC", "DisconnectNamedPipe failed in _listen_for_alloc");
    }    
    CloseHandle(_intrnl_pipe_hndl);
}


bool 
DynamicIPCSlave::wait_for_master()
{    
    if(_xtrnl_pipe_hndl && _xtrnl_pipe_hndl != INVALID_HANDLE_VALUE){
        DisconnectNamedPipe(_xtrnl_pipe_hndl);    
    }else{   
        _xtrnl_pipe_hndl = CreateNamedPipe( _xtrnl_pipe_str.c_str(), 
                                            PIPE_ACCESS_DUPLEX,
                                            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                            1, 0, 0, INFINITE, 
                                            &(_sec_attr[PIPE1]) );
    }

    if(!_xtrnl_pipe_hndl || _xtrnl_pipe_hndl == INVALID_HANDLE_VALUE){
        IPC_SLAVE_ERROR("CreateNamedPipe failed in wait_for_master");            
        return false;
    }    

    ConnectNamedPipe(_xtrnl_pipe_hndl, NULL);    

    if( !send((long)_mmap_sz) ) /* OK: _mmap_sz < 2**31 - 1) */
        IPC_SLAVE_ERROR("send failed in wait_for_master");

    return true;
}

int DynamicIPCSlave::
_set_security()
{    
    SID_NAME_USE dummy;
    BOOL ret; /* int */
    DWORD dom_sz = 128;
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;

    SmartBuffer<char> dom_buf(dom_sz);
    SmartBuffer<void> sid(sid_sz);
  
    ret = LookupAccountName(NULL, "Everyone", sid.get(), &sid_sz, dom_buf.get(), &dom_sz, &dummy);
    if(!ret)
        return -1;

    for(int i = 0; i < NSECURABLE; ++i){ 
        _sec_attr[(Securable)i].nLength = sizeof(SECURITY_ATTRIBUTES);
        _sec_attr[(Securable)i].bInheritHandle = FALSE;
        _sec_attr[(Securable)i].lpSecurityDescriptor = &(_sec_desc[(Securable)i]);

        /* memcpy 'TRUE' is error */
        ret = memcpy_s(_sids[(Securable)i].get(), SECURITY_MAX_SID_SIZE, sid.get(), SECURITY_MAX_SID_SIZE);
        if(ret)
            return -2; 

        ret = InitializeSecurityDescriptor(&(_sec_desc[(Securable)i]), SECURITY_DESCRIPTOR_REVISION);
        if(!ret)
            return -3;

        ret = SetSecurityDescriptorGroup(&(_sec_desc[(Securable)i]), _sids[(Securable)i].get(), FALSE);
        if(!ret)
            return -4;

        ret = InitializeAcl(_acls[(Securable)i].get(), ACL_SIZE, ACL_REVISION);
        if(!ret)
            return -5;
    }
                           
    /* add ACEs individually */

    ret = AddAccessAllowedAce(_acls[SHEM1].get(), ACL_REVISION, FILE_MAP_WRITE, _sids[SHEM1].get()); 
    if(!ret)
        return -6;

    ret = AddAccessAllowedAce(_acls[PIPE1].get(), ACL_REVISION, FILE_GENERIC_WRITE, _sids[PIPE1].get());
    if(!ret)
        return -6;

    ret = AddAccessAllowedAce(_acls[PIPE1].get(), ACL_REVISION, FILE_GENERIC_READ, _sids[PIPE1].get()); 
    if(!ret)
        return -6;

    ret = AddAccessAllowedAce(_acls[MUTEX1].get(), ACL_REVISION, SYNCHRONIZE, _sids[MUTEX1].get());
    if(!ret)
        return -6;

    for(int i = 0; i < NSECURABLE; ++i){
        ret = SetSecurityDescriptorDacl(&(_sec_desc[(Securable)i]), TRUE, _acls[(Securable)i].get(), FALSE);
        if(!ret)
            return -7;
    }
   
    return 0;
}
