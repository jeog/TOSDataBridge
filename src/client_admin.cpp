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

std::recursive_mutex* const global_rmutex = new std::recursive_mutex;

namespace {        

    typedef std::tuple< unsigned int, unsigned int,    
                        std::set< const TOSDBlock* >, 
                        HANDLE, HANDLE >                  buffer_info_type;
    typedef std::pair< TOS_Topics::TOPICS, std::string >  buffer_id_type;
    typedef std::map< buffer_id_type, buffer_info_type >  buffers_type;
    typedef std::lock_guard< std::mutex >                 our_lock_guard_type;

    std::map<std::string,TOSDBlock*>  dde_blocks;

    DynamicIPCMaster   master( TOSDB_COMM_CHANNEL );
    std::atomic<bool>  aware_of_connection(false);   

    buffers_type       buffers;
    std::mutex         buffer_mutex;       
    unsigned short     buffer_latency   = (unsigned short)TOSDB_DEF_LATENCY;
    HANDLE             buffer_thread    =  NULL;
    DWORD              buffer_thread_id =  0;   

    steady_clock_type  steady_clock;

    TOSDBlock*    _getBlockPtr( std::string id);
    DWORD WINAPI  BlockCleanup( LPVOID lParam );

    
    int RequestStreamOP( TOS_Topics::TOPICS tTopic, 
                         std::string sItem, 
                         unsigned long timeout, 
                         unsigned int opcode )
    {  /* 
        * this needs exclusivity, but we can't block; calling code must lock 
        */
        int ret;
        bool ret_stat;
        unsigned int lcount = 0;

        if( opcode != TOSDB_SIG_ADD && opcode != TOSDB_SIG_REMOVE )
            return -1;     
       
        while( master.grab_pipe() <= 0 ){
            Sleep( TOSDB_DEF_TIMEOUT / 10 );   
            lcount += ( TOSDB_DEF_TIMEOUT / 10 );
            if( lcount >= TOSDB_DEF_TIMEOUT ){
                TOSDB_LogH( "IPC", "RequestStreamOP timed out "
                                   "trying to grab pipe" );
                return -2;
            }
        }

        DynamicIPCMaster::shem_chunk arg1 = master.insert( &tTopic );
        DynamicIPCMaster::shem_chunk arg2 = 
            master.insert( sItem.c_str(), (size_type)sItem.size() + 1 ); 
        DynamicIPCMaster::shem_chunk arg3 = master.insert( &timeout );

        master << DynamicIPCMaster::shem_chunk( opcode, 0 ) 
               << arg1 << arg2 << arg3 
               << DynamicIPCMaster::shem_chunk(0,0);

        /* don't remove until we get a response ! */
        ret_stat = master.recv( ret ); 

        master.remove( std::move(arg1) )
              .remove( std::move(arg2) )
              .remove( std::move(arg3) );

        if( !ret_stat ){
            master.release_pipe();
            TOSDB_LogH("IPC","recv failed; problem with connection");
            return -3;
        }

        master.release_pipe();
        return ret;
    }

    void CaptureBuffer( TOS_Topics::TOPICS tTopic, 
                        std::string sItem, 
                        const TOSDBlock* db )
    {
        std::string buf_name;
        void *fm_hndl, *mem_addr, *mtx_hndl;

        buffers_type::key_type buf_key(tTopic, sItem );        
        our_lock_guard_type lock(buffer_mutex);
        buffers_type::iterator b_iter = buffers.find( buf_key );

        if( b_iter != buffers.end() ){    
            std::get<2>(b_iter->second).insert( db );         
        }else{      

            buf_name = CreateBufferName( TOS_Topics::map[tTopic], sItem );
            fm_hndl = OpenFileMapping( FILE_MAP_READ, 0, buf_name.c_str() );

            if( !fm_hndl 
                || !(mem_addr = MapViewOfFile(fm_hndl,FILE_MAP_READ,0,0,0))){    

                    if( fm_hndl )
                        CloseHandle( fm_hndl);
                    throw TOSDB_BufferError( 
                        std::string("failure to map shared memory for: ")
                            .append( buf_name ).c_str() );
                }

            CloseHandle( fm_hndl ); 

            std::string mtx_name = std::string( buf_name ).append("_mtx");
            if( !(mtx_hndl = OpenMutex(SYNCHRONIZE,FALSE,mtx_name.c_str()))){ 
                UnmapViewOfFile( mem_addr );
                throw TOSDB_BufferError( 
                    std::string("failure to open MUTEX handle for: ")
                        .append( buf_name ).c_str() );
            }

            std::set< const TOSDBlock*> db_set;
            db_set.insert( db ); 
               
            buffers.insert( 
                buffers_type::value_type( buf_key, 
                                          std::make_tuple(0,0,std::move(db_set),
                                                          mem_addr,mtx_hndl) ) );                
        }       
 
    }    

