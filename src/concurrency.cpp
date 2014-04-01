/* 
Copyright (C) 2014 Jonathon Ogden	 < jeog.dev@gmail.com >

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
		if( _count.load() > 0 )
		{
			--_count;
			return;
		}
	}
	std::unique_lock<std::mutex> lck(_mtx);
	_cnd.wait( 
		lck,			
		[&]{ 
			if( _count.load() > 0)
			{
				--_count;
				return true;
			}
			return false;
		} 
	);		
}
void BoundedSemaphore::release( size_t num )
{
	{
		std::lock_guard<std::mutex> lck(_mtx);
		if( (_count.load() + num) > _max )
			throw std::out_of_range("BoundedSemaphore count > max");
		_count.fetch_add( num );
	}
	while( num-- )
		_cnd.notify_one();		
}

bool CyclicCountDownLatch::wait_for( size_t timeout, size_t delay )
{	
	if( delay && _ids.empty() )
		std::this_thread::sleep_for( std::chrono::milliseconds(delay) );
	if( !_inFlag && _ids.empty() )
		return true;
	std::unique_lock<std::mutex> lck(_mtx); 	
	bool res = _cnd.wait_for( 
					lck,
					std::chrono::milliseconds(timeout),
					[&]{ return _ids.empty(); }
					);
	if( !res )
		_ids.clear(); 				
	_inFlag = false;
	return res;
}
void CyclicCountDownLatch::wait( size_t delay )
{	
	if( delay && _ids.empty() )
		std::this_thread::sleep_for( std::chrono::milliseconds(delay) );
	if( _inFlag && _ids.empty() )
		return;		
	std::unique_lock<std::mutex> lck(_mtx);
	_cnd.wait( lck,	[&]{ return _ids.empty(); } );		
	_ids.clear();
	_inFlag = false;
}
void CyclicCountDownLatch::count_down( std::string strID )
{ 			
	_idsTy::const_iterator idIter;
	_sem.wait(); // wait until all inc calls have gotten to .insert()
	std::lock_guard<std::mutex> lck(_mtx);		 
	if ( (idIter = _ids.find( strID )) != _ids.cend() ) // is it a valid entry ?
	{	
		_ids.erase( idIter ); 
		if( _ids.empty() ) 
		{
			_inFlag = false;
			_cnd.notify_all();				
		}
	}
	else
		_sem.release(); // re-open the spot
}	
void CyclicCountDownLatch::increment( std::string strID )
{		
	_inFlag = true;    // signal we should wait( in wait/wait_for), regardless
	++_in;             // tally # of calls this side of lock
	std::lock_guard<std::mutex> lck( _mtx );	
	_ids.insert( strID );		
	if( _in.load() == ++_out ) // has everything gotten thru the lock ?
	{
		_in.fetch_sub( _out ); // safely reset the in-count
		_sem.release( _out );  // open the semaphore being held in count_down			
		_out = 0; 
	}		
}

bool SignalManager::wait( std::string unqID )
{	
	std::unique_lock<std::mutex> lck( _mtx );			
	std::map< std::string, _flagPairTy >::iterator flgIter = _unqFlags.find( unqID );
	if( flgIter == _unqFlags.end() )
		return false;
	_cnd.wait( lck, [=]{ return flgIter->second.first; } );
	_unqFlags.erase( flgIter );	
	return flgIter->second.second;
}

bool SignalManager::wait_for( std::string unqID, size_t timeout )
{
	bool waitRes;
	{
		std::unique_lock<std::mutex> lck( _mtx );
		std::map< std::string, _flagPairTy >::iterator flgIter = _unqFlags.find( unqID );
		if( flgIter == _unqFlags.end() )
			return false;
		waitRes = _cnd.wait_for( 
							lck, 
							std::chrono::milliseconds(timeout), 
							[=]{ return flgIter->second.first; } 
							);
		waitRes = waitRes && flgIter->second.second;
		_unqFlags.erase( flgIter );		
	}	
	return waitRes;
}

void SignalManager::set_signal_ID( std::string unqID )
{
	std::unique_lock<std::mutex> lck( _mtx );
	_unqFlags.insert( std::pair< std::string, _flagPairTy >(unqID, _flagPairTy( false, true ) ) );
}

bool SignalManager::signal( std::string unqID, bool secondary )
{	
	{			
		std::unique_lock<std::mutex> lck( _mtx );
		std::map< std::string, _flagPairTy >::iterator flgIter = _unqFlags.find( unqID );
		if( flgIter == _unqFlags.end() ) 
			return false;		
		flgIter->second.first = true;
		flgIter->second.second = secondary;
		
	}	
	_cnd.notify_one();		
	return true;
}
#else

void SignalManager::set_signal_ID( std::string unqID )
{		
	WinLockGuard _lock_( _mtx );
	_unqFlags.insert( std::pair< std::string, volatile bool >(unqID, true ) );	
}

bool SignalManager::wait( std::string unqID )
{			
	std::map< std::string, volatile bool >::iterator flgIter;
	{
		WinLockGuard _lock_( _mtx );		
		flgIter = _unqFlags.find( unqID );
		if( flgIter == _unqFlags.end() )
			return false;
	}
	WaitForSingleObject( _event, INFINITE );	
	WinLockGuard _lock_( _mtx );
	_unqFlags.erase( flgIter );		
	return flgIter->second;		
}		

bool SignalManager::wait_for( std::string unqID, size_type timeout )
{		
	std::map< std::string, volatile bool >::iterator flgIter;
	DWORD waitRes; 
	bool bRes;
	{
		WinLockGuard _lock_( _mtx );		
		flgIter = _unqFlags.find( unqID );
		if( flgIter == _unqFlags.end() )
			return false;			
	}
	waitRes = WaitForSingleObject( _event, timeout );	
	WinLockGuard _lock_( _mtx );	
	bRes = flgIter->second;
	_unqFlags.erase( flgIter );		
	return (waitRes == WAIT_TIMEOUT) ? false : bRes;
}

bool SignalManager::signal( std::string unqID, bool secondary )
{
	
	{
		WinLockGuard _lock_( _mtx );
		std::map< std::string, volatile bool >::iterator flgIter = _unqFlags.find( unqID );
		if( flgIter == _unqFlags.end() ) 			
			return false;			
		flgIter->second = secondary;
	}
	SetEvent( _event );		
	return true;
}


#endif
