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

#include <iostream>
#include <mutex>
#include <set>
#include <ctime>
#include <algorithm>
#include <atomic>
#include <memory>
#include <fstream>
#include "tos_databridge.h"
#include "client.hpp"
#include "raw_data_block.hpp"
#include "dynamic_ipc.hpp"

std::recursive_mutex* const globalMutex =  new std::recursive_mutex;

namespace {        

    typedef std::tuple< 
        unsigned int, 
        unsigned int,    
        std::set< const TOSDBlock* >, 
        HANDLE, 
        HANDLE >                                            BufferInfoTy;
    typedef std::pair< TOS_Topics::TOPICS, std::string >    BufferIDTy;
    typedef std::map< BufferIDTy, BufferInfoTy >            InptBuffersTy;
    typedef std::lock_guard< std::mutex >                   GuardTy;

    DynamicIPCMaster rpcMaster( TOSDB_COMM_CHANNEL ); 
    
    std::map< std::string, TOSDBlock* >  globalDDEBlockMap;
    InptBuffersTy                        globalInputBuffers;
    HANDLE                               bufferThread = NULL;
    DWORD                                bufferThreadID = 0;
    unsigned short                       globalLatency= (unsigned short)TOSDB_DEF_LATENCY;
    std::mutex                           globalBufferMutex;
    std::atomic<bool>                    awareOfConnection(false);    

    TOSDBlock*    _getBlockPtr( std::string id);
    DWORD WINAPI  BlockCleanup( LPVOID lParam );
    
    steady_clock_type steadyClock;

    int RequestStreamOP( TOS_Topics::TOPICS tTopic, std::string sItem, unsigned long timeout, unsigned int opCode )
    { /* this needs exclusivity, but we can't block, so calling code must lock */
        int ret;
        bool retStat;
        unsigned int lCount = 0;
        if( opCode != TOSDB_SIG_ADD && opCode != TOSDB_SIG_REMOVE )
            return -1;            
        while( rpcMaster.grab_pipe() <= 0 )
        {
            Sleep(TOSDB_DEF_TIMEOUT/10);            
            if( (lCount+=(TOSDB_DEF_TIMEOUT/10)) > TOSDB_DEF_TIMEOUT )
            {
                TOSDB_LogH("IPC","RequestStreamOP timed out trying to grab pipe");
                return -2;
            }
        }
        DynamicIPCMaster::shem_chunk arg1 = rpcMaster.insert( &tTopic );
        DynamicIPCMaster::shem_chunk arg2 = rpcMaster.insert( sItem.c_str(), (size_type)sItem.size() + 1 ); 
        DynamicIPCMaster::shem_chunk arg3 = rpcMaster.insert( &timeout );
        rpcMaster << DynamicIPCMaster::shem_chunk( opCode, 0 ) 
                    << arg1 << arg2 << arg3 << DynamicIPCMaster::shem_chunk(0,0);
        retStat = rpcMaster.recv( ret ); /* don't remove until we get a response ! */
        rpcMaster.remove( std::move(arg1) ).remove( std::move(arg2) ).remove( std::move(arg3) );
        if( !retStat )
        {
            rpcMaster.release_pipe();
            TOSDB_LogH("IPC","recv failed; problem with connection");
            return -3;
        }
        rpcMaster.release_pipe();
        return ret;
    }

    void CaptureBuffer( TOS_Topics::TOPICS tTopic, std::string sItem, const TOSDBlock* db )
    {
        std::string qBufName;
        void *fmHndl, *memAddr, *mtxHndl;
        InptBuffersTy::key_type bufKey(tTopic, sItem );        
        GuardTy _lock_(globalBufferMutex);
        InptBuffersTy::iterator bIter = globalInputBuffers.find( bufKey );
        if( bIter != globalInputBuffers.end() )            
            std::get<2>(bIter->second).insert( db );            
        else
        {        
            qBufName = CreateBufferName( TOS_Topics::globalTopicMap[tTopic], sItem );
            fmHndl = OpenFileMapping( FILE_MAP_READ, 0, qBufName.c_str() );
            if( !fmHndl || !(memAddr = MapViewOfFile( fmHndl, FILE_MAP_READ, 0, 0, 0 )) )                            
            {    
                if( fmHndl )
                    CloseHandle( fmHndl);
                throw TOSDB_buffer_error( 
                    std::string("failure to map shared memory for: ").append( qBufName ).c_str() 
                    );
            }
            CloseHandle( fmHndl ); 
            std::string mtxName = std::string( qBufName ).append("_mtx");
            if( !(mtxHndl = OpenMutex( SYNCHRONIZE, FALSE, mtxName.c_str() )))
            {        
                UnmapViewOfFile( memAddr );
                throw TOSDB_buffer_error( 
                    std::string("failure to open MUTEX handle for: ").append( qBufName ).c_str()  
                    );
            }
            std::set< const TOSDBlock*> dbSet;
            dbSet.insert( db );                
            globalInputBuffers.insert( 
                InptBuffersTy::value_type( bufKey, std::make_tuple(0,0,std::move(dbSet),memAddr,mtxHndl) ) 
                );                
        }        
    }    

