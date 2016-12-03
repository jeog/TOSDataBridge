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

void Connect(void *ctx=nullptr); 
void Disconnect(void *ctx=nullptr); 
void IsConnected(void *ctx=nullptr);
void CreateBlock(void *ctx=nullptr); 
void CloseBlock(void *ctx=nullptr); 
void CloseBlocks(void *ctx=nullptr);
void GetBlockLimit(void *ctx=nullptr);
void SetBlockLimit(void *ctx=nullptr);   
void GetBlockCount(void *ctx=nullptr);
void GetBlockIDs(void *ctx=nullptr);
void GetBlockSize(void *ctx=nullptr); 
void SetBlockSize(void *ctx=nullptr);
void GetLatency(void *ctx=nullptr); 
void SetLatency(void *ctx=nullptr);
void Add(void *ctx=nullptr);
void AddTopic(void *ctx=nullptr);
void AddItem(void *ctx=nullptr);
void AddTopics(void *ctx=nullptr);
void AddItems(void *ctx=nullptr); 
void RemoveTopic(void *ctx=nullptr);
void RemoveItem(void *ctx=nullptr);
void GetItemCount(void *ctx=nullptr);
void GetTopicCount(void *ctx=nullptr);
void GetTopicNames(void *ctx=nullptr);
void GetItemNames(void *ctx=nullptr);
void GetPreCachedTopicEnums(void *ctx=nullptr);    
void GetPreCachedItemCount(void *ctx=nullptr); 
void GetPreCachedTopicCount(void *ctx=nullptr);
void GetPreCachedItemNames(void *ctx=nullptr); 
void GetPreCachedTopicNames(void *ctx=nullptr);
void GetTypeBits(void *ctx=nullptr);
void GetTypeString(void *ctx=nullptr);
void IsUsingDateTime(void *ctx=nullptr);
void GetStreamOccupancy(void *ctx=nullptr);
void GetMarkerPosition(void *ctx=nullptr);
void IsMarkerDirty(void *ctx=nullptr);
void DumpBufferStatus(void *ctx=nullptr);

commands_map_ty
build_commands_map_admin()
{
    commands_map_ty m;

    m.insert( commands_map_elem_ty("Connect",Connect) );
    m.insert( commands_map_elem_ty("Disconnect",Disconnect) );
    m.insert( commands_map_elem_ty("IsConnected",IsConnected) );
    m.insert( commands_map_elem_ty("CreateBlock",CreateBlock) );
    m.insert( commands_map_elem_ty("CloseBlock",CloseBlock) );
    m.insert( commands_map_elem_ty("CloseBlocks",CloseBlocks) );
    m.insert( commands_map_elem_ty("GetBlockLimit",GetBlockLimit) );
    m.insert( commands_map_elem_ty("SetBlockLimit",SetBlockLimit) );
    m.insert( commands_map_elem_ty("GetBlockCount",GetBlockCount) );
    m.insert( commands_map_elem_ty("GetBlockIDs",GetBlockIDs) );
    m.insert( commands_map_elem_ty("GetBlockSize",GetBlockSize) );
    m.insert( commands_map_elem_ty("SetBlockSize",SetBlockSize) );
    m.insert( commands_map_elem_ty("GetLatency",GetLatency) );
    m.insert( commands_map_elem_ty("SetLatency",SetLatency) );
    m.insert( commands_map_elem_ty("Add",Add) );
    m.insert( commands_map_elem_ty("AddTopic",AddTopic) );
    m.insert( commands_map_elem_ty("AddItem",AddItem) );
    m.insert( commands_map_elem_ty("AddTopics",AddTopics) );
    m.insert( commands_map_elem_ty("AddItems",AddItems) );
    m.insert( commands_map_elem_ty("RemoveTopic",RemoveTopic) );
    m.insert( commands_map_elem_ty("RemoveItem",RemoveItem) );
    m.insert( commands_map_elem_ty("GetItemCount",GetItemCount) );
    m.insert( commands_map_elem_ty("GetTopicCount",GetTopicCount) );
    m.insert( commands_map_elem_ty("GetTopicNames",GetTopicNames) );
    m.insert( commands_map_elem_ty("GetItemNames",GetItemNames) );
    m.insert( commands_map_elem_ty("GetPreCachedTopicEnums",GetPreCachedTopicEnums) );
    m.insert( commands_map_elem_ty("GetPreCachedItemCount",GetPreCachedItemCount) );
    m.insert( commands_map_elem_ty("GetPreCachedTopicCount",GetPreCachedTopicCount) );
    m.insert( commands_map_elem_ty("GetPreCachedItemNames",GetPreCachedItemNames) );
    m.insert( commands_map_elem_ty("GetPreCachedTopicNames",GetPreCachedTopicNames) );
    m.insert( commands_map_elem_ty("GetTypeBits",GetTypeBits) );
    m.insert( commands_map_elem_ty("GetTypeString",GetTypeString) );
    m.insert( commands_map_elem_ty("IsUsingDateTime",IsUsingDateTime) );
    m.insert( commands_map_elem_ty("GetStreamOccupancy",GetStreamOccupancy) );
    m.insert( commands_map_elem_ty("GetMarkerPosition",GetMarkerPosition) );
    m.insert( commands_map_elem_ty("IsMarkerDirty",IsMarkerDirty) );
    m.insert( commands_map_elem_ty("DumpBufferStatus",DumpBufferStatus) );

    return m;
}

};


