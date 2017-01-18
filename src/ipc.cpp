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
#include "concurrency.hpp"
#include "ipc.hpp"

#include <fstream>
#include <iomanip>


bool 
IPCBase::send(std::string msg) const
{
    DWORD d; 

    if(msg.length() > MAX_MESSAGE_SZ)
        throw std::runtime_error("IPCBase::send() - msg length > MAX_MESSAGE_SZ");

    BOOL ret = WriteFile( _main_channel_pipe_hndl, 
                         (void*)msg.c_str(), msg.length() + 1, &d, NULL );

    if(!ret){
        errno_t e = GetLastError();     
        /* we should expect broken pipes */
        if(e == ERROR_BROKEN_PIPE)            
            TOSDB_LogDebug("***IPC*** SEND - (BROKEN_PIPE)");
        else
            TOSDB_LogEx("IPC", "WriteFile failed in _recv()", e);      
    }

    return ret;
}
 

bool 
IPCBase::recv(std::string *msg) const 
{    
    DWORD d;   
    
    msg->clear();
    msg->resize(MAX_MESSAGE_SZ); 
    
    BOOL ret = ReadFile( _main_channel_pipe_hndl, 
                         (void*)msg->c_str(), MAX_MESSAGE_SZ + 1, &d, NULL );

    if(!ret){
        errno_t e = GetLastError();
        /* we should expect broken pipes */
        if(e == ERROR_BROKEN_PIPE)            
            TOSDB_LogDebug("***IPC*** RECEIVE - (BROKEN_PIPE)");
        else
            TOSDB_LogEx("IPC", "ReadFile failed in _recv()", e);   
    }

    return ret;
}


bool 
IPCBase::connected(unsigned long timeout) 
{ 
    uint8_t i = PROBE_BYTE;
    uint8_t o = 0;  
    DWORD r = 0;
    BOOL ret;

    if( !_probe_channel_mtx.try_lock( /* do we always want to log this stuff ? */
            timeout,
            //[](errno_t e){ TOSDB_LogEx("IPC", "failed to open mutex in connected", e); },
            nullptr,
            [](){ TOSDB_LogH("IPC", "timed out waiting to lock mutex in connected"); },
            [](errno_t e){ TOSDB_LogEx("IPC", "failed to lock mutex in connected", e); } ))
    {
        return false;
    }

    /*** CRITICAL SECTION ***/   
    try{     
        ret = CallNamedPipe(_probe_channel_pipe_name.c_str(), 
                            (void*)&i, sizeof(i), (void*)&o, sizeof(o), &r, timeout);     
        
        if(ret == 0){       
            /* GetLastError needs to get called before we try to unlock */
            errno_t e = GetLastError();      
            _probe_channel_mtx.unlock();
            /* if the slave hasn't created the pipe DONT log*/
            if(e != ERROR_FILE_NOT_FOUND) 
                TOSDB_LogEx("IPC", "CallNamedPipe failed in connected()", e);    
            return false;
        }        
    }catch(std::exception& e){
        _probe_channel_mtx.unlock();
        TOSDB_LogH("IPC", ("exception caught in connected: " + std::string(e.what())).c_str());           
        return false;
    }    
    /*** CRITICAL SECTION ***/

    _probe_channel_mtx.unlock();    
    return (o == PROBE_BYTE) && (r == 1); 
}


bool
IPCMaster::call(std::string *msg, unsigned long timeout)
{
    bool ret_stat;

    if( !_grab_pipe(timeout) ){
        TOSDB_LogH("IPC-MASTER", "_grab_pipe failed in call()");
        return false;
    }

    ret_stat = send(*msg);
    if(!ret_stat){       
        TOSDB_LogH("IPC-Master", ("send() failed in call(), msg: " + *msg).c_str() );        
        return false;
    }

    msg->clear();
    ret_stat = recv(msg);
    if(!ret_stat){
        TOSDB_LogH("IPC-Master", ("recv() failed in call(), msg: " + *msg).c_str() );        
        return false;
    } 

    _release_pipe();
    return true;
}


