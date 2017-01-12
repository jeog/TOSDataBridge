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

#include "shell.hpp"
  
namespace{

void Connect(CommandCtx *ctx); 
void Disconnect(CommandCtx *ctx); 
void IsConnected(CommandCtx *ctx);
void IsConnectedToEngine(CommandCtx *ctx);
void IsConnectedToEngineAndTOS(CommandCtx *ctx);
void ConnectionState(CommandCtx *ctx);
void CreateBlock(CommandCtx *ctx); 
void CloseBlock(CommandCtx *ctx); 
void CloseBlocks(CommandCtx *ctx);
void GetBlockLimit(CommandCtx *ctx);
void SetBlockLimit(CommandCtx *ctx);   
void GetBlockCount(CommandCtx *ctx);
void GetBlockIDs(CommandCtx *ctx);
void GetBlockSize(CommandCtx *ctx); 
void SetBlockSize(CommandCtx *ctx);
void GetLatency(CommandCtx *ctx); 
void SetLatency(CommandCtx *ctx);
void Add(CommandCtx *ctx);
void AddTopic(CommandCtx *ctx);
void AddItem(CommandCtx *ctx);
void AddTopics(CommandCtx *ctx);
void AddItems(CommandCtx *ctx); 
void RemoveTopic(CommandCtx *ctx);
void RemoveItem(CommandCtx *ctx);
void GetItemCount(CommandCtx *ctx);
void GetTopicCount(CommandCtx *ctx);
void GetTopicNames(CommandCtx *ctx);
void GetItemNames(CommandCtx *ctx);
void GetTopicEnums(CommandCtx *ctx);
void GetPreCachedTopicEnums(CommandCtx *ctx);    
void GetPreCachedItemCount(CommandCtx *ctx); 
void GetPreCachedTopicCount(CommandCtx *ctx);
void GetPreCachedItemNames(CommandCtx *ctx); 
void GetPreCachedTopicNames(CommandCtx *ctx);
void GetTypeBits(CommandCtx *ctx);
void GetTypeString(CommandCtx *ctx);
void IsUsingDateTime(CommandCtx *ctx);
void GetStreamOccupancy(CommandCtx *ctx);
void GetMarkerPosition(CommandCtx *ctx);
void IsMarkerDirty(CommandCtx *ctx);
void DumpBufferStatus(CommandCtx *ctx);

}; /* namespace */

CommandsMap commands_admin(
    CommandsMap::InitChain("Connect",Connect,"connect to the library")
                          ("Disconnect",Disconnect)                              
                          ("IsConnected",IsConnected)       
                          ("IsConnectedToEngine",IsConnectedToEngine)
                          ("IsConnectedToEngineAndTOS",IsConnectedToEngineAndTOS)
                          ("ConnectionState",ConnectionState)  
                          ("CreateBlock",CreateBlock)                              
                          ("CloseBlock",CloseBlock)                              
                          ("CloseBlocks",CloseBlocks)                              
                          ("GetBlockLimit",GetBlockLimit)                              
                          ("SetBlockLimit",SetBlockLimit)                              
                          ("GetBlockCount",GetBlockCount)                              
                          ("GetBlockIDs",GetBlockIDs)                              
                          ("GetBlockSize",GetBlockSize)                              
                          ("SetBlockSize",SetBlockSize)                              
                          ("GetLatency",GetLatency)                              
                          ("SetLatency",SetLatency)                              
                          ("Add",Add)                              
                          ("AddTopic",AddTopic)                              
                          ("AddItem",AddItem)                              
                          ("AddTopics",AddTopics)                              
                          ("AddItems",AddItems)                              
                          ("RemoveTopic",RemoveTopic)                              
                          ("RemoveItem",RemoveItem)                              
                          ("GetItemCount",GetItemCount)                              
                          ("GetTopicCount",GetTopicCount)                              
                          ("GetTopicNames",GetTopicNames)                                  
                          ("GetItemNames",GetItemNames)                              
                          ("GetTopicEnums",GetTopicEnums)                              
                          ("GetPreCachedTopicEnums",GetPreCachedTopicEnums)                              
                          ("GetPreCachedItemCount",GetPreCachedItemCount)                              
                          ("GetPreCachedTopicCount",GetPreCachedTopicCount)                              
                          ("GetPreCachedItemNames",GetPreCachedItemNames)                              
                          ("GetPreCachedTopicNames",GetPreCachedTopicNames)                              
                          ("GetTypeBits",GetTypeBits)                              
                          ("GetTypeString",GetTypeString)                              
                          ("IsUsingDateTime",IsUsingDateTime)                              
                          ("GetStreamOccupancy",GetStreamOccupancy)                              
                          ("GetMarkerPosition",GetMarkerPosition)                              
                          ("IsMarkerDirty",IsMarkerDirty)                              
                          ("DumpBufferStatus",DumpBufferStatus) 
);


