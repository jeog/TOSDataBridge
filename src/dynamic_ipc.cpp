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
#include "dynamic_ipc.hpp"
#include <fstream>
#include <iomanip>

void* ExplicitHeap::allocate(size_type size)
{  
  _ptr_type blck;  
  size_type rmndr;

  size_type width = HEAD_SZ + size;
  size_type snglton[1] = {width};

  auto free_heap_end = _free_heap.cend();

  /* check for space on free heap */
  auto found = std::find_first_of(_free_heap.cbegin(), free_heap_end,
                                  snglton, snglton + 1,
                                  [&](_heap_pair_type elem, size_type val){ 
                                    return (elem.first >= val); 
                                  });  
  if(found == free_heap_end) 
    throw std::bad_alloc();
  else{         
    blck = found->second;      
    rmndr = found->first - width;
    if(rmndr){ 
      /* if we are not taking the whole block */
      _free_heap.insert(_heap_pair_type(rmndr, found->second + width));  
    }
    _free_heap.erase(found); /* remove old block */
    *(_head_type*)blck = size;   /* add header */
    _allocated_set.insert(blck + HEAD_SZ); /* add to allocated set */

    return (void*)(blck + HEAD_SZ); /* return header-adjusted block addr */
  } 
}  

bool ExplicitHeap::deallocate(void* start)
{   
  if(!_valid_start(start))
    return false;

  _ptr_type phead = (_ptr_type)start - HEAD_SZ; /* back up to the header */
  _head_type head = *(_head_type*)(phead); /* get the value in the header */  

  auto free_heap_end = _free_heap.cend();  

  /* look for contiguous free blocks to right */
  auto rght = std::find_if(_free_heap.cbegin(), free_heap_end,
                           [&](_heap_pair_type elem){
                             return (elem.second == ((_ptr_type)start + head));
                           });  
  /* look for contiguous free blocks to left */
  auto left = std::find_if(_free_heap.cbegin(), free_heap_end,
                           [&](_heap_pair_type elem){
                             return (((_ptr_type)elem.second + elem.first) 
                                    == phead);
                           });

  if((rght != free_heap_end) && (left != free_heap_end))
  {
    _free_heap.insert(
      _heap_pair_type(left->first + HEAD_SZ + head + rght->first,left->second));
    _free_heap.erase(rght);
    _free_heap.erase(left);
  }
  else if(rght != free_heap_end)
  {
    _free_heap.insert(_heap_pair_type(head + HEAD_SZ + rght->first, phead));
    _free_heap.erase(rght);
  }
  else if(left != free_heap_end)
  {
    _free_heap.insert(_heap_pair_type(left->first+HEAD_SZ+head,left->second));
    _free_heap.erase(left);
  }
  else
    _free_heap.insert(_heap_pair_type(head + HEAD_SZ, phead));
  
  _allocated_set.erase((_ptr_type)start); /* remove from allocated set */
  return true; 
}

size_type ExplicitHeap::size(void* start)
{
  if(!_valid_start(start))
    return 0;
  /* backup to the header */
  _ptr_type p = (_ptr_type)start - HEAD_SZ; 
  /* get the value in the header */
  return *(_head_type*)(p); 
}

#ifdef KGBLNS_
const char* DynamicIPCBase::KMUTEX_NAME = "Global\\DynamicIPC_Master_MUTEX1";
#else
const char* DynamicIPCBase::KMUTEX_NAME = "DynamicIPC_Master_MUTEX1";
#endif

void* DynamicIPCBase::_allocate(size_type sz) const
{
  size_type obuf[2];
  obuf[0] = ALLOC;
  obuf[1] = sz;

  size_type off = 0;
  DWORD read = 0;
  
  return CallNamedPipe(_intrnl_pipe_str.c_str(), (void*)obuf, sizeof(obuf),
                       (void*)&(off), sizeof(off), &read, TOSDB_DEF_TIMEOUT)               
                         ? ptr(off) 
                         : nullptr;
}