    void ReleaseBuffer( TOS_Topics::TOPICS tTopic, std::string sItem, const TOSDBlock* db )
    {
        InptBuffersTy::key_type bufKey(tTopic, sItem );
        GuardTy _lock_(globalBufferMutex);
        InptBuffersTy::iterator bIter = globalInputBuffers.find( bufKey );
        if( bIter != globalInputBuffers.end() )
        {
            std::get<2>(bIter->second).erase( db );
            if( std::get<2>(bIter->second).empty() )
            {
                UnmapViewOfFile( std::get<3>(bIter->second) );
                CloseHandle( std::get<4>(bIter->second) );
                globalInputBuffers.erase( bIter );        
            }
        }    
    }    

    template< typename T > inline T    CastToVal( char* val ) { return *(T*)val; }
    template<> inline std::string    CastToVal<std::string>( char* val ) { return std::string(val); }
    
    template< typename T > void
    ExtractFromBuffer( TOS_Topics::TOPICS topic, std::string item, BufferInfoTy& bufInfo )
    { /* some unecessary comp in here, may want to cache some of the buffer params */
        long newRelPos;
        long long loopDiff, newElems;
        unsigned int dataLen;
        char* daSpot;
        pBufferHead pHead = (pBufferHead)std::get<3>(bufInfo);
        /* attempt to bail early if there's a chance the buffer hasn't changed */
        /* dont wait for the mutex, move on to the next */
        if( pHead->next_offset - pHead->beg_offset == std::get<0>(bufInfo) && 
            pHead->loop_seq == std::get<1>(bufInfo) ||
            WaitForSingleObject( std::get<4>(bufInfo), 0 )
            ) return;     
        /* get the effective size of the buffer */
        dataLen = pHead->end_offset - pHead->beg_offset; 
        /* get the relative position since last read (our begin offset needs to be 0 ) */    
        newRelPos = pHead->next_offset - pHead->beg_offset - std::get<0>(bufInfo); 
        /* how many times has the buffer looped around, if any */        
        if( (loopDiff = (pHead->loop_seq - std::get<1>(bufInfo))) < 0 ) 
            loopDiff+=(((long long)UINT_MAX+1)); /* in case we hit the numerical limit */
        /* find how many elems have been written, 'borrow' a dataLen if necesary */
        newElems = ( newRelPos + (loopDiff * dataLen) ) / pHead->elem_size;        
        if( !newElems) 
        { /* if no new elems we're done */
            ReleaseMutex( std::get<4>(bufInfo) );
            return;
        }
        else if( newElems < 0 ) 
        { /* if something very bad happened */
            ReleaseMutex( std::get<4>(bufInfo) );
            throw TOSDB_buffer_error( "numElems < 0" );
        }
        else
        {   /* make sure we don't insert more than the max elems */
            newElems = ((unsigned long long)newElems < (dataLen / pHead->elem_size)) 
                        ? newElems : (dataLen / pHead->elem_size);
            do 
            {  /* go through each elem, last first */
                daSpot = (char*)pHead + (((pHead->next_offset - (newElems * pHead->elem_size)) + dataLen) % dataLen);            
                for( const TOSDBlock* block : std::get<2>(bufInfo) )
                { /* insert those elements into each block's raw data block */                    
                    block->block->insertData( 
                            topic, 
                            item, 
                            CastToVal<T>(daSpot), 
                            std::move( *(pDateTimeStamp)(daSpot + ((pHead->elem_size)-sizeof(DateTimeStamp))))
                            ); 
                }
            } while( --newElems );
        } /* adjust our buffer info to the present values */
        std::get<0>(bufInfo) = pHead->next_offset - pHead->beg_offset;
        std::get<1>(bufInfo) = pHead->loop_seq; 
        ReleaseMutex( std::get<4>(bufInfo) );
    }

