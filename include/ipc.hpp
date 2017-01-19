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
    static const int MAX_MESSAGE_SZ = 255; 
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

    IPCBase(std::string name)
        :    
            _main_channel_pipe_name(std::string("\\\\.\\pipe\\")
                .append(name)
                .append("_main_channel_pipe")),
            _probe_channel_pipe_name(std::string("\\\\.\\pipe\\")
                .append(name)
                .append("_probe_channel_pipe")),        
            _main_channel_pipe_hndl(INVALID_HANDLE_VALUE),
            _probe_channel_pipe_hndl(INVALID_HANDLE_VALUE)          
        {
        }
     
    ~IPCBase() 
        {            
        }
};

/* adjust security for removed mutexs */
class IPCSlave
        : public IPCBase{    
    static const int ACL_SIZE = 144;

    SECURITY_ATTRIBUTES _sec_attr;
    SECURITY_DESCRIPTOR _sec_desc;  
    SmartBuffer<SID> _sec_sid;      
    SmartBuffer<ACL> _sec_acl;

    bool _probe_channel_run_flag;

    int    
    _set_security(errno_t *e);

    HANDLE
    _create_pipe(std::string name);
 
    void
    _listen_for_probes();

public:
    IPCSlave(std::string name); 
    
    ~IPCSlave();

    bool
    wait_for_master();

    void 
    drop_master();

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
