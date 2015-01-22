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

#ifndef JO_TOSDB_DYNAMIC_IPC
#define JO_TOSDB_DYNAMIC_IPC

#include <map>
#include <set>
#include <algorithm>
#include <iterator>
#include <vector>
#include <memory>
#include <future>
#include <windows.h>

/* 
   DynamicIPCMaster / Slave are objects that provide a flexible
   means of IPC.  They combine a shared mem mapping and a duplexed
   named-pipe for external use. On construction the Slave creates a 
   shared memory segment, managed by an ExplicitHeap, accepting allocation 
   requests through an internal named-pipe on its own thread. It waits on 
   the master through a call to wait_for_master(). 

   To support multiple Masters(clients) from different processes and to 
   preempt blocking by the slave the pipe instances are acquired and then 
   released manually via grab_pipe() and release_pipe(), respecitively.
   (The concept is similar to the CallNamedPipe() system call.)  

   shem_chunks are structs that have an offset and byte-size used between
   different address spaces to convey the beginning and end of data inserted 
   into the shared memory segment.

   The sequence of calls by the master are:
   1) grab_pipe()
   2) insert()    : pass raw buffers, sets or vectors of data
   3) send()    : pass the shem_chunks returned by insert or a scallar
   4) recv()    : populate a shem_chunk or scalar with what's passed to send
   5) remove() : 'deallocate' the shem_chunks created by insert
   6) release_pipe()

   The memory management is handled internally after calls to insert and 
   remove but its important that anything passed to insert also be removed 
   AFTER the slave sends back its cofirmation.  If it's not eventually 
   the heap managed by the slave will become full resulting in undefined
   behvarior. (Currently there isn't a means for dealing with this because
   the objects are only being used for trivial RPC.)

   shem_chunk { x, 0 } is reserved for special signals, opcodes etc.
   shem_chunk { 0, 0 } signals the end of a particular set of transmissions.
*/

class ExplicitHeap{

    typedef unsigned long                    headTy, sizeTy;
    typedef unsigned char                    *ptrTy;    
    typedef std::multimap< sizeTy, ptrTy >   heapTy;
    typedef std::set< ptrTy >                setTy;
    typedef std::pair< sizeTy, ptrTy >       heapPairTy;    

    static const unsigned short HEADER = sizeof(headTy);

    heapTy _free_heap;
    setTy _allocated_set;

    bool _valid_start( void* start )
    {   
        return (_allocated_set.find( (ptrTy)start ) != _allocated_set.cend());
    }

    // void _grow();     // TODO
    // void _shrink();   // TODO
    // void _coalesce(); // TODO

public:    

    ExplicitHeap( void* beg_addr, sizeTy sz )
        {
            this->_free_heap.insert(  heapPairTy( sz, (ptrTy)beg_addr ) ); 
        }        

    ~ExplicitHeap()
        {
        }
    
    void*      allocate( sizeTy size);
    bool       deallocate( void* start );
    size_type  size( void* start );

    void clear()
    {
        _free_heap.clear();
        _allocated_set.clear();
    }
};

class DynamicIPCBase{ 
public:

    struct shem_chunk
    {
        size_type offset;
        int bSize;
        shem_chunk() 
            : 
            offset(0), 
            bSize(0)
            {
            }
        shem_chunk(size_type offset, size_type bSize)
            : 
            offset(offset), 
            bSize(bSize)
            {
            }
        ~shem_chunk(){}
    };

private:    

    bool  _send( const shem_chunk& item ) const;    
    bool  _recv( shem_chunk& item ) const;    
    void* _allocate( size_type sz ) const;
    bool  _deallocate( void* start ) const;

    template< typename C >
    shem_chunk _insert( C cont ) const
    {
        void* blk = nullptr;        
        C::const_iterator bIter = cont.cbegin();
        C::const_iterator eIter = cont.cend();         
        size_type bSz = sizeof(C::value_type) * cont.size();  
      
        if( !(blk = _allocate( bSz )) )
            return shem_chunk(0,0);

        for( int i = 0; bIter != eIter; ++i, bIter++ )
            ((C::value_type*)blk)[i] = *bIter; 

        return shem_chunk((size_type)blk - (size_type)_mMapAddr, bSz);
    }

protected:

    static const int           PAUSE = 1000;    
    static const int           ACL_SIZE = 144;
    static const char*         KMUTEX_NAME;
    static const unsigned int  ALLOC = 1;
    static const unsigned int  DEALLOC = 2;
    static const unsigned int  PING = 4;
    std::string                _shemStr;
    std::string                _pipeStr;
    std::string                _intrnlPipeStr;
    void*                      _fMapHndl;    
    void*                      _mMapAddr;
    void*                      _pipeHndl;
    void*                      _intrnlPipeHndl;
    void*                      _mtx;
    int                        _mMapSz;

