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
#include "ipc.hpp"

std::recursive_mutex global_rmutex;

namespace { 

typedef std::tuple<unsigned int, unsigned int,  
                   std::set<const TOSDBlock*>, HANDLE, HANDLE>  buffer_info_ty;

typedef std::map<std::pair<TOS_Topics::TOPICS, std::string>, buffer_info_ty>  buffers_ty;

LPCSTR LOG_NAME = "client-log.log";

/* blocks in *this* instance of the dll */
std::map<std::string,TOSDBlock*> dde_blocks;

/* buffers in shared mem */
buffers_ty buffers;
std::mutex buffers_mtx;
    
/* !!! 'buffers_lock_guard_' is reserved inside this namespace !!! */
#define LOCAL_BUFFERS_LOCK_GUARD \
    std::lock_guard<std::mutex> buffers_lock_guard_(buffers_mtx)

/* for 'scheduling' buffer reads */
steady_clock_type steady_clock;

unsigned long  buffer_latency = TOSDB_DEF_LATENCY;
HANDLE buffer_thread = NULL;
DWORD buffer_thread_id = 0;   

/* our IPC mechanism */
DynamicIPCMaster master(TOSDB_COMM_CHANNEL);

/* atomic flag that supports IPC connectivity */
std::atomic<bool> aware_of_connection(false);  

/* get our block (or NULL) to modify internally */
TOSDBlock* 
_getBlockPtr(std::string id)
{ 
    try{        
        return dde_blocks.at(id);
    }catch(...){     
        return NULL; 
    }
}

long 
RequestStreamOP(TOS_Topics::TOPICS topic_t, 
                std::string item, 
                unsigned long timeout, 
                unsigned int opcode)
/* needs exclusivity but can't block; CALLING CODE MUST LOCK
   returns 0 on sucess
         < 0 on IPC error
         = 1 if the engine fails to do what is requested */
{
    long ret;
    bool ret_stat;  
    DynamicIPCMaster::shem_chunk arg1, arg2, arg3;

    if(opcode != TOSDB_SIG_ADD && opcode != TOSDB_SIG_REMOVE)
        return -1;              

    IPC_TIMED_WAIT(master.grab_pipe() <= 0,
                   "RequestStreamOP timed out trying to grab pipe", 
                   -2);    

    /* need to stay alive until we can remove(below) */
    arg1 = master.insert(&topic_t);
    arg2 = master.insert(item.c_str(), item.size() + 1); 
    arg3 = master.insert(&timeout);

    master << DynamicIPCMaster::shem_chunk(opcode, 0) 
           << arg1 << arg2 << arg3 
           << DynamicIPCMaster::NULL_SHEM_CHUNK;    

    ret_stat = master.recv(ret); 
    /* don't remove until we get a response ! */
    master.remove(std::move(arg1)).remove(std::move(arg2)).remove(std::move(arg3));

    if(!ret_stat){
        master.release_pipe(); /* switch to non-blocking(raw) logging */
        TOSDB_Log_Raw("IPC; recv failed; problem with connection");        
        return -3;
    }
    master.release_pipe();

    return ret;
}


void 
CaptureBuffer(TOS_Topics::TOPICS topic_t, 
              std::string item, 
              const TOSDBlock* db)
{
    std::string buf_name;
    void *fm_hndl;
    void *mem_addr;
    void *mtx_hndl;
    buffers_ty::key_type buf_key(topic_t, item); 

    LOCAL_BUFFERS_LOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    buffers_ty::iterator b_iter = buffers.find(buf_key);
    if( b_iter != buffers.end() ){  
        std::get<2>(b_iter->second).insert(db);     
    }else{ 
        buf_name = CreateBufferName(TOS_Topics::map[topic_t], item);
        fm_hndl = OpenFileMapping(FILE_MAP_READ, 0, buf_name.c_str());

        if( !fm_hndl || !(mem_addr = MapViewOfFile(fm_hndl,FILE_MAP_READ,0,0,0)) )
        {  
            if(fm_hndl)
                CloseHandle(fm_hndl);
            std::string e("failure to map shared memory: "); 
            throw TOSDB_BufferError( e.append(buf_name).c_str() );
        }
        CloseHandle(fm_hndl); 

        std::string mtx_name = std::string(buf_name).append("_mtx");
        mtx_hndl = OpenMutex(SYNCHRONIZE,FALSE,mtx_name.c_str());
        if(!mtx_hndl){ 
            UnmapViewOfFile(mem_addr);
            std::string e("failure to open MUTEX handle: ");
            throw TOSDB_BufferError( e.append(buf_name).c_str() );
        }

        std::set<const TOSDBlock*> db_set;
        db_set.insert(db);  

        auto binfo = std::make_tuple(0,0,std::move(db_set),mem_addr,mtx_hndl);
        buffers.insert( buffers_ty::value_type(std::move(buf_key),std::move(binfo)) );        
    }     
    /* --- CRITICAL SECTION --- */
}  

void 
ReleaseBuffer(TOS_Topics::TOPICS topic_t, 
              std::string item, 
              const TOSDBlock* db)
{
    buffers_ty::key_type buf_key(topic_t, item);

    LOCAL_BUFFERS_LOCK_GUARD;
    /* --- CRITICAL SECTION --- */
    buffers_ty::iterator b_iter = buffers.find(buf_key);
    if(b_iter != buffers.end()){
        std::get<2>(b_iter->second).erase(db);
        if(std::get<2>(b_iter->second).empty())
        {
            UnmapViewOfFile(std::get<3>(b_iter->second));
            CloseHandle(std::get<4>(b_iter->second));
            buffers.erase(b_iter);    
        }
    }  
    /* --- CRITICAL SECTION --- */
}  

template<typename T> 
inline T 
CastToVal(char* val) 
{ 
    return *(T*)val; 
}
  
template<> 
inline std::string 
CastToVal<std::string>(char* val)
{ 
    return std::string(val); 
}
  

template<typename T> 
void 
ExtractFromBuffer(TOS_Topics::TOPICS topic, 
                  std::string item, 
                  buffer_info_ty& buf_info)
{
    /* some unecessary comp in here; cache some of the buffer params */ 

    long npos;
    long long loop_diff, nelems;
    unsigned int dlen;
    char* spot;

    pBufferHead head = (pBufferHead)std::get<3>(buf_info);
        
    if( head->next_offset - head->beg_offset == std::get<0>(buf_info) 
        && head->loop_seq == std::get<1>(buf_info) 
        || WaitForSingleObject(std::get<4>(buf_info), 0) )
    {
        /* attempt to bail early if chance buffer hasn't changed;
           dont wait for the mutex, move on to the next */
        return;   
    }

    /* get the effective size of the buffer */
    dlen = head->end_offset - head->beg_offset; 

    /* relative position since last read (begin offset needs to be 0) */  
    npos = head->next_offset - head->beg_offset - std::get<0>(buf_info); 
        
    if((loop_diff = (head->loop_seq - std::get<1>(buf_info))) < 0)
    {/* how many times has the buffer looped around, if any */
        loop_diff+=(((long long)UINT_MAX+1)); /* in case of num limit */
    }

    /* how many elems have been written, 'borrow' a dlen if necesary */
    nelems = (npos + (loop_diff * dlen)) / head->elem_size;      
    if(!nelems){ /* if no new elems we're done */
        ReleaseMutex(std::get<4>(buf_info));
        return;
    }else if(nelems < 0){ /* if something very bad happened */
        ReleaseMutex(std::get<4>(buf_info));
        throw TOSDB_BufferError("numElems < 0");
    }else{ /* extract */  

        if((unsigned long long)nelems >= (dlen / head->elem_size)){ 
            /* make sure we don't insert more than the max elems */
            nelems = dlen / head->elem_size;
        }
        
        do{ /* go through each elem, last first  */
            spot = (char*)head + 
                   (((head->next_offset - (nelems * head->elem_size)) + dlen) % dlen);  
      
            for(const TOSDBlock* block : std::get<2>(buf_info)){ 
                /* insert those elements into each block's raw data block */          
                block->   
                block->
                    insert_data(
                        topic, 
                        item, 
                        CastToVal<T>(spot), 
                        *(pDateTimeStamp)(spot + ((head->elem_size) - sizeof(DateTimeStamp)))
                    ); 
            }
        }while(--nelems);
    } 
    /* adjust our buffer info to the present values */
    std::get<0>(buf_info) = head->next_offset - head->beg_offset;
    std::get<1>(buf_info) = head->loop_seq; 

    ReleaseMutex(std::get<4>(buf_info));
}


DWORD WINAPI 
BlockCleanup(LPVOID lParam)
{
    TOSDB_RawDataBlock* cln = (TOSDB_RawDataBlock*)lParam;
    if(cln)
        delete cln;

    return 0;
}


DWORD WINAPI 
Threaded_Init(LPVOID lParam)
{        
    steady_clock_type::time_point tbeg, tend;
    long tdiff;   
 
    aware_of_connection.store(true);
    while(master.connected() && aware_of_connection.load()){ 
        /* the concurrent read loop errs on the side of greedyness */
        tbeg = steady_clock.now(); /* include time waiting for lock */   
        {       
            LOCAL_BUFFERS_LOCK_GUARD;  
            /* --- CRITICAL SECTION --- */             
            for(buffers_ty::value_type & buf : buffers)
            {
                switch(TOS_Topics::TypeBits(buf.first.first)){
                case TOSDB_STRING_BIT :                  
                    ExtractFromBuffer<std::string>(buf.first.first, buf.first.second, buf.second); 
                    break;
                case TOSDB_INTGR_BIT :                  
                    ExtractFromBuffer<def_size_type>(buf.first.first, buf.first.second, buf.second); 
                    break;                      
                case TOSDB_QUAD_BIT :                   
                    ExtractFromBuffer<ext_price_type>(buf.first.first, buf.first.second, buf.second); 
                    break;            
                case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :                  
                    ExtractFromBuffer<ext_size_type>(buf.first.first, buf.first.second, buf.second); 
                    break;              
                default : 
                    ExtractFromBuffer<def_price_type>(buf.first.first, buf.first.second, buf.second);                         
                };        
            }
            /* --- CRITICAL SECTION --- */
        } /* make sure we give up this lock each time through the buffers */

        tend = steady_clock.now();
        tdiff = std::chrono::duration_cast<std::chrono::duration<long, std::milli>>(tend - tbeg).count();    
        Sleep(std::min<long>(std::max<long>((buffer_latency - tdiff),0),Glacial)); 
    }

    aware_of_connection.store(false);
    master.disconnect();  
    buffer_thread = NULL;
    buffer_thread_id = 0;

    return 0;
}

}; /* namespace */