bool DynamicIPCBase::_deallocate(void* start) const
{ 
  size_type obuf[2];
  obuf[0] = DEALLOC;
  obuf[1] = offset(start);

  size_type val = 0;
  DWORD read = 0;
   
  return CallNamedPipe(_intrnl_pipe_str.c_str(), (void*)obuf, sizeof(obuf),
                       (void*)&(val), sizeof(val), &read, TOSDB_DEF_TIMEOUT)             
                         ? (val == TRUE) 
                         : false;
}

bool DynamicIPCBase::_send(const shem_chunk& item) const
{
  unsigned long d; 
  unsigned long sz = sizeof(shem_chunk);
 
  return WriteFile(_xtrnl_pipe_hndl, (void*)&item, sz, &d, NULL);
}
  
bool DynamicIPCBase::_recv(shem_chunk& item) const 
{
  unsigned long d;  
  unsigned long sz = sizeof(shem_chunk);
 
  return ReadFile(_xtrnl_pipe_hndl, (void*)&item, sz, &d, NULL);
}

int DynamicIPCMaster::grab_pipe()
{
  int ret;

  _mtx = OpenMutex(SYNCHRONIZE, FALSE, KMUTEX_NAME);
  if(!_mtx){
    TOSDB_LogEx("IPC-Master","OpenMutex failed in grab_pipe()",GetLastError()); 
    return -1; 
  }  
  
  if(WaitForSingleObject(_mtx,TOSDB_DEF_TIMEOUT) == WAIT_TIMEOUT)
  {
    TOSDB_LogEx("IPC-Master", 
                "WaitForSingleObject() [_mtx] timed out in grab_pipe()", 
                GetLastError()); 
    CloseHandle(_mtx);
    return -1;  
  }
  
  /* does this need timeout ?? */
  if(!WaitNamedPipe(_xtrnl_pipe_str.c_str(),0))
  {  
    TOSDB_LogEx("IPC-Master", "WaitNamedPipe() failed in grab_pipe()", 
                GetLastError());
    ReleaseMutex(_mtx);
    CloseHandle(_mtx);
    return -1;  
  }  
  
  _xtrnl_pipe_hndl = 
    CreateFile(_xtrnl_pipe_str.c_str(), GENERIC_READ | GENERIC_WRITE,
               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 
               FILE_ATTRIBUTE_NORMAL, NULL);  

  if(!_xtrnl_pipe_hndl 
     || (_xtrnl_pipe_hndl == INVALID_HANDLE_VALUE))
    {
      TOSDB_LogEx("IPC-Master", 
                  "No pipe handle(or CreateFile() failed) in grab_pipe()", 
                  GetLastError());
      ReleaseMutex(_mtx);
      CloseHandle(_mtx);
      return -1;
    }

  if(!(this->recv(ret)))
    TOSDB_LogEx("IPC-Master", "->recv() failed in grab_pipe()",GetLastError());
  
  _pipe_held = true;
  return ret;
}

void DynamicIPCMaster::release_pipe()
{
  if(_pipe_held)
  {
    CloseHandle(_xtrnl_pipe_hndl);
    _xtrnl_pipe_hndl = INVALID_HANDLE_VALUE;
    ReleaseMutex(_mtx);
    CloseHandle(_mtx);
  }
  _pipe_held = false;
}

bool DynamicIPCMaster::try_for_slave()
{  
  disconnect(); 
   
  _mmap_sz = grab_pipe();
  if(_mmap_sz <= 0)
  {       
    TOSDB_LogEx("IPC-Master", "grab_pipe() in try_for_slave() failed", 
                 GetLastError());
    disconnect(0);    
    return false;
  }
  /* 
   * WE HAD A RACE CONDITION HERE WHERE THE SLAVE DELETES SHARED MEM FIRST;  
   * SHOULD BE O.K. NOW 
   */
  _fmap_hndl = OpenFileMapping(FILE_MAP_WRITE, 0, _shem_str.c_str()); 
  if(!_fmap_hndl)
  {
    TOSDB_LogEx("IPC-Master", "OpenFileMapping() in try_for_slave() failed", 
                GetLastError());
    release_pipe();
    disconnect(1);     
    return false;
  }

  _mmap_addr = MapViewOfFile(_fmap_hndl, FILE_MAP_WRITE, 0, 0, 0);
  if(!_mmap_addr)
  {
    TOSDB_LogEx("IPC-Master", "MapViewOfFile() in try_for_slave() failed", 
                GetLastError());
    release_pipe();
    disconnect(2);
    return false;
  }  
  /* release connection AFTER FileMapping calls to avoid race with slave */
  release_pipe();  
  return true;
}