commands_map_ty commands_admin = build_commands_map_admin();


namespace {


void 
Connect(void *ctx)
{
    int ret = TOSDB_Connect();
    check_display_ret(ret);
}

  
void
Disconnect(void *ctx)
{
    int ret = TOSDB_Disconnect();
    check_display_ret(ret);
}
   

void
IsConnected(void *ctx)
{
    unsigned int ret = TOSDB_IsConnected();
    std::cout<< std::endl << std::boolalpha << (ret == 1) << std::endl << std::endl;
}


void
CreateBlock(void *ctx)
{
    std::string timeout;
    std::string block;
    std::string size;
    std::string dts_y_or_n;
    unsigned long timeout_num;
    int ret;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"block size: ";
    prompt>> size;
        
    prompt<<"use datetime stamp?(y/n) ";    
    std::cin.get();
    prompt>> dts_y_or_n ;
    if(dts_y_or_n != "y" && dts_y_or_n != "n")
        std::cerr<< std::endl << "INVALID - default to 'n'" << std::endl << std::endl;

    prompt<<"timeout(milliseconds) [positive number, otherwise use default]: ";
    prompt>> timeout;
                       
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
    check_display_ret(ret);            
}


void
CloseBlock(void *ctx)
{
    int ret;
    std::string block;

    prompt<<"block id: ";
    prompt>> block;          

    ret = TOSDB_CloseBlock(block.c_str());
    check_display_ret(ret);
}


void
CloseBlocks(void *ctx)
{
    int ret =  TOSDB_CloseBlocks();
    check_display_ret(ret);
}


void
GetBlockLimit(void *ctx)
{
      std::cout<< std::endl << TOSDB_GetBlockLimit() << std::endl << std::endl;
}


void
SetBlockLimit(void *ctx)
{
    std::string size;

    prompt<<"block limit: ";
    prompt>> size;
    std::cout<< std::endl << TOSDB_SetBlockLimit(std::stoul(size)) << std::endl << std::endl;
}

void
GetBlockCount(void *ctx)
{
    std::cout<< TOSDB_GetBlockCount() <<std::endl;
}


void
GetBlockIDs(void *ctx)
{
    if(prompt_for_cpp()){
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
            check_display_ret(ret,strs,nblocks);

            DeleteStrings(strs, nblocks);
        }catch(...){    
            DeleteStrings(strs, nblocks);
            throw;
        }            
    }
}


void
GetBlockSize(void *ctx)
{
    int ret;
    std::string block;

    size_type sz = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){            
        std::cout<< std::endl << TOSDB_GetBlockSize(block) << std::endl << std::endl;
    }else{          
        ret = TOSDB_GetBlockSize(block.c_str(), &sz);
        check_display_ret(ret, sz);      
    }
}


void
SetBlockSize(void *ctx)
{
    int ret;
    std::string block;
    std::string size;      

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"block size: ";
    prompt>> size;

    ret = TOSDB_SetBlockSize(block.c_str(), std::stoul(size));
    check_display_ret(ret);
}


void
GetLatency(void *ctx)
{
     std::cout<< TOSDB_GetLatency() <<std::endl;
}


void
SetLatency(void *ctx)
{
    unsigned long lat;

    prompt<<"enter latency value(milleseconds)";
    prompt>> lat;       

    TOSDB_SetLatency((UpdateLatency)lat);
}