BOOL WINAPI  
DllMain(HANDLE mod, DWORD why, LPVOID res) /* ENTRY POINT */
{    
    /* NOTE: using HANDLE vs HINSTANCE for some reason;
             no current need for mod so just leave for now to avoid
             breaking something (Nov 10 2016)
    */
    switch(why){
    case DLL_PROCESS_ATTACH:
        /* ! NO AUTO-CONNECT ! need blocking ops deep into ->try_for_slave()   */
        {      
            std::string p = BuildLogPath(LOG_NAME);	                  
            StartLogging(p.c_str());  
        } 
        break;   
    case DLL_THREAD_ATTACH: 
        break;
    case DLL_THREAD_DETACH: 
        break;
    case DLL_PROCESS_DETACH:  
        {
            if( master.connected() ){
                aware_of_connection.store(false);        
                for(const auto & buffer : buffers)
                {/* signal the service and close the handles */        
                    RequestStreamOP(buffer.first.first, buffer.first.second, 
                                    TOSDB_DEF_TIMEOUT, TOSDB_SIG_REMOVE);
                    UnmapViewOfFile(std::get<3>(buffer.second));
                    CloseHandle(std::get<4>(buffer.second));
                }
                TOSDB_Disconnect();  
            }
            StopLogging();
        } 
        break;    
    default: 
        break;
    }

    return TRUE;
}