bool DynamicIPCMaster::connected() const 
{ 
  size_type obuf[2];
  obuf[0] = PING;
  obuf[1] = 999;

  size_type off = 0;
  DWORD read = 0;
  BOOL res = CallNamedPipe(_intrnl_pipe_str.c_str(), (void*)obuf, sizeof(obuf),
                          (void*)&(off), sizeof(off), &read, TOSDB_DEF_TIMEOUT);

  return (_mmap_addr && _fmap_hndl && res && (off == obuf[1]));  
}

void DynamicIPCMaster::disconnect(int level)
{  
  switch(level){  
  case 3:
  {
    if(_mmap_addr)           
      if(!UnmapViewOfFile(_mmap_addr))
      {
        TOSDB_LogEx("IPC-Master",
                    "UnmapViewOfFile in DynamicIPCMaster::disconnect() failed", 
                    GetLastError()); 
      }        
    _mmap_addr = NULL;   
  }
  case 2: 
    CloseHandle(_fmap_hndl);    
    _fmap_hndl = NULL;  
  case 1:
    release_pipe();
  default:  
    break;
  }      
}

DynamicIPCSlave::DynamicIPCSlave(std::string name, int sz)
  :   
    _alloc_flag(true),
    DynamicIPCBase(name, sz)
  { 
    for(int i = 0; i < 3; ++i)
    { 
      _sec_attr[(Securable)i] = SECURITY_ATTRIBUTES();
      _sec_desc[(Securable)i] = SECURITY_DESCRIPTOR();  
      _sids[(Securable)i] = SmartBuffer<void>(SECURITY_MAX_SID_SIZE);
      _acls[(Securable)i] = SmartBuffer<ACL>(ACL_SIZE);    
    }                   
    _set_security();                    

    _mtx = CreateMutex(&_sec_attr[MUTEX1], FALSE, KMUTEX_NAME);
    if(!_mtx){
      TOSDB_LogEx("IPC-Slave","CreateMutex() in slave constructor failed", 
                  GetLastError());    
    }
    
    _intrnl_pipe_hndl = 
      CreateNamedPipe(_intrnl_pipe_str.c_str(), PIPE_ACCESS_DUPLEX,
                      PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                      PIPE_UNLIMITED_INSTANCES,0,0,INFINITE, &_sec_attr[PIPE1]);  
   
    if(!_intrnl_pipe_hndl)
    {
      TOSDB_LogEx("IPC-Slave", 
                  "CreateNamedPipe()[internal] in slave constructor failed", 
                  GetLastError());
    }    
        
    _fmap_hndl = /* get handle to page file */
      CreateFileMapping(INVALID_HANDLE_VALUE, &_sec_attr[SHEM1], PAGE_READWRITE, 
                        (unsigned long long)_mmap_sz >>32,
                        ((unsigned long long)_mmap_sz <<32) >>32,
                        _shem_str.c_str());  

    if(!_fmap_hndl){
      TOSDB_LogEx("IPC-Slave", "CreateFileMapping() in slave constructor failed", 
                  GetLastError());     
    }
   
    _mmap_addr = MapViewOfFile(_fmap_hndl, FILE_MAP_WRITE, 0, 0, 0);
    if(!_mmap_addr){
      TOSDB_LogEx("IPC-Slave", "MapViewOfFile() in slave constructor failed", 
                  GetLastError());    
    }
  
    /* consturct our heap for managing the mappings allocatable space */
    _uptr_heap =  
      std::unique_ptr<ExplicitHeap>(new ExplicitHeap(_mmap_addr, _mmap_sz));
    
    /* create a listener thread for allocs/deallocs */
    std::async(std::launch::async, [this]{ _listen_for_alloc(); }); 
  }  

