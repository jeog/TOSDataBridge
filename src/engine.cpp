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

#include <algorithm>
#include <ostream> /* debug */
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <cctype>
#include "tos_databridge.h"
#include "engine.hpp"
#include "dynamic_ipc.hpp"
#include "concurrency.hpp"

namespace {

    unsigned long long COUNTER = 0;
    
    LPCSTR  CLASS_NAME = "DDE_CLIENT_WINDOW";
    LPCSTR  LOG_NAME   = "engine-log.log";    
    
    const unsigned int COMM_BUFFER_SIZE   = 5; //opcode + 3 args + {0,0}
    const unsigned int ACL_SIZE           = 96;
    const unsigned int UPDATE_PERIOD      = 2000;

    /* not guaranteed to be unique; need them at compile time */
    const unsigned int LINK_DDE_ITEM      = 0x0500;
    const unsigned int REQUEST_DDE_ITEM   = 0x0501;
    const unsigned int DELINK_DDE_ITEM    = 0x0502;
    const unsigned int CLOSE_CONVERSATION = 0x0503;    
        
    HINSTANCE    hInstance = NULL;
    SYSTEM_INFO  sysInfo;

    SECURITY_ATTRIBUTES secAttr[2];
    SECURITY_DESCRIPTOR secDesc[2];

    SmartBuffer<void> everyoneSIDs[2] = { 
        SmartBuffer<void>(SECURITY_MAX_SID_SIZE), 
        SmartBuffer<void>(SECURITY_MAX_SID_SIZE) 
    };
    SmartBuffer<ACL> everyoneACLs[2] = { 
        SmartBuffer<ACL>(ACL_SIZE), 
        SmartBuffer<ACL>(ACL_SIZE) 
    };     
    
    GlobalBuffersTy  globalBuffers;    
    GlobalTopicsTy   globalTopics; 
    GlobalConvosTy   globalConvos;    
    LightWeightMutex globalTopicMutex;
    LightWeightMutex globalBufferMutex;
    SignalManager    globalAckSignals;

    HANDLE globalInitEvent      =  NULL;
    HANDLE globalContinueEvent  =  NULL;

    HANDLE msgThread     =  NULL;
    HWND   msgWindow     =  NULL;
    DWORD  msgThreadID   =  0;
    LPCSTR msgWindowName =  "TOSDB_ENGINE_MSG_WNDW";

    volatile bool shutdownFlag  =  false;    
    volatile bool pauseFlag     =  false;
    volatile bool isService     =  true;

    template< typename T > 
    int   RouteToBuffer( DDE_Data<T>&& data );    
    int   MainCommLoop(); 
    void  TearDownTopic( TOS_Topics::TOPICS tTopic, size_type timeout );
    bool  DestroyBuffer( TOS_Topics::TOPICS tTopic, std::string sItem );
    void  HandleData( UINT msg, WPARAM wparam, LPARAM lparam );
    void  DeAllocKernResources();
    int   CleanUpMain( int retCode );
    void  DumpBufferStatus();
    void  CloseAllStreams( size_type timeout );
    int   SetSecurityPolicy();
   
    int   AddStream( TOS_Topics::TOPICS tTopic, 
                     std::string sItem, 
                     size_type timeout );

    bool  RemoveStream( TOS_Topics::TOPICS tTopic, 
                        std::string sItem, 
                        size_type timeout );

    bool  PostItem( std::string sItem, 
                    TOS_Topics::TOPICS tTopic, 
                    size_type timeout );

    bool  PostCloseItem( std::string sItem, 
                         TOS_Topics::TOPICS tTopic, 
                         size_type timeout );
   
    bool  CreateBuffer( TOS_Topics::TOPICS tTopic, 
                        std::string sItem, 
                        unsigned int bufSz = TOSDB_SHEM_BUF_SZ );



    DWORD WINAPI      Threaded_Init( LPVOID lParam );
    LRESULT CALLBACK  WndProc(HWND, UINT, WPARAM, LPARAM);     

};

const TOS_Topics::topic_map_type& 
TOS_Topics::map = TOS_Topics::_map;