    TOSDBlock* _getBlockPtr( std::string id)
    { 
        try
            {                
                return globalDDEBlockMap.at(id);
            } 
        catch(...) 
            { 
                TOSDB_Log("TOSDBlock", "TOSDBlock does not exist.");
                return NULL; 
            }
    }

    DWORD WINAPI BlockCleanup( LPVOID lParam )
    {
        TOSDB_RawDataBlock* cln;
        if( cln = (TOSDB_RawDataBlock*)lParam )
            delete cln;
        return 0;
    }

    DWORD WINAPI Threaded_Init( LPVOID lParam )
    {                
        steady_clock_type::time_point tBeg,tEnd;
        long tDiff;    
        awareOfConnection.store(true);
        while( rpcMaster.connected() && awareOfConnection.load() )
        { /* the concurrent read loop errs on the side of greedyness */
            tBeg = steadyClock.now(); /* include time waiting for lock */    
            { /* make sure we give up this lock each time through the buffers */            
                GuardTy _lock_(globalBufferMutex);                                
                for( InptBuffersTy::value_type & buf : globalInputBuffers )
                {
                    switch( TOS_Topics::TypeBits( buf.first.first ) )
                    {
                    case TOSDB_STRING_BIT :    
                        ExtractFromBuffer<std::string>( buf.first.first, buf.first.second, buf.second ); 
                        break;
                    case TOSDB_INTGR_BIT :    
                        ExtractFromBuffer<def_size_type>( buf.first.first, buf.first.second, buf.second ); 
                        break;                                            
                    case TOSDB_QUAD_BIT :        
                        ExtractFromBuffer<ext_price_type>( buf.first.first, buf.first.second, buf.second ); 
                        break;                        
                    case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :    
                        ExtractFromBuffer<ext_size_type>( buf.first.first, buf.first.second, buf.second ); 
                        break;                            
                    default :                 
                        ExtractFromBuffer<def_price_type>( buf.first.first, buf.first.second, buf.second );                             
                    };            
                }
            }
            tDiff = std::chrono::duration_cast<milli_sec_type>( steadyClock.now() - tBeg).count();
            /* [ 0, 30000 ] */
            Sleep( std::min<long>( std::max<long>(((long)globalLatency - tDiff), 0 ), Glacial) ); 
        }
        awareOfConnection.store(false);
        rpcMaster.disconnect();    
        bufferThread = NULL;
        bufferThreadID = 0;
        return 0;
    }
};

const TOS_Topics::topic_map_type& TOS_Topics::globalTopicMap = TOS_Topics::_globalTopicMap;

BOOL WINAPI DllMain(HANDLE mod, DWORD why, LPVOID res)
{        
    switch(why)
    {
    case DLL_PROCESS_ATTACH:
        {              
        TOSDB_StartLogging( std::string(std::string(TOSDB_LOG_PATH) + std::string("client-log.log")).c_str() );
        // DO NOT AUTO-CONNECT; we need blocking ops deep into ->try_for_slave();
        // TOSDB_Connect(); 
        break; 
        }
    case DLL_THREAD_ATTACH:            
        break;
    case DLL_THREAD_DETACH:            
        break;
    case DLL_PROCESS_DETACH:    
        if( rpcMaster.connected() )
        {
            awareOfConnection.store(false);                
            for( const auto & buffer : globalInputBuffers )
            { /*don't call Release Buffer, just signal the service and close the handles */                
                RequestStreamOP( buffer.first.first, buffer.first.second, TOSDB_DEF_TIMEOUT, TOSDB_SIG_REMOVE );
                UnmapViewOfFile( std::get<3>(buffer.second) );
                CloseHandle( std::get<4>(buffer.second) );
            }
            TOSDB_Disconnect();    
        }
        break;        
    default:
        break;
    }
    return TRUE;
}

int TOSDB_Connect()
{     
    if( rpcMaster.connected() && awareOfConnection.load() )
        return 0;
    if( !rpcMaster.try_for_slave() )
    {
        TOSDB_Log_Raw_("could not connect with slave");
        return -1;
    }    
    if( bufferThread )
        return 0;
    if( !(bufferThread = CreateThread( 0, 0, Threaded_Init, 0, 0, &bufferThreadID)) )
    {
        TOSDB_Log_Raw_("error initializing communication thread");                
        return -2;    
    }
    return 0;
}