bool
IPCMaster::_grab_pipe(unsigned long timeout)
{   
    if(_pipe_held)
        return true;

    if( !_main_channel_mtx.try_lock(
            timeout,
            [](errno_t e){ TOSDB_LogEx("IPC", "failed to open mutex in _grab_pipe", e); },
            [](){ TOSDB_LogH("IPC", "timed out waiting to lock mutex in _grab_pipe"); },
            [](errno_t e){ TOSDB_LogEx("IPC", "failed to lock mutex in _grab_pipe", e); } ))
    {
        return false;
    }

    try{  
        if( !WaitNamedPipe(_main_channel_pipe_name.c_str(), timeout) )
        {    
            errno_t e = GetLastError(); 
            _main_channel_mtx.unlock();   
            if(e == ERROR_SEM_TIMEOUT)
                TOSDB_LogH("IPC-Master", "WaitNamedPipe timed out in _grab_pipe");
            else
                TOSDB_LogEx("IPC-Master", "WaitNamedPipe failed in _grab_pipe", e);             
            return false;    
        }    
    
        _main_channel_pipe_hndl = CreateFile( _main_channel_pipe_name.c_str(), 
                                              GENERIC_READ | GENERIC_WRITE,
                                              FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                              NULL, OPEN_EXISTING, 
                                              FILE_ATTRIBUTE_NORMAL, NULL );    

        if(_main_channel_pipe_hndl == INVALID_HANDLE_VALUE){
            errno_t e = GetLastError(); 
            _main_channel_mtx.unlock(); 
            TOSDB_LogEx("IPC-Master", "CreaetFile failed in _grab_pipe", e);                                    
            return false;
        }
        _pipe_held = true;

    }catch(std::exception& e){        
        _release_pipe();        
        std::string msg = "exception caught in _grab_pipe: " + std::string(e.what());
        TOSDB_LogH("IPC", msg.c_str());        
    }   
     
    return _pipe_held;
}


void 
IPCMaster::_release_pipe()
{
    if(_pipe_held && (_main_channel_pipe_hndl != INVALID_HANDLE_VALUE))
    {        
        CloseHandle(_main_channel_pipe_hndl);
        _main_channel_pipe_hndl = INVALID_HANDLE_VALUE;     
    }
    _main_channel_mtx.unlock();
    _pipe_held = false;
}


bool 
IPCSlave::wait_for_master()
{    
    if(_main_channel_pipe_hndl == INVALID_HANDLE_VALUE){    
        /* if first time here create the pipe */
        _main_channel_pipe_hndl = CreateNamedPipe( _main_channel_pipe_name.c_str(), 
                                                   PIPE_ACCESS_DUPLEX,
                                                   PIPE_TYPE_MESSAGE 
                                                       | PIPE_READMODE_MESSAGE 
                                                       | PIPE_WAIT,
                                                   1, 0, 0, INFINITE, &(_sec_attr[PIPE1]) );
        
        if(_main_channel_pipe_hndl == INVALID_HANDLE_VALUE){          
            TOSDB_LogEx("IPC-Slave", "CreateNamedPipe failed in wait_for_master", GetLastError());        
            return false;
        }   

    }else{ /* otherwise just disconnect */
        DisconnectNamedPipe(_main_channel_pipe_hndl);
    }

    /* BLOCK until master calls _grab_pipe() */
    if( !ConnectNamedPipe(_main_channel_pipe_hndl, NULL) )
    {       
        TOSDB_LogEx("IPC-Slave", "ConnectNamedPipe failed in wait_for_master", GetLastError());        
        return false;
    }

    return true;
}

HANDLE IPCSlave::_probe_channel::_pipe_hndl = INVALID_HANDLE_VALUE;

bool IPCSlave::_probe_channel::_run_flag = false;

void
IPCSlave::_probe_channel::launch(std::string pipe_name, SECURITY_ATTRIBUTES* sa)
{    
    _pipe_hndl = CreateNamedPipe( pipe_name.c_str(), PIPE_ACCESS_DUPLEX,
                                  PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                  1, 0, 0, INFINITE, sa );    
     
    if(_pipe_hndl == INVALID_HANDLE_VALUE){        
        TOSDB_LogEx("IPC-Slave", "IPCSlave failed to create named pipe", GetLastError());
        throw std::runtime_error("IPCSlave failed to create named pipe");
    }  

    _run_flag = true;
    std::async(std::launch::async, _run);    
}


void
IPCSlave::_probe_channel::_run()
{
    BOOL ret;
    DWORD r;
    uint8_t b;
    errno_t e;          

    while(_run_flag){ 
        ret = ConnectNamedPipe(_pipe_hndl, NULL);
        if(!ret){
            e = GetLastError();                               
            if(e != ERROR_PIPE_CONNECTED)  /* incase client connects first */
                TOSDB_LogEx("IPC", "ConnectNamedPipe failed in probe_channel", e);
        }

        ret = ReadFile(_pipe_hndl, (void*)&b, sizeof(b), &r, NULL);
        if(!ret)           
            TOSDB_LogEx("IPC", "ReadFile failed in probe_channel", GetLastError());
                          
                                     
        if(b != PROBE_BYTE || r != 1){
            TOSDB_LogH("IPC", ("didn't receive PROBE_BYTE, b:" +std::to_string(b)).c_str());
            b = 0;
        }

        ret = WriteFile(_pipe_hndl, (void*)&b, sizeof(b), &r, NULL);
        if(!ret)
            TOSDB_LogEx("IPC", "WriteFile failed in probe_channel", GetLastError());
        
        ret = DisconnectNamedPipe(_pipe_hndl);
        if(!ret)            
            TOSDB_LogEx("IPC", "DisconnectNamedPipe failed in probe_channel", GetLastError());        
    }    

    CloseHandle(_pipe_hndl); 
    _pipe_hndl = INVALID_HANDLE_VALUE;       
}


