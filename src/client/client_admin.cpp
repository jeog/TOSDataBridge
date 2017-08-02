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

/* item/topic to test connection */
const TOS_Topics::TOPICS TEST_TOPIC = TOS_Topics::TOPICS::LAST;
const std::string TEST_ITEM = "SPY";

/* blocks in *this* instance of the dll */
std::map<std::string,TOSDBlock*> dde_blocks;

/* buffers in shared mem */
buffers_ty buffers;
std::mutex buffers_mtx;
    
/* !!! 'buffers_lock_guard_' is reserved inside this namespace !!! */
#define LOCAL_BUFFERS_LOCK_GUARD std::lock_guard<std::mutex> buffers_lock_guard_(buffers_mtx)

/* for 'scheduling' buffer reads */
steady_clock_type steady_clock;

unsigned long  buffer_latency = TOSDB_DEF_LATENCY;
HANDLE buffer_thread = NULL;
DWORD buffer_thread_id = 0;   

/* our IPC mechanism */
IPCMaster master(TOSDB_COMM_CHANNEL);

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


bool
_connected(bool log_if_not_connected=false)
{    
    if(!aware_of_connection.load()){
        if(log_if_not_connected)
            TOSDB_LogH("IPC", "not connected to slave (!aware_of_connection)");
        return false;
    }

    if(!master.connected(TOSDB_DEF_TIMEOUT)){
        if(log_if_not_connected)        
            TOSDB_LogH("IPC", "not connected to slave (!master.connected)");            
        return false;
    }

    return true;
}


long 
_requestStreamOP(TOS_Topics::TOPICS topic_t, 
                std::string item, 
                unsigned long timeout, 
                unsigned int opcode)
{ /* needs exclusivity but can't block; CALLING CODE MUST LOCK
     returns 0 on sucess, TOSDB_ERROR... on error */

    /* build ipc msg early so we can log it on error */
    std::string msg = std::to_string(opcode) + ' ' + TOS_Topics::MAP()[topic_t] + ' '
                    + item + ' ' + std::to_string(timeout); 

    switch(opcode){
    case TOSDB_SIG_ADD:
    case TOSDB_SIG_REMOVE:
    case TOSDB_SIG_TEST:
        break;
    default:
        TOSDB_LogRawH("IPC", ("_requestStreamOP received bad opcode, msg:" + msg).c_str());
        return TOSDB_ERROR_BAD_SIG;
    }         

    if( !_connected() ){
        TOSDB_LogRawH("IPC", ("_requestStreamOP failed, not connected, msg:" + msg).c_str());
        return TOSDB_ERROR_NOT_CONNECTED;
    }

    if( !master.call(&msg,timeout) ){
        TOSDB_LogRawH("IPC",("master.call failled in _requestStreamOP, msg:" + msg).c_str());
        return TOSDB_ERROR_IPC;
    }

    try{
        long r = std::stol(msg);        
        if(r)
            TOSDB_LogRawH("ENGINE", ("error code returned from engine: " + std::to_string(r)).c_str());
        return r;
    }catch(...){
        TOSDB_LogRawH("IPC", ("failed to convert return message to long, msg:" + msg).c_str());
        return TOSDB_ERROR_IPC;
    }    
}