void
Add(void *ctx)
{
    std::string block;
    size_type nitems;
    size_type ntopics;
    int ret;

    char **items_raw = nullptr;
    char **topics_raw = nullptr;

    prompt<<"block id: ";
    prompt>> block;   

    try{           
        nitems = get_cstr_items(&items_raw);
        ntopics = get_cstr_topics(&topics_raw);

        if(prompt_for_cpp()){    
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

        check_display_ret(ret);

        del_cstr_items(items_raw, nitems);
        del_cstr_topics(topics_raw, ntopics); 
    }catch(...){
        del_cstr_items(items_raw, nitems);
        del_cstr_topics(topics_raw, ntopics); 
        throw;
    }
}


void
AddTopic(void *ctx)
{
    std::string block;
    std::string topic;
    int ret;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"topic: ";
    prompt>> topic;  

    ret = prompt_for_cpp()
        ? TOSDB_AddTopic(block, TOS_Topics::MAP()[topic])
        : TOSDB_AddTopic(block.c_str(), topic.c_str());            
        
    check_display_ret(ret);
}


void
AddItem(void *ctx)
{    
    std::string block;
    std::string item;
    int ret;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"item: ";
    prompt>> item;    

    ret = prompt_for_cpp()
        ? TOSDB_AddItem(block, item)
        : TOSDB_AddItem(block.c_str(), item.c_str());

    check_display_ret(ret);
}


void
AddTopics(void *ctx)
{
    std::string block;        
    size_type ntopics;
    int ret;

    char **topics_raw = nullptr;

    prompt<<"block id: ";
    prompt>> block;

    try{ 
        ntopics = get_cstr_topics(&topics_raw); 

        if(prompt_for_cpp()){
            auto topics = topic_set_type( 
                              topics_raw, 
                              ntopics, 
                              [=](LPCSTR str){ return TOS_Topics::MAP()[str]; }
                          );
            ret = TOSDB_AddTopics(block, topics);
        }else{    
            ret = TOSDB_AddTopics(block.c_str(), (const char**)topics_raw, ntopics);
        }
        check_display_ret(ret);
     
        del_cstr_topics(topics_raw, ntopics);
    }catch(...){
        del_cstr_topics(topics_raw, ntopics);
        throw;
    }
}


void
AddItems(void *ctx)
{
    std::string block;       
    size_type nitems;
    int ret;

    char **items_raw = nullptr;

    prompt<<"block id: ";
    prompt>> block;
           
    try{            
        nitems = get_cstr_items(&items_raw);

        ret = prompt_for_cpp()
            ? TOSDB_AddItems(block, str_set_type(items_raw, nitems))
            : TOSDB_AddItems(block.c_str(), (const char**)items_raw, nitems);
            
        check_display_ret(ret);
        del_cstr_items(items_raw, nitems);
    }catch(...){
        del_cstr_items(items_raw, nitems);      
        throw;
    }         
}


void
RemoveTopic(void *ctx)
{
    std::string block;
    std::string topic;
    int ret;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"topic: ";
    prompt>> topic;  

    ret = prompt_for_cpp() 
        ? TOSDB_RemoveTopic(block, TOS_Topics::MAP()[topic])
        : TOSDB_RemoveTopic(block.c_str(), topic.c_str());   

    check_display_ret(ret);
}


void
RemoveItem(void *ctx)
{
    std::string block;
    std::string item;
    int ret;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"item: ";
    prompt>> item;    

    ret = prompt_for_cpp() 
        ? TOSDB_RemoveItem(block, item)
        : TOSDB_RemoveItem(block.c_str(), item.c_str());

    check_display_ret(ret);
}


void
GetItemCount(void *ctx)
{
    std::string block;        
    int ret;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl << TOSDB_GetItemCount(block) << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{         
        ret = TOSDB_GetItemCount(block.c_str(), &count);
        check_display_ret(ret, count);
    }
}


void
GetTopicCount(void *ctx)
{
    std::string block;
    int ret;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl << TOSDB_GetTopicCount(block) << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{         
        ret = TOSDB_GetTopicCount(block.c_str(), &count);
        check_display_ret(ret, count);
    }
}


void
GetTopicNames(void *ctx)
{              
    std::string block;
    int ret;
    char **strs;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){         
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
            check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }            
    }
}


void
GetItemNames(void *ctx)
{
    int ret;
    char **strs;
    std::string block;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){         
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
            check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }
    }
}


void
GetTopicEnums(void *ctx)
{
    std::string block;

    prompt<<"block id: ";
    prompt>> block;      
      
    std::cout<< std::endl;
    for(auto & t : TOSDB_GetTopicEnums(block))
        std::cout<< (TOS_Topics::enum_value_type)t <<' '
                  << TOS_Topics::MAP()[t] << std::endl;
    std::cout<< std::endl;
}

void
GetPreCachedTopicNames(void *ctx)
{
    char **strs;
    int ret;
    std::string block;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){    
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
            check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }          
    }
}


void
GetPreCachedItemNames(void *ctx)
{
    char **strs;
    int ret;
    std::string block;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){    
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
            check_display_ret(ret, strs, count);

            DeleteStrings(strs, count);
        }catch(...){
            DeleteStrings(strs, count);
            throw;
        }            
    }
}