void
IPCSlave::_init_slave_mutex(std::string name, HANDLE *phndl)
{
    *phndl = CreateMutex(&(_sec_attr[MUTEX1]), FALSE, name.c_str());

    if( !(*phndl) ){
        DWORD e = GetLastError();
        std::string msg = "IPCSlave failed to create mutex: "  + name;
        TOSDB_LogEx("IPC-Slave", msg.c_str(), e);

        if(e = ERROR_ACCESS_DENIED){
            TOSDB_LogH("IPC-Slave", "  Likely causes of this error:");
            TOSDB_LogH("IPC-Slave", "    1) The mutex has already been created by another running process");
            TOSDB_LogH("IPC-Slave", "    2) Trying to create a global mutex (Global\\) without adequate privileges");
        }

        throw std::runtime_error(msg);
    }  
}

int 
IPCSlave::_set_security(errno_t *e)
{    
    SID_NAME_USE dummy;
    BOOL ret; /* int */
    DWORD dom_sz = 128;
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;

    SmartBuffer<char> dom_buf(dom_sz);
    SmartBuffer<void> sid(sid_sz);

     /* allocate underlying security objects */
    for(int i = 0; i < NSECURABLE; ++i){ 
        _sec_attr[(Securable)i] = SECURITY_ATTRIBUTES();
        _sec_desc[(Securable)i] = SECURITY_DESCRIPTOR();    
        _sids[(Securable)i] = SmartBuffer<void>(SECURITY_MAX_SID_SIZE);
        _acls[(Securable)i] = SmartBuffer<ACL>(ACL_SIZE);        
    }   
  
    ret = LookupAccountName(NULL, "Everyone", sid.get(), &sid_sz, dom_buf.get(), &dom_sz, &dummy);
    if(!ret){
        *e = GetLastError();
        return -1;
    }

    for(int i = 0; i < NSECURABLE; ++i){ 
        _sec_attr[(Securable)i].nLength = sizeof(SECURITY_ATTRIBUTES);
        _sec_attr[(Securable)i].bInheritHandle = FALSE;
        _sec_attr[(Securable)i].lpSecurityDescriptor = &(_sec_desc[(Securable)i]);

        /* memcpy 'TRUE' is error */
        ret = memcpy_s(_sids[(Securable)i].get(), SECURITY_MAX_SID_SIZE, sid.get(), SECURITY_MAX_SID_SIZE);
        if(ret){
            *e = GetLastError();
            return -2;
        }

        ret = InitializeSecurityDescriptor(&(_sec_desc[(Securable)i]), SECURITY_DESCRIPTOR_REVISION);
        if(!ret){
            *e = GetLastError();
            return -3;
        }

        ret = SetSecurityDescriptorGroup(&(_sec_desc[(Securable)i]), _sids[(Securable)i].get(), FALSE);
        if(!ret){
            *e = GetLastError();
            return -4;
        }

        ret = InitializeAcl(_acls[(Securable)i].get(), ACL_SIZE, ACL_REVISION);
        if(!ret){
            *e = GetLastError();
            return -5;
        }
    }
                           
    /* add ACEs individually */

    ret = AddAccessAllowedAce(_acls[SHEM1].get(), ACL_REVISION, FILE_MAP_WRITE, _sids[SHEM1].get()); 
    if(!ret){
        *e = GetLastError();
        return -6;
    }

    ret = AddAccessAllowedAce(_acls[PIPE1].get(), ACL_REVISION, FILE_GENERIC_WRITE, _sids[PIPE1].get());
    if(!ret){
        *e = GetLastError();
        return -6;
    }

    ret = AddAccessAllowedAce(_acls[PIPE1].get(), ACL_REVISION, FILE_GENERIC_READ, _sids[PIPE1].get()); 
    if(!ret){
        *e = GetLastError();
        return -6;
    }

    ret = AddAccessAllowedAce(_acls[MUTEX1].get(), ACL_REVISION, SYNCHRONIZE, _sids[MUTEX1].get());
    if(!ret){
        *e = GetLastError();
        return -6;
    }

    for(int i = 0; i < NSECURABLE; ++i){
        ret = SetSecurityDescriptorDacl(&(_sec_desc[(Securable)i]), TRUE, _acls[(Securable)i].get(), FALSE);
        if(!ret){
            *e = GetLastError();
            return -7;
        }
    }
    
    *e = 0;
    return 0;
}