/* Nov 1 2016: Change (non-blocking) raw log calls to regular (blocking) 

   We should be able to block in here as long as this is not called from DllMain

   !!! BUGS IN IPC (after NOV 1 2016) WE SHOULD LOOK HERE FIRST !!! */
int 
TOSDB_Connect() 
{  
    unsigned int lcount = 0;

    if(master.connected() && aware_of_connection.load())
        return 0;

    if(!master.try_for_slave()){
        TOSDB_LogH("IPC","TOSDB_Connect(): could not connect with slave");
        return -1;
    }  

    if(buffer_thread)
        return 0;

    buffer_thread = CreateThread(0, 0, Threaded_Init, 0, 0, &buffer_thread_id);
    if(!buffer_thread){
        TOSDB_LogH("IPC","TOSDB_Connect(): error initializing communication thread");        
        return -2;  
    }

    /* we need a timed wait on aware_of_connection to avoid situations 
       where a lib call is made before Threaded_Init sets it to true   */    
    IPC_TIMED_WAIT(!aware_of_connection.load(),
                   "timed out waiting for aware_of_connection", 
                   -3);
    return 0;
}

int 
TOSDB_Disconnect()
{  
    master.disconnect();
    return 0;
}


unsigned int 
TOSDB_IsConnected()
{
    return master.connected() && aware_of_connection.load() ? 1 : 0;
}


