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
        throw std::runtime_error("msg length > MAX_MESSAGE_SZ");

    BOOL ret = WriteFile(_main_channel_pipe_hndl, (void*)msg.c_str(), msg.length() + 1, &d, NULL);
    if(!ret){
        errno_t e = GetLastError();      
        if(e != ERROR_BROKEN_PIPE)            
            TOSDB_LogEx("IPC", "WriteFile failed in _recv()", e);
    }

    return ret;
}
 

bool 
IPCBase::recv(std::string *msg) const 
{    
    DWORD d;   
    char recv_buffer[MAX_MESSAGE_SZ];  

    msg->clear();
    
    BOOL ret = ReadFile(_main_channel_pipe_hndl, (void*)recv_buffer, MAX_MESSAGE_SZ + 1, &d, NULL);
    if(!ret){
        errno_t e = GetLastError();        
        if(e != ERROR_BROKEN_PIPE) /* we expect a broken pipe error */           
            TOSDB_LogEx("IPC", "ReadFile failed in _recv()", e);
    }else{        
        msg->append(recv_buffer);
    }

    return ret;
}


bool
IPCMaster::call(std::string *msg, unsigned long timeout)
{
    bool ret_stat;

    _grab_pipe(timeout);

    ret_stat = send(*msg);
    if(!ret_stat){       
        TOSDB_Log_Raw( ("IPC call - send failed - msg: " + *msg).c_str() );        
        return false;
    }

    msg->clear();
    ret_stat = recv(msg);
    if(!ret_stat){
        TOSDB_Log_Raw( ("IPC call - recv failed - msg: " + *msg).c_str() );        
        return false;
    } 

    _release_pipe();
    return true;
}


bool 
IPCMaster::try_for_slave(unsigned long timeout)
{      
    if(_grab_pipe(timeout) <= 0)
    {              
        errno_t e = GetLastError(); 
        TOSDB_LogEx("IPC-Master", "grab_pipe in try_for_slave failed", e); 
        return false;
    }

    _release_pipe();   
    return true;
}


bool 
IPCMaster::connected(unsigned long timeout) 
{ 
    uint8_t i = 255;
    uint8_t o = 0;  
    DWORD r = 0;
    BOOL ret;

    try{
        bool is_locked = 
            _probe_channel_mtx.try_lock(
                timeout,
                [](){ TOSDB_LogH("IPC-MASTER", "timed out waiting to lock mutex in connected"); },
                [](errno_t e){ TOSDB_LogEx("IPC-MASTER", "failed to lock mutex in connected", e); }
            );

        if(!is_locked)
            return false;
    
        /*** CRITICAL SECTION ***/   
        ret = CallNamedPipe(_probe_channel_pipe_name.c_str(), 
                            (void*)&i, sizeof(i), (void*)&o, sizeof(o), &r, 
                            NMPWAIT_USE_DEFAULT_WAIT);     
        
        if(ret == 0){       
            errno_t e = GetLastError();        
            if(e != ERROR_FILE_NOT_FOUND) /* if the slave hasn't created the pipe DONT log*/
                TOSDB_LogEx("IPC-MASTER", "CallNamedPipe failed in connected()", e);   
            return false;
        }
 
        return (i == 0);
        /*** CRITICAL SECTION ***/
    }catch(...){
    }

    _probe_channel_mtx.unlock();
}


bool
IPCMaster::_grab_pipe(unsigned long timeout)
{   
    if(_pipe_held)
        return true;
    
    try{
        bool is_locked = 
            _main_channel_mtx.try_lock(
                timeout,
                [](){ TOSDB_LogH("IPC-MASTER", "timed out waiting to lock mutex in grab_pipe"); },
                [](errno_t e){ TOSDB_LogEx("IPC-MASTER", "failed to lock mutex in grab_pipe", e); } 
            );

        if(!is_locked)
            return false;
 
        /*** CRITICAL SECTION (BEGIN) ***/    
        if( !WaitNamedPipe(_main_channel_pipe_name.c_str(),0) ) /* does this need timeout ?? */
        {    
            errno_t e = GetLastError(); 
            TOSDB_LogEx("IPC-Master", "WaitNamedPipe failed in grab_pipe", e);           
            _main_channel_mtx.unlock();
            //CloseHandle(_main_channel_mtx);
            return false;    
        }    
    
        _main_channel_pipe_hndl = CreateFile( _main_channel_pipe_name.c_str(), 
                                       GENERIC_READ | GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                       NULL, OPEN_EXISTING, 
                                       FILE_ATTRIBUTE_NORMAL, NULL );    

        if(!_main_channel_pipe_hndl || (_main_channel_pipe_hndl == INVALID_HANDLE_VALUE))
        {
            errno_t e = GetLastError(); 
            TOSDB_LogEx("IPC-Master", "CreaetFile failed in grab_pipe", e);             
            _main_channel_mtx.unlock();
            //CloseHandle(_main_channel_mtx);
            return false;
        }

        _pipe_held = true;        
    }catch(...){
        _pipe_held = false;
        _main_channel_mtx.unlock();        
    }   

    return _pipe_held;
}