int WINAPI WinMain( HINSTANCE hInst, 
                    HINSTANCE hPrevInst, 
                    LPSTR lpCmdLn, 
                    int nShowCmd )
{    
    TOSDB_StartLogging( std::string( std::string(TOSDB_LOG_PATH) 
                                     + std::string(LOG_NAME)).c_str() );         

    bool res; 
   
    int errVal     = 0;
    size_t bufIndx = 0;  
    void *pArg1    = nullptr;
    void *pArg2    = nullptr;
    void *pArg3    = nullptr;
    WNDCLASS clss  = {};

    DynamicIPCSlave rpcSlave( TOSDB_COMM_CHANNEL, TOSDB_SHEM_BUF_SZ );
    DynamicIPCSlave::shem_chunk shem_buf[COMM_BUFFER_SIZE];
        
    if( !strcmp(lpCmdLn,"--noservice") )
        isService = false;

    if( errVal = SetSecurityPolicy() ){
        TOSDB_LogH( "STARTUP", 
                    "engine failed to set initialize security objects" );
        return -1;
    }
    
    hInstance = GetModuleHandle(NULL);
    clss.hInstance = hInstance;
    clss.lpfnWndProc = WndProc; 
    clss.lpszClassName = CLASS_NAME;    

    RegisterClass( &clss ); 
        
    if( !(msgThread = CreateThread( NULL, 0, Threaded_Init, NULL, 0, 
                                    &msgThreadID )) ){
        return -1;
    }

    if( WaitForSingleObject(globalInitEvent,TOSDB_DEF_TIMEOUT) == WAIT_TIMEOUT){
        TOSDB_LogH( "STARTUP", "engine's core thread did not receive signal " 
                               "from msg thread");
        return CleanUpMain(-1);
    }        
    
    GetSystemInfo( &sysInfo );        

    while( !shutdownFlag ){ 
       
        if( !rpcSlave.wait_for_master() ){
            TOSDB_LogH("IPC", "wait_for_master() failed");
            return CleanUpMain(-1);
        }        

        while( !shutdownFlag && (res = rpcSlave.recv( shem_buf[bufIndx] ) ) ){            

            if( shem_buf[bufIndx].bSize == 0 && shem_buf[bufIndx].offset == 0 ){        
                int i;

                switch( shem_buf[0].offset ){
                case TOSDB_SIG_ADD:

                    {
                    if( !(pArg1 = rpcSlave.shem_ptr( shem_buf[1] )) 
                        || !(pArg2 = rpcSlave.shem_ptr( shem_buf[2] )) 
                        || !(pArg3 = rpcSlave.shem_ptr( shem_buf[3] )) )
                       {
                          TOSDB_LogH("IPC","invalid shem_chunk passed to slave");
                          break;
                       }     
              
                    i = AddStream( *(TOS_Topics::TOPICS*)pArg1, 
                                   std::string((char*)pArg2),
                                   *(size_type*)pArg3 );                     
                    rpcSlave.send( i );          
                    }

                    break;
                case TOSDB_SIG_REMOVE:

                    {
                    if( !(pArg1 = rpcSlave.shem_ptr( shem_buf[1] )) 
                        || !(pArg2 = rpcSlave.shem_ptr( shem_buf[2] )) 
                        || !(pArg3 = rpcSlave.shem_ptr( shem_buf[3] )) )
                       {
                          TOSDB_LogH("IPC","invalid shem_chunk passed to slave");
                          break;
                       }

                    i = RemoveStream( *(TOS_Topics::TOPICS*)pArg1, 
                                      std::string((char*)pArg2), 
                                      *(size_type*)pArg3 ) ? 0 : 1;
                    rpcSlave.send( i );                        
                    }    

                    break;
                case TOSDB_SIG_PAUSE:

                    {
                    TOSDB_Log( "IPC", "TOSDB_SIG_PAUSE message received" );
                    if( isService ){            
                        pauseFlag = true;
                        rpcSlave.send( TOSDB_SIG_GOOD );                            
                    }                        
                    }

                    break;
                case TOSDB_SIG_CONTINUE:

                    {                     
                    TOSDB_Log("IPC", "TOSDB_SIG_CONTINUE message received");                    
                    if( isService ){
                        pauseFlag = false;                            
                       rpcSlave.send( TOSDB_SIG_GOOD );                        
                    }
                    }

                    break;
                case TOSDB_SIG_STOP:

                    {                     
                    TOSDB_Log( "IPC", "TOSDB_SIG_STOP message received" );                 
                    if( isService ){
                        shutdownFlag = true;
                        TOSDB_Log( "IPC", "shutdown flag set" );
                        rpcSlave.send( TOSDB_SIG_GOOD );
                    }
                    }

                    break;
                case TOSDB_SIG_DUMP:

                    {
                    TOSDB_Log( "IPC", "TOSDB_SIG_DUMP message received" );
                    DumpBufferStatus();
                    rpcSlave.send( 0 );
                    }

                    break;
                default:
                    TOSDB_LogH("IPC","invalid opCode");
                }

                bufIndx = 0;        
                pArg1 = pArg2 = pArg3 = nullptr;

            }else if( ++bufIndx >= COMM_BUFFER_SIZE ){
                TOSDB_LogH("IPC","shem_chunk buffer full, reseting msg loop");
                bufIndx = 0;                
            }            
        }    
    }    
    CloseAllStreams( TOSDB_DEF_TIMEOUT );
    return CleanUpMain(0);            
}