#define RETURN_IF_NOT_CONNECTED(ret) do{ \
    if( !aware_of_connection.load() ){ \
        TOSDB_LogH("IPC", "not connected to slave (!aware_of_connection)"); \
        return ret; \
    } \
    if( !master.connected() ){ \
        TOSDB_LogH("IPC", "not connected to slave (!master.connected)"); \
        return ret; \
    } \
}while(0)

int 
TOSDB_CreateBlock(LPCSTR id,
                  size_type sz,
                  BOOL is_datetime,
                  size_type timeout)
{  
    TOSDBlock* db;

    RETURN_IF_NOT_CONNECTED(-1);

    if( !CheckIDLength(id) )    
        return -2;

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    if( GetBlockPtr(id,false) ){
        TOSDB_LogH("BLOCK", "TOSDBlock with this ID already exists");
        return -3; 
    }

    db = new TOSDBlock;   
    db->timeout = std::max<unsigned long>(timeout,TOSDB_MIN_TIMEOUT);
    db->block = nullptr;

    try{
        db->block = TOSDB_RawDataBlock::CreateBlock(sz, is_datetime); 
    }catch(TOSDB_DataBlockLimitError&){
        TOSDB_LogH("BLOCK", "attempt to exceed block limit");
    }catch(std::exception& e){
        TOSDB_LogH("BLOCK", e.what());
    }
   
    if(!db->block){
        TOSDB_LogH("BLOCK", "creation error, TOSDBlock will be destroyed.");
        delete db;
        return -4;
    }   
 
    dde_blocks.insert( std::pair<std::string, TOSDBlock*>(id, db) );  
    return 0;
    /* --- CRITICAL SECTION --- */
}