void
GetPreCachedTopicEnums(void *ctx)
{
    std::string block;

    prompt<<"block id: ";
    prompt>> block;      
        
    std::cout<< std::endl; 
    for(auto & t : TOSDB_GetPreCachedTopicEnums(block))
        std::cout<< (TOS_Topics::enum_value_type)t << ' ' 
                  << TOS_Topics::MAP()[t] << std::endl;
    std::cout<< std::endl; 
}


void
GetPreCachedItemCount(void *ctx)
{
    int ret;
    std::string block;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetPreCachedItemCount(block) 
                      << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetPreCachedItemCount(block.c_str(), &count);
        check_display_ret(ret, count);
    }
}


void
GetPreCachedTopicCount(void *ctx)
{
    int ret;
    std::string block;

    size_type count = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetPreCachedTopicCount(block) 
                      << std::endl << std::endl;
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetPreCachedTopicCount(block.c_str(), &count);
        check_display_ret(ret, count);
    }
}


void
GetTypeBits(void *ctx)
{
    int ret;
    std::string topic;

    type_bits_type bits = 0;

    prompt<<"topic: ";
    prompt>> topic;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetTypeBits(TOS_Topics::MAP()[topic]) 
                      << std::endl << std::endl; 
        }catch(std::exception & e){
            std::cout<< std::endl <<"error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetTypeBits(topic.c_str(), &bits);
        check_display_ret(ret, ((int)bits));
    }
}


void
GetTypeString(void *ctx)
{        
    int ret;
    char tstring[256];
    std::string topic;

    prompt<<"topic: ";
    prompt>> topic;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl 
                      << TOSDB_GetTypeString(TOS_Topics::MAP()[topic]) 
                      << std::endl << std::endl; 
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{            
        ret = TOSDB_GetTypeString(topic.c_str(), tstring, 256);
        check_display_ret(ret, tstring);
    }         
}


void
IsUsingDateTime(void *ctx)
{
    int ret;        
    std::string block;     

    unsigned int using_dt = 0;

    prompt<<"block id: ";
    prompt>> block;

    if(prompt_for_cpp()){
        try{
            std::cout<< std::endl << std::boolalpha 
                      << TOSDB_IsUsingDateTime(block) 
                      << std::endl << std::endl; 
        }catch(std::exception & e){
            std::cout<< std::endl << "error: " << e.what() << std::endl << std::endl;
        }
    }else{
        ret = TOSDB_IsUsingDateTime(block.c_str(), &using_dt);
        check_display_ret(ret, (using_dt==1));           
    }
}


void
GetStreamOccupancy(void *ctx)
{
    int ret;
    std::string block;
    std::string item;
    std::string topic;

    size_type occ = 0;

    prompt_for_block_item_topic(&block, &item, &topic);

    if(prompt_for_cpp()){
        std::cout<< std::endl 
                  << TOSDB_GetStreamOccupancy(block, item, TOS_Topics::MAP()[topic]) 
                  << std::endl << std::endl; 
    }else{            
        ret = TOSDB_GetStreamOccupancy(block.c_str(), item.c_str(), topic.c_str(), &occ);
        check_display_ret(ret, occ);
    }
}


void
GetMarkerPosition(void *ctx)
{
    int ret;
    std::string block;
    std::string item;
    std::string topic;

    long long pos = 0;

    prompt_for_block_item_topic(&block, &item, &topic);  

    if(prompt_for_cpp()){
        std::cout<< std::endl 
                  << TOSDB_GetMarkerPosition(block, item, TOS_Topics::MAP()[topic]) 
                  << std::endl << std::endl; 
    }else{             
        ret = TOSDB_GetMarkerPosition(block.c_str(), item.c_str(), topic.c_str(), &pos);
        check_display_ret(ret, pos);
    }
}


void
IsMarkerDirty(void *ctx)
{
    int ret;
    std::string block;
    std::string item;
    std::string topic;
        
    unsigned int is_dirty = false;

    prompt_for_block_item_topic(&block, &item, &topic);

    if(prompt_for_cpp()){
        std::cout<< std::endl << std::boolalpha 
                  << TOSDB_IsMarkerDirty(block, item,TOS_Topics::MAP()[topic]) 
                  << std::endl << std::endl; 
    }else{
        ret = TOSDB_IsMarkerDirty(block.c_str(), item.c_str(),topic.c_str(), &is_dirty);
        check_display_ret(ret, (is_dirty==1));
    }
}


void
DumpBufferStatus(void *ctx)
{
    TOSDB_DumpSharedBufferStatus();
}


};