void 
_captureBuffer(TOS_Topics::TOPICS topic_t, 
              std::string item, 
              const TOSDBlock* db)
{ 
    void *fm_hndl;
    void *mem_addr;
    void *mtx_hndl;
    DWORD err;
    std::string err_msg;
    buffers_ty::key_type buf_key(topic_t, item); 

    LOCAL_BUFFERS_LOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    buffers_ty::iterator b_iter = buffers.find(buf_key);
    if( b_iter != buffers.end() ){  
        std::get<2>(b_iter->second).insert(db);     
    }else{ 
        std::string buf_name = CreateBufferName(TOS_Topics::map[topic_t], item);
        std::string mtx_name = std::string(buf_name).append("_mtx");

        fm_hndl = OpenFileMapping(FILE_MAP_READ, 0, buf_name.c_str());
        if( !fm_hndl ){
            err = GetLastError();
            err_msg = "failed to open file mapping: " + buf_name;
            TOSDB_LogEx("DATA BUFFER", err_msg.c_str(), err);
            throw TOSDB_BufferError(err_msg);            
        }

        mem_addr = MapViewOfFile(fm_hndl,FILE_MAP_READ,0,0,0);
        if( !mem_addr ){   
            err = GetLastError();            
            err_msg = "failed to map shared memory: " + buf_name; 
            TOSDB_LogEx("DATA BUFFER", err_msg.c_str(), err);
            CloseHandle(fm_hndl);
            throw TOSDB_BufferError(err_msg);
        }

        CloseHandle(fm_hndl);        

        mtx_hndl = OpenMutex(SYNCHRONIZE,FALSE,mtx_name.c_str());
        if( !mtx_hndl ){ 
            err = GetLastError();            
            err_msg = "failure to open MUTEX handle: " + mtx_name; 
            TOSDB_LogEx("DATA BUFFER", err_msg.c_str(), err);
            UnmapViewOfFile(mem_addr);         
            throw TOSDB_BufferError(err_msg);
        }

        std::set<const TOSDBlock*> db_set;
        db_set.insert(db);  

        auto binfo = std::make_tuple(0,0,std::move(db_set),mem_addr,mtx_hndl);
        buffers.insert( buffers_ty::value_type(std::move(buf_key),std::move(binfo)) );        
    }     
    /* --- CRITICAL SECTION --- */
}  


void 
_releaseBuffer(TOS_Topics::TOPICS topic_t, 
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
_castToVal(char* val) 
{ 
    return *(T*)val; 
}
  
template<> 
inline std::string 
_castToVal<std::string>(char* val)
{ 
    return std::string(val); 
}
  

template<typename T> 
void 
_extractFromBuffer(TOS_Topics::TOPICS topic, 
                   std::string item, 
                   buffer_info_ty& buf_info)
{  /* some unecessary comp in here; cache some of the buffer params */ 
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
                        _castToVal<T>(spot), 
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
_cleanupBlock(LPVOID lParam)
{
    TOSDB_RawDataBlock* cln = (TOSDB_RawDataBlock*)lParam;
    if(cln)
        delete cln;

    return 0;
}


DWORD WINAPI 
_threadedExtractLoop(LPVOID lParam)
{       
    using namespace std::chrono;

    steady_clock_type::time_point tbeg;
    steady_clock_type::time_point tend;
    long tdiff;  
    long probe_waiting;    
 
    if( master.connected(TOSDB_DEF_TIMEOUT) )
        aware_of_connection.store(true);

    while( _connected() ){
     /* [jan 2017] we should be more careful about how often we call _connected 
                   (i.e every 30 msec is no good) */
        probe_waiting = 0;
        /* after waiting for (atleast) TOSDB_PROBE_WAIT msec break to check for connection */
        while(probe_waiting < TOSDB_PROBE_WAIT && aware_of_connection.load()){                     
            /* the concurrent read loop errs on the side of greedyness */
            tbeg = steady_clock.now(); /* include time waiting for lock */   
            {       
                LOCAL_BUFFERS_LOCK_GUARD;  
                /* --- CRITICAL SECTION --- */             
                for(buffers_ty::value_type & buf : buffers)
                {
                    switch(TOS_Topics::TypeBits(buf.first.first)){
                    case TOSDB_STRING_BIT :                  
                        _extractFromBuffer<std::string>(buf.first.first, buf.first.second, buf.second); 
                        break;
                    case TOSDB_INTGR_BIT :                  
                        _extractFromBuffer<long>(buf.first.first, buf.first.second, buf.second); 
                        break;                      
                    case TOSDB_QUAD_BIT :                   
                        _extractFromBuffer<double>(buf.first.first, buf.first.second, buf.second); 
                        break;            
                    case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :                  
                        _extractFromBuffer<long long>(buf.first.first, buf.first.second, buf.second); 
                        break;              
                    default : 
                        _extractFromBuffer<float>(buf.first.first, buf.first.second, buf.second);                         
                    };        
                }
                /* --- CRITICAL SECTION --- */
            } /* make sure we give up this lock each time through the buffers */
            tend = steady_clock.now();
            tdiff = duration_cast<duration<long, std::milli>>(tend - tbeg).count();  
            /* 0 <= (buffer_latency - tdiff) <= Glacial */
            Sleep(std::min<long>(std::max<long>((buffer_latency - tdiff),0),Glacial));
            probe_waiting += buffer_latency;
        }
    }
    aware_of_connection.store(false);   
    buffer_thread = NULL;
    buffer_thread_id = 0;
    return 0;
}


int
_createBlock(LPCSTR id,
             size_type sz,
             BOOL is_datetime,
             size_type timeout)  
{
    TOSDBlock* db;

    if( !IsValidBlockSize(sz) )
        return TOSDB_ERROR_BLOCK_SIZE;  

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    if( GetBlockPtr(id,false) ){
        TOSDB_LogH("BLOCK", ("block (" + std::string(id) + ") already exists").c_str());
        return TOSDB_ERROR_BLOCK_ALREADY_EXISTS; 
    }         

    db = new TOSDBlock;   
    db->timeout = std::max<unsigned long>(timeout,TOSDB_MIN_TIMEOUT);
    db->block = nullptr;

    try{
        db->block = TOSDB_RawDataBlock::CreateBlock(sz, is_datetime); 
    }catch(const TOSDB_DataBlockLimitError){
        TOSDB_LogH("BLOCK", "attempt to exceed block limit");
    }catch(const std::exception& e){
        TOSDB_LogH("BLOCK", e.what());
    }
   
    if(!db->block){
        TOSDB_LogH("BLOCK", "failed to create block");
        delete db;
        return TOSDB_ERROR_BLOCK_CREATION;
    }   
 
    dde_blocks.insert( std::pair<std::string, TOSDBlock*>(id, db) );  
    return 0;
    /* --- CRITICAL SECTION --- */
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
            if( master.connected(TOSDB_DEF_TIMEOUT) ){                        
                for(const auto & buffer : buffers)
                {/* signal the service and close the handles */        
                    _requestStreamOP(buffer.first.first, buffer.first.second, 
                                    TOSDB_DEF_TIMEOUT, TOSDB_SIG_REMOVE);
                    UnmapViewOfFile(std::get<3>(buffer.second));
                    CloseHandle(std::get<4>(buffer.second));
                }                                  
            }
            /* needs to come after close ops or _requestStreamOP will fail on _connected() */
            aware_of_connection.store(false);
            StopLogging();
        } 
        break;    
    default: 
        break;
    }

    return TRUE;
}


