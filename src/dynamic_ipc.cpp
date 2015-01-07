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

#include "tos_databridge.h"
#include "dynamic_ipc.hpp"
#include <fstream>
#include <iomanip>

void* ExplicitHeap::allocate( sizeTy size)
{    
    ptrTy blck;    
    sizeTy rmndr;

    sizeTy width = HEADER + size;
    sizeTy snglton[1] = {width};
    auto free_heap_end = _free_heap.cend();
    // CHECK FOR SPACE ON THE FREE HEAP
    auto found = 
       std::find_first_of( _free_heap.cbegin(), 
                           free_heap_end,
                           snglton,
                           snglton + 1,
                           [&]( heapPairTy elem, sizeTy val){ 
                               return (elem.first >= val); 
                           });   
 
    if( found == free_heap_end ) // try to grow once we define _grow() 
        throw std::bad_alloc();
    else 
    {                 
        blck = found->second;            
        rmndr = found->first - width;
        if( rmndr ) // if we are not taking the whole block
            _free_heap.insert( heapPairTy(rmndr, found->second + width) );        
        _free_heap.erase(found); // remove old block        
        *(headTy*)blck = size;    // add header                
        _allocated_set.insert( blck + HEADER ); // add to allocated set    
        return (void*)(blck + HEADER); // return header-adjusted block addr        
    } 
}    

bool ExplicitHeap::deallocate( void* start )
{   
    if( !_valid_start( start ) )
        return false;

    ptrTy pHead = (ptrTy)start - HEADER; // back up to the header
    headTy head = *(headTy*)(pHead); // get the value in the header        
    auto free_heap_end = _free_heap.cend();
    
    auto foundR = // look for contiguous free blocks to right 
        std::find_if( _free_heap.cbegin(),
                      free_heap_end,
                      [&]( heapPairTy elem ) {
                          return elem.second == ((ptrTy)start + head);
                      } );    
    auto foundL = // look for contiguous free blocks to left 
        std::find_if( _free_heap.cbegin(),
                      free_heap_end,
                      [&]( heapPairTy elem ) {
                          return ( (ptrTy)elem.second + elem.first ) == pHead;
                      });

    if( (foundR != free_heap_end) && (foundL != free_heap_end) ){
        _free_heap.insert( 
            heapPairTy( foundL->first + HEADER + head + foundR->first, 
                        foundL->second ) );
        _free_heap.erase( foundR );
        _free_heap.erase( foundL );
    } else if( foundR != free_heap_end){
        _free_heap.insert( heapPairTy( head + HEADER + foundR->first, 
                                       pHead ) );
        _free_heap.erase( foundR );
    } else if( foundL != free_heap_end){
        _free_heap.insert( heapPairTy( foundL->first + HEADER + head, 
                                       foundL->second ) );
        _free_heap.erase( foundL );
    } else
        _free_heap.insert( heapPairTy( head + HEADER, pHead ) );

    _allocated_set.erase( (ptrTy)start ); // remove from allocated set
    return true; 
}

size_type ExplicitHeap::size( void* start )
{
    if( !_valid_start( start ) )
        return 0;
    ptrTy pHead = (ptrTy)start - HEADER; // backup to the header
    return *(headTy*)(pHead); // get the value in the header
}

#ifdef KGBLNS_
const char* DynamicIPCBase::KMUTEX_NAME = "Global\\DynamicIPC_Master_MUTEX1";
#else
const char* DynamicIPCBase::KMUTEX_NAME = "DynamicIPC_Master_MUTEX1";
#endif

void* DynamicIPCBase::_allocate( size_type sz ) const
{
    size_type outBuf[2];
    outBuf[0] = ALLOC;
    outBuf[1] = sz;
    size_type retOff = 0;
    DWORD bRead = 0;
    return CallNamedPipe( _intrnlPipeStr.c_str(),
                         (void*)outBuf, 
                         sizeof(outBuf),
                         (void*)&(retOff), 
                         sizeof(retOff),
                         &bRead,
                         TOSDB_DEF_TIMEOUT ) == TRUE 
                             ? ptr(retOff) 
                             : nullptr;
}