int 
TOSDB_Add(std::string id, str_set_type items, topic_set_type topics_t)
/* adding sets of topics and strings, dealing with pre-cache; 
   all Add_ methods end up in here  */
{
    topic_set_type tdiff;
    topic_set_type old_topics;
    topic_set_type tot_topics;
    str_set_type idiff;
    str_set_type old_items;
    str_set_type tot_items;
    str_set_type iunion;
    bool is_empty;
    TOSDBlock *db;

    HWND hndl = NULL;
    int err = 0;

    RETURN_IF_NOT_CONNECTED(-2);    
	
    /* remove NULL topics */
    auto is_null = [&](TOS_Topics::TOPICS t){ return t == TOS_Topics::TOPICS::NULL_TOPIC; };    

    topics_t.erase(std::remove_if(topics_t.begin(), topics_t.end(), is_null), topics_t.end());	

    /* fail if both are empty - note: this could result from NULL topics */
    if( items.empty() && topics_t.empty() )
        return -1; /* closer to bad param than anything else */

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", "block doesn't exist");
        return -3;
    }

    old_topics = db->block->topics();
    old_items = db->block->items(); 
  
    if( !db->item_precache.empty() ) 
       /* if we have pre-cached items, include them */
       std::set_union(items.cbegin(), items.cend(),
                      db->item_precache.cbegin(), db->item_precache.cend(),
                      std::insert_iterator<str_set_type>(tot_items,tot_items.begin())); 
    else 
        tot_items = items; /* 'copy', keep items available for pre-caching */ 
   
  
    if( !db->topic_precache.empty() ) 
        /* if we have pre-cached topics, include them */
        std::set_union(topics_t.cbegin(), topics_t.cend(),
                       db->topic_precache.cbegin(), db->topic_precache.cend(),
                       std::insert_iterator<topic_set_type>(tot_topics,tot_topics.begin()),
                       TOS_Topics::top_less()); 
    else 
        tot_topics = std::move(topics_t); /* move, don't need topics_t anymore */  
    

    /* find new items and topics to add */
    std::set_difference(tot_topics.cbegin(), tot_topics.cend(),
                        old_topics.cbegin(), old_topics.cend(),
                        std::insert_iterator<topic_set_type>(tdiff,tdiff.begin()),
                        TOS_Topics::top_less());

    std::set_difference(tot_items.cbegin(), tot_items.cend(),
                        old_items.cbegin(), old_items.cend(),
                        std::insert_iterator<str_set_type>(idiff, idiff.begin()));


    if( !tdiff.empty() ){
        /* if new topics, atleast one item, add them to the block
           add ALL the items (new and old) for each */
        std::set_union(tot_items.cbegin(), tot_items.cend(),
                       old_items.cbegin(), old_items.cend(),
                       std::insert_iterator<str_set_type>(iunion, iunion.begin()));
        
        is_empty = iunion.empty();

        for(auto & topic : tdiff){  
            if(is_empty)
                db->topic_precache.insert(topic);

            for(auto & item : iunion){
                /* TRY TO ADD TO BLOCK */
                if( !RequestStreamOP(topic, item, db->timeout, TOSDB_SIG_ADD) ){
                    db->block->add_topic(topic);
                    db->block->add_item(item);
                    db->item_precache.clear();
                    db->topic_precache.clear();
                    CaptureBuffer(topic, item, db);  
                }else{
                    ++err;        
                }
            }          
        }    
    }else if(old_topics.empty()){ /* don't ignore items if no topics yet.. */
        for(auto & i : items)
            db->item_precache.insert(i); /* ...pre-cache them */     
    }
    

    /* add new items to the old topics */
    for(auto & topic : old_topics){     
        for(auto & item : idiff){       
            /* TRY TO ADD TO BLOCK */
            if( !RequestStreamOP(topic, item, db->timeout, TOSDB_SIG_ADD) ){
                db->block->add_item(item);          
                CaptureBuffer(topic, item, db);   
            }else{
                ++err; 
            }
        }
    }

    return err;
    /* --- CRITICAL SECTION --- */
}


int 
TOSDB_AddTopic(std::string id, TOS_Topics::TOPICS topic_t)
{
    return TOSDB_Add(id, str_set_type(), topic_t);
}


int   
TOSDB_AddItem(std::string id, std::string item)
{
    return TOSDB_Add(id, item, topic_set_type());
}


int 
TOSDB_AddTopic(LPCSTR id, LPCSTR topic_str)
{
    if( !CheckIDLength(id) || !CheckStringLength(topic_str) )
        return -1;

    return TOSDB_AddTopic(id, GetTopicEnum(topic_str));
}


int 
TOSDB_AddTopics(std::string id, topic_set_type topics_t)
{  
    return TOSDB_Add(id, str_set_type(), topics_t);
}


int 
TOSDB_AddTopics(LPCSTR id, LPCSTR* topics_str, size_type topics_len)
{  
    if( !CheckIDLength(id) || !CheckStringLengths(topics_str, topics_len) ) 
        return -1;  

    auto f =  [=](LPCSTR str){ return TOS_Topics::map[str]; };
    topic_set_type tset(topics_str, topics_len, f);

    return TOSDB_Add(id, str_set_type(), std::move(tset));
}


int 
TOSDB_AddItem(LPCSTR id, LPCSTR item)
{  
    if( !CheckIDLength(id) || !CheckStringLength(item) )
        return -1;

    return TOSDB_Add(id, std::string(item), topic_set_type());
}


int 
TOSDB_AddItems(std::string id, str_set_type items)
{
    return TOSDB_Add(id, items, topic_set_type());
}


int 
TOSDB_AddItems(LPCSTR id, LPCSTR* items, size_type items_len)
{  
    if( !CheckIDLength(id) || !CheckStringLengths(items, items_len) ) 
        return -1; 
   
    return TOSDB_Add(id, str_set_type(items,items_len), topic_set_type());  
}


int 
TOSDB_Add(LPCSTR id, 
          LPCSTR* items, 
          size_type items_len, 
          LPCSTR* topics_str, 
          size_type topics_len)
{ 
    if( !CheckIDLength(id) 
        || !CheckStringLengths(items, items_len) 
        || !CheckStringLengths(topics_str, topics_len) ){  
        return -1;  
    }

    auto f = [=](LPCSTR str){ return TOS_Topics::map[str]; };
    topic_set_type tset(topics_str, topics_len, f);
  
    return TOSDB_Add(id, str_set_type(items,items_len), std::move(tset)); 
}


