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
protected:
    std::string _main_channel_mtx_name;
    std::string _main_channel_pipe_name;
    void *_main_channel_pipe_hndl;

    std::string _probe_channel_mtx_name;
    std::string _probe_channel_pipe_name;    
    void *_probe_channel_pipe_hndl;

    bool
    send(std::string msg) const;
   
    bool
    recv(std::string *msg) const;

public:
    static const int MAX_MESSAGE_SZ = 255;

    IPCBase(std::string name)
        :
#ifdef NO_KGBLNS           
            _main_channel_mtx_name(std::string(name).append("_main_channel_mtx")),  
            _probe_channel_mtx_name(std::string(name).append("_probe_channel_mtx")),
#else                     
            _main_channel_mtx_name(std::string("Global\\").append(name).append("_main_channel_mtx")),
            _probe_channel_mtx_name(std::string("Global\\").append(name).append("_probe_channel_mtx")),
#endif
            _main_channel_pipe_name(std::string("\\\\.\\pipe\\").append(name).append("_main_channel_pipe")),
            _probe_channel_pipe_name(std::string("\\\\.\\pipe\\").append(name).append("_probe_channel_pipe")),        
            _main_channel_pipe_hndl(INVALID_HANDLE_VALUE),
            _probe_channel_pipe_hndl(INVALID_HANDLE_VALUE)
        {
        }

    
    virtual 
    ~IPCBase() 
        {            
        }
};


class IPCSlave
        : public IPCBase{
    static const int NSECURABLE = 3;
    static const int ACL_SIZE = 144;

    std::unordered_map<Securable, SECURITY_ATTRIBUTES> _sec_attr;
    std::unordered_map<Securable, SECURITY_DESCRIPTOR> _sec_desc;     
    std::unordered_map<Securable, SmartBuffer<void>> _sids; 
    std::unordered_map<Securable, SmartBuffer<ACL>> _acls; 

    void *_main_channel_mtx;        
    void *_probe_channel_mtx;

    int    
    _set_security(errno_t *e);

    void
    _init_slave_objects();

    void
    _launch_probe_server_loop();

public:
    IPCSlave::IPCSlave(std::string name)
        :    
            IPCBase(name),                       
            _main_channel_mtx(NULL),
            _probe_channel_mtx(NULL) 
        { 
            /* initialize object security */  
            errno_t e = 0;
            if(_set_security(&e) != 0){
                std::string msg = "failed to initialize object security (" + std::to_string(e) + ")";
                TOSDB_LogEx("IPC-Slave", msg.c_str(), e); 
                throw std::runtime_error(msg);
            }            
             
            /* create our mutexs and pipes */
            _init_slave_objects();

            /* launch our async server loop so master can prob connection state */
            _launch_probe_server_loop();
        }    


    IPCSlave::~IPCSlave()
        {           
            CloseHandle(_main_channel_mtx);      
            DisconnectNamedPipe(_main_channel_pipe_hndl);
            CloseHandle(_main_channel_pipe_hndl);            
        }

    bool
    wait_for_master();

    using IPCBase::send;
    using IPCBase::recv;
    // RECV

};


class IPCMaster
        : public IPCBase{
    bool _pipe_held;

    InterProcessNamedMutex _probe_channel_mtx;
    InterProcessNamedMutex _main_channel_mtx;

    bool
    _grab_pipe(unsigned long timeout);

    void
    _release_pipe();

public:
    IPCMaster(std::string name)
        :
            IPCBase(name),
            _pipe_held(false),
            _probe_channel_mtx(_probe_channel_mtx_name),
            _main_channel_mtx(_main_channel_mtx_name)
        {
        }

    ~IPCMaster()
        {
            disconnect();           
        }

    inline void
    disconnect()
    {
        _release_pipe();
    }

    bool 
    try_for_slave(unsigned long timeout);

    bool
    connected(unsigned long timeout);

    bool
    call(std::string *msg, unsigned long timeout);

};



#endif