bool DynamicIPCBase::_deallocate( void* start ) const
{
    size_type outBuf[2];
    outBuf[0] = DEALLOC;
    outBuf[1] = offset(start);
    size_type retVal = 0;
    DWORD bRead = 0;
    return CallNamedPipe( _intrnlPipeStr.c_str(),
                         (void*)outBuf, 
                         sizeof(outBuf),
                         (void*)&(retVal), 
                         sizeof(retVal),
                         &bRead,
                         TOSDB_DEF_TIMEOUT ) == TRUE 
                             ? (retVal == TRUE 
                                   ? true 
                                   : false) 
                             : false;
}

bool DynamicIPCBase::_send( const shem_chunk& item ) const
{
    unsigned long bDone;    
    return WriteFile( _pipeHndl, (void*)&item, sizeof(shem_chunk),&bDone, NULL) 
              ? true 
              : false;
}
    
bool DynamicIPCBase::_recv( shem_chunk& item ) const 
{
    unsigned long bDone;    
    return ReadFile( _pipeHndl, (void*)&item, sizeof(shem_chunk), &bDone, NULL ) 
             ? true 
             : false; 
}

int DynamicIPCMaster::grab_pipe()
{
    int retVal;

    if( !(_mtx = OpenMutex( SYNCHRONIZE, FALSE, KMUTEX_NAME ) ))
    {
        TOSDB_LogEx( "IPC-Master", 
                     "OpenMutex failed in grab_pipe()", 
                      GetLastError() );    
        return -1; 
    }    
    
    if( WaitForSingleObject( _mtx, TOSDB_DEF_TIMEOUT ) == WAIT_TIMEOUT )
    {
        TOSDB_LogEx( "IPC-Master", 
                     "WaitForSingleObject() [_mtx] timed out in grab_pipe()", 
                     GetLastError() );        
        CloseHandle( _mtx );
        return -1;    
    }

    if( !WaitNamedPipe( _pipeStr.c_str(), 0 ) )  // ADD A TIMEOUT FOR THIS ???
    {
        TOSDB_LogEx( "IPC-Master", 
                     "WaitNamedPipe() failed in grab_pipe()", 
                     GetLastError() );
        ReleaseMutex( _mtx );
        CloseHandle( _mtx );
        return -1;    
    }  
  
    _pipeHndl = CreateFile( _pipeStr.c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL );    
    if( !_pipeHndl || _pipeHndl == INVALID_HANDLE_VALUE )
    {
        TOSDB_LogEx( "IPC-Master", 
                     "No pipe handle(or CreateFile() failed) in grab_pipe()", 
                     GetLastError() );
        ReleaseMutex( _mtx );
        CloseHandle( _mtx );
        return -1;
    }

    if( !(this->recv(retVal)) )
        TOSDB_LogEx( "IPC-Master", 
                     "->recv() failed in grab_pipe()", 
                     GetLastError() );

    _pipeHeld = true;
    return retVal;
}

void DynamicIPCMaster::release_pipe()
{
    if( _pipeHeld )
    {
        CloseHandle( _pipeHndl );
        _pipeHndl = INVALID_HANDLE_VALUE;
        ReleaseMutex( _mtx );
        CloseHandle( _mtx );
    }
    _pipeHeld = false;
}

bool DynamicIPCMaster::try_for_slave()
{    
    disconnect(); 
   
    if( (_mMapSz = grab_pipe()) <= 0 )        
    {            
        TOSDB_LogEx( "IPC-Master", 
                     "grab_pipe() in try_for_slave() failed", 
                     GetLastError() );
        disconnect( 0 );        
        return false;
    }

    /* WE HAD A RACE CONDITION HERE WHERE THE SLAVE MAY DELETE THE 
       SHARED MEM FIRST;  SHOULD BE O.K. NOW */
    _fMapHndl = OpenFileMapping( FILE_MAP_WRITE, 0, _shemStr.c_str() ); 
    if( !_fMapHndl) 
    {
        TOSDB_LogEx("IPC-Master", 
                    "OpenFileMapping() in try_for_slave() failed", 
                    GetLastError() );
        release_pipe();
        disconnect( 1 );        
        return false;
    }

    if( !(_mMapAddr = MapViewOfFile( _fMapHndl, FILE_MAP_WRITE, 0, 0, 0 )))
    {
        TOSDB_LogEx( "IPC-Master", 
                     "MapViewOfFile() in try_for_slave() failed", 
                     GetLastError() );
        release_pipe();
        disconnect( 2 );
        return false;
    }    

    /* release our exclusive connection AFTER FileMapping calls
       to avoid race with slave */
    release_pipe();    
    return true;
}