int TOSDB_Disconnect()
{    
    rpcMaster.disconnect();
    return 0;
}

unsigned int TOSDB_IsConnected()
{
    return rpcMaster.connected() && awareOfConnection.load() ? 1 : 0;
}

int TOSDB_CreateBlock( LPCSTR id, size_type sz, BOOL datetime, size_type timeout )
{    
    TOSDBlock* db;
    if( !rpcMaster.connected() || !awareOfConnection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -1;
    }    
    if( !ChceckIDLength( id ) )        
        return -2;
    rGuardTy _lock_(*globalMutex);
    if( GetBlockPtr(id) )
    {
        TOSDB_LogH("TOSDBlock", "TOSDBlock with this ID already exists");
        return -3; 
    }
    db = new TOSDBlock;     
    db->timeout = std::max<unsigned long>(timeout,TOSDB_MIN_TIMEOUT);
    db->block = TOSDB_RawDataBlock::CreateBlock( sz, datetime ? true : false );    
    if ( !db->block ) 
    {
        TOSDB_LogH( "DataBlock", "Error Creating TOSDB_RawDataBlock. TOSDBlock will be destroyed.");
        delete db;
        return -4;
    }    
    globalDDEBlockMap.insert( std::pair<std::string, TOSDBlock*>(id, db) );    
    return 0;
}

/* algo for adding sets of topics and strings, dealing with pre-cache; 
    all add methods end up here */
int TOSDB_Add( std::string id, str_set_type sItems, topic_set_type tTopics )
{
    topic_set_type  tDiff, oldTopics, totTopics;
    str_set_type    iDiff, oldItems, iUnion, totItems;
    bool            isEmpty;
    TOSDBlock       *db;
    HWND            hndl = NULL;
    int             errVal = 0;
    if( !rpcMaster.connected() || !awareOfConnection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -2;
    }    
    rGuardTy _lock_(*globalMutex);
    if( !(db = _getBlockPtr(id) ) )
        return -3;
    oldTopics = db->block->topics();
    oldItems = db->block->items(); 
    if( !db->itemPreCache.empty() )
    {    /* if we have pre-cached items, include them */    
        std::set_union(
            sItems.cbegin(),
            sItems.cend(),
            db->itemPreCache.cbegin(),
            db->itemPreCache.cend(),
            std::insert_iterator<str_set_type>(totItems,totItems.begin())            
            ); 
    }
    else /* 'copy', keep sItems available for pre-caching */
        totItems = sItems;    
    if( !db->topicPreCache.empty() )
    {    /* if we have pre-cached topics, include them */
        std::set_union(
            tTopics.cbegin(),
            tTopics.cend(),
            db->topicPreCache.cbegin(),
            db->topicPreCache.cend(),
            std::insert_iterator<topic_set_type>(totTopics,totTopics.begin()),
            TOS_Topics::top_less()
            ); 
    }
    else /* move, we don't need tTopics anymore */
        totTopics = std::move(tTopics); 
    /* find new items and topics to add */
    std::set_difference( 
            totTopics.cbegin(),
            totTopics.cend(),
            oldTopics.cbegin(),
            oldTopics.cend(),
            std::insert_iterator<topic_set_type>(tDiff,tDiff.begin()),
            TOS_Topics::top_less()
            );
    std::set_difference(
            totItems.cbegin(),
            totItems.cend(),
            oldItems.cbegin(),
            oldItems.cend(),
            std::insert_iterator<str_set_type>(iDiff,iDiff.begin())        
            );
    /* if some new topics; if atleast one item add them
        to the block; add ALL the items(new and old) for each */
    if (!tDiff.empty() ) 
    { 
        std::set_union(
                totItems.cbegin(),
                totItems.cend(),
                oldItems.cbegin(),
                oldItems.cend(),
                std::insert_iterator<str_set_type>(iUnion,iUnion.begin())                
                );
        isEmpty = iUnion.empty();
        for(auto & topic : tDiff)
        {                
            if( isEmpty )
                db->topicPreCache.insert( topic );
            for(auto & item : iUnion) 
            {                
                if( !RequestStreamOP( topic, item, db->timeout, TOSDB_SIG_ADD ) )                
                {
                    db->block->add_topic( topic );
                    db->block->add_item( item );
                    db->itemPreCache.clear();
                    db->topicPreCache.clear();
                    CaptureBuffer( topic, item, db );    
                }        
                else
                    ++errVal;
            }                    
        }            
    } 
    else if( oldTopics.empty() ) /* don't ignore items if no topics yet.. pre-cache them */
        for( auto & i : sItems)
            db->itemPreCache.insert( i );                
    for(auto & topic : oldTopics) /* make sure we add the new items to the old topics */
        for( auto & item : iDiff) 
            if( !RequestStreamOP( topic, item, db->timeout, TOSDB_SIG_ADD ) )
            {
                db->block->add_item( item );                    
                CaptureBuffer( topic, item, db );    
            }
            else
                ++errVal;
    return errVal;
}

