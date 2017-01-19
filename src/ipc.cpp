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
    BOOL ret;

    if(msg.length() > MAX_MESSAGE_SZ)
        throw std::runtime_error("IPCBase::send() - msg length > MAX_MESSAGE_SZ");

    ret = WriteFile(_main_channel_pipe_hndl, (void*)msg.c_str(), msg.length() + 1, &d, NULL);

    if(!ret){
        errno_t e = GetLastError();     
        /* we should expect broken pipes */
        if(e == ERROR_BROKEN_PIPE)            
            TOSDB_LogDebug("***IPC*** MAIN SEND - (BROKEN_PIPE)");
        else
            TOSDB_LogEx("IPC", "WriteFile failed in _recv()", e);  
        return false;
    }

    return true;
}
 

bool 
IPCBase::recv(std::string *msg) const 
{    
    DWORD d; 
    BOOL ret;
    
    msg->clear();
    msg->resize(MAX_MESSAGE_SZ); 
    
    ret = ReadFile(_main_channel_pipe_hndl, (void*)msg->c_str(), MAX_MESSAGE_SZ + 1, &d, NULL);

    if(!ret || d == 0){
        errno_t e = GetLastError();
        /* we should expect broken pipes */
        if(e == ERROR_BROKEN_PIPE)            
            TOSDB_LogDebug("***IPC*** MAIN RECEIVE - (BROKEN_PIPE)");
        else
            TOSDB_LogEx("IPC", "ReadFile failed in _recv()", e);
        return false;
    }

    return true;
}


bool 
IPCBase::connected(unsigned long timeout) 
{ 
    using namespace std::chrono;

    uint8_t i = PROBE_BYTE;
    uint8_t o = 0;  
    DWORD r = 0;
    BOOL ret;        
   
    auto tbeg = steady_clock_type::now();
    do{        
        ret = CallNamedPipe(_probe_channel_pipe_name.c_str(), 
                            (void*)&i, sizeof(i), (void*)&o, sizeof(o), &r, 
                            NMPWAIT_NOWAIT);     

        if(ret){
            return (o == PROBE_BYTE) && (r == 1);                 
        }

        errno_t e = GetLastError(); 
        switch(e){
        case(ERROR_PIPE_BUSY): /* try again (or timeout) */
            continue;
        case(ERROR_FILE_NOT_FOUND): /* don't log (slave is not up yet) */
            return false;
        default:
            TOSDB_LogEx("IPC", "CallNamedPipe failed in connected()", e);
            return false;
        };               
      
    }while(duration_cast<milliseconds>(steady_clock_type::now() - tbeg).count() < timeout);
     
    /* timeout */
    TOSDB_LogH("IPC", "connected() timed out");
    return false;
}


bool
IPCMaster::call(std::string *msg, unsigned long timeout)
{    
    using namespace std::chrono;

    DWORD r;
    BOOL ret;

    /* buffer for returned string */
    std::string recv(MAX_MESSAGE_SZ, '\0');    

    auto tbeg = steady_clock_type::now();
    do{        
        ret = CallNamedPipe( _main_channel_pipe_name.c_str(), 
                             (void*)msg->c_str(), msg->length() + 1 , 
                             (void*)recv.c_str(), recv.length() + 1, &r, 
                             NMPWAIT_NOWAIT );        

        if(ret){
            *msg = recv;
            return true;
        }

        errno_t e = GetLastError(); 
        switch(e){
        case(ERROR_PIPE_BUSY): /* try again (or timeout) */
            continue;
        case(ERROR_FILE_NOT_FOUND): /* DO LOG */
            TOSDB_LogH("IPC", "main pipe was not found (slave is not available)");
            return false;
        default:
            TOSDB_LogEx("IPC", "CallNamedPipe failed in call()", e);
            return false;
        };             

    }while(duration_cast<milliseconds>(steady_clock_type::now() - tbeg).count() < timeout);
    
    /* timeout */
    TOSDB_LogH("IPC", "call() timed out");
    return false; /* timeout */
}


