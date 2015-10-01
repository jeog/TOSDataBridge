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

#ifdef CPP_COND_VAR_
void BoundedSemaphore::wait()
{
  { 
    std::lock_guard<std::mutex> lck(_mtx);
    /* --- CRITICAL SECTION --- */
    if(_count.load() > 0){
      --_count;
      return;
    }
    /* --- CRITICAL SECTION --- */
  }
  std::unique_lock<std::mutex> lck(_mtx);
  _cnd.wait(lck, [&]{ 
                   if(_count.load() > 0){
                     --_count;
                     return true;
                   }
                   return false;
                 });    
}

void BoundedSemaphore::release(size_t num)
{
  {
    std::lock_guard<std::mutex> lck(_mtx);
    /* --- CRITICAL SECTION --- */
    if((_count.load() + num) > _max)
      throw std::out_of_range("BoundedSemaphore count > max");      

    _count.fetch_add(num);
    /* --- CRITICAL SECTION --- */
  }
  while(num--)
    _cnd.notify_one();    
}

bool CyclicCountDownLatch::wait_for(size_t timeout, size_t delay)
{  
  if(delay && _ids.empty())
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));

  if(!_inflag && _ids.empty())
    return true;

  std::unique_lock<std::mutex> lck(_mtx);   
  bool res = _cnd.wait_for(lck, std::chrono::milliseconds(timeout),
                           [&]{ return _ids.empty(); });
  if(!res)
    _ids.clear();         
  _inflag = false;

  return res;
}

void CyclicCountDownLatch::wait(size_t delay)
{  
  if(delay && _ids.empty())
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));

  if(_inflag && _ids.empty())
    return;    

  std::unique_lock<std::mutex> lck(_mtx);
  _cnd.wait(lck, [&]{ return _ids.empty(); }); 

  _ids.clear();
  _inflag = false;
}

void CyclicCountDownLatch::count_down(std::string str_id)
{       
  _ids_type::const_iterator iter;

  _sem.wait(); /* wait until all inc calls have gotten to .insert() */
  std::lock_guard<std::mutex> lck(_mtx);
  /* --- CRITICAL SECTION --- */
  if ((iter = _ids.find(str_id)) != _ids.cend())
  { /* valid entry ? */  
    _ids.erase(iter); 
    if(_ids.empty()){
      _inflag = false;
      _cnd.notify_all();        
    }
  }else /* re-open the spot */  
    _sem.release(); 
  /* --- CRITICAL SECTION --- */
}  
  
void CyclicCountDownLatch::increment(std::string str_id)
{    
  _inflag = true; /* signal we should wait(in wait/wait_for), regardless */
  ++_in;          /* tally # of calls this side of lock */

  std::lock_guard<std::mutex> lck(_mtx);  
  /* --- CRITICAL SECTION --- */  
  _ids.insert(str_id);     
  if(_in.load() == ++_out) /* has everything gotten thru the lock ? */
  {   
    _in.fetch_sub(_out); /* safely reset the in-count */
    _sem.release(_out);  /* open semaphore being held in count_down */
    _out = 0; 
  }    
  /* --- CRITICAL SECTION --- */
}

bool SignalManager::wait(std::string unq_id)
{  
  std::unique_lock<std::mutex> lck(_mtx);   
  std::map<std::string,_flag_pair_type>::iterator iter = _unq_flags.find(unq_id);
  
  if(iter == _unq_flags.end())
    return false;

  _cnd.wait(lck, [=]{ return iter->second.first; });
  _unq_flags.erase(iter);  

  return iter->second.second;  
}

bool SignalManager::wait_for(std::string unq_id, size_t timeout)
{
  bool waitRes;
  {
    std::unique_lock<std::mutex> lck(_mtx);  
    std::map<std::string,_flag_pair_type>::iterator iter = 
      _unq_flags.find(unq_id);

    if(iter == _unq_flags.end())
      return false;

    waitRes = _cnd.wait_for(lck, std::chrono::milliseconds(timeout), 
                            [=]{ return iter->second.first; });

    waitRes = waitRes && iter->second.second;
    _unq_flags.erase(iter); 
  } 
  return waitRes;
}

void SignalManager::set_signal_ID(std::string unq_id)
{
  std::lock_guard<std::mutex> lck(_mtx); // unique_lock -> lock_guard Sep29 15
  /* --- CRITICAL SECTION --- */
  _unq_flags.insert(
    std::pair<std::string,_flag_pair_type>(unq_id,_flag_pair_type(false,true)));
  /* --- CRITICAL SECTION --- */
}

bool SignalManager::signal(std::string unq_id, bool secondary)
{
  {      
    std::lock_guard<std::mutex> lck(_mtx); // unique_lock -> lock_guard Sep29 15
    /* --- CRITICAL SECTION --- */
    std::map< std::string, _flag_pair_type >::iterator iter = 
      _unq_flags.find(unq_id);

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

void SignalManager::set_signal_ID(std::string unq_id)
{    
  WinLockGuard lock(_mtx);
  /* --- CRITICAL SECTION --- */
  _unq_flags.insert(std::pair<std::string, volatile bool>(unq_id, true));  
  /* --- CRITICAL SECTION --- */
}

bool SignalManager::wait(std::string unq_id)
{      
  std::map< std::string, volatile bool >::iterator iter;
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

bool SignalManager::wait_for(std::string unq_id, size_type timeout)
{    
  std::map< std::string, volatile bool >::iterator iter;
  DWORD waitRes; 
  bool bRes;
  {
    WinLockGuard lock(_mtx);   
    /* --- CRITICAL SECTION --- */
    iter = _unq_flags.find(unq_id);
    if(iter == _unq_flags.end())
      return false;     
    /* --- CRITICAL SECTION --- */
  }
  waitRes = WaitForSingleObject(_event, timeout);  

  WinLockGuard _lock_(_mtx); 
  /* --- CRITICAL SECTION --- */
  bRes = iter->second;
  _unq_flags.erase(iter);   

  return (waitRes == WAIT_TIMEOUT) ? false : bRes;
  /* --- CRITICAL SECTION --- */
}

bool SignalManager::signal(std::string unq_id, bool secondary)
{  
  {
    WinLockGuard _lock_(_mtx);
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


#endif