int 
TOSDB_Connect() 
{      
    /* We should be able to block in here as long as this is not called from DllMain  */
    if(_connected()) 
        return 0;
 
    if(buffer_thread)
        return 0;

    char mod_name[MAX_PATH+1];
    memset(mod_name, 0, MAX_PATH+1);
    GetModuleFileName(NULL, mod_name, MAX_PATH+1);
    std::string mod_name_str(mod_name);

    buffer_thread = CreateThread(0, 0, _threadedExtractLoop, 0, 0, &buffer_thread_id);
    if(!buffer_thread){
        TOSDB_LogH("THREAD", "error initializing _threadedExtractLoop"); 
        TOSDB_Log("IPC", ("NOT connected to engine, client: " + mod_name_str).c_str());
        return TOSDB_ERROR_CONCURRENCY;  
    }
        
    for(int msec = TOSDB_DEF_PAUSE; msec <= TOSDB_DEF_TIMEOUT; msec += TOSDB_DEF_PAUSE){
        /* we need a timed wait on aware_of_connection to avoid situations 
           where a lib call is made before _threadedExtractLoop sets it to true   */ 
        if(aware_of_connection.load()){
            TOSDB_Log("IPC", ("connected to engine, client: " + mod_name_str).c_str());
            return 0;
        }
        Sleep(TOSDB_DEF_PAUSE);
    }
    
    TOSDB_LogH("IPC", "timed out waiting for aware_of_connection");    
    TOSDB_Log("IPC", ("NOT connected to engine, client: " + mod_name_str).c_str());
    return TOSDB_ERROR_TIMEOUT;
}