bool DynamicIPCMaster::connected() const 
{ 
    size_type outBuf[2];
    outBuf[0] = PING;
    outBuf[1] = 999;
    size_type retOff = 0;
    DWORD bRead = 0;
    BOOL res = CallNamedPipe( _intrnlPipeStr.c_str(),
                              (void*)outBuf, 
                              sizeof(outBuf),
                              (void*)&(retOff), 
                              sizeof(retOff),
                              &bRead,
                              TOSDB_DEF_TIMEOUT );
    if( _mMapAddr && _fMapHndl && res && (retOff == outBuf[1]) )
        return true;
    else
        return false;
}

void DynamicIPCMaster::disconnect( int level )
{    
    switch( level )
    {    
    case 3:
        if(_mMapAddr)                    
            if( !UnmapViewOfFile( _mMapAddr ))
                TOSDB_LogEx( "IPC-Master", 
                             "UnmapViewOfFile in DynamicIPCMaster::disconnect()"
                             " failed", 
                             GetLastError() );        
        _mMapAddr = NULL;                    
    case 2:    
        CloseHandle( _fMapHndl );        
        _fMapHndl = NULL;    
    case 1:
        release_pipe();
    default:    
        break;
    }            
}

DynamicIPCSlave::DynamicIPCSlave( std::string name, int sz )
    :     
    _allocLoopF(true),
    DynamicIPCBase( name, sz )
    { 
        for( int i = 0; i < 3; ++i)
        {            
            _secAttr[(Securable)i] = SECURITY_ATTRIBUTES();
            _secDesc[(Securable)i] = SECURITY_DESCRIPTOR();        
            _everyoneSIDs[(Securable)i] = 
                std::move( SmartBuffer<void>(SECURITY_MAX_SID_SIZE) );
            _everyoneACLs[(Securable)i] = 
                std::move( SmartBuffer<ACL>(ACL_SIZE) );        
        }   
                              
        _set_security();                                        

        if( !(_mtx = CreateMutex( &_secAttr[MUTEX1], FALSE, KMUTEX_NAME )) )    
            TOSDB_LogEx( "IPC-Slave", 
                         "CreateMutex() in slave constructor failed", 
                         GetLastError() );        

        _intrnlPipeHndl = 
            CreateNamedPipe( _intrnlPipeStr.c_str(), 
                             PIPE_ACCESS_DUPLEX,
                             PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
                             PIPE_UNLIMITED_INSTANCES, 0, 0, 
                             INFINITE,
                             &_secAttr[PIPE1]
                             );            
        if(!_intrnlPipeHndl)
            TOSDB_LogEx( "IPC-Slave", 
                         "CreateNamedPipe()[internal] in slave "
                         "constructor failed", 
                         GetLastError() );

        _fMapHndl = /* get handle to page file */
            CreateFileMapping( INVALID_HANDLE_VALUE,
                               &_secAttr[SHEM1],
                               PAGE_READWRITE, 
                               (unsigned long long)_mMapSz >> 32,
                               ((unsigned long long)_mMapSz << 32) >> 32,
                               _shemStr.c_str() );
        
        if(!_fMapHndl)
            TOSDB_LogEx( "IPC-Slave", 
                         "CreateFileMapping() in slave constructor failed", 
                         GetLastError()); 
   
        if( !(_mMapAddr = MapViewOfFile( _fMapHndl, FILE_MAP_WRITE, 0, 0, 0 )))
            TOSDB_LogEx( "IPC-Slave", 
                         "MapViewOfFile() in slave constructor failed", 
                         GetLastError() );  
  
        /* consturct our heap for managing the mappings allocatable space */
        _pHeap = 
            std::unique_ptr<ExplicitHeap>(new ExplicitHeap(_mMapAddr,_mMapSz));
        
        std::async( std::launch::async, [this]{ _listen_for_alloc(); } ); 
    }    