namespace {

void 
_check_display_ret(int r);

template<typename T>
void 
_check_display_ret(int r, T v);

template<>
void 
_check_display_ret(int r, bool v);

template<typename T>
void 
_check_display_ret(int r, T *v, size_type n);


inline size_type 
_get_cstr_items(char ***p, CommandCtx *ctx)
{  
    return get_cstr_entries("item", p, ctx); 
}


inline size_type
_get_cstr_topics(char ***p, CommandCtx *ctx)
{  
    return get_cstr_entries("topic", p, ctx);
}


inline void 
_del_cstr_items(char **items, size_type nitems)
{
    DeleteStrings(items, nitems);
}


inline void 
_del_cstr_topics(char **topics, size_type ntopics)
{
    DeleteStrings(topics, ntopics);
}


void 
Connect(CommandCtx *ctx)
{
    int ret = TOSDB_Connect();
    _check_display_ret(ret);
}

  
void
Disconnect(CommandCtx *ctx)
{
    int ret = TOSDB_Disconnect();
    _check_display_ret(ret);
}
   

void
IsConnected(CommandCtx *ctx)
{
    unsigned int ret = TOSDB_IsConnected();
    std::cout<< std::endl << "*** DEPRECATED (jan 2017) ***" << std::endl
             << std::endl << std::boolalpha << (ret == 1) << std::endl << std::endl;
}


void
IsConnectedToEngine(CommandCtx *ctx)
{
    unsigned int ret = TOSDB_IsConnectedToEngine();
    std::cout<< std::endl << std::boolalpha << (ret == 1) << std::endl << std::endl;
}


void
IsConnectedToEngineAndTOS(CommandCtx *ctx)
{
    unsigned int ret = TOSDB_IsConnectedToEngineAndTOS();
    std::cout<< std::endl << std::boolalpha << (ret == 1) << std::endl << std::endl;
}


void
ConnectionState(CommandCtx *ctx)
{
    unsigned int ret = TOSDB_ConnectionState();

    std::cout<< std::endl;

    switch(ret){
    case TOSDB_CONN_NONE:
        std::cout<< "TOSDB_CONN_NONE";
        break;
    case TOSDB_CONN_ENGINE:
        std::cout<< "TOSDB_CONN_ENGINE";
        break;
    case TOSDB_CONN_ENGINE_TOS:
        std::cout<< "TOSDB_CONN_ENGINE_TOS";
        break; 
    default:
        std::cout<< "Invalid connection state returned.";
        break;
    }

    std::cout<< std::endl << std::endl;
}


void
CreateBlock(CommandCtx *ctx)
{
    std::string timeout;
    std::string block;
    std::string size;
    std::string dts_y_or_n;
    unsigned long timeout_num;
    int ret;

    prompt_for("block id", &block, ctx);
    prompt_for("block size", &size, ctx);        
    prompt_for("use datetime stamp?(y/n)", &dts_y_or_n, ctx);

    if(dts_y_or_n != "y" && dts_y_or_n != "n")
        std::cerr<< std::endl << "INVALID - default to 'n'" << std::endl << std::endl;

    prompt_for("timeout(milliseconds) [positive number or defaults]", &timeout, ctx);
                       
    try{
        timeout_num = std::stoul(timeout);
        if(timeout_num <= 0)
            throw std::exception("dummy");

        if(timeout_num < TOSDB_MIN_TIMEOUT){
            std::cout<< std::endl << "WARNING - using a timeout < " << TOSDB_MIN_TIMEOUT
                      << " milliseconds is NOT recommended" << std::endl;
        }
    }catch(...){
        timeout_num = TOSDB_DEF_TIMEOUT;
        std::cout<< std::endl << "Using default timeout of " << timeout_num
                  << " milliseconds" << std::endl;           
    }

    ret =  TOSDB_CreateBlock(block.c_str(), std::stoul(size) , (dts_y_or_n == "y"), timeout_num);      
    _check_display_ret(ret);            
}


void
CloseBlock(CommandCtx *ctx)
{
    int ret;
    std::string block;

    prompt_for("block id", &block, ctx);    

    ret = TOSDB_CloseBlock(block.c_str());
    _check_display_ret(ret);
}


void
CloseBlocks(CommandCtx *ctx)
{
    int ret =  TOSDB_CloseBlocks();
    _check_display_ret(ret);
}


void
GetBlockLimit(CommandCtx *ctx)
{
    std::cout<< std::endl << TOSDB_GetBlockLimit() << std::endl << std::endl;
}


void
SetBlockLimit(CommandCtx *ctx)
{
    std::string size;

    prompt_for("block limit", &size, ctx);
    
    std::cout<< std::endl << TOSDB_SetBlockLimit(std::stoul(size)) << std::endl << std::endl;
}

void
GetBlockCount(CommandCtx *ctx)
{
    std::cout<< std::endl << TOSDB_GetBlockCount() << std::endl << std::endl;
}


void
GetBlockIDs(CommandCtx *ctx)
{
    if(prompt_for_cpp(ctx)){
        std::cout<< std::endl;
        for(auto & str : TOSDB_GetBlockIDs())
            std::cout<< str << std::endl;
        std::cout<< std::endl;
    }else{  
        char** strs;
        size_type nblocks = TOSDB_GetBlockCount();            
        try{
            int ret;

            strs = NewStrings(nblocks, TOSDB_BLOCK_ID_SZ);

            ret = TOSDB_GetBlockIDs(strs, nblocks, TOSDB_BLOCK_ID_SZ);
            _check_display_ret(ret,strs,nblocks);

            DeleteStrings(strs, nblocks);
        }catch(...){    
            DeleteStrings(strs, nblocks);
            throw;
        }            
    }
}


void
GetBlockSize(CommandCtx *ctx)
{
    int ret;
    std::string block;

    size_type sz = 0;

     prompt_for("block id", &block, ctx);

    if(prompt_for_cpp(ctx)){            
        std::cout<< std::endl << TOSDB_GetBlockSize(block) << std::endl << std::endl;
    }else{          
        ret = TOSDB_GetBlockSize(block.c_str(), &sz);
        _check_display_ret(ret, sz);      
    }
}


void
SetBlockSize(CommandCtx *ctx)
{
    int ret;
    std::string block;
    std::string size;      

    prompt_for("block id", &block, ctx);
    prompt_for("block size", &size, ctx);    

    ret = TOSDB_SetBlockSize(block.c_str(), std::stoul(size));
    _check_display_ret(ret);
}


void
GetLatency(CommandCtx *ctx)
{
     std::cout<< std::endl << TOSDB_GetLatency() << std::endl << std::endl;
}


void
SetLatency(CommandCtx *ctx)
{
    std::string lat;

    prompt_for("latency value(milleseconds)", &lat, ctx);    

    TOSDB_SetLatency((UpdateLatency)std::stoul(lat));
}


void
Add(CommandCtx *ctx)
{
    std::string block;
    size_type nitems;
    size_type ntopics;
    int ret;

    char **items_raw = nullptr;
    char **topics_raw = nullptr;

    prompt_for("block id", &block, ctx);    

    try{           
        nitems = _get_cstr_items(&items_raw, ctx);
        ntopics = _get_cstr_topics(&topics_raw, ctx);

        if(prompt_for_cpp(ctx)){    
            auto topics = topic_set_type( 
                              topics_raw, 
                              ntopics,
                              [=](LPCSTR str){ return TOS_Topics::MAP()[str]; }
                          );               
            ret =  TOSDB_Add(block, str_set_type(items_raw, nitems), topics);                  
        }else{             
            ret = TOSDB_Add(block.c_str(), (const char**)items_raw, nitems, 
                            (const char**)topics_raw, ntopics);          
        }   

        _check_display_ret(ret);

        _del_cstr_items(items_raw, nitems);
        _del_cstr_topics(topics_raw, ntopics); 
    }catch(...){
        _del_cstr_items(items_raw, nitems);
        _del_cstr_topics(topics_raw, ntopics); 
        throw;
    }
}


void
AddTopic(CommandCtx *ctx)
{
    std::string block;
    std::string topic;
    int ret;

    prompt_for("block id", &block, ctx); 
    prompt_for("topic", &topic, ctx);    

    ret = prompt_for_cpp(ctx)
        ? TOSDB_AddTopic(block, TOS_Topics::MAP()[topic])
        : TOSDB_AddTopic(block.c_str(), topic.c_str());            
        
    _check_display_ret(ret);
}


void
AddItem(CommandCtx *ctx)
{    
    std::string block;
    std::string item;
    int ret;

    prompt_for("block id", &block, ctx); 
    prompt_for("item", &item, ctx);
    
    ret = prompt_for_cpp(ctx)
        ? TOSDB_AddItem(block, item)
        : TOSDB_AddItem(block.c_str(), item.c_str());

    _check_display_ret(ret);
}


void
AddTopics(CommandCtx *ctx)
{
    std::string block;        
    size_type ntopics;
    int ret;

    char **topics_raw = nullptr;

    prompt_for("block id", &block, ctx); 

    try{ 
        ntopics = _get_cstr_topics(&topics_raw, ctx); 

        if(prompt_for_cpp(ctx)){
            auto topics = topic_set_type( 
                              topics_raw, 
                              ntopics, 
                              [=](LPCSTR str){ return TOS_Topics::MAP()[str]; }
                          );
            ret = TOSDB_AddTopics(block, topics);
        }else{    
            ret = TOSDB_AddTopics(block.c_str(), (const char**)topics_raw, ntopics);
        }
        _check_display_ret(ret);
     
        _del_cstr_topics(topics_raw, ntopics);
    }catch(...){
        _del_cstr_topics(topics_raw, ntopics);
        throw;
    }
}


void
AddItems(CommandCtx *ctx)
{
    std::string block;       
    size_type nitems;
    int ret;

    char **items_raw = nullptr;

    prompt_for("block id", &block, ctx); 
           
    try{            
        nitems = _get_cstr_items(&items_raw, ctx);

        ret = prompt_for_cpp(ctx)
            ? TOSDB_AddItems(block, str_set_type(items_raw, nitems))
            : TOSDB_AddItems(block.c_str(), (const char**)items_raw, nitems);
            
        _check_display_ret(ret);
        _del_cstr_items(items_raw, nitems);
    }catch(...){
        _del_cstr_items(items_raw, nitems);      
        throw;
    }         
}


void
RemoveTopic(CommandCtx *ctx)
{
    std::string block;
    std::string topic;
    int ret;

    prompt_for("block id", &block, ctx); 
    prompt_for("topic", &topic, ctx);
    
    ret = prompt_for_cpp(ctx) 
        ? TOSDB_RemoveTopic(block, TOS_Topics::MAP()[topic])
        : TOSDB_RemoveTopic(block.c_str(), topic.c_str());   

    _check_display_ret(ret);
}


void
RemoveItem(CommandCtx *ctx)
{
    std::string block;
    std::string item;
    int ret;

    prompt_for("block id", &block, ctx); 
    prompt_for("item: ", &item, ctx);
    
    ret = prompt_for_cpp(ctx) 
        ? TOSDB_RemoveItem(block, item)
        : TOSDB_RemoveItem(block.c_str(), item.c_str());

    _check_display_ret(ret);
}


void
GetItemCount(CommandCtx *ctx)
{
    std::string block;        
    int ret;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl << TOSDB_GetItemCount(block) << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{         
        ret = TOSDB_GetItemCount(block.c_str(), &count);
        _check_display_ret(ret, count);
    }
}


void
GetTopicCount(CommandCtx *ctx)
{
    std::string block;
    int ret;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl << TOSDB_GetTopicCount(block) << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{         
        ret = TOSDB_GetTopicCount(block.c_str(), &count);
        _check_display_ret(ret, count);
    }
}


void
GetTopicNames(CommandCtx *ctx)
{              
    std::string block;
    int ret;
    char **strs;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){         
        try{           
            std::cout<< std::endl;
            for(auto & t : TOSDB_GetTopicNames(block))
                std::cout<< t << std::endl;
            std::cout<< std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{          
        TOSDB_GetTopicCount(block.c_str(), &count); 
        try{
            strs = NewStrings(count, TOSDB_MAX_STR_SZ);

            ret = TOSDB_GetTopicNames(block.c_str(), strs, count, TOSDB_MAX_STR_SZ);
            _check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }            
    }
}


void
GetItemNames(CommandCtx *ctx)
{
    int ret;
    char **strs;
    std::string block;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){         
        try{     
            std::cout<< std::endl;
            for(auto & i : TOSDB_GetItemNames(block))
                std::cout<< i << std::endl;
            std::cout<< std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{     
        TOSDB_GetItemCount(block.c_str(), &count);            
        try{
            strs = NewStrings(count, TOSDB_MAX_STR_SZ);

            ret = TOSDB_GetItemNames(block.c_str(), strs, count, TOSDB_MAX_STR_SZ);
            _check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }
    }
}


void
GetTopicEnums(CommandCtx *ctx)
{
    std::string block;

    prompt_for("block id", &block, ctx);       
      
    std::cout<< std::endl;
    for(auto & t : TOSDB_GetTopicEnums(block))
        std::cout<< (TOS_Topics::enum_value_type)t <<' '
                  << TOS_Topics::MAP()[t] << std::endl;
    std::cout<< std::endl;
}

void
GetPreCachedTopicNames(CommandCtx *ctx)
{
    char **strs;
    int ret;
    std::string block;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){    
        try{         
            std::cout<< std::endl;
            for(auto & t : TOSDB_GetPreCachedTopicNames(block))
                std::cout<< t << std::endl;
            std::cout<< std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{                  
        TOSDB_GetPreCachedTopicCount(block.c_str(), &count);            
        try{
            strs = NewStrings(count, TOSDB_MAX_STR_SZ);

            ret = TOSDB_GetPreCachedTopicNames(block.c_str(), strs, count, TOSDB_MAX_STR_SZ);
            _check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }          
    }
}


void
GetPreCachedItemNames(CommandCtx *ctx)
{
    char **strs;
    int ret;
    std::string block;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){    
        try{            
            std::cout<< std::endl;
            for(auto & i : TOSDB_GetPreCachedItemNames(block))
                std::cout<< i << std::endl;
            std::cout<< std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{                
        TOSDB_GetPreCachedItemCount(block.c_str(), &count);
        try{
            strs = NewStrings(count, TOSDB_MAX_STR_SZ);

            ret = TOSDB_GetPreCachedItemNames(block.c_str(), strs, count, TOSDB_MAX_STR_SZ);
            _check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }            
    }
}

void
GetPreCachedTopicEnums(CommandCtx *ctx)
{
    std::string block;

    prompt_for("block id", &block, ctx);       
        
    std::cout<< std::endl; 
    for(auto & t : TOSDB_GetPreCachedTopicEnums(block))
        std::cout<< (TOS_Topics::enum_value_type)t << ' ' 
                  << TOS_Topics::MAP()[t] << std::endl;
    std::cout<< std::endl; 
}


void
GetPreCachedItemCount(CommandCtx *ctx)
{
    int ret;
    std::string block;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetPreCachedItemCount(block) 
                      << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetPreCachedItemCount(block.c_str(), &count);
        _check_display_ret(ret, count);
    }
}


void
GetPreCachedTopicCount(CommandCtx *ctx)
{
    int ret;
    std::string block;

    size_type count = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetPreCachedTopicCount(block) 
                      << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetPreCachedTopicCount(block.c_str(), &count);
        _check_display_ret(ret, count);
    }
}


void
GetTypeBits(CommandCtx *ctx)
{
    int ret;
    std::string topic;

    type_bits_type bits = 0;

    prompt_for("topic", &topic, ctx); 

    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl << std::hex
                      << (int)TOSDB_GetTypeBits(TOS_Topics::MAP()[topic]) 
                      << std::endl << std::endl; 
        }catch(std::exception & e){
            std::cout<< std::endl <<"error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetTypeBits(topic.c_str(), &bits);
        if(ret) 
            std::cout<< std::endl << "error: "<< ret << std::endl << std::endl; 
        else 
            std::cout<< std::endl << std::hex << (int)bits << std::endl << std::endl;         
    }
}


void
GetTypeString(CommandCtx *ctx)
{        
    int ret;
    char tstring[256];
    std::string topic;

    prompt_for("topic", &topic, ctx);
    
    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetTypeString(TOS_Topics::MAP()[topic]) 
                      << std::endl << std::endl; 
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetTypeString(topic.c_str(), tstring, 256);
        _check_display_ret(ret, tstring);
    }         
}


void
IsUsingDateTime(CommandCtx *ctx)
{
    int ret;        
    std::string block;     

    unsigned int using_dt = 0;

    prompt_for("block id", &block, ctx); 

    if(prompt_for_cpp(ctx)){
        try{
            std::cout<< std::endl << std::boolalpha 
                      << TOSDB_IsUsingDateTime(block) 
                      << std::endl << std::endl; 
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{
        ret = TOSDB_IsUsingDateTime(block.c_str(), &using_dt);
        _check_display_ret(ret, (using_dt==1));           
    }
}


void
GetStreamOccupancy(CommandCtx *ctx)
{
    int ret;
    std::string block;
    std::string item;
    std::string topic;

    size_type occ = 0;

    prompt_for_block_item_topic(&block, &item, &topic, ctx);

    if(prompt_for_cpp(ctx)){
        std::cout<< std::endl 
                  << TOSDB_GetStreamOccupancy(block, item, TOS_Topics::MAP()[topic]) 
                  << std::endl << std::endl; 
    }else{            
        ret = TOSDB_GetStreamOccupancy(block.c_str(), item.c_str(), topic.c_str(), &occ);
        _check_display_ret(ret, occ);
    }
}


void
GetMarkerPosition(CommandCtx *ctx)
{
    int ret;
    std::string block;
    std::string item;
    std::string topic;

    long long pos = 0;

    prompt_for_block_item_topic(&block, &item, &topic, ctx);  

    if(prompt_for_cpp(ctx)){
        std::cout<< std::endl 
                  << TOSDB_GetMarkerPosition(block, item, TOS_Topics::MAP()[topic]) 
                  << std::endl << std::endl; 
    }else{             
        ret = TOSDB_GetMarkerPosition(block.c_str(), item.c_str(), topic.c_str(), &pos);
        _check_display_ret(ret, pos);
    }
}


void
IsMarkerDirty(CommandCtx *ctx)
{
    int ret;
    std::string block;
    std::string item;
    std::string topic;
        
    unsigned int is_dirty = false;

    prompt_for_block_item_topic(&block, &item, &topic, ctx);

    if(prompt_for_cpp(ctx)){
        std::cout<< std::endl << std::boolalpha 
                  << TOSDB_IsMarkerDirty(block, item,TOS_Topics::MAP()[topic]) 
                  << std::endl << std::endl; 
    }else{
        ret = TOSDB_IsMarkerDirty(block.c_str(), item.c_str(),topic.c_str(), &is_dirty);
        _check_display_ret(ret, (is_dirty==1));
    }
}


void
DumpBufferStatus(CommandCtx *ctx)
{
    TOSDB_DumpSharedBufferStatus();
}


template<typename T>
void 
_check_display_ret(int r, T v)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << v << std::endl << std::endl; 
}


void 
_check_display_ret(int r)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << "SUCCESS" << std::endl << std::endl; 
}


template<>
void 
_check_display_ret(int r, bool v)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << std::boolalpha << v << std::endl << std::endl; 
}


template<typename T>
void 
_check_display_ret(int r, T *v, size_type n)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else{ 
        std::cout<< std::endl; 
        for(size_type i = 0; i < n; ++i) 
            std::cout<< v[i] << std::endl; 
        std::cout<< std::endl; 
    } 
}

}; /* namespace */