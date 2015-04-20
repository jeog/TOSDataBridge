/* 
Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >

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
#ifdef CPP_COND_VAR_
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>

class BoundedSemaphore {

    std::mutex                        _mtx; 
    std::condition_variable           _cnd;
    std::atomic< unsigned long long > _count;
    unsigned long long                _max;

public:

    BoundedSemaphore( size_t count = 1, unsigned long long max = ULLONG_MAX )
        :
        _count( count ),
        _max( max )
        { 
        }

    void wait();
    void release( size_t num = 1 );
    unsigned long long count() const
    {
        return this->_count.load();
    }
};

class CyclicCountDownLatch {
/* 
 *  similar concept to a Java CountDownLatch but should provide :
 *    1) The ability to increment the latch
 *    2) The use of specific strings to check that a 
 *       count_down should occur
 *    3) The ability to reuse the latch (which may cause
 *       problems in non-trivial cases)                            
 */
    typedef std::multiset< std::string >  _ids_type;
    
    BoundedSemaphore         _sem;
    std::mutex               _mtx;
    std::condition_variable  _cnd;
    std::atomic< size_t>     _in;
    size_t                   _out;
    volatile bool            _inflag;
    _ids_type                _ids;    

public:
    CyclicCountDownLatch( const _ids_type& ms = _ids_type() ) 
        : 
         _ids( ms ),
         _sem( 0 ),
         _in( 0 ), 
         _out( 0 ),
         _inflag(false)        
        {
        }

    bool wait_for( size_t timeout, size_t delay = 0 );    
    void wait( size_t delay = 0);
    void count_down( std::string str_id );
    void increment( std::string str_id );
};

class SignalManager {  
      
    typedef std::pair< volatile bool, volatile bool >  _flag_pair_type;
    
    std::mutex                                     _mtx;
    std::condition_variable                        _cnd; 
    std::multimap< std::string, _flag_pair_type >  _unq_flags; 

    SignalManager( const SignalManager& );
    SignalManager( SignalManager&& );
    SignalManager& operator=( const SignalManager& );
    SignalManager& operator=( SignalManager&& );

public:
    SignalManager()
        {
        }

    void set_signal_ID( std::string unq_id );
    bool wait( std::string unq_id );
    bool wait_for( std::string unq_id, size_t timeout );    
    bool signal( std::string unq_id, bool secondary );
};
#else
#include <Windows.h>

class LightWeightMutex{

    CRITICAL_SECTION _cs;
    LightWeightMutex( const LightWeightMutex& );
    LightWeightMutex& operator=( const LightWeightMutex& );

public:
    LightWeightMutex()
        {
            InitializeCriticalSection( &_cs );
        }

    ~LightWeightMutex()
        {
            DeleteCriticalSection( &_cs );
        }

    void lock()
    {
        EnterCriticalSection( &_cs );
    }

    bool try_lock()
    {
        return TryEnterCriticalSection( &_cs ) ? true : false;
    }

    void unlock()
    {
        LeaveCriticalSection( &_cs );
    }    
};

class WinLockGuard {    

    LightWeightMutex& _mtx;
    WinLockGuard( const WinLockGuard& );
    WinLockGuard& operator=( const WinLockGuard& ); 

public:
    WinLockGuard( LightWeightMutex& mutex )
        : 
        _mtx( mutex )
        {
            _mtx.lock();
        }

    ~WinLockGuard()
        {
            _mtx.unlock();
        }
};

class SignalManager { 

    std::multimap< std::string, volatile bool > _unq_flags; 
    LightWeightMutex                            _mtx;
    HANDLE                                      _event;        

    SignalManager( const SignalManager& );
    SignalManager& operator=( const SignalManager& );    

public:
    SignalManager()
        : 
        _event( CreateEvent( NULL, FALSE, FALSE, NULL) )
        {            
        }

    ~SignalManager()
        {
            CloseHandle( _event );        
        }

    void set_signal_ID( std::string unq_id );
    bool wait( std::string unq_id );        
    bool wait_for( std::string unq_id, size_type timeout );
    bool signal( std::string unq_id, bool secondary );
};

#endif 
#endif