HANDLE
IPCSlave::_create_pipe(std::string name)
{
    HANDLE h = CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX,
                               PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                               1, 0, 0, INFINITE, &_sec_attr);    
     
    if(h == INVALID_HANDLE_VALUE){        
        errno_t e = GetLastError();
        std::string msg = "IPCSlave failed to create pipe: " + name + " ("  + std::to_string(e) + ")";
        TOSDB_LogEx("IPC-Slave", msg.c_str(), e);        
        throw std::runtime_error(msg);
    }

    return h;
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
        if(!ret || ret == 0){           
            errno_t e = GetLastError();
            if(e == ERROR_BROKEN_PIPE)
                TOSDB_LogDebug("***IPC*** PROBE RECV - (BROKEN_PIPE)");
            else
                TOSDB_LogEx("IPC", "ReadFile failed in _listen_for_probes()", e);            
        }
                          
                                     
        if(b != PROBE_BYTE || r != 1){
            TOSDB_LogH("IPC", ("didn't receive PROBE_BYTE, b:" +std::to_string(b)).c_str());
            b = 0;
        }

        ret = WriteFile(_probe_channel_pipe_hndl, (void*)&b, sizeof(b), &r, NULL);
        if(!ret)
            TOSDB_LogEx("IPC", "WriteFile failed in _listen_for_probes()", GetLastError());

        FlushFileBuffers(_probe_channel_pipe_hndl);        
        DisconnectNamedPipe(_probe_channel_pipe_hndl);       
    } 
}


void 
IPCSlave::_init_security_objects()
{   
    std::string call_str;

    SID_NAME_USE dummy;   
    DWORD dom_sz = 128;
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;
    
    SmartBuffer<char> dom_buf(dom_sz);   

    if( !LookupAccountName(NULL, "Everyone", _sec_sid.get(), &sid_sz, dom_buf.get(), &dom_sz, &dummy) ){    
        call_str = "LookupAccountName";
        goto handle_error;
    }
   
    _sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
    _sec_attr.bInheritHandle = FALSE;
    _sec_attr.lpSecurityDescriptor = &_sec_desc;

    if( !InitializeSecurityDescriptor(&_sec_desc, SECURITY_DESCRIPTOR_REVISION) ){    
        call_str = "LookupAccountName";
        goto handle_error;
    }

    if( !SetSecurityDescriptorGroup(&_sec_desc, _sec_sid.get(), FALSE) ){    
        call_str = "SetSecurityDescriptorGroup";
        goto handle_error;
    }

    if( !InitializeAcl(_sec_acl.get(), ACL_SIZE, ACL_REVISION) ){    
        call_str = "InitializeAcl";
        goto handle_error;
    }
    
    if( !AddAccessAllowedAce(_sec_acl.get(), ACL_REVISION, FILE_GENERIC_WRITE, _sec_sid.get()) ){    
        call_str = "AddAccessAllowedAce(... FILE_GENERIC_WRITE ...)";
        goto handle_error;
    }

    if( !AddAccessAllowedAce(_sec_acl.get(), ACL_REVISION, FILE_GENERIC_READ, _sec_sid.get()) ){    
        call_str = "AddAccessAllowedAce(... FILE_GENERIC_READ ...)";;
        goto handle_error;
    }
    
    if( !SetSecurityDescriptorDacl(&_sec_desc, TRUE, _sec_acl.get(), FALSE) ){    
        call_str = "SetSecurityDescriptorDacl";
        goto handle_error;
    }    
    
    return;

    handle_error:
    {
        errno_t e = GetLastError();
        call_str.append(" failed in _init_security_objects()");
        TOSDB_LogEx("IPC-Slave", call_str.c_str(), e);
        throw std::runtime_error(call_str);
    }

}
