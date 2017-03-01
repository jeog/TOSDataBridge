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

#include "concurrency.hpp"


class IPCBase{
public:
    /* we limit message size to make the implemenation much easier/safer; 
       a more robust mechanism could manually write/read to/from the pipe server 
       and loop on the read calls until ERROR_MORE_DATA isn't returned */
    static const int MAX_MESSAGE_SZ = 511; // 2**9 excluding \0 

    /* arbitrary value the 'probe' channel sends/recieves to confirm connection */
    static const uint8_t PROBE_BYTE = 0xff;

    bool
    connected(unsigned long timeout);

protected:
    std::string _main_channel_pipe_name;
    std::string _probe_channel_pipe_name; 

    HANDLE _main_channel_pipe_hndl;       
    HANDLE _probe_channel_pipe_hndl;

    bool
    send(std::string msg) const;
   
    bool
    recv(std::string *msg) const;

    template<typename T>
    bool 
    _call_pipe( std::string name,
                T* in,
                DWORD in_sz,
                T* out, 
                DWORD out_sz,
                unsigned long timeout, 
                std::function<bool(T*,DWORD)> handle_good_call,
                std::function<void(void)> handle_file_not_found = nullptr ); 

    IPCBase(std::string name)
        :    
            _main_channel_pipe_name(std::string("\\\\.\\pipe\\")
                .append(name).append("_main_channel_pipe")),
            _probe_channel_pipe_name(std::string("\\\\.\\pipe\\")
                .append(name).append("_probe_channel_pipe")),        
            _main_channel_pipe_hndl(INVALID_HANDLE_VALUE),
            _probe_channel_pipe_hndl(INVALID_HANDLE_VALUE)          
        {
        }
     
    ~IPCBase() 
        {            
        }
};


class IPCSlave
        : public IPCBase{    
    static const int ACL_SIZE = 144;

    SECURITY_ATTRIBUTES _sec_attr;
    SECURITY_DESCRIPTOR _sec_desc;  
    SmartBuffer<SID> _sec_sid;      
    SmartBuffer<ACL> _sec_acl;   

    std::thread _probe_channel_thread;
    bool _probe_channel_run_flag;

    void    
    _init_security_objects();

    HANDLE
    _create_pipe(std::string name);
 
    void
    _listen_for_probes();

public:
    IPCSlave(std::string name)
        :    
            IPCBase(name),   
            _sec_attr(SECURITY_ATTRIBUTES()),
            _sec_desc(SECURITY_DESCRIPTOR()),
            _sec_sid(SECURITY_MAX_SID_SIZE),
            _sec_acl(ACL_SIZE),
            _probe_channel_run_flag(true)
        {           
            _init_security_objects();    
        
            _main_channel_pipe_hndl = _create_pipe(_main_channel_pipe_name);
            _probe_channel_pipe_hndl = _create_pipe(_probe_channel_pipe_name); 

            /* launch our probe channel so master can asynchronously check connection status*/
            _probe_channel_thread = std::thread( std::bind(&IPCSlave::_listen_for_probes,this) );       
        }    

    ~IPCSlave()
        {          
            CloseHandle(_main_channel_pipe_hndl);         

            _probe_channel_run_flag = false;            
            connected(TOSDB_DEF_TIMEOUT); /* force evaluation of the run flag */

            if(_probe_channel_thread.joinable())
                _probe_channel_thread.join();

            CloseHandle(_probe_channel_pipe_hndl);
        }
    
    bool 
    wait_for_master();

    void 
    drop_master()
    {
        FlushFileBuffers(_main_channel_pipe_hndl);
        DisconnectNamedPipe(_main_channel_pipe_hndl);    
    }

    using IPCBase::send;
    using IPCBase::recv;  
};


class IPCMaster
        : public IPCBase{
public:
    IPCMaster(std::string name)
        :
            IPCBase(name)
        {
        }

    ~IPCMaster()
        {                      
        }

    bool
    call(std::string *msg, unsigned long timeout);
};




#endif
