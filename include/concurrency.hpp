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

#ifndef JO_TOSDB_CONCURRENCY
#define JO_TOSDB_CONCURRENCY

#include <climits>
#include <map>
#include <set>
#include <string>

#ifdef CPP_COND_VAR

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

class SignalManager {      
    typedef std::pair<volatile bool, volatile bool> _flag_pair_ty;
    typedef std::pair<std::string,_flag_pair_ty> _sig_pair_ty;

    std::mutex _mtx;
    std::condition_variable _cnd; 

    std::multimap<std::string, _flag_pair_ty> _unq_flags; 

    SignalManager(const SignalManager&);
    SignalManager(SignalManager&&);
    SignalManager& operator=(const SignalManager&);
    SignalManager& operator=(SignalManager&&);

public:
    SignalManager() 
        {
        }
  
    void 
    set_signal_ID(std::string unq_id);

    bool 
    wait(std::string unq_id);

    bool 
    wait_for(std::string unq_id, size_t timeout);  

    bool 
    signal(std::string unq_id, bool secondary);
};

#else

#include <Windows.h>

class LightWeightMutex{
    CRITICAL_SECTION _cs;

    LightWeightMutex(const LightWeightMutex&);
    LightWeightMutex& operator=(const LightWeightMutex&);

public:
    LightWeightMutex() 
        { 
            InitializeCriticalSection(&_cs); 
        }

    ~LightWeightMutex() 
        { 
            DeleteCriticalSection(&_cs); 
        }

    inline void 
    lock() 
    { 
        EnterCriticalSection(&_cs); 
    }

    inline bool 
    try_lock() 
    { 
        return TryEnterCriticalSection(&_cs); 
    }

    inline void 
    unlock() 
    { 
        LeaveCriticalSection(&_cs); 
    }  
};


class WinLockGuard{  
    LightWeightMutex& _mtx;

    WinLockGuard(const WinLockGuard&);
    WinLockGuard& operator=(const WinLockGuard&); 

public:
    WinLockGuard(LightWeightMutex& mutex)
        : 
            _mtx(mutex) 
        { 
            _mtx.lock(); 
        }

    ~WinLockGuard() 
        { 
            _mtx.unlock(); 
        }
};

/* move impl to .cpp */
class IPCNamedMutexClient{
    HANDLE _mtx;   
    std::string _name;

    IPCNamedMutexClient(const IPCNamedMutexClient&);
    IPCNamedMutexClient(IPCNamedMutexClient&&);
    IPCNamedMutexClient& operator=(IPCNamedMutexClient& ipm);

public:
    IPCNamedMutexClient(std::string name) 
        :            
            _name(name)
        {   
        }

    bool
    try_lock(unsigned long timeout,
             std::function<void(errno_t)> no_open_cb = nullptr,
             std::function<void(void)> timeout_cb = nullptr,
             std::function<void(errno_t)> fail_cb = nullptr);

    void
    unlock();

    inline bool
    locked() const
    {
        return (_mtx != NULL);
    }

    ~IPCNamedMutexClient()
    {
        unlock();
    }
};


class SignalManager {    
    std::multimap<std::string, volatile bool> _unq_flags; 
    LightWeightMutex _mtx;
    HANDLE _event;    

    SignalManager(const SignalManager&);
    SignalManager& operator=(const SignalManager&);  

public:
    SignalManager()
        : 
            _event(CreateEvent(NULL, FALSE, FALSE, NULL)) 
        {
        }

    ~SignalManager() 
        { 
            CloseHandle(_event); 
        }

    void 
    set_signal_ID(std::string unq_id);

    bool 
    wait(std::string unq_id);    

    bool 
    wait_for(std::string unq_id, size_type timeout);

    bool 
    signal(std::string unq_id, bool secondary);
};

#endif /* CPP_COND_VAR */

#endif