int TOSDB_AddTopic( std::string id, TOS_Topics::TOPICS tTopic )
{
    return TOSDB_Add( id, str_set_type(), tTopic );
}

int TOSDB_AddTopic( LPCSTR id, LPCSTR sTopic )
{
    if( !ChceckIDLength(id) || !ChceckStringLength( sTopic ) )
        return -1;
    return TOSDB_AddTopic( id, GetTopicEnum(sTopic) );
}

int TOSDB_AddTopics( std::string id, topic_set_type tTopics )
{    
    return TOSDB_Add( id, str_set_type(), tTopics );
}

int TOSDB_AddTopics( LPCSTR id, LPCSTR* sTopics, size_type szTopics )
{    
    if( !ChceckIDLength(id) || !ChceckStringLengths(sTopics, szTopics) ) 
        return -1;         
    return TOSDB_Add( 
            id, 
            str_set_type(), 
            std::move( topic_set_type( 
                        sTopics, 
                        szTopics, 
                        [=](LPCSTR str){ return TOS_Topics::globalTopicMap[str]; } 
                        ) ) 
                    );
}

int TOSDB_AddItem( LPCSTR id, LPCSTR sItem )
{    
    if( !ChceckIDLength(id) || !ChceckStringLength( sItem ) )
        return -1;
    return TOSDB_Add( id, std::string(sItem), topic_set_type() );
}

int TOSDB_AddItems( std::string id, str_set_type sItems )
{
    return TOSDB_Add( id, sItems, topic_set_type() );
}

int TOSDB_AddItems( LPCSTR id, LPCSTR* sItems, size_type szItems )
{    
    if( !ChceckIDLength(id) || !ChceckStringLengths(sItems, szItems) ) 
        return -1;    
    return TOSDB_Add(id, std::move( str_set_type(sItems,szItems) ), topic_set_type());    
}

int TOSDB_Add( LPCSTR id, LPCSTR* sItems, size_type szItems, LPCSTR* sTopics, size_type szTopics)
{ 
    if( !ChceckIDLength(id) || 
        !ChceckStringLengths(sItems, szItems) || 
        !ChceckStringLengths(sTopics, szTopics) 
        ) return -1;    
    return TOSDB_Add(
            id, 
            std::move( str_set_type(sItems,szItems) ), 
            std::move( topic_set_type( 
                        sTopics, 
                        szTopics, 
                        [=](LPCSTR str){ return TOS_Topics::globalTopicMap[str]; } 
                        ) ) 
                    ); 
}

int TOSDB_RemoveTopic( LPCSTR id, LPCSTR sTopic )
{
    if( !ChceckIDLength(id) || !ChceckStringLength( sTopic ) )
        return -1;
    return TOSDB_RemoveTopic(id, TOS_Topics::globalTopicMap[sTopic] );
}

int TOSDB_RemoveTopic( std::string id, TOS_Topics::TOPICS tTopic )
{    
    TOSDBlock* db;
    int errVal = 0;
    if( !rpcMaster.connected() || !awareOfConnection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -2;
    }    
    rGuardTy _lock_(*globalMutex);
    if ( !(db = _getBlockPtr(id)) || !(TOS_Topics::enum_type)(tTopic) )
    {
        TOSDB_LogH("TOSDBlock", "Could not Remove.");
        return -3;
    }        
    if( db->block->has_topic( tTopic) )
    {
        for( const std::string & item : db->block->items() )
        {
            ReleaseBuffer( tTopic, item, db ); 
            if( RequestStreamOP( tTopic, item, db->timeout, TOSDB_SIG_REMOVE ))
            {
                ++errVal;
                TOSDB_LogH("IPC", "RequestStreamOP(TOSDB_SIG_REMOVE) failed, stream leaked");
            }
        }
        db->block->remove_topic( tTopic );
        if( db->block->topics().empty() )        
            for( const std::string & item : db->block->items() )
            {
                db->itemPreCache.insert( item );
                db->block->remove_item( item ); 
            }        
    }
    else
        errVal = -4;
    db->topicPreCache.erase( tTopic );
    return errVal;
}