DynamicIPCSlave::~DynamicIPCSlave()
  {  
    _alloc_flag = false;      
    
    CloseHandle( /* !! TROUBLE written all over this !! */
      CreateFile(_intrnl_pipe_str.c_str(), GENERIC_READ | GENERIC_WRITE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));  

    CloseHandle(_mtx);   
  
    if(!UnmapViewOfFile(_mmap_addr)){
      TOSDB_LogEx("IPC-Slave", "UnmapViewOfFile() in slave destructor failed", 
                  GetLastError());      
    }
    CloseHandle(_fmap_hndl);      
    DisconnectNamedPipe(_xtrnl_pipe_hndl);
    CloseHandle(_xtrnl_pipe_hndl);      
  }

void DynamicIPCSlave::_listen_for_alloc()
{
  size_type args[2];

  DWORD done = 0;  
  void* start = nullptr;          

  while(_alloc_flag)
  {  
    ConnectNamedPipe(_intrnl_pipe_hndl, NULL);
    ReadFile(_intrnl_pipe_hndl, (void*)&args, sizeof(args), &done, NULL);

    switch(args[0]){
    case ALLOC:
    {        
      start = _uptr_heap->allocate(args[1]);
      args[1] = offset(start);
      WriteFile(_intrnl_pipe_hndl,(void*)&args[1],sizeof(args[1]),&done,NULL);        
    } break;
    case DEALLOC:
    {        
      args[1] = (size_type)_uptr_heap->deallocate(ptr(args[1]));
      WriteFile(_intrnl_pipe_hndl,(void*)&args[1],sizeof(args[1]),&done,NULL);
    } break;
    case PING:
    {        
      if(!_mmap_addr || !_fmap_hndl){
        /* no mmap send back a different val */
        --(args[1]); 
      }
      WriteFile(_intrnl_pipe_hndl,(void*)&args[1],sizeof(args[1]),&done,NULL);
    } break;      
    default:
      TOSDB_LogH("IPC-Slave", "Invalid OpCode passed to _listen_for_alloc()");      
      break;
    } 
    DisconnectNamedPipe(_intrnl_pipe_hndl);
  }  
  CloseHandle(_intrnl_pipe_hndl);
}

bool DynamicIPCSlave::wait_for_master()
{  
  if(!_xtrnl_pipe_hndl 
     || _xtrnl_pipe_hndl == INVALID_HANDLE_VALUE)
    {
      _xtrnl_pipe_hndl = 
        CreateNamedPipe(_xtrnl_pipe_str.c_str(), PIPE_ACCESS_DUPLEX,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        1, 0, 0, INFINITE, &_sec_attr[PIPE1]); 
    }
  else
    {
      DisconnectNamedPipe(_xtrnl_pipe_hndl);
    }  

  if(!_xtrnl_pipe_hndl 
     || _xtrnl_pipe_hndl == INVALID_HANDLE_VALUE)
    {
      TOSDB_LogEx("IPC-Slave","CreateNamedPipe() failed in wait_for_master()", 
                  GetLastError());      
      return false;
    }  

  ConnectNamedPipe(_xtrnl_pipe_hndl, NULL); 
  
  if(!(this->send(_mmap_sz)))
  { 
    TOSDB_LogEx("IPC-Slave", "->send() failed in wait_for_master()", 
                GetLastError());   
  }  
  return true;
}