int 
TOSDB_RemoveTopic(LPCSTR id, LPCSTR topic_str)
{
    if( !CheckIDLength(id) || !CheckStringLength(topic_str) )
        return -1;

    return TOSDB_RemoveTopic(id, TOS_Topics::map[topic_str]);
}


int 
TOSDB_RemoveTopic(std::string id, TOS_Topics::TOPICS topic_t)
{  
    TOSDBlock* db;
    int err = 0;

    RETURN_IF_NOT_CONNECTED(-2);
  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", "block doesn't exist");
        return -3;
    }   

    if( !(TOS_Topics::enum_value_type)(topic_t) ){
        TOSDB_LogH("TOPIC", "NULL topic");
        return -3;
    }   

    /* OCT 30 2016 --- if not in block, check if in pre-cache, before returning error */
    if( db->block->has_topic(topic_t) ){
        for(auto & item : db->block->items())
        {
            ReleaseBuffer(topic_t, item, db); 
            if( RequestStreamOP(topic_t, item, db->timeout, TOSDB_SIG_REMOVE) ){
                ++err;
                TOSDB_LogH("IPC","RequestStreamOP(REMOVE) failed, stream leaked");
            }
        }

        db->block->remove_topic(topic_t);

        if( db->block->topics().empty() ){
            for(const std::string & item : db->block->items()){
                db->item_precache.insert(item);
                db->block->remove_item(item); 
            }  
        }
    }else if(db->topic_precache.find(topic_t) == db->topic_precache.end()){
        err = -4;  
    }

    db->topic_precache.erase(topic_t);
    return err;
    /* --- CRITICAL SECTION --- */
}


int   
TOSDB_RemoveItem(std::string id, std::string item)
{
    return TOSDB_RemoveItem(id.c_str(), item.c_str());

}


int 
TOSDB_RemoveItem(LPCSTR id, LPCSTR item)
{  
    TOSDBlock* db;
    int err = 0;

    RETURN_IF_NOT_CONNECTED(-1);
  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    if( !CheckIDLength(id) ){
        TOSDB_LogH("BLOCK", "invalid id length");
        return -2;
    }  

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", "block doesn't exist");
        return -2;
    }  

    if( !CheckStringLength(item) )
        return -3;

    /* OCT 30 2016 --- if not in block, check if in pre-cache, before returning error */
    if( db->block->has_item(item) ){
        for(auto topic : db->block->topics())
        {
            ReleaseBuffer(topic, item, db); 
            if( RequestStreamOP(topic, item, db->timeout, TOSDB_SIG_REMOVE) ){
                ++err;
                TOSDB_LogH("IPC","RequestStreamOP(REMOVE) failed, stream leaked");
            }
        }

        db->block->remove_item(item);

        if( db->block->items().empty() ){
            for(const TOS_Topics::TOPICS topic : db->block->topics()){
                db->topic_precache.insert (topic);
                db->block->remove_topic(topic);
            }
        }
    }else if(db->item_precache.find(item) == db->item_precache.end()){
        err = -4;  
    }

    db->item_precache.erase(item);
    return err;
  /* --- CRITICAL SECTION --- */
}


int 
TOSDB_CloseBlock(LPCSTR id)
{
    TOSDBlock* db;
    HANDLE del_thrd_hndl;
    DWORD del_thrd_id;

    int err = 0;

    RETURN_IF_NOT_CONNECTED(-1);

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    if( !CheckIDLength(id) ){
        TOSDB_LogH("BLOCK", "invalid id length");
        return -2;
    }  

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", "block doesn't exist");
        return -2;
    }  

    for(auto & item : db->block->items()){
        for(auto topic : db->block->topics())
        {
            ReleaseBuffer(topic, item, db);
            if( RequestStreamOP(topic, item, db->timeout, TOSDB_SIG_REMOVE) ){
                ++err;
                TOSDB_LogH("IPC", "RequestStreamOP(REMOVE) failed, stream leaked");
            }
        }
    }

    dde_blocks.erase(id);       

    /* spin-off block destruction to its own thread so we don't block main */
    del_thrd_hndl = CreateThread(NULL, 0, BlockCleanup, (LPVOID)db->block, 0, &(del_thrd_id));
    if(!del_thrd_hndl){
        err = -3;
        TOSDB_LogH("THREAD", "using main thread to clean-up(THIS MAY BLOCK)"); 
        BlockCleanup((LPVOID)db->block);
    }

    delete db;

    return err;
    /* --- CRITICAL SECTION --- */
}

