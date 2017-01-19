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
      
    ret = CallNamedPipe(_probe_channel_pipe_name.c_str(), 
                        (void*)&i, sizeof(i), (void*)&o, sizeof(o), &r, timeout);     
        
    if(ret == 0){       
        /* GetLastError needs to get called before we try to unlock */
        errno_t e = GetLastError();               
        /* if the slave hasn't created the pipe DONT log*/
        if(e != ERROR_FILE_NOT_FOUND) 
            TOSDB_LogEx("IPC", "CallNamedPipe failed in connected()", e);    
        return false;
    }      
    
    return (o == PROBE_BYTE) && (r == 1); 
}


bool
IPCMaster::call(std::string *msg, unsigned long timeout)
{    
    DWORD r;
    std::string recv(MAX_MESSAGE_SZ, '\0');    

    BOOL ret = CallNamedPipe(_main_channel_pipe_name.c_str(), 
                             (void*)msg->c_str(), msg->length() + 1 , 
                             (void*)recv.c_str(), recv.length() + 1, &r, timeout);     
        
    if(ret == 0){       
        /* GetLastError needs to get called before we try to unlock */
        errno_t e = GetLastError();               
        /* if the slave hasn't created the pipe DONT log*/
        if(e != ERROR_FILE_NOT_FOUND) 
            TOSDB_LogEx("IPC", "CallNamedPipe failed in call", e);    
        return false;
    }     

    *msg = recv;
    return true;
}


IPCSlave::IPCSlave(std::string name)
    :    
        IPCBase(name),   
        _sec_attr(SECURITY_ATTRIBUTES()),
        _sec_desc(SECURITY_DESCRIPTOR()),
        _sec_sid(SECURITY_MAX_SID_SIZE),
        _sec_acl(ACL_SIZE)
    {             
        /* initialize object security */  
        errno_t e = 0;
        if(_set_security(&e) != 0){
            std::string msg = "failed to initialize object security (" + std::to_string(e) + ")";
            TOSDB_LogEx("IPC-Slave", msg.c_str(), e); 
            throw std::runtime_error(msg);
        }                     

        _main_channel_pipe_hndl = _create_pipe(_main_channel_pipe_name);
        _probe_channel_pipe_hndl = _create_pipe(_probe_channel_pipe_name); 

        _probe_channel_run_flag = true;
        /* launch our probe channel so master can asynchronously check connection status*/
        std::async(std::launch::async, [=]{_listen_for_probes();});       
    }    


HANDLE
IPCSlave::_create_pipe(std::string name)
{
    HANDLE h = CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX,
                               PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                               PIPE_UNLIMITED_INSTANCES, 0, 0, INFINITE, &_sec_attr);    
     
    if(h == INVALID_HANDLE_VALUE){        
        errno_t e = GetLastError();
        std::string msg = "IPCSlave failed to create pipe: " + name + " ("  + std::to_string(e) + ")";
        TOSDB_LogEx("IPC-Slave", msg.c_str(), e);        
        throw std::runtime_error(msg);
    }

    return h;
}


IPCSlave::~IPCSlave()
    {          
        CloseHandle(_main_channel_pipe_hndl);              
        _probe_channel_run_flag = false;
        /* this will force evaluation of the run flag */
        connected(TOSDB_DEF_TIMEOUT);  
        /*_listen_for_probes will close _probe_channel_pipe_hndl */
    }


bool 
IPCSlave::wait_for_master()
{    
    if( !ConnectNamedPipe(_main_channel_pipe_hndl, NULL) )
    {       
        TOSDB_LogEx("IPC-Slave", "ConnectNamedPipe failed in wait_for_master", GetLastError());        
        return false;
    }
    return true;
}


void 
IPCSlave::drop_master()
{
    if( !DisconnectNamedPipe(_main_channel_pipe_hndl) )
    {
        TOSDB_LogEx("IPC-Slave", "DisonnectNamedPipe failed in wait_for_master", GetLastError()); 
    }
}


void
IPCSlave::_listen_for_probes()
{
    BOOL ret;
    DWORD r;
    uint8_t b;
    errno_t e;          

    while(_probe_channel_run_flag){ 
        ret = ConnectNamedPipe(_probe_channel_pipe_hndl, NULL);
        if(!ret){
            e = GetLastError();                               
            if(e != ERROR_PIPE_CONNECTED)  /* incase client connects first */
                TOSDB_LogEx("IPC", "ConnectNamedPipe failed in probe_channel", e);
        }

        ret = ReadFile(_probe_channel_pipe_hndl, (void*)&b, sizeof(b), &r, NULL);
        if(!ret)           
            TOSDB_LogEx("IPC", "ReadFile failed in probe_channel", GetLastError());
                          
                                     
        if(b != PROBE_BYTE || r != 1){
            TOSDB_LogH("IPC", ("didn't receive PROBE_BYTE, b:" +std::to_string(b)).c_str());
            b = 0;
        }

        ret = WriteFile(_probe_channel_pipe_hndl, (void*)&b, sizeof(b), &r, NULL);
        if(!ret)
            TOSDB_LogEx("IPC", "WriteFile failed in probe_channel", GetLastError());
        
        ret = DisconnectNamedPipe(_probe_channel_pipe_hndl);
        if(!ret)            
            TOSDB_LogEx("IPC", "DisconnectNamedPipe failed in probe_channel", GetLastError());        
    }      
    CloseHandle(_probe_channel_pipe_hndl);
}


int 
IPCSlave::_set_security(errno_t *e)
{    
    SID_NAME_USE dummy; 

    DWORD dom_sz = 128;
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;

    SmartBuffer<char> dom_buf(dom_sz);

    if( !LookupAccountName(NULL, "Everyone", _sec_sid.get(), &sid_sz, dom_buf.get(), &dom_sz, &dummy) ){    
        *e = GetLastError(); 
        return -1;
    }
   
    _sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
    _sec_attr.bInheritHandle = FALSE;
    _sec_attr.lpSecurityDescriptor = &_sec_desc;

    if( !InitializeSecurityDescriptor(&_sec_desc, SECURITY_DESCRIPTOR_REVISION) ){    
        *e = GetLastError();
        return -2;
    }

    if( !SetSecurityDescriptorGroup(&_sec_desc, _sec_sid.get(), FALSE) ){    
        *e = GetLastError();
        return -3;
    }

    if( !InitializeAcl(_sec_acl.get(), ACL_SIZE, ACL_REVISION) ){    
        *e = GetLastError();
        return -4;
    }
    
    if( !AddAccessAllowedAce(_sec_acl.get(), ACL_REVISION, FILE_GENERIC_WRITE, _sec_sid.get()) ){    
        *e = GetLastError();
        return -5;
    }

    if( !AddAccessAllowedAce(_sec_acl.get(), ACL_REVISION, FILE_GENERIC_READ, _sec_sid.get()) ){    
        *e = GetLastError();
        return -6;
    }
    
    if( !SetSecurityDescriptorDacl(&_sec_desc, TRUE, _sec_acl.get(), FALSE) ){    
        *e = GetLastError();
        return -7;
    }    
    
    *e = 0;
    return 0;
}
