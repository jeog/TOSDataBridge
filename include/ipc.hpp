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
    std::string _main_channel_mtx_name;
    std::string _main_channel_pipe_name;
    HANDLE _main_channel_pipe_hndl;

    std::string _probe_channel_mtx_name;
    std::string _probe_channel_pipe_name;    
    HANDLE _probe_channel_pipe_hndl;
    IPCNamedMutexClient _probe_channel_mtx;

    bool
    send(std::string msg) const;
   
    bool
    recv(std::string *msg) const;

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
            _probe_channel_pipe_hndl(INVALID_HANDLE_VALUE),
            _probe_channel_mtx(_probe_channel_mtx_name)
        {
        }
     
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

    HANDLE _main_channel_mtx;        
    HANDLE _probe_channel_mtx;

    int    
    _set_security(errno_t *e);

    void
    _init_slave_mutex(std::string name, HANDLE *phndl);
        
    class _probe_channel{
        /* how we handle the connection probing mechanism:
           ::launch() creates the pipe and asynchronously runs ::_run() 
           ::_run() waits for master, reads, checks for PROBE_BYTE, writes, disconnects pipe
           ::stop() sets the _run_flag to false to break out of the run loop */
        static HANDLE _pipe_hndl;
        static bool _run_flag;

        static void
        _run();
    public:
        static void
        launch(std::string pipe_name, SECURITY_ATTRIBUTES* sa);

        static inline void
        stop()
        {
            _run_flag = false;
        }
    };

public:
    IPCSlave::IPCSlave(std::string name)
        :    
            IPCBase(name)                    
        { 
            /* initialize object security */  
            errno_t e = 0;
            if(_set_security(&e) != 0){
                std::string msg = "failed to initialize object security (" + std::to_string(e) + ")";
                TOSDB_LogEx("IPC-Slave", msg.c_str(), e); 
                throw std::runtime_error(msg);
            }            
             
            /* create our mutexs */
            _init_slave_mutex(_main_channel_mtx_name, &_main_channel_mtx);
            _init_slave_mutex(_probe_channel_mtx_name, &_probe_channel_mtx);

            /* launch our probe channel so master can asynchronously check connection status*/
            _probe_channel::launch(_probe_channel_pipe_name, &_sec_attr[PIPE1]);
        }    


    IPCSlave::~IPCSlave()
        {           
            CloseHandle(_main_channel_mtx);      
            CloseHandle(_probe_channel_mtx);

            if(_main_channel_pipe_hndl != INVALID_HANDLE_VALUE){
                DisconnectNamedPipe(_main_channel_pipe_hndl);
                CloseHandle(_main_channel_pipe_hndl);            
            }
                        
            _probe_channel::stop();
            /* this will force evaluation of the run flag */
            connected(TOSDB_DEF_TIMEOUT);  
        }

    bool
    wait_for_master();

    using IPCBase::send;
    using IPCBase::recv;  
};


class IPCMaster
        : public IPCBase{
    bool _pipe_held;  
    
    IPCNamedMutexClient _main_channel_mtx;

    bool
    _grab_pipe(unsigned long timeout);

    void
    _release_pipe();

public:
    IPCMaster(std::string name)
        :
            IPCBase(name),
            _pipe_held(false),              
            _main_channel_mtx(_main_channel_mtx_name)
        {
        }

    ~IPCMaster()
        {                      
        }

    bool
    call(std::string *msg, unsigned long timeout);
};



#endif