int 
TOSDB_CloseBlocks()
{
    std::map<std::string, TOSDBlock*> bcopy;  
    int err = 0;
    try{ 
        GLOBAL_RLOCK_GUARD;  
        /* --- CRITICAL SECTION --- */

        /* need a copy, _CloseBlock removes from original */    
        std::insert_iterator<std::map<std::string,TOSDBlock*>> i(bcopy,bcopy.begin());
        std::copy(dde_blocks.begin(), dde_blocks.end(), i); 
        for(auto & b: bcopy){    
            if( TOSDB_CloseBlock(b.first.c_str()) )
                ++err;
        }
        /* --- CRITICAL SECTION --- */
    }catch(...){ 
        return -1; 
    }

    return err;
}


int
TOSDB_DumpSharedBufferStatus()
{
    long ret;  
    unsigned int lcount = 0;

    RETURN_IF_NOT_CONNECTED(-1);

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    IPC_TIMED_WAIT(master.grab_pipe() <= 0,
                   "TOSDB_DumpSharedBufferStatus timeout trying to grab pipe",
                   -1);
  
    master<< DynamicIPCMaster::shem_chunk(TOSDB_SIG_DUMP, 0) 
          << DynamicIPCMaster::NULL_SHEM_CHUNK;

    if( !master.recv(ret) ){
        master.release_pipe();
        TOSDB_LogH("IPC","recv failed; problem with connection");
        return -2;
    }
    master.release_pipe();

    return (int)ret;
    /* --- CRITICAL SECTION --- */
}



int 
TOSDB_GetBlockIDs(LPSTR* dest, size_type array_len, size_type str_len)
{  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */
    if(array_len < dde_blocks.size()) 
      return -1;

    int i = 0;
    int err = 0;

    for(auto & name : dde_blocks){
        err = strcpy_s(dest[i++], str_len, name.first.c_str());
        if(err) 
            return err;    
    }
     
    return err; 
    /* --- CRITICAL SECTION --- */
}

str_set_type 
TOSDB_GetBlockIDs()
{
    str_set_type tmp;

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */
    for(auto & name : dde_blocks)
        tmp.insert(name.first);

    return tmp;
    /* --- CRITICAL SECTION --- */
}


unsigned long 
TOSDB_GetLatency() 
{ 
    return buffer_latency; 
}


int
TOSDB_GetClientLogPath(char* path, size_type sz)
{       
    std::string p = BuildLogPath(LOG_NAME); 
    size_type psz = p.size() + 1; /* for /n */

    if(sz < psz)
        return psz; 
    
    return strcpy_s(path,psz,p.c_str()) ? -1 : 0;
}

unsigned long 
TOSDB_SetLatency(UpdateLatency latency) 
{
    GLOBAL_RLOCK_GUARD;  
    /* --- CRITICAL SECTION --- */
    unsigned long tmp = buffer_latency;

    switch(latency){
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
  /* --- CRITICAL SECTION --- */
}


const TOSDBlock* 
GetBlockPtr(const std::string id, bool log)
{
    try{      
        return dde_blocks.at(id);
    }catch(...){ 
        if(log){
            std::string msg = "block " + id + " doesn't exist";
            TOSDB_LogH("BLOCK", msg.c_str());
        }
        return NULL; 
    }
}


const TOSDBlock* 
GetBlockOrThrow(std::string id)
{
    const TOSDBlock* db;

    db = GetBlockPtr(id,false);
    if(!db) 
        throw TOSDB_DataBlockError("block doesn't exist");
 
    return db;
}

/*
TOS_Topics::TOPICS 
GetTopicEnum(std::string topic_str)
{
    TOS_Topics::TOPICS t = TOS_Topics::map[topic_str];
    /* why are we logging this ?? 
    if( !(TOS_Topics::enum_value_type)t )
        TOSDB_Log("TOS_Topic", "TOS_Topic has no corresponding enum type in map"); */ /*
    return t;
}
*/