void 
IPCMaster::_release_pipe()
{
    if(_pipe_held){
        CloseHandle(_main_channel_pipe_hndl);
        _main_channel_pipe_hndl = INVALID_HANDLE_VALUE;
        _main_channel_mtx.unlock();
        /*** CRITICAL SECTION (END) ***/
        //CloseHandle(_main_channel_mtx);
    }

    _pipe_held = false;
}


/**** SLAVE ****/


bool 
IPCSlave::wait_for_master()
{    
    if(_main_channel_pipe_hndl && _main_channel_pipe_hndl != INVALID_HANDLE_VALUE)
    {
        DisconnectNamedPipe(_main_channel_pipe_hndl);    
    }
    else
    {   
        _main_channel_pipe_hndl = CreateNamedPipe( _main_channel_pipe_name.c_str(), 
                                                   PIPE_ACCESS_DUPLEX,
                                                   PIPE_TYPE_MESSAGE 
                                                       | PIPE_READMODE_MESSAGE 
                                                       | PIPE_WAIT,
                                                   1, 0, 0, INFINITE, &(_sec_attr[PIPE1]) );
    }

    if(!_main_channel_pipe_hndl || _main_channel_pipe_hndl == INVALID_HANDLE_VALUE)
    {
        errno_t e = GetLastError(); 
        TOSDB_LogEx("IPC-Slave", "CreateNamedPipe failed in wait_for_master", e);        
        return false;
    }    

    /* BLOCK UNTIL MASTER CALLS try_for_slave() */
    ConnectNamedPipe(_main_channel_pipe_hndl, NULL);   

    return true;
}


void
IPCSlave::_launch_probe_server_loop()
{    
    std::async(std::launch::async, 
        [=](){
            while(1){
                if( ConnectNamedPipe(_probe_channel_pipe_hndl, NULL) == 0 )
                {
                    errno_t e = GetLastError();                               
                    if(e != ERROR_PIPE_CONNECTED)  /* incase client connects first */
                        TOSDB_LogEx("IPC", "ConnectNamedPipe failed in _listen_for_alloc", e);
                }

                char b;
                DWORD r;
                if( !ReadFile(_probe_channel_pipe_hndl, (void*)&b, sizeof(b), &r, NULL) ){
                    errno_t e = GetLastError();
                    TOSDB_LogEx("IPC", "ReadFile failed in _listen_for_alloc", e);
                }                  
                                                     
                WriteFile(_probe_channel_pipe_hndl, (void*)&b, sizeof(b), &r, NULL);

                if( !DisconnectNamedPipe(_probe_channel_pipe_hndl) ){
                    errno_t e = GetLastError();
                    TOSDB_LogEx("IPC", "DisconnectNamedPipe failed in _listen_for_alloc", e);
                }
            }    
            CloseHandle(_probe_channel_pipe_hndl);                       
        } 
    );
}



void
IPCSlave::_init_slave_objects()
{
    _main_channel_mtx = CreateMutex(&(_sec_attr[MUTEX1]), FALSE, _main_channel_mtx_name.c_str());
    if(!_main_channel_mtx){
        DWORD err = GetLastError();
        std::string msg = "IPCSlave failed to create 'xtrnl' mutex: "  + _main_channel_mtx_name;
        TOSDB_LogEx("IPC-Slave", msg.c_str(), err);
        if(err = ERROR_ACCESS_DENIED){
            TOSDB_LogH("IPC-Slave", "  Likely causes of this error:");
            TOSDB_LogH("IPC-Slave", "    1) The mutex has already been created by another running process");
            TOSDB_LogH("IPC-Slave", "    2) Trying to create a global mutex (Global\\) without adequate privileges");
        }
        throw std::runtime_error(msg);
    }       
 
    _probe_channel_mtx = CreateMutex(&(_sec_attr[MUTEX1]), FALSE, _probe_channel_mtx_name.c_str());
    if(!_probe_channel_mtx){
        DWORD err = GetLastError();
        std::string msg = "IPCSlave failed to create 'intrnl' mutex: "  + _probe_channel_mtx_name;
        TOSDB_LogEx("IPC-Slave", msg.c_str(), err);
        if(err = ERROR_ACCESS_DENIED){
            TOSDB_LogH("IPC-Slave", "  Likely causes of this error:");
            TOSDB_LogH("IPC-Slave", "    1) The mutex has already been created by another running process");
            TOSDB_LogH("IPC-Slave", "    2) Trying to create a global mutex (Global\\) without adequate privileges");
        }
        throw std::runtime_error(msg);
    }  

    _probe_channel_pipe_hndl = CreateNamedPipe(_probe_channel_pipe_name.c_str(), 
                                        PIPE_ACCESS_DUPLEX,
                                        PIPE_TYPE_MESSAGE 
                                            | PIPE_READMODE_BYTE 
                                            | PIPE_WAIT,
                                        PIPE_UNLIMITED_INSTANCES, 0, 0,
                                        INFINITE, &_sec_attr[PIPE1]);    
     
    if(!_probe_channel_pipe_hndl){
        errno_t e = GetLastError(); 
        TOSDB_LogEx("IPC-Slave", "IPCSlave failed to create named pipe", e);
        throw std::runtime_error("IPCSlave failed to create named pipe");
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