int DynamicIPCSlave::_set_security()
{  
  SID_NAME_USE dummy;

  DWORD        dom_sz = 128;
  DWORD        sid_sz = SECURITY_MAX_SID_SIZE;

  SmartBuffer<char>  dom_buf(dom_sz);
  SmartBuffer<void>  sid(sid_sz);
    
  _sec_attr[SHEM1].nLength = 
    _sec_attr[PIPE1].nLength = 
      _sec_attr[MUTEX1].nLength = sizeof(SECURITY_ATTRIBUTES);

  _sec_attr[SHEM1].bInheritHandle = 
    _sec_attr[PIPE1].bInheritHandle = 
      _sec_attr[MUTEX1].bInheritHandle = FALSE;

  _sec_attr[SHEM1].lpSecurityDescriptor = &_sec_desc[SHEM1];
  _sec_attr[PIPE1].lpSecurityDescriptor = &_sec_desc[PIPE1];
  _sec_attr[MUTEX1].lpSecurityDescriptor = &_sec_desc[MUTEX1];

  if(!LookupAccountName(NULL, "Everyone", sid.get(), &sid_sz, dom_buf.get(), 
                        &dom_sz, &dummy))
    {
      return -1;
    }
   

  if(memcpy_s(_sids[SHEM1].get(), SECURITY_MAX_SID_SIZE, sid.get(), 
              SECURITY_MAX_SID_SIZE) 
     || memcpy_s(_sids[PIPE1].get(), SECURITY_MAX_SID_SIZE, sid.get(), 
                 SECURITY_MAX_SID_SIZE) 
     || memcpy_s(_sids[MUTEX1].get(), SECURITY_MAX_SID_SIZE, sid.get(), 
                 SECURITY_MAX_SID_SIZE))
    {   
      return -2;
    }

  if(!InitializeSecurityDescriptor(&_sec_desc[SHEM1],
                                   SECURITY_DESCRIPTOR_REVISION) 
     || !InitializeSecurityDescriptor(&_sec_desc[PIPE1], 
                                      SECURITY_DESCRIPTOR_REVISION) 
     || !InitializeSecurityDescriptor(&_sec_desc[MUTEX1], 
                                      SECURITY_DESCRIPTOR_REVISION))
    {
      return -3; 
    }

  if(!SetSecurityDescriptorGroup(&_sec_desc[SHEM1], _sids[SHEM1].get(), FALSE) 
     || !SetSecurityDescriptorGroup(&_sec_desc[PIPE1],_sids[PIPE1].get(),FALSE) 
     || !SetSecurityDescriptorGroup(&_sec_desc[MUTEX1],_sids[MUTEX1].get(),FALSE))
    { 
      return -4;
    }

  if(!InitializeAcl(_acls[SHEM1].get(), ACL_SIZE, ACL_REVISION) 
     || !InitializeAcl(_acls[PIPE1].get(), ACL_SIZE, ACL_REVISION) 
     || !InitializeAcl(_acls[MUTEX1].get(), ACL_SIZE, ACL_REVISION))
    {
        return -5; 
    }
                                  
  if(!AddAccessAllowedAce(_acls[SHEM1].get(), ACL_REVISION, FILE_MAP_WRITE, 
                          _sids[SHEM1].get()) 
     || !AddAccessAllowedAce(_acls[PIPE1].get(), ACL_REVISION, 
                             FILE_GENERIC_WRITE, _sids[PIPE1].get()) 
     || !AddAccessAllowedAce(_acls[PIPE1].get(), ACL_REVISION, 
                             FILE_GENERIC_READ, _sids[PIPE1].get()) 
     || !AddAccessAllowedAce(_acls[MUTEX1].get(), ACL_REVISION, 
                             SYNCHRONIZE, _sids[MUTEX1].get()))
    {
      return -6;
    }

  if(!SetSecurityDescriptorDacl(&_sec_desc[SHEM1], TRUE,
                                _acls[SHEM1].get(), FALSE) 
     || !SetSecurityDescriptorDacl(&_sec_desc[PIPE1], TRUE, 
                                   _acls[PIPE1].get(), FALSE) 
     || !SetSecurityDescriptorDacl(&_sec_desc[MUTEX1], TRUE, 
                                   _acls[MUTEX1].get(), FALSE))
    {
      return -7;
    }

  return 0;
}