int TOSDB_RemoveItem( LPCSTR id, LPCSTR sItem )
{    
    TOSDBlock* db;
    int errVal = 0;
    if( !rpcMaster.connected() || !awareOfConnection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -1;
    }    
    rGuardTy _lock_(*globalMutex);
    if ( !ChceckIDLength( id ) || !(db = _getBlockPtr(id) ) )
    {
        TOSDB_LogH("TOSDBlock", "Could not Remove.");
        return -2;
    }    
    if( !ChceckStringLength( sItem ) )
        return -3;
    if( db->block->has_item( sItem ) )
    {
        for( const TOS_Topics::TOPICS topic : db->block->topics() )
        {
            ReleaseBuffer( topic, sItem, db ); 
            if( RequestStreamOP( topic, sItem, db->timeout, TOSDB_SIG_REMOVE ) )
            {
                ++errVal;
                TOSDB_LogH("IPC", "RequestStreamOP(TOSDB_SIG_REMOVE) failed, stream leaked");
            }
        }
        db->block->remove_item( sItem );
        if( db->block->items().empty() )
        {
            for( const TOS_Topics::TOPICS topic : db->block->topics() )
            {
                db->topicPreCache.insert (topic );
                db->block->remove_topic( topic );
            }
        }
    }
    else
        errVal = -4;
    db->itemPreCache.erase( sItem );
    return errVal;
}

int TOSDB_CloseBlock( LPCSTR id )
{
    TOSDBlock* db;
    HANDLE delThread;
    DWORD delThreadID;
    int errVal = 0;
    if( !rpcMaster.connected() || !awareOfConnection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -1;
    }
    rGuardTy _lock_(*globalMutex);
    if ( !ChceckIDLength( id ) || !(db = _getBlockPtr(id) ) )
    {
        TOSDB_LogH("TOSDBlock", "Could not close.");
        return -2;
    }    
    for( const std::string & item : db->block->items() )
        for( TOS_Topics::TOPICS topic : db->block->topics() )
        {
            ReleaseBuffer( topic, item, db );
            if( RequestStreamOP( topic, item, db->timeout, TOSDB_SIG_REMOVE ))
            {
                ++errVal;
                TOSDB_LogH("IPC", "RequestStreamOP(TOSDB_SIG_REMOVE) failed, stream leaked");
            }
        }
    globalDDEBlockMap.erase( id );             
    if ( !(delThread = CreateThread( NULL, 0, BlockCleanup, (LPVOID)db->block, 0, &(delThreadID) )) )
    {
        errVal = -3;
        TOSDB_LogH( "Threading", "Error initializing clean-up thrad - using main \
                                  thread. ( THIS MAY BLOCK... for a long... long time! )");                
        BlockCleanup( (LPVOID)db->block );
    }
    delete db;
    return errVal;
}

int TOSDB_CloseBlocks()
{
    std::map< std::string, TOSDBlock* > gbCopy;    
    int errVal = 0;
    try
    { 
        rGuardTy _lock_(*globalMutex);        
        std::copy( /* need a copy, _CloseBlock removes from original */
            globalDDEBlockMap.begin(),
            globalDDEBlockMap.end(),
            std::insert_iterator< std::map< std::string, TOSDBlock*>>( gbCopy, gbCopy.begin())
            ); 
        for ( const auto& block: gbCopy )        
            if( TOSDB_CloseBlock( block.first.c_str() ) )
                ++errVal;
    }    
    catch( ... )
    {
        return -1;
    }
    return errVal;
}