int 
TOSDB_Disconnect()
{  
    aware_of_connection.store(false);
    return 0;
}


/* DEPRECATED - Jan 10 2017 */
unsigned int 
TOSDB_IsConnected()
{
    return TOSDB_IsConnectedToEngine();
}


/* ... in favor of... */
unsigned int 
TOSDB_IsConnectedToEngine()
{    
    return (unsigned int)_connected();
}


/* ... and... */
unsigned int 
TOSDB_IsConnectedToEngineAndTOS() 
{
    if(!_connected())
        return 0;

    return _requestStreamOP(TEST_TOPIC, TEST_ITEM, TOSDB_MIN_TIMEOUT, TOSDB_SIG_TEST) ? 0 : 1;    
}


/* ... and... */
unsigned int   
TOSDB_ConnectionState() 
{   
    if(!_connected())
        return TOSDB_CONN_NONE;

    if( _requestStreamOP(TEST_TOPIC, TEST_ITEM, TOSDB_MIN_TIMEOUT, TOSDB_SIG_TEST) )
        return TOSDB_CONN_ENGINE;

    /* TODO: develop a heuristic for inicating:
               a) we're connected to engine AND TOS
               b) we're not getting good/normal data */
    
    return TOSDB_CONN_ENGINE_TOS;

}


/* note: if we want to do anything else with the Reserved Block(s) we'll 
         need to introduce private versions of the API that don't call 
         IsValidBlockID (i.e TOSDB_CreateBlock / _createBlock) */
int 
TOSDB_CreateBlock(LPCSTR id,
                  size_type sz,
                  BOOL is_datetime,
                  size_type timeout)
{   
    if( !_connected(true) )
        return TOSDB_ERROR_NOT_CONNECTED;

    if( !IsValidBlockID(id) )    
        return TOSDB_ERROR_BAD_INPUT;

    return _createBlock(id,sz,is_datetime,timeout);
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
    int err = TOSDB_ERROR_DECREMENT_BASE;

     if( !_connected(true) )
        return TOSDB_ERROR_NOT_CONNECTED;

    /* log and remove NULL topics */
    old_topics = topics_t;
    topics_t.clear();
    for( auto& t : old_topics ){
        if( t == TOS_Topics::TOPICS::NULL_TOPIC )
            TOSDB_LogH("INPUT", "NULL topic removed from TOSDB_Add");
        else
            topics_t.insert(t);
    }        
    old_topics.clear();

    /* log and remove INVALID items */
    old_items = items;
    items.clear();
    for( auto& s : old_items ){
        if( !IsValidItemString(s.c_str()) )
            TOSDB_LogH("INPUT", ("Invalid item string removed from TOSDB_Add: " + s).c_str() );
        else
            items.insert(s);
    }
    old_items.clear();

    /* fail if both are empty - note: this could result from NULL topics and/or invalid items*/
    if( items.empty() && topics_t.empty() )
        return TOSDB_ERROR_BAD_INPUT; 

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", ("block (" + id + ") doesn't exist").c_str());
        return TOSDB_ERROR_BLOCK_DOESNT_EXIST;
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
                if(_requestStreamOP(topic, item, db->timeout, TOSDB_SIG_ADD) == 0){
                    db->block->add_topic(topic);
                    db->block->add_item(item);
                    db->item_precache.clear();
                    db->topic_precache.clear();
                    _captureBuffer(topic, item, db);  
                }else
                    --err;                        
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
            if(_requestStreamOP(topic, item, db->timeout, TOSDB_SIG_ADD) == 0){
                db->block->add_item(item);          
                _captureBuffer(topic, item, db);   
            }else
                --err;             
        }
    }

    /* if we didn't decr err return success */
    return (err == TOSDB_ERROR_DECREMENT_BASE) ? 0 : err;
    /* --- CRITICAL SECTION --- */
}


