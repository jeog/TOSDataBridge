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

#ifdef CPP_COND_VAR

bool 
SignalManager::wait(std::string unq_id)
{  
    std::unique_lock<std::mutex> lck(_mtx);     
    auto iter = _unq_flags.find(unq_id);
  
    if(iter == _unq_flags.end())
        return false;

    _cnd.wait(lck, [=]{ return iter->second.first; });
    _unq_flags.erase(iter);  

    return iter->second.second;  
}

bool 
SignalManager::wait_for(std::string unq_id, size_t timeout)
{
    bool wait_res;
    {
        std::unique_lock<std::mutex> lck(_mtx);     
        auto iter = _unq_flags.find(unq_id);

        if(iter == _unq_flags.end())
            return false;

        wait_res = _cnd.wait_for(lck, std::chrono::milliseconds(timeout), 
                                 [=]{ return iter->second.first; });

        wait_res = wait_res && iter->second.second;
        _unq_flags.erase(iter); 
    } 
    return wait_res;
}

void 
SignalManager::set_signal_ID(std::string unq_id)
{
    std::lock_guard<std::mutex> lck(_mtx); 
    /* --- CRITICAL SECTION --- */
    _unq_flags.insert(_sig_pair_ty(unq_id,_flag_pair_ty(false,true)));
   /* --- CRITICAL SECTION --- */
}

bool 
SignalManager::signal(std::string unq_id, bool secondary)
{
    {      
        std::lock_guard<std::mutex> lck(_mtx); 
        /* --- CRITICAL SECTION --- */      
        auto iter = _unq_flags.find(unq_id);

        if(iter == _unq_flags.end()) 
            return false;    

        iter->second.first = true;
        iter->second.second = secondary;  
        /* --- CRITICAL SECTION --- */
    }  
    _cnd.notify_one();   
    return true;
}

#else

void 
SignalManager::set_signal_ID(std::string unq_id)
{    
    WinLockGuard lock(_mtx);
    /* --- CRITICAL SECTION --- */
    _unq_flags.insert(std::pair<std::string, volatile bool>(unq_id, true));  
    /* --- CRITICAL SECTION --- */
}

bool 
SignalManager::wait(std::string unq_id)
{      
    std::map<std::string, volatile bool>::iterator iter;
    {
        WinLockGuard lock(_mtx);    
        /* --- CRITICAL SECTION --- */
        iter = _unq_flags.find(unq_id);
        if(iter == _unq_flags.end())
            return false;
        /* --- CRITICAL SECTION --- */
    }
    WaitForSingleObject(_event, INFINITE);  

    WinLockGuard lock(_mtx);
    /* --- CRITICAL SECTION --- */
    _unq_flags.erase(iter); 
    return iter->second;    
    /* --- CRITICAL SECTION --- */
}    

bool 
SignalManager::wait_for(std::string unq_id, size_type timeout)
{    
    std::map<std::string, volatile bool>::iterator iter;
    DWORD wait_res; 
    bool b_res;
    {
       WinLockGuard lock(_mtx);   
       /* --- CRITICAL SECTION --- */
       iter = _unq_flags.find(unq_id);
       if(iter == _unq_flags.end())
           return false;     
       /* --- CRITICAL SECTION --- */
    }
    wait_res = WaitForSingleObject(_event, timeout);  

    WinLockGuard lock(_mtx); 
    /* --- CRITICAL SECTION --- */
    b_res = iter->second;
    _unq_flags.erase(iter);   
    return (wait_res == WAIT_TIMEOUT) ? false : b_res;
    /* --- CRITICAL SECTION --- */
}

bool 
SignalManager::signal(std::string unq_id, bool secondary)
{  
    {
        WinLockGuard lock(_mtx);
        /* --- CRITICAL SECTION --- */
        std::map<std::string,volatile bool>::iterator iter = _unq_flags.find(unq_id);
        if(iter == _unq_flags.end())       
            return false;  
        iter->second = secondary;
        /* --- CRITICAL SECTION --- */
    }
    SetEvent(_event); 
    return true;
}


bool
IPCNamedMutexClient::try_lock(unsigned long timeout,
                              std::function<void(errno_t)> no_open_cb,
                              std::function<void(void)> timeout_cb,
                              std::function<void(errno_t)> fail_cb)
{
    if(_mtx)
        return true;
          
    /* open late to be sure slave has already created */
    HANDLE tmp = OpenMutex(SYNCHRONIZE, FALSE, _name.c_str());
    if(!tmp){
        no_open_cb( GetLastError() );
        return false;
    }

    /* we use 'tmp' because technically the mutex has yet to be 
        locked but locked() simply checks that _mtx != NULL */

    DWORD wstate = WaitForSingleObject(tmp, timeout);
    errno_t e = GetLastError();

    switch(wstate){
    case WAIT_TIMEOUT: 
        CloseHandle(tmp);                 
        if(timeout_cb)
            timeout_cb();              
        return false;
    case WAIT_FAILED:    
        CloseHandle(tmp);                 
        if(fail_cb)
            fail_cb(e);          
        return false;          
    }        

    /* on success we pass the handle to _mtx */
    _mtx = tmp;
    return true;
}


void
IPCNamedMutexClient::unlock()
{
    if(locked()){
        ReleaseMutex(_mtx);                  
        CloseHandle(_mtx);
        _mtx = NULL;
    }
}


NamedMutexLockGuard::NamedMutexLockGuard(std::string name)
{
    _mtx = CreateMutex(NULL, FALSE, name.c_str());
    if(!_mtx){        
        errno_t e = GetLastError();
        std::stringstream m;
        m << "failed to create mutex: " << name << ", error: " << e;
        throw std::runtime_error(m.str());
    }
        
    if( WaitForSingleObject(_mtx, INFINITE) == WAIT_FAILED )
    {
        errno_t e = GetLastError();
        std::stringstream m;
        m << "failed to lock mutex: " << name << ", error: " << e;
        throw std::runtime_error(m.str());
    }       
}

NamedMutexLockGuard::~NamedMutexLockGuard()
{
    if(_mtx){
        ReleaseMutex(_mtx);
        CloseHandle(_mtx);        
    }
}

#endif