DynamicIPCSlave::~DynamicIPCSlave()
    {    
        _allocLoopF = false;        
        CloseHandle( /* trouble written all over this */
            CreateFile( _intrnlPipeStr.c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL ) );    
        CloseHandle( _mtx );         
    
        if( !UnmapViewOfFile( _mMapAddr ))
            TOSDB_LogEx( "IPC-Slave", 
                         "UnmapViewOfFile() in slave destructor failed", 
                         GetLastError() );    

        CloseHandle( _fMapHndl );            
        DisconnectNamedPipe( _pipeHndl );
        CloseHandle( _pipeHndl );            
    }

void DynamicIPCSlave::_listen_for_alloc()
{
    size_type args[2];
    DWORD bDone = 0;    
    void* start = nullptr;                    
    while( _allocLoopF )
    {
        ConnectNamedPipe( _intrnlPipeHndl, NULL );
        ReadFile( _intrnlPipeHndl, (void*)&args, sizeof(args), &bDone, NULL );
        switch( args[0] )
        {
        case ALLOC:
            {                
                start = _pHeap->allocate( args[1] );
                args[1] = offset(start);
                WriteFile( _intrnlPipeHndl, 
                           (void*)&args[1], 
                           sizeof(args[1]), 
                           &bDone, 
                           NULL );                
            }
            break;
        case DEALLOC:
            {                
                args[1] = _pHeap->deallocate( ptr(args[1]) ) ? TRUE : FALSE;
                WriteFile( _intrnlPipeHndl, 
                           (void*)&args[1], 
                           sizeof(args[1]), 
                           &bDone, 
                           NULL );
            }
            break;
        case PING:
            {
                if( !_mMapAddr || !_fMapHndl )
                    --(args[1]); /* no mmap send back a differen val */
                WriteFile( _intrnlPipeHndl, 
                           (void*)&args[1], 
                           sizeof(args[1]), 
                           &bDone, 
                           NULL );
            }
            break;            
        default:
            TOSDB_LogH( "IPC-Slave", 
                        "Invalid OpCode passed to _listen_for_alloc()" );
            break;
        }            
        DisconnectNamedPipe( _intrnlPipeHndl );
    }    
    CloseHandle( _intrnlPipeHndl );
}

bool DynamicIPCSlave::wait_for_master()
{    
    if( !_pipeHndl || _pipeHndl == INVALID_HANDLE_VALUE )
    {
        _pipeHndl = 
            CreateNamedPipe( _pipeStr.c_str(),
                             PIPE_ACCESS_DUPLEX,
                             PIPE_TYPE_MESSAGE 
                                | PIPE_READMODE_MESSAGE 
                                | PIPE_WAIT,
                             1, 
                             0, 
                             0,
                             INFINITE, 
                             &_secAttr[PIPE1] ); 
    }
    else
        DisconnectNamedPipe( _pipeHndl );

    if( !_pipeHndl || _pipeHndl == INVALID_HANDLE_VALUE )
    {
        TOSDB_LogEx( "IPC-Slave", 
                     "CreateNamedPipe() failed in wait_for_master()", 
                     GetLastError() );            
        return false;
    }    

    ConnectNamedPipe( _pipeHndl, NULL ); 
    
    if( !(this->send(_mMapSz)) )
        TOSDB_LogEx( "IPC-Slave", 
                     "->send() failed in wait_for_master()", 
                     GetLastError() );  
  
    return true;
}