int 
TOSDB_AddTopic(std::string id, TOS_Topics::TOPICS topic_t)
{
    if( !IsValidBlockID(id.c_str()) )
        return TOSDB_ERROR_BAD_INPUT;

    if(topic_t == TOS_Topics::TOPICS::NULL_TOPIC){
        TOSDB_LogH("INPUT", "NULL topic passed to TOSDB_AddTopic");
        return TOSDB_ERROR_BAD_TOPIC;  
    }

    return TOSDB_Add(id, str_set_type(), topic_t);
}


int   
TOSDB_AddItem(std::string id, std::string item)
{
    if( !IsValidBlockID(id.c_str()) || !IsValidItemString(item.c_str()) )
        return TOSDB_ERROR_BAD_INPUT;

    return TOSDB_Add(id, item, topic_set_type());
}


int 
TOSDB_AddTopic(LPCSTR id, LPCSTR topic_str)
{
    if( !IsValidBlockID(id) || !CheckStringLength(topic_str) )  
        return TOSDB_ERROR_BAD_INPUT;    

    TOS_Topics::TOPICS t = GetTopicEnum(topic_str);
    if(t == TOS_Topics::TOPICS::NULL_TOPIC)
        return TOSDB_ERROR_BAD_TOPIC;    

    return TOSDB_AddTopic(id, t);
}


int 
TOSDB_AddTopics(std::string id, topic_set_type topics_t)
{  
    if( !IsValidBlockID(id.c_str()) )
        return TOSDB_ERROR_BAD_INPUT;

    return TOSDB_Add(id, str_set_type(), topics_t);
}


int 
TOSDB_AddTopics(LPCSTR id, LPCSTR* topics_str, size_type topics_len)
{  
    if( !IsValidBlockID(id) || !CheckStringLengths(topics_str, topics_len) )    
        return TOSDB_ERROR_BAD_INPUT;      

    auto f =  [=](LPCSTR str){ return GetTopicEnum(str); };
    topic_set_type tset(topics_str, topics_len, f);

    return TOSDB_Add(id, str_set_type(), std::move(tset));
}


int 
TOSDB_AddItem(LPCSTR id, LPCSTR item)
{  
    if( !IsValidBlockID(id) || !IsValidItemString(item) ) 
        return TOSDB_ERROR_BAD_INPUT;    

    return TOSDB_Add(id, std::string(item), topic_set_type());
}


int 
TOSDB_AddItems(std::string id, str_set_type items)
{
    if( !IsValidBlockID(id.c_str()) )
        return TOSDB_ERROR_BAD_INPUT;

    return TOSDB_Add(id, items, topic_set_type());
}


int 
TOSDB_AddItems(LPCSTR id, LPCSTR* items, size_type items_len)
{  
    if( !IsValidBlockID(id) || !CheckStringLengths(items, items_len) )    
        return TOSDB_ERROR_BAD_INPUT;     

    return TOSDB_Add(id, str_set_type(items,items_len), topic_set_type());  
}


int 
TOSDB_Add(LPCSTR id, 
          LPCSTR* items, 
          size_type items_len, 
          LPCSTR* topics_str, 
          size_type topics_len)
{ 
    if( !IsValidBlockID(id) 
          || !CheckStringLengths(items, items_len) 
          || !CheckStringLengths(topics_str, topics_len) )
    {  
        return TOSDB_ERROR_BAD_INPUT;
    }

    auto f = [=](LPCSTR str){ return GetTopicEnum(str); };
    topic_set_type tset(topics_str, topics_len, f);
  
    return TOSDB_Add(id, str_set_type(items,items_len), std::move(tset)); 
}


int 
TOSDB_RemoveTopic(LPCSTR id, LPCSTR topic_str)
{
    if( !IsValidBlockID(id) || !CheckStringLength(topic_str) )
        return TOSDB_ERROR_BAD_INPUT;
    
    TOS_Topics::TOPICS t = GetTopicEnum(topic_str);
    if(t == TOS_Topics::TOPICS::NULL_TOPIC){
        return TOSDB_ERROR_BAD_TOPIC; 
    }

    return TOSDB_RemoveTopic(id, t);
}