    void ReleaseBuffer( TOS_Topics::TOPICS tTopic, 
                        std::string sItem, 
                        const TOSDBlock* db )
    {
        buffers_type::key_type buf_key(tTopic, sItem );

        our_lock_guard_type lock(buffer_mutex);

        buffers_type::iterator b_iter = buffers.find( buf_key );

        if( b_iter != buffers.end() ){
            std::get<2>(b_iter->second).erase( db );
            if( std::get<2>(b_iter->second).empty() ){
                UnmapViewOfFile( std::get<3>(b_iter->second) );
                CloseHandle( std::get<4>(b_iter->second) );
                buffers.erase( b_iter );        
            }
        }    
    }    

    template< typename T > 
    inline T CastToVal( char* val )
    { 
        return *(T*)val; 
    }
    
    template<> 
    inline std::string CastToVal<std::string>( char* val ) 
    {
        return std::string(val); 
    }
    
    template< typename T > 
    void ExtractFromBuffer( TOS_Topics::TOPICS topic, 
                            std::string item, 
                            buffer_info_type& buf_info )
    { /* 
       * some unecessary comp in here; cache some of the buffer params 
       */
        long npos;
        long long loop_diff, nelems;
        unsigned int dlen, dtsz;
        char* spot;

        pBufferHead head = (pBufferHead)std::get<3>(buf_info);
                
        if( head->next_offset - head->beg_offset == std::get<0>(buf_info) 
            && head->loop_seq == std::get<1>(buf_info) 
            || WaitForSingleObject( std::get<4>(buf_info), 0 ) ){
            /* 
             * attempt to bail early if chance buffer hasn't changed;
             * dont wait for the mutex, move on to the next 
             */
            return;     
        }

        /* get the effective size of the buffer */
        dlen = head->end_offset - head->beg_offset; 

        /* relative position since last read (begin offset needs to be 0) */    
        npos = head->next_offset - head->beg_offset - std::get<0>(buf_info); 
                
        if( (loop_diff = (head->loop_seq - std::get<1>(buf_info))) < 0 ){
            /* 
             * how many times has the buffer looped around, if any 
             */
            loop_diff+=(((long long)UINT_MAX+1)); /* in case of num limit */
        }

        /* how many elems have been written, 'borrow' a dlen if necesary */
        nelems = ( npos + (loop_diff * dlen) ) / head->elem_size; 
       
        if( !nelems){ 
            /* 
             * if no new elems we're done 
             */
            ReleaseMutex( std::get<4>(buf_info) );
            return;
        }else if( nelems < 0 ){ 
            /* 
             * if something very bad happened 
             */
            ReleaseMutex( std::get<4>(buf_info) );
            throw TOSDB_BufferError( "numElems < 0" );
        }else{ 
            /* make sure we don't insert more than the max elems */
            nelems = ((unsigned long long)nelems < (dlen / head->elem_size) ) 
                        ? nelems 
                        : (dlen / head->elem_size);

            dtsz = sizeof(DateTimeStamp);

            do{ /* 
                 * go through each elem, last first 
                 */
                spot = (char*)head + 
                       (((head->next_offset - (nelems * head->elem_size)) +dlen) 
                            % dlen );  
          
                for( const TOSDBlock* block : std::get<2>(buf_info) ){ 
                    /* 
                     * insert those elements into each block's raw data block 
                     */                    
                    block->     
                    block->
                       insertData( topic, item, CastToVal<T>(spot), std::move( 
                          *(pDateTimeStamp)(spot + ((head->elem_size) - dtsz)))
                          ); 
                }
            } while( --nelems );
        } 
        /* 
         * adjust our buffer info to the present values 
         */
        std::get<0>(buf_info) = head->next_offset - head->beg_offset;
        std::get<1>(buf_info) = head->loop_seq; 

        ReleaseMutex( std::get<4>(buf_info) );
    }