namespace {        

int CleanUpMain( int retCode )
{
    retCode ? TOSDB_LogEx( "SHUTDOWN", "engine preparing to exit from error", 
                           retCode )
            : TOSDB_Log("SHUTDOWN", "engine preparing to exit");

    if( msgWindow )
        DestroyWindow( msgWindow );

    if( !hInstance )
        hInstance = GetModuleHandle(NULL);

    UnregisterClass( CLASS_NAME, hInstance );
    return retCode;
}
    
int AddStream( TOS_Topics::TOPICS tTopic, 
               std::string sItem, 
               unsigned long timeout )
{        
    int errVal = 0;

    if( !(TOS_Topics::enum_type)tTopic )
        return -1;

    GlobalTopicsTy::iterator topicIter = globalTopics.find( tTopic );    
    if( topicIter == globalTopics.end() ){ 
    /* if topic isn't in our global mapping */     
        ATOM aTopic;
        ATOM aApplication;
        std::string sTopic;

        globalInitEvent = CreateEvent( NULL, FALSE, FALSE, NULL);                    
        sTopic = TOS_Topics::map[ tTopic ];    
        aTopic = GlobalAddAtom( sTopic.c_str() );
        aApplication = GlobalAddAtom( TOSDB_APP_NAME );

        globalAckSignals.set_signal_ID( TOS_Topics::map[ tTopic ] ); 

        if( aTopic )
            SendMessageTimeout( (HWND)HWND_BROADCAST, WM_DDE_INITIATE,
                                (WPARAM)msgWindow, MAKELONG(aApplication,aTopic),
                                SMTO_NORMAL, 500, NULL );        

        if(aApplication) 
            GlobalDeleteAtom(aApplication);

        if(aTopic) 
            GlobalDeleteAtom(aTopic);

         /* wait for ack from DDE server */
        if( !globalAckSignals.wait_for( TOS_Topics::map[ tTopic ], 
                                        timeout ) ){
            errVal = -2;   
        }
         
        if( !errVal ){
            globalTopics[tTopic] = ItemsRefCountTy();
            if( PostItem( sItem, tTopic, timeout ) ){
                globalTopics[tTopic][sItem] = 1;
                if( !CreateBuffer( tTopic, sItem ) )
                    errVal = -4;         
            }
            else             
                errVal = -3;           
        }

    }else{ 
    /* if topic IS already in our global mapping */ 
        ItemsRefCountTy::iterator itemIter = topicIter->second.find(sItem); 
        if( itemIter == topicIter->second.end() ){ 
        /* and it doesn't have that item yet */
            if( PostItem( sItem, tTopic, timeout ) ){
                topicIter->second[sItem] = 1;
                if( !CreateBuffer( tTopic, sItem ) )
                    errVal = -4;          
            }
            else
                errVal = -3;
        }
        else /* if both already there, increment the ref-count */
            itemIter->second++;        
    }    

    /* unwind if it fails during creation */
    switch( errVal ){     
    case -4:        
        PostCloseItem( sItem, tTopic, timeout );
        globalTopics[tTopic].erase( sItem );
    case -3:
        if( !globalTopics[tTopic].empty() ) 
            break;                
    case -2:                
        TearDownTopic( tTopic, timeout );            
    };

    return errVal;
}


bool RemoveStream( TOS_Topics::TOPICS tTopic, 
                   std::string sItem, 
                   unsigned long timeout)
{    
    if( !(TOS_Topics::enum_type)tTopic )
        return false;

    GlobalTopicsTy::iterator topicIter = globalTopics.find( tTopic );    
    if( topicIter != globalTopics.end() ){ 
    /* if topic is in our global mapping */    
        ItemsRefCountTy::iterator itemIter = topicIter->second.find( sItem );
        if( itemIter != topicIter->second.end() ){ 
        /* if it has that item */            
            if( !(--(itemIter->second)) ){ 
            /* if ref-count hits zero */
                if( !PostCloseItem( sItem, tTopic, timeout) )
                    return false; 
                DestroyBuffer( tTopic, sItem );
                topicIter->second.erase( itemIter );             
            }
        } /* if no items close the convo */

        if( topicIter->second.empty() ) 
            TearDownTopic( tTopic, timeout );  
      
        return true;    
    }    
    return false;
}

void CloseAllStreams( unsigned long timeout )
{ /* need to iterate through copies */    
    GlobalTopicsTy gtCopy;
    ItemsRefCountTy ircCopy;

    std::copy( globalTopics.begin(), globalTopics.end(),
               std::insert_iterator<GlobalTopicsTy>( gtCopy, gtCopy.begin() ) );

    for( const auto& topic: gtCopy ){
        std::copy( topic.second.begin(), topic.second.end(),
                   std::insert_iterator<ItemsRefCountTy >( ircCopy, 
                                                           ircCopy.begin() ) );
        for( const auto& item : ircCopy )
            RemoveStream( topic.first, item.first, timeout );

        ircCopy.clear();
    }
}

int SetSecurityPolicy() 
{        
    SID_NAME_USE       sidUseDummy;
    DWORD              domSz = 128;
    DWORD              sidSz = SECURITY_MAX_SID_SIZE;
    SmartBuffer<char>  domBuffer(domSz);
    SmartBuffer<void>  everyoneSID(sidSz);    

    secAttr[SHEM1].nLength = 
        secAttr[MUTEX1].nLength = sizeof(SECURITY_ATTRIBUTES);

    secAttr[SHEM1].bInheritHandle = 
        secAttr[MUTEX1].bInheritHandle = FALSE;

    secAttr[SHEM1].lpSecurityDescriptor = &secDesc[SHEM1];
    secAttr[MUTEX1].lpSecurityDescriptor = &secDesc[MUTEX1];

    if( !LookupAccountName( NULL, "Everyone", everyoneSID.get(), &sidSz,
                            domBuffer.get(), &domSz, &sidUseDummy) ){
        return -1;
    }

    if( memcpy_s( everyoneSIDs[SHEM1].get(), SECURITY_MAX_SID_SIZE, 
                  everyoneSID.get(), SECURITY_MAX_SID_SIZE ) 
        || memcpy_s( everyoneSIDs[MUTEX1].get(), SECURITY_MAX_SID_SIZE, 
                     everyoneSID.get(), SECURITY_MAX_SID_SIZE ) ){
    
        return -2;
    }

    if( !InitializeSecurityDescriptor( &secDesc[SHEM1], 
                                       SECURITY_DESCRIPTOR_REVISION ) 
        || !InitializeSecurityDescriptor( &secDesc[MUTEX1], 
                                          SECURITY_DESCRIPTOR_REVISION ) ){ 
     
        return -3; 
    }

    if( !SetSecurityDescriptorGroup( &secDesc[SHEM1], everyoneSIDs[SHEM1].get(), 
                                     FALSE ) 
        || !SetSecurityDescriptorGroup( &secDesc[MUTEX1], 
                                        everyoneSIDs[MUTEX1].get(), FALSE ) ){

        return -4;
    }

    if( !InitializeAcl( everyoneACLs[SHEM1].get(), ACL_SIZE, ACL_REVISION ) 
        || !InitializeAcl( everyoneACLs[MUTEX1].get(), ACL_SIZE, ACL_REVISION)){
 
        return -5;
    }

    if( !AddAccessDeniedAce( everyoneACLs[SHEM1].get(), ACL_REVISION, 
                             FILE_MAP_WRITE, everyoneSIDs[SHEM1].get() ) 
        || !AddAccessAllowedAce( everyoneACLs[SHEM1].get(), ACL_REVISION, 
                                 FILE_MAP_READ, everyoneSIDs[SHEM1].get() ) 
        || !AddAccessAllowedAce( everyoneACLs[MUTEX1].get(), ACL_REVISION, 
                                 SYNCHRONIZE, everyoneSIDs[MUTEX1].get() ) ){
       
        return -6;
    }

    if( !SetSecurityDescriptorDacl( &secDesc[SHEM1], TRUE, 
                                    everyoneACLs[SHEM1].get(), FALSE ) 
        || !SetSecurityDescriptorDacl( &secDesc[MUTEX1], TRUE, 
                                       everyoneACLs[MUTEX1].get(), FALSE ) ){

        return -7;
    }

    return 0;
}

DWORD WINAPI Threaded_Init( LPVOID lParam )
{
    if( !hInstance )
        hInstance = GetModuleHandle(NULL);

    msgWindow = CreateWindow( CLASS_NAME, msgWindowName, WS_OVERLAPPEDWINDOW,
                              0, 0, 0, 0, NULL, NULL, hInstance, NULL );    

    if( !msgWindow ) 
        return 1; 

    SetEvent( globalInitEvent );

    MSG msg = {};     
    while (GetMessage(&msg, NULL, 0, 0)){    
        TranslateMessage(&msg);
        DispatchMessage(&msg);        
    }     
       
    return 0;
}
    
bool PostItem( std::string sItem, 
               TOS_Topics::TOPICS tTopic, 
               unsigned long timeout )
{    
    HWND convo = globalConvos[tTopic];
    std::string sigID = std::to_string( (size_t)convo ) + sItem;

    globalAckSignals.set_signal_ID( sigID );
    PostMessage( msgWindow, REQUEST_DDE_ITEM, (WPARAM)convo, 
                 (LPARAM)(sItem.c_str()) ); 
    /* 
       for whatever reason a bad item gets a posive ack from an attempt 
       to link it, so that message must post second to give the request 
       a chance to preempt it 
    */        
    PostMessage( msgWindow, LINK_DDE_ITEM, (WPARAM)convo, 
                 (LPARAM)(sItem.c_str()) );        

    return globalAckSignals.wait_for( sigID , timeout );
}

bool PostCloseItem( std::string sItem, 
                    TOS_Topics::TOPICS tTopic, 
                    unsigned long timeout )
{    
    HWND convo = globalConvos[tTopic];
    std::string sigID = std::to_string( (size_t)convo ) + sItem;

    globalAckSignals.set_signal_ID( sigID );
    PostMessage( msgWindow, DELINK_DDE_ITEM, (WPARAM)convo, 
                 (LPARAM)(sItem.c_str()) );    

    return globalAckSignals.wait_for( sigID, timeout );
}

void TearDownTopic( TOS_Topics::TOPICS tTopic, unsigned long timeout )
{    

    PostMessage( msgWindow, CLOSE_CONVERSATION, (WPARAM)globalConvos[tTopic], 
                 NULL );                
    globalTopics.erase( tTopic ); 
    globalConvos.remove( tTopic );    
}

bool CreateBuffer( TOS_Topics::TOPICS tTopic, 
                   std::string sItem, 
                   unsigned int bufSz )
{        
    type_bits_type tBits;
    StreamBuffer buf;
    std::string sBuf;
    IdTy id(sItem, tTopic);    

    if( globalBuffers.find( id ) != globalBuffers.end() )
        return false;

    sBuf = CreateBufferName( TOS_Topics::map[tTopic], sItem );
    buf.rawSz = ( bufSz < sysInfo.dwPageSize ) ? sysInfo.dwPageSize : bufSz;
    buf.hFile = CreateFileMapping( INVALID_HANDLE_VALUE, &secAttr[SHEM1],
                                   PAGE_READWRITE, 0, buf.rawSz, sBuf.c_str() ); 
    if( !buf.hFile )
        return false;

    buf.rawAddr = MapViewOfFile( buf.hFile, FILE_MAP_ALL_ACCESS, 0, 0, 0 );         
    if( !buf.rawAddr ){
        CloseHandle( buf.hFile );
        return false;     
    }

    std::string mtxName = std::string( sBuf ).append("_mtx");
    buf.hMutex = CreateMutex( &secAttr[MUTEX1], FALSE, mtxName.c_str() );
    if( !buf.hMutex ){
        CloseHandle( buf.hFile );
        UnmapViewOfFile( buf.rawAddr ); 
        return false;
    }

    tBits = TOS_Topics::TypeBits( tTopic );   
 
    /* cast mem-map to our header and fill values */
    pBufferHead pTmp = (pBufferHead)(buf.rawAddr); 
    pTmp->loop_seq = 0;
    pTmp->next_offset = pTmp->beg_offset = sizeof(BufferHead);

    pTmp->elem_size = (( tBits & TOSDB_STRING_BIT ) 
                          ? TOSDB_STR_DATA_SZ 
                          : ( tBits & TOSDB_QUAD_BIT ) ? 8 : 4 ) 
                      + sizeof(DateTimeStamp);

    pTmp->end_offset = pTmp->beg_offset 
                       + (( buf.rawSz - pTmp->beg_offset ) / pTmp->elem_size ) 
                       * pTmp->elem_size ;

    globalBuffers.insert( GlobalBuffersTy::value_type( id, buf ) );
    return true;
}

bool DestroyBuffer( TOS_Topics::TOPICS tTopic, std::string sItem )
{ 
    /* don't allow buffer to be destroyed while we're writing to it */
    WinLockGuard _lock_( globalBufferMutex );

    GlobalBuffersTy::iterator bufIter = globalBuffers.find( IdTy( sItem, 
                                                                  tTopic ) );
    if( bufIter != globalBuffers.end() ){ 
        UnmapViewOfFile( bufIter->second.rawAddr );
        CloseHandle( bufIter->second.hFile );
        CloseHandle( bufIter->second.hMutex );
        globalBuffers.erase( bufIter );
        return true;
    }

    return false;
}

/* only called if we're forced to exit abruptly */
void DeAllocKernResources() 
{ 
    WinLockGuard _lock_( globalBufferMutex );

    for( GlobalBuffersTy::value_type & bufItem : globalBuffers )
        DestroyBuffer( bufItem.first.second, bufItem.first.first );

    CloseHandle( globalInitEvent );
}

template< typename T > 
inline void ValToBuf( void* pos, T val ) 
{ 
    *(T*)pos = val; 
}

template<> 
inline void ValToBuf( void* pos, std::string val ) 
{ 
    /* copy the string, truncate if necessary */
    strncpy_s( (char*)pos, TOSDB_STR_DATA_SZ, val.c_str(), 
               TOSDB_STR_DATA_SZ - 1 );
}

template< typename T >
int RouteToBuffer( DDE_Data< T >&& data )
{    
    pBufferHead pHead;    
    int errVal = 0;

    /* BEGIN - INTRA-PROCESS CRITICAL SECTION */
    WinLockGuard _lock_( globalBufferMutex );
    /******************************************/
    GlobalBuffersTy::iterator bufIter = globalBuffers.find( IdTy( data.item, 
                                                                  data.topic) );
    if( bufIter == globalBuffers.end() )    
        return -1;

    pHead = (pBufferHead)(bufIter->second.rawAddr);

    /* BEGIN - INTER-PROCESS CRITICAL SECTION */
    WaitForSingleObject( bufIter->second.hMutex, INFINITE );
    /*-----------------------------------------------------*/

    ValToBuf( (void*)((char*)pHead + pHead->next_offset), data.data );

    *(pDateTimeStamp)((char*)pHead 
                      + pHead->next_offset 
                      + (pHead->elem_size - sizeof(DateTimeStamp))) = *data.time; 

    if( (pHead->next_offset +  pHead->elem_size) >= pHead->end_offset ){
        pHead->next_offset = pHead->beg_offset; 
        ++(pHead->loop_seq);
    }else
        pHead->next_offset += pHead->elem_size;

    /*--------------------------------------*/
    ReleaseMutex( bufIter->second.hMutex);
    /* END - INTER-PROCESS CRITICAL SECTION */   
    /****************************************/ 
    return errVal;    
    /* END - INTRA-PROCESS CRITICAL SECTION */
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{    
    switch (message){
    case WM_DDE_DATA: 

        { /*
              ideally we should de-link it all and store state, then re-init 
              on continue for now lets just not handle the data from the server 
          */
        if( !pauseFlag ) 
            HandleData( message, wParam, lParam );
        }

        break;         
    case LINK_DDE_ITEM:

        {            
        DDEADVISE FAR* lpOptions;
        LPARAM lP;            
        ATOM item;             

        HGLOBAL hOptions = GlobalAlloc( GMEM_MOVEABLE, sizeof(DDEADVISE) );           
        if (!hOptions)
                break;

        lpOptions = (DDEADVISE FAR*)GlobalLock(hOptions);
        if (!lpOptions){
           GlobalFree(hOptions);
           break;
        }

        lpOptions->cfFormat = CF_TEXT;
        lpOptions->fAckReq = FALSE;   
        lpOptions->fDeferUpd = FALSE; 

        GlobalUnlock(hOptions);

        if( !(item = GlobalAddAtom( (LPCSTR)lParam )) )
           break;

        lP = PackDDElParam(WM_DDE_ADVISE, (UINT)hOptions, item);            
        
        if( !PostMessage( (HWND)wParam, WM_DDE_ADVISE, (WPARAM)msgWindow, lP )){
            GlobalDeleteAtom(item);
            GlobalFree(hOptions);
            FreeDDElParam(WM_DDE_ADVISE, lP);
        }
        }
     
        break;
    case REQUEST_DDE_ITEM:

        {          
        ATOM item = GlobalAddAtom( (LPCSTR)lParam );                    
        if( !item) 
            break;
    
        if( !PostMessage( (HWND)wParam, WM_DDE_REQUEST, (WPARAM)(msgWindow), 
                          PackDDElParam(WM_DDE_REQUEST, CF_TEXT, item) ) ){
            GlobalDeleteAtom(item); 
        }
        }

        break; 
    case DELINK_DDE_ITEM:

        {            
        ATOM item = GlobalAddAtom( (LPCSTR)lParam );            
        LPARAM lP = PackDDElParam( WM_DDE_UNADVISE,    0, item );
            
        if( !item ) 
            break;

        if( !PostMessage( (HWND)wParam, WM_DDE_UNADVISE, 
                          (WPARAM)(msgWindow), lP ) ){
            GlobalDeleteAtom(item);            
            FreeDDElParam(WM_DDE_UNADVISE, lP);
        }        
        }

        break;
    case CLOSE_CONVERSATION:

        {           
        PostMessage( (HWND)wParam, WM_DDE_TERMINATE, (WPARAM)msgWindow, NULL );                    
        }

        break;    
    case WM_DESTROY:

        {            
        PostQuitMessage(0);
        }

        break;
    case WM_DDE_ACK:  

        {            
        char aTopic[TOSDB_MAX_STR_SZ + 1];
        char aApp[TOSDB_MAX_STR_SZ + 1];
        char lowApp[TOSDB_MAX_STR_SZ + 1];
        char aItem[TOSDB_MAX_STR_SZ + 1];  
      
        UINT_PTR hiP = 0; 
        UINT_PTR lowP = 0;     
        
        if (lParam <= 0){            
            hiP = HIWORD(lParam); /*topic*/
            lowP = LOWORD(lParam); /*app*/

            GlobalGetAtomName( (ATOM)(hiP), aTopic, (TOSDB_MAX_STR_SZ + 1)); 
            GlobalGetAtomName( (ATOM)(lowP), aApp, (TOSDB_MAX_STR_SZ + 1));                                
            strcpy_s(lowApp, TOSDB_APP_NAME);            
            _strlwr_s( aApp, TOSDB_MAX_STR_SZ ); 
            _strlwr_s( lowApp, TOSDB_MAX_STR_SZ );           
      
            if( strcmp( aApp, lowApp) )
                break;     

            globalConvos.insert( 
                GlobalConvosTy::pair1_type( TOS_Topics::map[aTopic], 
                                            (HWND)wParam) );
            globalAckSignals.signal( aTopic, true );
        }else{    
                 
            UnpackDDElParam( message,lParam, (PUINT_PTR)&lowP, (PUINT_PTR)&hiP); 
            GlobalGetAtomName( (ATOM)(hiP), aItem, (TOSDB_MAX_STR_SZ + 1) );        

            if(lowP == 0x0000){
                globalAckSignals.signal( 
                    std::to_string( (size_t)(HWND)wParam )+ std::string(aItem), 
                    false );
                TOSDB_LogH( "DDE", std::string("NEG ACK from server for item: ")
                                   .append(aItem).c_str() );

            }else if(lowP == 0x8000){
                globalAckSignals.signal( 
                    std::to_string( (size_t)(HWND)wParam ) + std::string(aItem), 
                    true );   
            }             
        } 
               
        }
        break;        
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }    
    return 0;
}

void HandleData( UINT msg, WPARAM wparam, LPARAM lparam )
{    
    DDEDATA FAR* ddeData;
    BOOL clntRel;
    PVOID data;    
    UINT_PTR atom;
    char copdData[TOSDB_STR_DATA_SZ+1]; /* include CR LF, excluded added \0 */
    char aItem[TOSDB_MAX_STR_SZ + 1];
    LPARAM lParamNeg;

    UnpackDDElParam( msg, lparam, (PUINT_PTR)&data, &atom );   
  
    if( !(ddeData = (DDEDATA FAR*) GlobalLock(data)) 
        || (ddeData->cfFormat != CF_TEXT)){ 
        /* if we can't lock the data or its not expected frmt */
        lParamNeg = PackDDElParam(WM_DDE_ACK, 0, (UINT_PTR)atom); 
        /* SEND NEG ACK TO SERVER *//* convo already destroyed if NULL */
        PostMessage( (HWND)wparam, WM_DDE_ACK, (WPARAM)msgWindow, lParamNeg );
        GlobalDeleteAtom((WORD)atom);
        FreeDDElParam(WM_DDE_ACK, lParamNeg);
        return; 
    }  

    if( strncpy_s(copdData,(LPCSTR)(ddeData->Value),TOSDB_STR_DATA_SZ) ){
        TOSDB_LogH( "DDE", "error copying data->value[] string" );
    }

    if(ddeData->fAckReq){
        /* SEND POS ACK TO SERVER *//* convo already destroyed if NULL */
        PostMessage( (HWND)wparam, WM_DDE_ACK, (WPARAM)msgWindow,
                     PackDDElParam( WM_DDE_ACK, 0x8000, (UINT_PTR)atom ) ); 
    }

    GlobalGetAtomName( (WORD)atom, aItem, TOSDB_MAX_STR_SZ + 1 );
    clntRel = ddeData->fRelease;

    GlobalUnlock(data);     
    if( clntRel )
        GlobalFree(data);
    GlobalDeleteAtom((WORD)atom); 
    /* need to free lParam, as well, or we leak */    
    FreeDDElParam( WM_DDE_DATA, lparam);

    TOS_Topics::TOPICS tTopic = globalConvos[(HWND)(wparam)];
    try{
        std::string str(copdData); 
        switch( TOS_Topics::TypeBits( tTopic ) ){
        case TOSDB_STRING_BIT : /* STRING */     
            /* clean up problem chars */               
            str.erase( std::remove_if( str.begin(), str.end(), 
                                       [](char c){ return c < 32; } ), 
                       str.end() );

            RouteToBuffer( std::move( DDE_Data< std::string >( tTopic, aItem, 
                                                               str, true )));        
            break;
        case TOSDB_INTGR_BIT : /* LONG */            
            /* remove commas */
            str.erase( std::remove_if( str.begin(), str.end(),
                                      [](char c){ return std::isdigit(c) == 0;}), 
                       str.end() );

            RouteToBuffer( std::move( DDE_Data< def_size_type >( tTopic, aItem, 
                                                                 std::stol(str), 
                                                                 true )));                
            break;                    
        case TOSDB_QUAD_BIT : /* DOUBLE */

            RouteToBuffer( std::move( DDE_Data< ext_price_type >( tTopic, aItem, 
                                                                 std::stod(str), 
                                                                 true )));            
            break;
        case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :/* LONG LONG */
            /* remove commas */
            str.erase( std::remove_if( str.begin(), str.end(),
                                      [](char c){ return std::isdigit(c) == 0;}), 
                       str.end() );

            RouteToBuffer( std::move( DDE_Data< ext_size_type >( tTopic, aItem, 
                                                                std::stoll(str), 
                                                                true )));            
            break;
        case 0 : /* FLOAT */ 
       
            RouteToBuffer( std::move( DDE_Data< def_price_type >( tTopic, aItem, 
                                                                 std::stof(str), 
                                                                 true )));                                
        default:
            break;
        };   
     
    }catch( const std::out_of_range& e ){            
        TOSDB_LogH( "DDE", e.what() );
    }catch( const std::invalid_argument& e ){        
        TOSDB_LogH( "DDE", std::string( e.what() ).append(" Value:: ")
                                                  .append( copdData ).c_str() );
    }catch( ... ){        
        throw TOSDB_DDE_Error("unexpected error handling dde data");
    }    
    return /* 0 */; 
}

void DumpBufferStatus()
{    
    const size_type logColW[5] = { 30, 30, 10, 60, 16};  
   
    std::string     nowTime( SysTimeString() );        
    std::string     fName( "buffer-status-" );
    std::ofstream   logOut;
        
    std::replace_if( nowTime.begin(), nowTime.end(),
                     [](char x){ return std::isalnum(x) == 0; }, '-' ); 

    logOut.open( std::string(TOSDB_LOG_PATH).append(fName).append(nowTime)
                                                          .append(".log"), 
                 std::ios::out | std::ios::app  );

    logOut <<" --- TOPIC INFO --- " << std::endl;
    logOut <<std::setw(logColW[0])<< std::left << "Topic"
           <<std::setw(logColW[1])<< std::left << "Item"
           <<std::setw(logColW[2])<< std::left << "Ref-Count" << std::endl;

    {
        WinLockGuard _lock_( globalTopicMutex );

        for( const auto & t : globalTopics )
            for( const auto & i : t.second )
                logOut<< std::setw(logColW[0])<< std::left 
                      << TOS_Topics::map[t.first]
                      << std::setw(logColW[1])<< std::left << i.first
                      << std::setw(logColW[2])<< std::left << i.second             
                      << std::endl;    
    }

    logOut <<" --- BUFFER INFO --- " << std::endl;    
    logOut << std::setw(logColW[3])<< std::left << "BufferName"
           << std::setw(logColW[4])<< std::left << "Handle" << std::endl;

    {
        WinLockGuard _lock_( globalBufferMutex );

        for( const auto & b : globalBuffers )
            logOut<< std::setw(logColW[3])<< std::left 
                  << CreateBufferName(TOS_Topics::map[b.first.second],
                                      b.first.first ) 
                  << std::setw(logColW[4]) << std::left 
                  << (size_t)b.second.hFile << std::endl; 
    }

    logOut<< " --- END END END --- "<<std::endl;    

}

};