int DynamicIPCSlave::_set_security()
{    
    SID_NAME_USE       sidUseDummy;
    DWORD              domSz = 128;
    DWORD              sidSz = SECURITY_MAX_SID_SIZE;
    SmartBuffer<char>  domBuffer(domSz);
    SmartBuffer<void>  everyoneSID(sidSz);
        
    _secAttr[SHEM1].nLength = 
        _secAttr[PIPE1].nLength = 
            _secAttr[MUTEX1].nLength = sizeof(SECURITY_ATTRIBUTES);

    _secAttr[SHEM1].bInheritHandle = 
        _secAttr[PIPE1].bInheritHandle = 
            _secAttr[MUTEX1].bInheritHandle = FALSE;

    _secAttr[SHEM1].lpSecurityDescriptor = &_secDesc[SHEM1];
    _secAttr[PIPE1].lpSecurityDescriptor = &_secDesc[PIPE1];
    _secAttr[MUTEX1].lpSecurityDescriptor = &_secDesc[MUTEX1];

    if( !LookupAccountName( NULL,
                           "Everyone",
                           everyoneSID.get(),
                           &sidSz,
                           domBuffer.get(),
                           &domSz,
                           &sidUseDummy) ) return -1;

    if( memcpy_s( _everyoneSIDs[SHEM1].get(), 
                  SECURITY_MAX_SID_SIZE, 
                  everyoneSID.get(), 
                  SECURITY_MAX_SID_SIZE ) 
        || memcpy_s( _everyoneSIDs[PIPE1].get(), 
                     SECURITY_MAX_SID_SIZE, 
                     everyoneSID.get(), 
                     SECURITY_MAX_SID_SIZE ) 
        || memcpy_s( _everyoneSIDs[MUTEX1].get(), 
                     SECURITY_MAX_SID_SIZE, 
                     everyoneSID.get(), 
                     SECURITY_MAX_SID_SIZE ) ) return -2;

    if( !InitializeSecurityDescriptor( &_secDesc[SHEM1], 
                                       SECURITY_DESCRIPTOR_REVISION ) 
        || !InitializeSecurityDescriptor( &_secDesc[PIPE1], 
                                          SECURITY_DESCRIPTOR_REVISION ) 
        || !InitializeSecurityDescriptor( &_secDesc[MUTEX1], 
                                          SECURITY_DESCRIPTOR_REVISION ) ) 
            return -3; 

    if( !SetSecurityDescriptorGroup( &_secDesc[SHEM1], 
                                     _everyoneSIDs[SHEM1].get(), 
                                     FALSE ) 
        || !SetSecurityDescriptorGroup( &_secDesc[PIPE1], 
                                        _everyoneSIDs[PIPE1].get(), 
                                        FALSE ) 
        || !SetSecurityDescriptorGroup( &_secDesc[MUTEX1], 
                                        _everyoneSIDs[MUTEX1].get(), 
                                        FALSE ) ) 
            return -4;

    if( !InitializeAcl( _everyoneACLs[SHEM1].get(), ACL_SIZE, ACL_REVISION ) ||
        !InitializeAcl( _everyoneACLs[PIPE1].get(), ACL_SIZE, ACL_REVISION ) ||
        !InitializeAcl( _everyoneACLs[MUTEX1].get(), ACL_SIZE, ACL_REVISION ) ) 
            return -5; 
                                                                    
    if( !AddAccessAllowedAce( _everyoneACLs[SHEM1].get(), 
                              ACL_REVISION, 
                              FILE_MAP_WRITE, 
                              _everyoneSIDs[SHEM1].get() ) 
        || !AddAccessAllowedAce( _everyoneACLs[PIPE1].get(), 
                                 ACL_REVISION, 
                                 FILE_GENERIC_WRITE, 
                                 _everyoneSIDs[PIPE1].get() ) 
        || !AddAccessAllowedAce( _everyoneACLs[PIPE1].get(), 
                                 ACL_REVISION, 
                                 FILE_GENERIC_READ, 
                                 _everyoneSIDs[PIPE1].get() ) 
        || !AddAccessAllowedAce( _everyoneACLs[MUTEX1].get(), 
                                 ACL_REVISION, 
                                 SYNCHRONIZE, 
                                 _everyoneSIDs[MUTEX1].get() ) ) return -6;

    if( !SetSecurityDescriptorDacl( &_secDesc[SHEM1], 
                                    TRUE, 
                                    _everyoneACLs[SHEM1].get(), 
                                    FALSE ) 
        || !SetSecurityDescriptorDacl( &_secDesc[PIPE1], 
                                       TRUE, 
                                       _everyoneACLs[PIPE1].get(), 
                                       FALSE ) 
        || !SetSecurityDescriptorDacl( &_secDesc[MUTEX1], 
                                       TRUE, 
                                       _everyoneACLs[MUTEX1].get(), 
                                       FALSE ) ) 
            return -7;

    return 0;
}