int 
TOSDB_RemoveTopic(std::string id, TOS_Topics::TOPICS topic_t)
{  
    TOSDBlock* db;
    int err = TOSDB_ERROR_DECREMENT_BASE;

    if(topic_t == TOS_Topics::TOPICS::NULL_TOPIC){
        TOSDB_Log("INPUT", "NULL topic passed to TOSDB_RemoveTopic");
        return TOSDB_ERROR_BAD_TOPIC;
    }   

    if( !_connected(true) )
        return TOSDB_ERROR_NOT_CONNECTED;
  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", ("block (" + id + ") doesn't exist").c_str());
        return TOSDB_ERROR_BLOCK_DOESNT_EXIST;
    }   
        
    if( db->block->has_topic(topic_t) ){
        for(auto & item : db->block->items())
        {
            _releaseBuffer(topic_t, item, db); 
            if(_requestStreamOP(topic_t, item, db->timeout, TOSDB_SIG_REMOVE) != 0){
                --err;
                TOSDB_LogH("IPC","_requestStreamOP(REMOVE) failed, stream leaked");
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
        return TOSDB_ERROR_BAD_TOPIC;  
    }

    db->topic_precache.erase(topic_t);
    /* if we didn't decr err return success */
    return (err == TOSDB_ERROR_DECREMENT_BASE) ? 0 : err;
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
    int err = TOSDB_ERROR_DECREMENT_BASE;

    if( !CheckStringLength(item) || !IsValidBlockID(id) )
        return TOSDB_ERROR_BAD_INPUT;
    
    if( !_connected(true) )
        return TOSDB_ERROR_NOT_CONNECTED;  
  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */ 

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", ("block (" + std::string(id) + ") doesn't exist").c_str());
        return TOSDB_ERROR_BLOCK_DOESNT_EXIST;
    }  
        
    if( db->block->has_item(item) ){
        for(auto topic : db->block->topics())
        {
            _releaseBuffer(topic, item, db); 
            if(_requestStreamOP(topic, item, db->timeout, TOSDB_SIG_REMOVE) != 0){
                --err;
                TOSDB_LogH("IPC","_requestStreamOP(REMOVE) failed, stream leaked");
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
        return TOSDB_ERROR_BAD_ITEM;
    }

    db->item_precache.erase(item);
    /* if we didn't decr err return success */
    return (err == TOSDB_ERROR_DECREMENT_BASE) ? 0 : err;    
  /* --- CRITICAL SECTION --- */
}


int 
TOSDB_CloseBlock(LPCSTR id)
{
    TOSDBlock* db;
    HANDLE del_thrd_hndl;
    DWORD del_thrd_id;

    int err = TOSDB_ERROR_DECREMENT_BASE;  

    if( !IsValidBlockID(id) )        
        return TOSDB_ERROR_BAD_INPUT;   

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */  

    db = _getBlockPtr(id);
    if(!db){
        TOSDB_LogH("BLOCK", ("block (" + std::string(id) + ") doesn't exist").c_str());
        return TOSDB_ERROR_BLOCK_DOESNT_EXIST;
    }  

    for(auto & item : db->block->items()){
        for(auto topic : db->block->topics())
        {
            _releaseBuffer(topic, item, db);
            if(_requestStreamOP(topic, item, db->timeout, TOSDB_SIG_REMOVE) != 0){
                --err;
                TOSDB_LogH("IPC", "_requestStreamOP(REMOVE) failed, stream leaked");
            }
        }
    }

    dde_blocks.erase(id);       

    /* spin-off block destruction to its own thread so we don't block main */
    del_thrd_hndl = CreateThread(NULL, 0, _cleanupBlock, (LPVOID)db->block, 0, &(del_thrd_id));
    if(!del_thrd_hndl){       
        TOSDB_LogH("THREAD", "using main thread to clean-up(THIS MAY BLOCK)"); 
        _cleanupBlock((LPVOID)db->block);
    }

    delete db;

    /* if we didn't decr err return success */
    return (err == TOSDB_ERROR_DECREMENT_BASE) ? 0 : err;   
    /* --- CRITICAL SECTION --- */
}

int 
TOSDB_CloseBlocks()
{
    std::map<std::string, TOSDBlock*> bcopy;  
    int err = TOSDB_ERROR_DECREMENT_BASE;
    try{ 
        GLOBAL_RLOCK_GUARD;  
        /* --- CRITICAL SECTION --- */

        /* need a copy, _CloseBlock removes from original */    
        std::insert_iterator<std::map<std::string,TOSDBlock*>> i(bcopy,bcopy.begin());
        std::copy(dde_blocks.begin(), dde_blocks.end(), i); 
        for(auto & b: bcopy){    
            if( TOSDB_CloseBlock(b.first.c_str()) )
                --err;
        }
        /* --- CRITICAL SECTION --- */
    }catch(...){ 
        return TOSDB_ERROR_UNKNOWN; // really ? 
    }

    /* if we didn't decr err return success */
    return (err == TOSDB_ERROR_DECREMENT_BASE) ? 0 : err;   
}


int
TOSDB_DumpSharedBufferStatus()
{
    if( !_connected(true) )
        return TOSDB_ERROR_NOT_CONNECTED;

    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */

    std::string msg = std::to_string(TOSDB_SIG_DUMP);

    if( !master.call(&msg, TOSDB_DEF_TIMEOUT) ){
        TOSDB_LogH("IPC",("master.call failled, msg:" + msg).c_str());
        return TOSDB_ERROR_IPC;
    }

    try{        
        return (std::stol(msg) == TOSDB_SIG_GOOD) ? 0 : TOSDB_ERROR_IPC;
    }catch(...){
        TOSDB_LogH("IPC", "failed to convert return message to long");
        return TOSDB_ERROR_IPC;
    }      
    /* --- CRITICAL SECTION --- */
}


/* WARNING - removes individual stream from the engine. It should only 
   be used when you are certain a client lib has failed to close a stream 
   during destruction of the containing block (e.g the program crashes 
   before DLL_PROCESS_DETACH in DllMain is hit.)

   YOU NEED TO BE SURE THIS IS THE CASE OR YOU CAN CORRUPT THE UNDERLYING 
   BUFFERS FOR ANY OR ALL CLIENT INSTANCE(S)! */
int           
TOSDB_RemoveOrphanedStream(LPCSTR item, LPCSTR topic_str)
{
    if( !CheckStringLength(item) || !CheckStringLength(topic_str) )
        return TOSDB_ERROR_BAD_INPUT;           

    TOS_Topics::TOPICS t = GetTopicEnum(topic_str);
    if(t == TOS_Topics::TOPICS::NULL_TOPIC){
        return TOSDB_ERROR_BAD_TOPIC; 
    }

    if( !_connected(true) )
        return TOSDB_ERROR_NOT_CONNECTED;    
  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */
    return _requestStreamOP(t, item, TOSDB_DEF_TIMEOUT, TOSDB_SIG_REMOVE);    
    /* --- CRITICAL SECTION --- */
}


int 
TOSDB_GetBlockIDs(LPSTR* dest, size_type array_len, size_type str_len)
{  
    GLOBAL_RLOCK_GUARD;
    /* --- CRITICAL SECTION --- */
    if(array_len < dde_blocks.size()) 
        return TOSDB_ERROR_BAD_INPUT;

    int i = 0;

    for(auto & name : dde_blocks){
        if( strcpy_s(dest[i++], str_len, name.first.c_str()) )
            return TOSDB_ERROR_BAD_INPUT;    
    }
     
    return 0; 
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


TOS_Topics::TOPICS 
GetTopicEnum(std::string topic_str, bool log_if_null)
{
    /* operator[] uses .at(), catches out_of_range, returns NULL_TOPIC */
    TOS_Topics::TOPICS t = TOS_Topics::map[topic_str];
    if(log_if_null && t == TOS_Topics::TOPICS::NULL_TOPIC)
        TOSDB_LogH("INPUT", ("bad topic string: " + std::string(topic_str)).c_str()); 

    return t;
}


const TOSDBlock* 
GetBlockPtr(const std::string id, bool log)
{
    try{      
        return dde_blocks.at(id);
    }catch(...){ 
        if(log){
            std::string msg = "block (" + id + ") doesn't exist";
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
        throw TOSDB_DataBlockDoesntExist(id.c_str());
 
    return db;
}