int    TOSDB_DumpSharedBufferStatus()
{
    int ret;    
    unsigned int lCount = 0;
    rGuardTy _lock_(*globalMutex);
    while( rpcMaster.grab_pipe() <= 0 )
    {
        Sleep(TOSDB_DEF_TIMEOUT/10);            
        if( (lCount+=(TOSDB_DEF_TIMEOUT/10)) > TOSDB_DEF_TIMEOUT )
        {
            TOSDB_LogH("IPC","TOSDB_DumpSharedBufferStatus timed out trying to grab pipe");
            return -1;
        }
    }    
    rpcMaster << DynamicIPCMaster::shem_chunk( TOSDB_SIG_DUMP, 0 ) << DynamicIPCMaster::shem_chunk(0,0);
    if( !rpcMaster.recv( ret ) )
    {
        rpcMaster.release_pipe();
        TOSDB_LogH("IPC","recv failed; problem with connection");
        return -2;
    }
    rpcMaster.release_pipe();
    return ret;
}

int    TOSDB_GetBlockIDs( LPSTR* dest, size_type arrLen, size_type strLen )
{    
    rGuardTy _lock_(*globalMutex);
    if ( arrLen < globalDDEBlockMap.size() ) 
        return -1;
    int i, errVal;
    i = errVal = 0;
    for( auto & name : globalDDEBlockMap )
        if(    errVal = strcpy_s( dest[i++], strLen, name.first.c_str() ) ) 
            return errVal;             
    return errVal; 
}

str_set_type TOSDB_GetBlockIDs()
{
    str_set_type tmpStrs;
    rGuardTy _lock_(*globalMutex);
    for( auto & name : globalDDEBlockMap )
        tmpStrs.insert( name.first );
    return tmpStrs;
}

unsigned short inline TOSDB_GetLatency() { return globalLatency; }

unsigned short TOSDB_SetLatency( UpdateLatency latency ) 
{
    rGuardTy _lock_(*globalMutex);    
    unsigned short tmp = globalLatency;
    switch( latency)
    {
        case Fastest:    
        case VeryFast:        
        case Fast:        
        case Moderate:
        case Slow:
        case Glacial:
            globalLatency = latency;
            break;
        default:
            globalLatency = Moderate;
    }    
    return tmp;
}

const TOSDBlock* GetBlockPtr( const std::string id)
{
    try
        {            
            return globalDDEBlockMap.at(id);
        } 
    catch ( ... ) 
        { 
            TOSDB_Log("TOSDBlock", "TOSDBlock does not exist.");
            return NULL; 
        }
}

const TOSDBlock* GetBlockOrThrow( std::string id )
{
    const TOSDBlock* db;
    db = GetBlockPtr(id);
    if ( !db ) 
        throw TOSDB_error("TOSDBlock does not exist","TOSDBlock");
    else
        return db;
}

bool ChceckStringLength(LPCSTR str)
{
#ifndef SPPRSS_INPT_CHCK_
    if( strnlen_s(str, TOSDB_MAX_STR_SZ+1) == (TOSDB_MAX_STR_SZ+1) )
    {
        TOSDB_LogH("User Input", "string length > TOSDB_MAX_STR_SZ");
        return false;
    }
#endif
    return true;
}

bool ChceckStringLength(LPCSTR str, LPCSTR str2)
{
#ifdef SPPRSS_INPT_CHCK_
    return true;
#endif
    if( !ChceckStringLength( str ) )
        return false;
    return ChceckStringLength( str2 );    
}

bool ChceckStringLengths(LPCSTR* str, size_type szItems)
{
#ifndef SPPRSS_INPT_CHCK_
    while( szItems-- )
        if( strnlen_s(str[szItems], TOSDB_MAX_STR_SZ+1) == (TOSDB_MAX_STR_SZ+1) )
        {
            TOSDB_LogH("User Input", "string length > TOSDB_MAX_STR_SZ");
            return false;
        }
#endif
    return true;
}


bool ChceckIDLength(LPCSTR id)
{
#ifndef SPPRSS_INPT_CHCK_
    if( strnlen_s(id, TOSDB_BLOCK_ID_SZ + 1) == (TOSDB_BLOCK_ID_SZ + 1) )
    {
        TOSDB_LogH("Strings", "name/id length > TOSDB_BLOCK_ID_SZ");
        return false;
    }
#endif
    return true;
}

TOS_Topics::TOPICS GetTopicEnum( std::string sTopic){
    TOS_Topics::TOPICS t = TOS_Topics::globalTopicMap[sTopic];
    if ( !(TOS_Topics::enum_type)t ) 
        TOSDB_Log("TOS_Topic", "TOS_Topic string does not have a corresponding enum type in globalTopicMap.");
    return t;
}