    TOSDBlock* _getBlockPtr( std::string id)
    { 
        try{                
            return dde_blocks.at(id);
        }catch(...){ 
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
        steady_clock_type::time_point tbeg;
        long tdiff;   
 
        aware_of_connection.store(true);
        while( master.connected() && aware_of_connection.load() ){ 
            /* 
             * the concurrent read loop errs on the side of greedyness 
             */
            tbeg = steady_clock.now(); /* include time waiting for lock */   
            {             
                our_lock_guard_type lock(buffer_mutex);                               
                /* 
                 * make sure we give up this lock each time through the buffers 
                 */ 
                for( buffers_type::value_type & buf : buffers ){

                    switch( TOS_Topics::TypeBits( buf.first.first ) )
                    {
                    case TOSDB_STRING_BIT :  

                        ExtractFromBuffer<std::string>( buf.first.first, 
                                                        buf.first.second, 
                                                        buf.second ); 
                        break;
                    case TOSDB_INTGR_BIT :    

                        ExtractFromBuffer<def_size_type>( buf.first.first, 
                                                          buf.first.second, 
                                                          buf.second ); 
                        break;                                            
                    case TOSDB_QUAD_BIT :   
     
                        ExtractFromBuffer<ext_price_type>( buf.first.first, 
                                                           buf.first.second, 
                                                           buf.second ); 
                        break;                        
                    case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :  
  
                        ExtractFromBuffer<ext_size_type>( buf.first.first, 
                                                          buf.first.second, 
                                                          buf.second ); 
                        break;                            
                    default :        
         
                        ExtractFromBuffer<def_price_type>( buf.first.first, 
                                                           buf.first.second, 
                                                           buf.second );                             
                    };   
         
                }
            }
            tdiff = std::chrono::duration_cast<milli_sec_type>(
                        steady_clock.now() - tbeg ).count();
            
            Sleep( std::min<long>( /* [ 0, 30000 ] */
                       std::max<long>( ((long)buffer_latency - tdiff), 0), 
                       Glacial ) ); 
        }
        aware_of_connection.store(false);
        master.disconnect();    
        buffer_thread = NULL;
        buffer_thread_id = 0;
        return 0;
    }
};

const TOS_Topics::topic_map_type& TOS_Topics::map = TOS_Topics::_map;

BOOL WINAPI DllMain(HANDLE mod, DWORD why, LPVOID res)
{        
    switch(why){
    case DLL_PROCESS_ATTACH:
        {              
        TOSDB_StartLogging( std::string( std::string(TOSDB_LOG_PATH) + 
                                         std::string("client-log.log")).c_str());
        // TOSDB_Connect(); 
        /* 
         * ! NO AUTO-CONNECT ! need blocking ops deep into ->try_for_slave() 
         */        
        break; 
        }
    case DLL_THREAD_ATTACH:            
        break;
    case DLL_THREAD_DETACH:            
        break;
    case DLL_PROCESS_DETACH:  
        if( master.connected() ){
            aware_of_connection.store(false);                
            for( const auto & buffer : buffers ){ 
               /* 
                * signal the service and close the handles 
                */                
                RequestStreamOP( buffer.first.first, buffer.first.second, 
                                 TOSDB_DEF_TIMEOUT, TOSDB_SIG_REMOVE );
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
    unsigned int lcount = 0;

    if( master.connected() && aware_of_connection.load() )
        return 0;

    if( !master.try_for_slave() ){
        TOSDB_Log_Raw_("could not connect with slave");
        return -1;
    }    

    if( buffer_thread )
        return 0;

    buffer_thread = CreateThread( 0, 0, Threaded_Init, 0, 0, &buffer_thread_id);
    if( !buffer_thread ){
        TOSDB_Log_Raw_("error initializing communication thread");                
        return -2;    
    }
    /* we need a timed wait on aware_of_connection to avoid situations 
     * where a lib call is made before Threaded_Init sets it to true
     */      
    while( !aware_of_connection.load() ){
        Sleep( TOSDB_DEF_TIMEOUT / 10 );  
        lcount += ( TOSDB_DEF_TIMEOUT / 10 );
        if( lcount >= TOSDB_DEF_TIMEOUT ){
            TOSDB_LogH( "IPC", "timed out waiting for aware_of_connection" );
            return -3;
        }
    }
    return 0;
}

int TOSDB_Disconnect()
{    
    master.disconnect();
    return 0;
}

unsigned int TOSDB_IsConnected()
{
    return master.connected() && aware_of_connection.load() ? 1 : 0;
}

int TOSDB_CreateBlock( LPCSTR id,
                       size_type sz, 
                       BOOL is_datetime, 
                       size_type timeout )
{    
    TOSDBlock* db;

    if( !aware_of_connection.load() ){
        TOSDB_LogH("IPC", "not connected to slave (aware_of_connection)");
        return -1;
    }    

    if( !master.connected() ){
        TOSDB_LogH("IPC", "not connected to slave (master.connected)");
        return -1;
    }

    if( !CheckIDLength( id ) )        
        return -2;

    our_rlock_guard_type lock(*global_rmutex);

    if( GetBlockPtr(id) ){
        TOSDB_LogH("TOSDBlock", "TOSDBlock with this ID already exists");
        return -3; 
    }

    db = new TOSDBlock;     
    db->timeout = std::max<unsigned long>(timeout,TOSDB_MIN_TIMEOUT);
    db->block = nullptr;
    try{
        db->block = TOSDB_RawDataBlock::CreateBlock( sz, (bool)is_datetime ); 
    }catch( TOSDB_DataBlockLimitError& e){
        TOSDB_LogH( "TOSDBlock", "Attempted to exceed Block Limit" );
    }catch( std::exception& e ){
        TOSDB_LogH( "TOSDBlock", e.what() );
    }
   
    if ( !db->block ){
        TOSDB_LogH( "DataBlock", "Error Creating TOSDB_RawDataBlock. "
                                 "TOSDBlock will be destroyed.");
        delete db;
        return -4;
    }   
 
    dde_blocks.insert( std::pair<std::string, TOSDBlock*>(id, db) );    
    return 0;
}

int TOSDB_Add( std::string id, str_set_type sItems, topic_set_type tTopics )
{  /* 
    * for adding sets of topics and strings, dealing with pre-cache; 
    * all add methods end up here 
    */
    topic_set_type  tdiff, old_topics, tot_topics;
    str_set_type    idiff, old_items, iunion, tot_items;

    bool            is_empty;
    TOSDBlock       *db;

    HWND            hndl = NULL;
    int             err = 0;

    if( !master.connected() || !aware_of_connection.load() ){
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -2;
    }    

    our_rlock_guard_type lock(*global_rmutex);

    if( !(db = _getBlockPtr(id) ) )
        return -3;

    old_topics = db->block->topics();
    old_items = db->block->items(); 

    
    if( !db->item_precache.empty() ){           
       /* 
        * if we have pre-cached items, include them 
        */
        std::set_union( sItems.cbegin(), sItems.cend(),
                        db->item_precache.cbegin(), db->item_precache.cend(),
                        std::insert_iterator<str_set_type>( tot_items,
                                                            tot_items.begin()) ); 
    }else{ 
        /* 
         * 'copy', keep sItems available for pre-caching 
         */
        tot_items = sItems;    
    }

    if( !db->topic_precache.empty() ){    
        /* 
         * if we have pre-cached topics, include them 
         */
        std::set_union( tTopics.cbegin(), tTopics.cend(),
                        db->topic_precache.cbegin(), db->topic_precache.cend(),
                        std::insert_iterator<topic_set_type>(tot_topics,
                                                             tot_topics.begin()),
                        TOS_Topics::top_less() ); 
    }else{ 
        /* 
         * move, we don't need tTopics anymore 
         */
        tot_topics = std::move(tTopics); 
    }

    /* 
     * find new items and topics to add 
     */
    std::set_difference( tot_topics.cbegin(), tot_topics.cend(),
                         old_topics.cbegin(), old_topics.cend(),
                         std::insert_iterator<topic_set_type>( tdiff,
                                                               tdiff.begin()),
                         TOS_Topics::top_less() );

    std::set_difference( tot_items.cbegin(), tot_items.cend(),
                         old_items.cbegin(), old_items.cend(),
                         std::insert_iterator<str_set_type>( idiff,
                                                             idiff.begin()) );

    if (!tdiff.empty() ){ 
       /* 
        * if new topics
        * if atleast one item add them to the block
        * add ALL the items (new and old) for each 
        */
        std::set_union( tot_items.cbegin(), tot_items.cend(),
                        old_items.cbegin(), old_items.cend(),
                        std::insert_iterator<str_set_type>( iunion,
                                                            iunion.begin()) );

        is_empty = iunion.empty();

        for(auto & topic : tdiff)
        {            
            if( is_empty )
                db->topic_precache.insert( topic );

            for(auto & item : iunion){            
                if( !RequestStreamOP(topic, item, db->timeout, TOSDB_SIG_ADD)){
                    db->block->add_topic( topic );
                    db->block->add_item( item );
                    db->item_precache.clear();
                    db->topic_precache.clear();
                    CaptureBuffer( topic, item, db );    
                }else
                    ++err;                
            }                    
        }      
    }else if( old_topics.empty() ){ 
        /* 
         * don't ignore items if no topics yet.. 
         */
        for( auto & i : sItems)
            db->item_precache.insert( i ); /* ...pre-cache them */       
    }
      
    for(auto & topic : old_topics) 
        /* 
         * add new items to the old topics 
         */
        for( auto & item : idiff){ 
            if( !RequestStreamOP( topic, item, db->timeout, TOSDB_SIG_ADD ) ){
                db->block->add_item( item );                    
                CaptureBuffer( topic, item, db );   
            }
            else
                ++err; 
        }

    return err;
}

int TOSDB_AddTopic( std::string id, TOS_Topics::TOPICS tTopic )
{
    return TOSDB_Add( id, str_set_type(), tTopic );
}

int TOSDB_AddTopic( LPCSTR id, LPCSTR sTopic )
{
    if( !CheckIDLength(id) || !CheckStringLength( sTopic ) )
        return -1;

    return TOSDB_AddTopic( id, GetTopicEnum(sTopic) );
}

int TOSDB_AddTopics( std::string id, topic_set_type tTopics )
{    
    return TOSDB_Add( id, str_set_type(), tTopics );
}

int TOSDB_AddTopics( LPCSTR id, LPCSTR* sTopics, size_type topics_len )
{    
    if( !CheckIDLength(id) || !CheckStringLengths(sTopics, topics_len) ) 
        return -1;  
       
    return TOSDB_Add( id, str_set_type(), 
                      std::move( topic_set_type( sTopics, topics_len, 
                                                 [=](LPCSTR str){ 
                                                    return TOS_Topics::map[str]; 
                                                 } ) ) );
}

int TOSDB_AddItem( LPCSTR id, LPCSTR sItem )
{    
    if( !CheckIDLength(id) || !CheckStringLength( sItem ) )
        return -1;

    return TOSDB_Add( id, std::string(sItem), topic_set_type() );
}

int TOSDB_AddItems( std::string id, str_set_type sItems )
{
    return TOSDB_Add( id, sItems, topic_set_type() );
}

int TOSDB_AddItems( LPCSTR id, LPCSTR* sItems, size_type items_len )
{    
    if( !CheckIDLength(id) || !CheckStringLengths(sItems, items_len) ) 
        return -1; 
   
    return TOSDB_Add( id, std::move( str_set_type(sItems,items_len) ), 
                      topic_set_type() );    
}

int TOSDB_Add( LPCSTR id, 
               LPCSTR* sItems, 
               size_type items_len, 
               LPCSTR* sTopics, 
               size_type topics_len )
{ 
    if( !CheckIDLength(id) 
        || !CheckStringLengths(sItems, items_len) 
        || !CheckStringLengths(sTopics, topics_len) )
    {    
        return -1;  
    }
  
    return TOSDB_Add( id, std::move( str_set_type(sItems,items_len) ), 
                      std::move( topic_set_type( sTopics, topics_len, 
                                                 [=](LPCSTR str){ 
                                                    return TOS_Topics::map[str]; 
                                                 } ) ) ); 
}

int TOSDB_RemoveTopic( LPCSTR id, LPCSTR sTopic )
{
    if( !CheckIDLength(id) || !CheckStringLength( sTopic ) )
        return -1;

    return TOSDB_RemoveTopic(id, TOS_Topics::map[sTopic] );
}

int TOSDB_RemoveTopic( std::string id, TOS_Topics::TOPICS tTopic )
{    
    TOSDBlock* db;
    int err = 0;

    if( !master.connected() || !aware_of_connection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -2;
    }
    
    our_rlock_guard_type lock(*global_rmutex);

    db = _getBlockPtr(id);
    if ( !db || !(TOS_Topics::enum_type)(tTopic) )
    {
        TOSDB_LogH("TOSDBlock", "Could not Remove.");
        return -3;
    }        

    if( db->block->has_topic( tTopic) )
    {
        for( const std::string & item : db->block->items() ){
            ReleaseBuffer( tTopic, item, db ); 
            if( RequestStreamOP( tTopic, item, db->timeout, TOSDB_SIG_REMOVE )){
                ++err;
                TOSDB_LogH( "IPC", "RequestStreamOP(TOSDB_SIG_REMOVE) failed, " 
                                   "stream leaked" );
            }
        }
        db->block->remove_topic( tTopic );

        if( db->block->topics().empty() )        
            for( const std::string & item : db->block->items() ){
                db->item_precache.insert( item );
                db->block->remove_item( item ); 
            }  
    }else
        err = -4;  

    db->topic_precache.erase( tTopic );
    return err;
}

int TOSDB_RemoveItem( LPCSTR id, LPCSTR sItem )
{    
    TOSDBlock* db;
    int err = 0;

    if( !master.connected() || !aware_of_connection.load() )
    {
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -1;
    }
    
    our_rlock_guard_type lock(*global_rmutex);

    if ( !CheckIDLength( id ) || !(db = _getBlockPtr(id) ) )
    {
        TOSDB_LogH("TOSDBlock", "Could not Remove.");
        return -2;
    }    

    if( !CheckStringLength( sItem ) )
        return -3;

    if( db->block->has_item( sItem ) )
    {
        for( const TOS_Topics::TOPICS topic : db->block->topics() ){
            ReleaseBuffer( topic, sItem, db ); 
            if( RequestStreamOP( topic, sItem, db->timeout, TOSDB_SIG_REMOVE )){
                ++err;
                TOSDB_LogH( "IPC", "RequestStreamOP(TOSDB_SIG_REMOVE) failed, "
                                   "stream leaked" );
            }
        }
        db->block->remove_item( sItem );

        if( db->block->items().empty() ){
            for( const TOS_Topics::TOPICS topic : db->block->topics() ){
                db->topic_precache.insert (topic );
                db->block->remove_topic( topic );
            }
        }
    }else
        err = -4;    

    db->item_precache.erase( sItem );
    return err;
}

int TOSDB_CloseBlock( LPCSTR id )
{
    TOSDBlock* db;
    HANDLE del_thrd_hndl;
    DWORD del_thrd_id;

    int err = 0;

    if( !master.connected() || !aware_of_connection.load() ){
        TOSDB_LogH("IPC", "not connected to slave/server");
        return -1;
    }

    our_rlock_guard_type lock(*global_rmutex);

    if ( !CheckIDLength( id ) || !(db = _getBlockPtr(id) ) ){
        TOSDB_LogH("TOSDBlock", "Could not close.");
        return -2;
    }    

    for( const std::string & item : db->block->items() )
        for( TOS_Topics::TOPICS topic : db->block->topics() ){
            ReleaseBuffer( topic, item, db );
            if( RequestStreamOP( topic, item, db->timeout, TOSDB_SIG_REMOVE )){
                ++err;
                TOSDB_LogH( "IPC", "RequestStreamOP(TOSDB_SIG_REMOVE) failed, "
                                   "stream leaked" );
            }
        }

    dde_blocks.erase( id );          
   
    del_thrd_hndl = CreateThread( NULL, 0, BlockCleanup, (LPVOID)db->block, 
                                  0, &(del_thrd_id) );
    if( !del_thrd_hndl ){
        err = -3;
        TOSDB_LogH( "Threading", 
                    "Error initializing clean-up thrad - using main thread."
                    " ( THIS MAY BLOCK... for a long... long time! )" ); 
        BlockCleanup( (LPVOID)db->block );
    }

    delete db;
    return err;
}

int TOSDB_CloseBlocks()
{
    std::map< std::string, TOSDBlock* > bcopy;    
    int err = 0;

    try{ 
        our_rlock_guard_type lock(*global_rmutex);  
        /* 
         * need a copy, _CloseBlock removes from original 
         */      
        std::copy( dde_blocks.begin(), dde_blocks.end(), 
                   std::insert_iterator<std::map< std::string,TOSDBlock*>>(
                       bcopy,bcopy.begin() )); 

        for ( const auto& block: bcopy )        
            if( TOSDB_CloseBlock( block.first.c_str() ) )
                ++err;
    }catch(...){
        return -1;
    }
    return err;
}

int TOSDB_DumpSharedBufferStatus()
{
    int ret;    
    unsigned int lcount = 0;

    our_rlock_guard_type lock(*global_rmutex);

    while( master.grab_pipe() <= 0 ){
        Sleep( TOSDB_DEF_TIMEOUT / 10 );           
        lcount += ( TOSDB_DEF_TIMEOUT / 10 );
        if( lcount >= TOSDB_DEF_TIMEOUT ){
            TOSDB_LogH( "IPC", "TOSDB_DumpSharedBufferStatus timed out "
                               "trying to grab pipe" );
            return -1;
        }
    }    
    master << DynamicIPCMaster::shem_chunk( TOSDB_SIG_DUMP, 0 ) 
           << DynamicIPCMaster::shem_chunk(0,0);

    if( !master.recv( ret ) ){
        master.release_pipe();
        TOSDB_LogH("IPC","recv failed; problem with connection");
        return -2;
    }
    master.release_pipe();
    return ret;
}

int TOSDB_GetBlockIDs( LPSTR* dest, size_type array_len, size_type str_len )
{    
    our_rlock_guard_type lock(*global_rmutex);

    if ( array_len < dde_blocks.size() ) 
        return -1;

    int i, err;
    i = err = 0;

    for( auto & name : dde_blocks )
        if( err = strcpy_s( dest[i++], str_len, name.first.c_str() ) ) 
            return err;      
       
    return err; 
}

str_set_type TOSDB_GetBlockIDs()
{
    str_set_type tmp;

    our_rlock_guard_type lock(*global_rmutex);

    for( auto & name : dde_blocks )
        tmp.insert( name.first );

    return tmp;
}

unsigned short inline TOSDB_GetLatency() 
{ 
    return buffer_latency; 
}

unsigned short TOSDB_SetLatency( UpdateLatency latency ) 
{
    our_rlock_guard_type lock(*global_rmutex);    

    unsigned short tmp = buffer_latency;
    switch( latency)
    {
        case Fastest:    
        case VeryFast:        
        case Fast:        
        case Moderate:
        case Slow:
        case Glacial:
            buffer_latency = latency;
            break;
        default:
            buffer_latency = Moderate;
    }    
    return tmp;
}

const TOSDBlock* GetBlockPtr( const std::string id)
{
    try{            
        return dde_blocks.at(id);
    }catch(...){ 
        TOSDB_Log("TOSDBlock", "TOSDBlock does not exist.");
        return NULL; 
    }
}

const TOSDBlock* GetBlockOrThrow( std::string id )
{
    const TOSDBlock* db;

    db = GetBlockPtr(id);
    if ( !db ) 
        throw TOSDB_Error("TOSDBlock does not exist","TOSDBlock");
    else
        return db;
}

bool CheckStringLength(LPCSTR str)
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

bool CheckStringLength(LPCSTR str, LPCSTR str2)
{
#ifdef SPPRSS_INPT_CHCK_
    return true;
#endif
    if( !CheckStringLength( str ) )
        return false;

    return CheckStringLength( str2 );    
}

bool CheckStringLengths(LPCSTR* str, size_type items_len)
{
#ifndef SPPRSS_INPT_CHCK_
    while( items_len-- )
        if( strnlen_s( str[items_len], TOSDB_MAX_STR_SZ + 1 ) 
                == (TOSDB_MAX_STR_SZ+1) )
        { 
            TOSDB_LogH("User Input", "string length > TOSDB_MAX_STR_SZ");
            return false;
        }
#endif
    return true;
}


bool CheckIDLength(LPCSTR id)
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

TOS_Topics::TOPICS GetTopicEnum( std::string sTopic)
{
    TOS_Topics::TOPICS t = TOS_Topics::map[sTopic];

    if ( !(TOS_Topics::enum_type)t )
    { 
        TOSDB_Log("TOS_Topic", "TOS_Topic string does not have a corresponding "
                               "enum type in map" );
    }
    return t;
}