    DynamicIPCBase( std::string name, int sz = 0 )
        :
#ifdef KGBLNS_
        _shemStr( std::move( std::string("Global\\")
                                 .append( std::string(name)
                                              .append("_shem") )) ),
#else
        _shemStr( std::move( std::string(name).append("_shem")) ),
#endif
        _pipeStr( std::move( std::string("\\\\.\\pipe\\")
                             .append( std::string(name)
                                      .append("_pipe") )) ),
        _intrnlPipeStr( std::move( std::string("\\\\.\\pipe\\")
                                   .append( std::string(name)
                                            .append("pipe_intrnl") )) ), 
        _fMapHndl( NULL ),
        _mMapAddr( NULL ),
        _pipeHndl( INVALID_HANDLE_VALUE ),
        _intrnlPipeHndl( INVALID_HANDLE_VALUE ),
        _mMapSz( sz ),
        _mtx( NULL )
        {            
        }

    virtual ~DynamicIPCBase()
        {            
        }

public:

    template< typename T>
    void extract( shem_chunk& chunk, T* dest, size_type bufSz ) const
    {
        void* ptr;

        if( !(ptr = shem_ptr( chunk )) ){
            dest = nullptr;
            return; 
        }  

        memcpy_s( dest, sizeof(T) * bufSz, ptr, chunk.bSize );        
    }
    
    template< typename T >
    shem_chunk insert( T* pData, size_type dataSz = 1 )
    {        
        size_type bSz = sizeof(T) * dataSz;
        void* blk = _allocate( bSz ); 

        if( !blk || memcpy_s(blk, bSz, pData, bSz ) )
            return shem_chunk(0,0);

        return shem_chunk((size_type)blk - (size_type)_mMapAddr, bSz);        
    }

    template< typename T, typename A >
    shem_chunk inline insert(std::vector<T,A>& vec )
    {
        return _insert( vec );
    }

    template< typename T, typename E, typename A >
    shem_chunk inline insert(std::set<T,E,A>& set )
    {
        return _insert( set );
    }

    const DynamicIPCBase& remove( const shem_chunk&& chunk ) const 
    {        
        void* start = (void*)((size_type)_mMapAddr + chunk.offset);
        _deallocate( start );
        return *this;
    }

    /* we check for overflow but NO GUARANTEE shem_chunk will point at 
       valid data */
    void* shem_ptr( shem_chunk& chunk ) const
    { /* if blck is 'abnormal', we don't have mem-map, 
         or a read will overflow it */        
        if( chunk.bSize <= 0 
            || !_mMapAddr 
            || !((chunk.offset + chunk.bSize) <= (size_type)_mMapSz ) )
           {
               return nullptr;      
           }
  
        return (void*)((size_type)_mMapAddr + chunk.offset);
    }

    void* ptr( size_type offset ) const
    {
        if(offset > (size_type)_mMapSz || !_mMapAddr )
            return nullptr;

        return (void*)((size_type)_mMapAddr + offset);
    }

    size_type offset( void* start ) const    
    {
        if( start < _mMapAddr 
            || (size_type)start >= ((size_type)_mMapAddr + (size_type)_mMapSz))
           {
               return 0;
           }

        return ( (size_type)start - (size_type)_mMapAddr ); 
    }

    bool send( const int val ) const
    {
        return _send( shem_chunk(val,0) );
    }

    bool recv( int& val ) const
    {
        shem_chunk tmpBlck;

        bool res = _recv( tmpBlck );
        val = tmpBlck.offset;

        return res;
    }

    bool inline send( const shem_chunk& chunk ) const
    {
        return _send( chunk );
    }

    bool inline recv( shem_chunk& chunk ) const
    {
        return _recv( chunk );
    }

    const inline DynamicIPCBase& operator<<( const shem_chunk& chunk ) const
    {
        this->_send( chunk );
        return *this;
    }

    const inline DynamicIPCBase& operator>>( shem_chunk& chunk ) const 
    {
        this->_recv( chunk );            
        return *this;
    }    
};

class DynamicIPCMaster
    : public DynamicIPCBase {    

    volatile bool _pipeHeld;

public:    

    DynamicIPCMaster( std::string name )
        : 
        DynamicIPCBase( name ),
        _pipeHeld( false )
        {    
        }

    virtual ~DynamicIPCMaster()
        {
            disconnect();
        }

    bool try_for_slave();
    void disconnect( int level = 3 );
    int  grab_pipe();
    void release_pipe();
    bool connected() const;
    bool inline pipe_held() const 
    {
        return _pipeHeld; 
    } 
    
};

class DynamicIPCSlave
    : public DynamicIPCBase { 
       
    volatile bool _allocLoopF;

    std::unique_ptr<ExplicitHeap> _pHeap;    

    std::unordered_map< Securable, SECURITY_ATTRIBUTES>  _secAttr;
    std::unordered_map< Securable, SECURITY_DESCRIPTOR>  _secDesc;     
    std::unordered_map< Securable, SmartBuffer<void> >   _everyoneSIDs; 
    std::unordered_map< Securable, SmartBuffer<ACL> >    _everyoneACLs; 
 
    int  _set_security();
    void _listen_for_alloc();

public:  
      
    DynamicIPCSlave( std::string name, int sz );
    virtual ~DynamicIPCSlave();
    bool    wait_for_master();
    
};


#endif
