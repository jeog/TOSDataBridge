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

#include <iomanip>

#include "tos_databridge.h"

/* 
    NOTE: this shell is for testing/querying/debugging interface and engine. 

   *** It DOES NOT check input, format all output etc. etc.***
 */

namespace{

#define CHECK_DISPLAY_RET_DEF(r) \
do{ \
    if(r) \
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; \
    else \
        std::cout<< std::endl << "SUCCESS" << std::endl << std::endl; \
}while(0)

#define CHECK_DISPLAY_RET_VAL(r,v) \
do{ \
    if(r) \
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; \
    else \
        std::cout<< std::endl << v << std::endl << std::endl; \
}while(0)

#define CHECK_DISPLAY_RET_VAL_DTS(r,v,d) \
do{ \
    if(r) \
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; \
    else \
        std::cout<< std::endl << v << ' ' << d << std::endl << std::endl; \
}while(0)

#define CHECK_DISPLAY_RET_BOOL(r,b) \
do{ \
    if(r) \
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; \
    else \
        std::cout<< std::endl << std::boolalpha << b << std::endl << std::endl; \
}while(0)

#define CHECK_DISPLAY_RET_MULTI(r,array,n) \
do{ \
    if(r) \
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; \
    else{ \
        std::cout<< std::endl; \
        for(size_type i = 0; i < n; ++i) \
            std::cout<< array[i] << std::endl; \
        std::cout<< std::endl; \
    } \
}while(0)

#define PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic) \
do{ \
    prompt<<"block id: "; \
    prompt>> block; \
    prompt<<"item: "; \
    prompt>> item; \
    prompt<<"topic: "; \
    prompt>> topic; \
}while(0)    

#define PROMPT_FOR_BLOCK_ITEM_TOPIC_INDEX(block,item,topic,index) \
do{ \
    PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic); \
    prompt<<"index: "; \
    prompt>> index; \
}while(0)   


class stream_prompt {
    std::string _prmpt;
public:
    stream_prompt(std::string prompt)
        :      
            _prmpt(prompt)
        {
       }

    template<typename T>
    std::ostream& 
    operator<<(const T& val)
    {
        std::cout<<_prmpt<<' '<<val; 
        return std::cout;
    }

    template< typename T >
    std::istream& 
    operator>>(T& val)
    {
        std::cout<<_prmpt<<' ';
        std::cin>>val;
        return std::cin;
    }

} prompt("[-->");


std::string admin_commands[] = {   
    "Connect", 
    "Disconnect", 
    "IsConnected",
    "CreateBlock", 
    "CloseBlock", 
    "CloseBlocks",
    "GetBlockLimit",
    "SetBlockLimit",   
    "GetBlockCount",
    "GetBlockIDs",
    "GetBlockSize", 
    "SetBlockSize",
    "GetLatency", 
    "SetLatency",
    "Add",
    "AddTopic",
    "AddItem",
    "AddTopics",
    "AddItems", 
    "RemoveTopic",
    "RemoveItem",
    "GetItemCount",
    "GetTopicCount",
    "GetTopicNames",
    "GetItemNames",
    "GetPreCachedTopicEnums",    
    "GetPreCachedItemCount", 
    "GetPreCachedTopicCount",
    "GetPreCachedItemNames", 
    "GetPreCachedTopicNames",
    "GetTypeBits",
    "GetTypeString",
    "IsUsingDateTime",
    "GetStreamOccupancy",
    "GetMarkerPosition",
    "IsMarkerDirty",
    "DumpBufferStatus"
};

std::string get_val_commands[] = { 
    "GetDouble",
    "GetFloat",
    "GetLongLong",
    "GetLong",
    "GetString",
    "GetGeneric"
};

std::string get_ds_commands[] = {
    "GetStreamSnapshotDoubles",
    "GetStreamSnapshotFloats", 
    "GetStreamSnapshotLongLongs",
    "GetStreamSnapshotLongs",
    "GetStreamSnapshotStrings",
    "GetStreamSnapshotGenerics",
    "GetStreamSnapshotDoublesFromMarker",
    "GetStreamSnapshotFloatsFromMarker", 
    "GetStreamSnapshotLongLongsFromMarker",
    "GetStreamSnapshotLongsFromMarker",
    "GetStreamSnapshotStringsFromMarker",
};

std::string get_frame_commands[] = {
    "GetItemFrame",
    "GetItemFrameStrings",
    "GetItemFrameDoubles",
    "GetItemFrameFloats",
    "GetItemFrameLongLongs",
    "GetItemFrameLongs",
    "GetTopicFrame",
    "GetTopicFrameStrings",
    "GetTotalFrame"
};

void 
on_cmd_commands();

bool 
on_cmd_admin(std::string cmd);

bool 
on_cmd_get(std::string cmd);

bool 
on_cmd_stream_snapshot(std::string cmd);

bool 
on_cmd_frame(std::string cmd);

size_type 
get_cstr_items(char ***p);

size_type
get_cstr_topics(char ***p);

void 
del_cstr_items(char **items, size_type nitems);

void 
del_cstr_topics(char **topics, size_type ntopics);

template<typename T>
void 
display_stream_data(size_type len, T* dat, pDateTimeStamp dts=nullptr);

template<typename T>
void 
_get();

template<typename T>
void 
_get( int(*func)(LPCSTR, LPCSTR, LPCSTR, long, T*, pDateTimeStamp) );

template<typename T>
void 
_get_item_frame( int(*func)(LPCSTR, LPCSTR, T*, size_type,
                            LPSTR*,size_type,pDateTimeStamp) );

template<typename T>
void 
_get_stream_snapshot( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*, 
                                 size_type, pDateTimeStamp, long, long) );

template<typename T>
void _get_stream_snapshot();

template<typename T>
void 
_get_stream_snapshot_from_marker( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*,
                                             size_type, pDateTimeStamp, long, long*) );

};

int main(int argc, char* argv[])
{  
    std::string cmd;
    char lpath[MAX_PATH+40+40];

    std::cout<<"[-------------------------------------------------------------]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[--     Welcome to the TOS-DataBridge Debug Shell           --]" <<std::endl;
    std::cout<<"[--       Copyright (C) 2014 Jonathon Ogden                 --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-------------------------------------------------------------]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-- This program is distributed WITHOUT ANY WARRANTY.       --]" <<std::endl;
    std::cout<<"[-- See the GNU General Public License for more details.    --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[--   Use 'Connect' command to connect to the Service.      --]" <<std::endl;
    std::cout<<"[--   Use 'commands' for a list of commands by category.    --]" <<std::endl;
    std::cout<<"[--   Use 'exit' to exit.                                   --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-- NOTE: Topics/Items are case sensitive; use upper-case   --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-------------------------------------------------------------]" <<std::endl;
    
    TOSDB_GetClientLogPath(lpath,MAX_PATH+40+40);
    std::cout<<"LOG: " << lpath << std::endl << std::endl;

    while(1){    
        try{
            prompt>> cmd;   
            if(cmd == "exit" || cmd == "quit" || cmd == "close")
                break;
            else if(cmd == "commands")
                on_cmd_commands();
            else{
                if( on_cmd_admin(cmd) 
                    || on_cmd_get(cmd)
                    || on_cmd_stream_snapshot(cmd)
                    || on_cmd_frame(cmd) )
                {
                    continue;
                }
                std::cout<< "BAD COMMAND" << std::endl;
            }
        }catch(TOSDB_Error& e){      
            std::cerr<< std::endl << "*** TOSDB_Error caught by shell **" << std::endl
                     << std::setw(20) << "    Process ID: "<< e.processID() << std::endl
                     << std::setw(20) << "    Thread ID: "<< e.threadID() << std::endl
                     << std::setw(20) << "    tag: " << e.tag() << std::endl
                     << std::setw(20) << "    info: " << e.info() << std::endl
                     << std::setw(20) << "    what: " << e.what() << std::endl << std::endl;
        }
    }

    return 0; 
}


namespace{

#define CMD_OUT_PER_PAGE 10

void
decr_page_latch_and_wait(int *n, std::function<void(void)> cb_on_wait )
{
    if(*n > 0){
        --(*n);       
    }else{
        std::cout<< std::endl << "(Press Enter)";
        std::cin.get();
        std::cout<< std::endl;
        cb_on_wait();
    }
}


bool 
use_cpp_version(int recurse=2)
{
    char cpp_y_or_no;

    prompt<< "use C++ version? (y/n):" ;   
    std::cin.get(); 
    prompt>> cpp_y_or_no;

    if(cpp_y_or_no == 'y')
        return true;
    else if(cpp_y_or_no == 'n')
        return false;
    else if(recurse > 0){
        std::cout<< "INVALID - enter 'y' or 'n'" << std::endl;
        return use_cpp_version(recurse-1);
    }else{
        std::cout<< "INVALID - defaulting to C version" << std::endl;
        return false;
    }
}


void 
on_cmd_commands()
{
    std::string cmd;
    int count = CMD_OUT_PER_PAGE;        

    std::cout<< std::endl << "What type of command?" << std::endl << std::endl
             << "- admin" << std::endl
             << "- get" << std::endl
             << "- stream-snapshot" << std::endl
             << "- frame" << std::endl
             << "- back" << std::endl << std::endl;

    prompt>> cmd;
    std::cin.get();

    if(cmd == "admin"){      
  
        std::cout<< std::endl << "ADMINISTRATIVE COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet< std::string>(admin_commands)){      
            std::cout<<"- "<< x << std::endl;
            decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } );                    
        }

    }else if(cmd == "get"){  

        std::cout<< std::endl << "GET COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet< std::string>(get_val_commands)){      
            std::cout<<"- "<< x << std::endl;
            decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } );                    
        }
     
    }else if(cmd == "stream-snapshot"){  

        std::cout<< std::endl << "STREAM SNAPSHOT COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet< std::string>(get_ds_commands)){      
            std::cout<<"- "<< x << std::endl;
            decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } );                    
        }
        
    }else if(cmd == "frame"){  

        std::cout<< std::endl << "FRAME COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet< std::string>(get_frame_commands)){      
            std::cout<<"- "<< x << std::endl;
            decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } );                    
        }           
        
    }else if(cmd != "back"){

        std::cout<< "BAD COMMAND";

    }
    
    std::cout<< std::endl;
}


bool 
on_cmd_admin(std::string cmd)
{
    if(cmd == "Connect"){  

        int ret = TOSDB_Connect();
        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "Disconnect"){

        int ret = TOSDB_Disconnect();
        CHECK_DISPLAY_RET_DEF(ret);
   
    }else if(cmd == "IsConnected"){

        unsigned int ret = TOSDB_IsConnected();
        std::cout<< std::endl << std::boolalpha << (ret == 1) << std::endl << std::endl;

    }else if(cmd == "CreateBlock"){

        unsigned long timeout;
        std::string block;
        std::string size;
        char dts_y_or_n;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"block size: ";
        prompt>> size;
        prompt<<"use datetime stamp?(y/n): ";    
        std::cin.get();
        prompt>> dts_y_or_n ;
        prompt<<"timeout: ";
        prompt>> timeout;

        int ret =  TOSDB_CreateBlock(block.c_str(), std::stoul(size) , (dts_y_or_n == 'y'), timeout);      
        CHECK_DISPLAY_RET_DEF(ret);
            
    }else if(cmd == "CloseBlock"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;          

        int ret = TOSDB_CloseBlock(block.c_str());
        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "CloseBlocks"){

        int ret =  TOSDB_CloseBlocks();
        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "GetBlockLimit"){   

        std::cout<< std::endl << TOSDB_GetBlockLimit() << std::endl << std::endl;

    }else if(cmd == "SetBlockLimit"){  

        std::string size;

        prompt<<"block limit: ";
        prompt>> size;
        std::cout<< std::endl << TOSDB_SetBlockLimit(std::stoul(size)) << std::endl << std::endl;

    }else if(cmd == "GetBlockCount"){    

        std::cout<< TOSDB_GetBlockCount() <<std::endl;

    }else if(cmd == "GetBlockIDs"){  

        if(use_cpp_version()){
            std::cout<< std::endl;
            for(auto & str : TOSDB_GetBlockIDs())
                std::cout<< str << std::endl;
            std::cout<< std::endl;
        }else{  
            char** strs;
            size_type nblocks = TOSDB_GetBlockCount();            
            try{
                strs = NewStrings(nblocks, TOSDB_BLOCK_ID_SZ);

                int ret = TOSDB_GetBlockIDs(strs, nblocks, TOSDB_BLOCK_ID_SZ);
                CHECK_DISPLAY_RET_MULTI(ret,strs,nblocks);

                DeleteStrings(strs, nblocks);
            }catch(...){    
                DeleteStrings(strs, nblocks);
                throw;
            }            
        }

    }else if(cmd == "GetBlockSize"){

        std::string block;

        if(use_cpp_version()){
            prompt<<"block id: ";
            prompt>> block;
            std::cout<< std::endl << TOSDB_GetBlockSize(block) << std::endl << std::endl;
        }else{
            size_type sz = 0;

            prompt<<"block id: ";
            prompt>> block;                    

            int ret = TOSDB_GetBlockSize(block.c_str(), &sz);
            CHECK_DISPLAY_RET_DEF(ret);      
        }

    }else if(cmd == "SetBlockSize"){

        std::string block;
        std::string size;      

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"block size: ";
        prompt>> size;

        int ret = TOSDB_SetBlockSize(block.c_str(), std::stoul(size));
        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "GetLatency"){

        std::cout<< TOSDB_GetLatency() <<std::endl;

    }else if(cmd == "SetLatency"){

        unsigned long lat;

        prompt<<"enter latency value(milleseconds)";
        prompt>> lat;       

        TOSDB_SetLatency((UpdateLatency)lat);

    }else if(cmd == "Add"){   

        std::string block;
        size_type nitems;
        size_type ntopics;

        char **items_raw = nullptr;
        char **topics_raw = nullptr;

        prompt<<"block id: ";
        prompt>> block;   

        try{
            int ret;
            nitems = get_cstr_items(&items_raw);
            ntopics = get_cstr_topics(&topics_raw);

            if(use_cpp_version()){    
                auto topics = topic_set_type( topics_raw, 
                                              ntopics,
                                              [=](LPCSTR str)
                                              { 
                                                  return TOS_Topics::MAP()[str];
                                              }
                                             );
                auto items = str_set_type(items_raw, nitems);
                ret =  TOSDB_Add(block, items, topics);                  
            }else{             
                ret = TOSDB_Add(block.c_str(), (const char**)items_raw, nitems, 
                                (const char**)topics_raw, ntopics);          
            }   
            CHECK_DISPLAY_RET_DEF(ret);

            del_cstr_items(items_raw, nitems);
            del_cstr_topics(topics_raw, ntopics); 
        }catch(...){
            del_cstr_items(items_raw, nitems);
            del_cstr_topics(topics_raw, ntopics); 
            throw;
        }
       
    }else if(cmd == "AddTopic"){    

        std::string block;
        std::string topic;
        int ret;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"topic: ";
        prompt>> topic;  

        if(use_cpp_version())  
            ret = TOSDB_AddTopic(block, TOS_Topics::MAP()[topic]);         
        else           
            ret = TOSDB_AddTopic(block.c_str(), topic.c_str());            
        
        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "AddItem"){    

        std::string block;
        std::string item;
        int ret;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"item: ";
        prompt>> item;    

        if(use_cpp_version())
            ret = TOSDB_AddItem(block, item);
        else
            ret = TOSDB_AddItem(block.c_str(), item.c_str());

        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "AddTopics"){      

        std::string block;        
        size_type ntopics;
        int ret;

        char **topics_raw = nullptr;

        prompt<<"block id: ";
        prompt>> block;

        try{ 
            ntopics = get_cstr_topics(&topics_raw); 

            if(use_cpp_version()){
                auto topics = topic_set_type( topics_raw, 
                                              ntopics, 
                                              [=](LPCSTR str)
                                              { 
                                                  return TOS_Topics::MAP()[str]; 
                                              }
                                             );
                ret = TOSDB_AddTopics(block, topics);
            }else{    
                ret = TOSDB_AddTopics(block.c_str(), (const char**)topics_raw, ntopics);
            }
            CHECK_DISPLAY_RET_DEF(ret);
     
            del_cstr_topics(topics_raw, ntopics);
        }catch(...){
            del_cstr_topics(topics_raw, ntopics);
            throw;
        }
    
    }else if(cmd == "AddItems"){        

        std::string block;       
        size_type nitems;
        int ret;

        char **items_raw = nullptr;

        prompt<<"block id: ";
        prompt>> block;
           
        try{            
            nitems = get_cstr_items(&items_raw);

            if(use_cpp_version()){
                auto items = str_set_type(items_raw, nitems);
                ret = TOSDB_AddItems(block, items);
            }else{
                ret = TOSDB_AddItems(block.c_str(), (const char**)items_raw, nitems);
            }
            CHECK_DISPLAY_RET_DEF(ret);

            del_cstr_items(items_raw, nitems);
        }catch(...){
            del_cstr_items(items_raw, nitems);      
            throw;
        }         

    }else if(cmd == "RemoveTopic"){    

        std::string block;
        std::string topic;
        int ret;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"topic: ";
        prompt>> topic;  

        if(use_cpp_version())
            ret = TOSDB_RemoveTopic(block, TOS_Topics::MAP()[topic]);
        else  
            ret = TOSDB_RemoveTopic(block.c_str(), topic.c_str());
        
        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "RemoveItem"){    

        std::string block;
        std::string item;
        int ret;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"item: ";
        prompt>> item;    

        if(use_cpp_version())
            ret = TOSDB_RemoveItem(block, item);
        else
            ret = TOSDB_RemoveItem(block.c_str(), item.c_str());

        CHECK_DISPLAY_RET_DEF(ret);

    }else if(cmd == "GetItemCount"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << TOSDB_GetItemCount(block) << std::endl << std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            size_type l;

            int ret = TOSDB_GetItemCount(block.c_str(), &l);
            CHECK_DISPLAY_RET_VAL(ret, l);
        }

    }else if(cmd == "GetTopicCount"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << TOSDB_GetTopicCount(block) << std::endl << std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            size_type l;

            int ret = TOSDB_GetTopicCount(block.c_str(), &l);
            CHECK_DISPLAY_RET_VAL(ret, l);
        }
         
    }else if(cmd == "GetTopicNames"){    

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){         
            try{           
                std::cout<< std::endl;
                for(auto & t : TOSDB_GetTopicNames(block))
                    std::cout<< t << std::endl;
                std::cout<< std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            char **strs;
            size_type l = TOSDB_GetTopicCount(block);  

            try{
                strs = NewStrings(l, TOSDB_MAX_STR_SZ);

                int ret = TOSDB_GetTopicNames(block.c_str(),strs,l,TOSDB_MAX_STR_SZ);
                CHECK_DISPLAY_RET_MULTI(ret,strs,l);

                DeleteStrings(strs, l);
            }catch(...){
                DeleteStrings(strs, l);
                throw;
            }            
        }

    }else if(cmd == "GetItemNames"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){         
            try{     
                std::cout<< std::endl;
                for(auto & i : TOSDB_GetItemNames(block))
                    std::cout<< i << std::endl;
                std::cout<< std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            char **strs;
            size_type l = TOSDB_GetItemCount(block);            

            try{
                strs = NewStrings(l, TOSDB_MAX_STR_SZ);

                int ret = TOSDB_GetItemNames(block.c_str(),strs,l,TOSDB_MAX_STR_SZ);
                CHECK_DISPLAY_RET_MULTI(ret,strs,l);

                DeleteStrings(strs, l);
            }catch(...){
                DeleteStrings(strs, l);
                throw;
            }
        }

    }else if(cmd == "GetTopicEnums"){    

        std::string block;

        prompt<<"block id: ";
        prompt>> block;      
      
        for(auto & t : TOSDB_GetTopicEnums(block))
            std::cout<< (TOS_Topics::enum_value_type)t <<' '
                     << TOS_Topics::MAP()[t] << std::endl;
    
    }else if(cmd == "GetPreCachedTopicNames"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){    
            try{         
                std::cout<< std::endl;
                for(auto & t : TOSDB_GetPreCachedTopicNames(block))
                    std::cout<< t << std::endl;
                std::cout<< std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{                  
            char **strs;
            size_type l = TOSDB_GetPreCachedTopicCount(block);

            try{
                strs = NewStrings(l, TOSDB_MAX_STR_SZ);

                int ret = TOSDB_GetPreCachedTopicNames(block.c_str(),strs,l,TOSDB_MAX_STR_SZ);
                CHECK_DISPLAY_RET_MULTI(ret,strs,l);

                DeleteStrings(strs, l);
            }catch(...){
                DeleteStrings(strs, l);
                throw;
            }          
        }

    }else if(cmd == "GetPreCachedItemNames"){    

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){    
            try{            
                for(auto & i : TOSDB_GetPreCachedItemNames(block))
                    std::cout<< i << std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{                
            char **strs;
            size_type l = TOSDB_GetPreCachedItemCount(block);

            try{
                strs = NewStrings(l, TOSDB_MAX_STR_SZ);

                int ret = TOSDB_GetPreCachedItemNames(block.c_str(),strs,l,TOSDB_MAX_STR_SZ);
                CHECK_DISPLAY_RET_MULTI(ret,strs,l);

                DeleteStrings(strs, l);
            }catch(...){
                DeleteStrings(strs, l);
                throw;
            }            
        }

    }else if(cmd == "GetPreCachedTopicEnums"){    

        std::string block;

        prompt<<"block id: ";
        prompt>> block;      
        
        std::cout<< std::endl; 
        for(auto & t : TOSDB_GetPreCachedTopicEnums(block))
            std::cout<< (TOS_Topics::enum_value_type)t << ' ' 
                     << TOS_Topics::MAP()[t] << std::endl;
        std::cout<< std::endl; 

    }else if(cmd == "GetPreCachedItemCount"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << TOSDB_GetPreCachedItemCount(block) << std::endl << std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            size_type l;
            int ret = TOSDB_GetPreCachedItemCount(block.c_str(), &l);
            CHECK_DISPLAY_RET_VAL(ret, l);
        }

    }else if(cmd == "GetPreCachedTopicCount"){

        std::string block;

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << TOSDB_GetPreCachedTopicCount(block) << std::endl << std::endl;
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            size_type l;
            int ret = TOSDB_GetPreCachedTopicCount(block.c_str(), &l);
            CHECK_DISPLAY_RET_VAL(ret, l);
        }

    }else if(cmd == "GetTypeBits"){

        std::string topic;

        prompt<<"topic: ";
        prompt>> topic;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << TOSDB_GetTypeBits(TOS_Topics::MAP()[topic]) << std::endl << std::endl; 
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            type_bits_type bits;
            int ret = TOSDB_GetTypeBits(topic.c_str(), &bits);
            CHECK_DISPLAY_RET_VAL(ret, bits);
        }

    }else if(cmd == "GetTypeString"){
        
        std::string topic;

        prompt<<"topic: ";
        prompt>> topic;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << TOSDB_GetTypeString(TOS_Topics::MAP()[topic]) << std::endl << std::endl; 
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            char s[256];
            int ret = TOSDB_GetTypeString(topic.c_str(), s, 256);
            CHECK_DISPLAY_RET_VAL(ret, s);
        }         

    }else if(cmd == "IsUsingDateTime"){      

        unsigned int b;
        std::string block;     

        prompt<<"block id: ";
        prompt>> block;

        if(use_cpp_version()){
            try{
                std::cout<< std::endl << std::boolalpha << TOSDB_IsUsingDateTime(block) << std::endl << std::endl; 
            }catch(std::exception & e){
                std::cout<< "error: " << e.what() << std::endl;
            }
        }else{
            int ret = TOSDB_IsUsingDateTime(block.c_str(), &b);
            CHECK_DISPLAY_RET_BOOL(ret, (b==1));           
        }
   
    }else if(cmd == "GetStreamOccupancy"){

        std::string block;
        std::string item;
        std::string topic;

        PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);

        if(use_cpp_version()){
            std::cout<< std::endl << TOSDB_GetStreamOccupancy(block, item, TOS_Topics::MAP()[topic]) << std::endl << std::endl; 
        }else{ 
            size_type sz;
            int ret = TOSDB_GetStreamOccupancy(block.c_str(), item.c_str(), topic.c_str(), &sz);
            CHECK_DISPLAY_RET_VAL(ret, sz);
        }
      
    }else if(cmd == "GetMarkerPosition"){

        std::string block;
        std::string item;
        std::string topic;

        PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);  

        if(use_cpp_version()){
            std::cout<< std::endl << TOSDB_GetMarkerPosition(block, item, TOS_Topics::MAP()[topic]) << std::endl << std::endl; 
        }else{ 
            long long sz;
            int ret = TOSDB_GetMarkerPosition(block.c_str(), item.c_str(), topic.c_str(), &sz);
            CHECK_DISPLAY_RET_VAL(ret, sz);
        }

    }else if(cmd == "IsMarkerDirty"){      

        std::string block;
        std::string item;
        std::string topic;

        PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);

        if(use_cpp_version()){
            std::cout<< std::endl << std::boolalpha << TOSDB_IsMarkerDirty(block, item,TOS_Topics::MAP()[topic]) << std::endl << std::endl; 
        }else{ 
            unsigned int b;
            int ret = TOSDB_IsMarkerDirty(block.c_str(), item.c_str(),topic.c_str(), &b);
            CHECK_DISPLAY_RET_BOOL(ret, (b==1));
        }

    }else if(cmd == "DumpBufferStatus"){       
        TOSDB_DumpSharedBufferStatus();
    }else{        
        return false;
    }

    return true;
}


bool 
on_cmd_get(std::string cmd)
{       
    if(cmd == "GetDouble"){

        use_cpp_version() ? _get<double>() : _get(TOSDB_GetDouble);       

    }else if(cmd == "GetFloat"){

        use_cpp_version() ? _get<float>() : _get(TOSDB_GetFloat);
       
    }else if(cmd == "GetLongLong"){

       use_cpp_version() ? _get<long long>() : _get(TOSDB_GetLongLong);
       
    }else if(cmd == "GetLong"){

       use_cpp_version() ? _get<long>() : _get(TOSDB_GetLong);
       
    }else if(cmd == "GetString"){

        if(use_cpp_version()){
            _get<std::string>();
        }else{
            char str[TOSDB_STR_DATA_SZ];
            DateTimeStamp dts;

            std::string block;
            std::string item;
            std::string topic;
            long index;

            PROMPT_FOR_BLOCK_ITEM_TOPIC_INDEX(block,item,topic,index);

            int ret = TOSDB_GetString(block.c_str(),item.c_str(),topic.c_str(),index,str,100,&dts);
            CHECK_DISPLAY_RET_VAL_DTS(ret, str, dts);
        }
       
    }else if(cmd == "GetGeneric"){      

        _get<generic_type >();    
        
    }else{
        return false;
    }

    return true;
}


bool 
on_cmd_stream_snapshot(std::string cmd)
{
    if(cmd == "GetStreamSnapshotDoubles"){

        use_cpp_version() ? _get_stream_snapshot<double>() 
                          : _get_stream_snapshot<double>(TOSDB_GetStreamSnapshotDoubles);
       
    }else if(cmd == "GetStreamSnapshotFloats"){

        use_cpp_version() ? _get_stream_snapshot<float>() 
                          : _get_stream_snapshot<float>(TOSDB_GetStreamSnapshotFloats);
       
    }else if(cmd == "GetStreamSnapshotLongLongs"){

        use_cpp_version() ? _get_stream_snapshot<long long>() 
                          : _get_stream_snapshot<long long>(TOSDB_GetStreamSnapshotLongLongs);
       
    }else if(cmd == "GetStreamSnapshotLongs"){

        use_cpp_version() ? _get_stream_snapshot<long>() 
                          : _get_stream_snapshot<long>(TOSDB_GetStreamSnapshotLongs);
       
    }else if(cmd == "GetStreamSnapshotStrings"){

        if(use_cpp_version()){
            _get_stream_snapshot<std::string>();
        }else{
            size_type len;            
            long beg;
            long end;

            unsigned int has_dt;
            char **dat = nullptr;
            pDateTimeStamp dts = nullptr;

            std::string block;
            std::string item;
            std::string topic;
    
            PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);

            prompt<<"length of array to pass: ";
            prompt>>len;
            prompt<<"beginning datastream index: ";
            prompt>>beg;
            prompt<<"ending datastream index: ";
            prompt>>end;

            TOSDB_IsUsingDateTime(block.c_str(), &has_dt);
            try{
                dat = NewStrings(len, TOSDB_STR_DATA_SZ - 1);
                dts = new DateTimeStamp[len];

                int ret = TOSDB_GetStreamSnapshotStrings(block.c_str(), item.c_str(), topic.c_str(),
                                                         dat, len, TOSDB_STR_DATA_SZ, 
                                                         (has_dt ? dts : nullptr), end, beg);
                if(ret) 
                    std::cout<<"error: "<<ret<<std::endl;
                else 
                    display_stream_data(len, dat, (has_dt ? dts : nullptr));

                DeleteStrings(dat, len);
                if(dts)
                    delete dts;

            }catch(...){
                DeleteStrings(dat, len);
                if(dts)
                    delete dts;        
                throw;
            }
            
        }

    }else if(cmd == "GetStreamSnapshotGenerics"){      

        _get_stream_snapshot<generic_type>();
       
    }else if(cmd == "GetStreamSnapshotDoublesFromMarker"){

        _get_stream_snapshot_from_marker<double>(TOSDB_GetStreamSnapshotDoublesFromMarker);
       
    }else if(cmd == "GetStreamSnapshotFloatsFromMarker"){      

        _get_stream_snapshot_from_marker<float>(TOSDB_GetStreamSnapshotFloatsFromMarker);
       
    }else if(cmd == "GetStreamSnapshotLongLongsFromMarker"){

        _get_stream_snapshot_from_marker<long long>(TOSDB_GetStreamSnapshotLongLongsFromMarker);
       
    }else if(cmd == "GetStreamSnapshotLongsFromMarker"){

        _get_stream_snapshot_from_marker<long>(TOSDB_GetStreamSnapshotLongsFromMarker);
       
    }else if(cmd == "GetStreamSnapshotStringsFromMarker"){

        size_type len;        
        long beg;
        long get_size;

        unsigned int has_dt = 0;
        char **dat = nullptr;
        pDateTimeStamp dts= nullptr;

        std::string block;
        std::string item;
        std::string topic;

        PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);

        prompt<<"length of array to pass: ";
        prompt>>len;
        prompt<<"beginning datastream index: ";
        prompt>>beg;

        TOSDB_IsUsingDateTime(block.c_str(), &has_dt);
        try{         
            dat = NewStrings(len, TOSDB_STR_DATA_SZ - 1);
            dts = new DateTimeStamp[len];

            int ret = TOSDB_GetStreamSnapshotStringsFromMarker(block.c_str(), item.c_str(), topic.c_str(), 
                                                               dat, len, TOSDB_STR_DATA_SZ, 
                                                               (has_dt ? dts : nullptr), beg, &get_size);
            if(ret) 
                std::cout<<"error: "<<ret<<std::endl;
            else 
                display_stream_data(len, dat, (has_dt ? dts : nullptr));

            DeleteStrings(dat, len);
            if(dts)
                delete dts;

        }catch(...){
            DeleteStrings(dat, len);
            if(dts)
                delete dts;
        }      
    }else{
        return false;
    }

    return true;
}


bool 
on_cmd_frame(std::string cmd)
{           
    if(cmd == "GetItemFrame"){
      
        std::string block;
        std::string topic;
        char dts_y_or_n;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"topic: ";
        prompt>> topic;  
        prompt<<"get datetime stamp?(y/n): ";    
        std::cin.get();
        prompt>> dts_y_or_n ;
                
        if(dts_y_or_n == 'y')
            std::cout<< std::endl << TOSDB_GetItemFrame<true>(block,TOS_Topics::MAP()[topic]);
        else if(dts_y_or_n == 'n')
            std::cout<< std::endl << TOSDB_GetItemFrame<false>(block,TOS_Topics::MAP()[topic]);          
        else
            std::cout<< "INVALID";
        std::cout<< std::endl;

    }else if(cmd == "GetItemFrameDoubles"){  

        _get_item_frame(TOSDB_GetItemFrameDoubles);      
       
    }else if(cmd == "GetItemFrameFloats"){  

        _get_item_frame(TOSDB_GetItemFrameFloats);      
       
    }else if(cmd == "GetItemFrameLongLongs"){
  
        _get_item_frame(TOSDB_GetItemFrameLongLongs);      
       
    }else if(cmd == "GetItemFrameLongs"){  

        _get_item_frame(TOSDB_GetItemFrameLongs);      
       
    }else if(cmd == "GetItemFrameStrings"){  

        size_type nitems;
        std::string block;
        std::string topic;

        unsigned int has_dt = 0;
        char **dat = nullptr;
        char **lab = nullptr;
        pDateTimeStamp dts = nullptr;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"topic: ";
        prompt>> topic;    
        
        TOSDB_GetItemCount(block.c_str(), &nitems);
        TOSDB_IsUsingDateTime(block.c_str(), &has_dt);
        try{
            dat = NewStrings(nitems, TOSDB_STR_DATA_SZ - 1);
            lab = NewStrings(nitems, TOSDB_STR_DATA_SZ - 1);    
            dts = new DateTimeStamp[ nitems ];

            int ret = TOSDB_GetItemFrameStrings(block.c_str(), topic.c_str(), dat, nitems, TOSDB_STR_DATA_SZ,
                                                lab, TOSDB_STR_DATA_SZ, (has_dt ? dts : nullptr) );
            if(ret) 
                std::cout<< "error: " << ret << std::endl;
            else{ 
                std::cout<< std::endl;
                for(size_type i = 0; i < nitems; ++i)
                    std::cout<< lab[i] << ' ' << dat[i] << ' ' << (has_dt ? &dts[i] : nullptr)  << std::endl;
                std::cout<< std::endl;
            }

            DeleteStrings(dat, nitems);
            DeleteStrings(lab, nitems);
            if(dts)
                delete dts;

        }catch(...){
            DeleteStrings(dat, nitems);
            DeleteStrings(lab, nitems);
            if(dts)
                delete dts;
            throw;
        }             

    }else if(cmd == "GetTopicFrameStrings"){
        
        size_type ntopics;
        std::string block;
        std::string item;

        unsigned int has_dt = 0;
        char** dat = nullptr;
        char** lab = nullptr;
        pDateTimeStamp dts = nullptr;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"item: ";
        prompt>> item;   

        TOSDB_GetTopicCount(block.c_str(), &ntopics);
        TOSDB_IsUsingDateTime(block.c_str(), &has_dt);
        try{
            int ret;

            dat = NewStrings(ntopics, TOSDB_STR_DATA_SZ -1);
            lab = NewStrings(ntopics, TOSDB_STR_DATA_SZ - 1);  
            dts = new DateTimeStamp[ntopics];
                 
            ret = TOSDB_GetTopicFrameStrings(block.c_str(), item.c_str(), dat, ntopics, TOSDB_STR_DATA_SZ, 
                                             lab, TOSDB_STR_DATA_SZ, (has_dt ? dts : nullptr) );
            if(ret) 
                std::cout<< "error: " << ret << std::endl;
            else{
                std::cout<< std::endl;
                for(size_type i = 0; i < ntopics; ++i)
                    std::cout<< lab[i] << ' ' << dat[i] << ' ' << (has_dt ? &dts[i] : nullptr) << std::endl;
                std::cout<< std::endl;
            }

            DeleteStrings(dat, ntopics);
            DeleteStrings(lab, ntopics);
            if(dts)
                delete dts; 

        }catch(...){
            DeleteStrings(dat, ntopics);
            DeleteStrings(lab, ntopics);
            if(dts)
                delete dts;
        }  
            
    }else if(cmd == "GetTopicFrame"){      

        std::string block;
        std::string item;
        char dts_y_or_n;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"item: ";
        prompt>> item;  
        prompt<<"get datetime stamp?(y/n): ";    
        std::cin.get();
        prompt>> dts_y_or_n ;

        if(dts_y_or_n == 'y')      
            std::cout<< std::endl << TOSDB_GetTopicFrame<true>(block,item);      
        else if(dts_y_or_n == 'n')
            std::cout<< std::endl << TOSDB_GetTopicFrame<false>(block,item); 
        else
            std::cout<< "INVALID";

        std::cout<< std::endl;
                         
    }else if(cmd == "GetTotalFrame"){

        std::string block;
        char dts_y_or_n;

        prompt<<"block id:";
        prompt>>block;
        prompt<<"get datetime stamp?(y/n): ";
        std::cin.get();
        prompt>> dts_y_or_n;

        if(dts_y_or_n == 'y')
            std::cout<< std::endl << TOSDB_GetTotalFrame<true>(block);
        else if(dts_y_or_n == 'n')
            std::cout<< std::endl << TOSDB_GetTotalFrame<false>(block);
        else
            std::cout<< "INVALID";

        std::cout<< std::endl;
         
    }else{
        return false;
    }

    return true;                  
}


size_type 
get_cstr_items(char ***p)
{  
    std::string nitems;
    std::string item;

    prompt<<"how many items? ";
    prompt>> nitems;

    *p = NewStrings(std::stoul(nitems), TOSDB_MAX_STR_SZ);

    for(size_type i = 0; i < std::stoul(nitems); ++i){
        prompt<<"item #"<<i+1<<": ";
        prompt>> item;    
        strcpy_s((*p)[i], item.length() + 1, item.c_str());
    }

    return std::stoul(nitems);
}

size_type
get_cstr_topics(char ***p)
{  
    std::string ntopics;
    std::string topic;

    prompt<<"how many topics? ";
    prompt>> ntopics;
 
    *p = NewStrings(std::stoul(ntopics), TOSDB_MAX_STR_SZ);

    for(size_type i = 0; i < std::stoul(ntopics); ++i){
      prompt<<"topic #"<<i+1<<": ";
      prompt>> topic;  
      strcpy_s((*p)[i], topic.length() + 1, topic.c_str());
    }

    return std::stoul(ntopics);
}


void 
del_cstr_items(char **items, size_type nitems)
{
    DeleteStrings(items, nitems);
}


void 
del_cstr_topics(char **topics, size_type ntopics)
{
    DeleteStrings(topics, ntopics);
}


template<typename T>
void 
display_stream_data(size_type len, T* dat, pDateTimeStamp dts)
{
    std::string index;
    char loop = 'n';
    do{
        std::cout << std::endl;
        prompt<< "Show data from what index value?('all' to show all, 'none' to quit): ";
        prompt>> index;

        if(index == "none")
            break;

        if(index == "all"){
            for(size_type i = 0; i < len; ++i)
                std::cout<< dat[i] << ' ' << (dts ? &dts[i] : nullptr) << std::endl;               
            break;
        }

        try{
            long long i = std::stoll(index);
            std::cout<< dat[i] << ' ' << (dts ? &dts[i] : nullptr) << std::endl;             
        }catch(...){
            std::cerr<<"BAD INPUT";
            continue;
        }      
       
        std::cout<<std::endl;

        prompt<<"Show more data?(y/n): ";
        prompt>>loop;

    }while(loop == 'y');

    std::cout<< std::endl;
}

template<typename T>
void 
_get()
{  
    std::string block;
    std::string item;
    std::string topic;
    char dts_y_or_n;
    long index;

    PROMPT_FOR_BLOCK_ITEM_TOPIC_INDEX(block,item,topic,index);
   
    prompt<<"get datetime stamp?(y/n): ";    
    std::cin.get();
    prompt>> dts_y_or_n ; 
 
    if(dts_y_or_n == 'y')
        std::cout<< std::endl << TOSDB_Get<T,true>(block, item, TOS_Topics::MAP()[topic], index)<<std::endl;    
    else if(dts_y_or_n == 'n')
        std::cout<< std::endl << TOSDB_Get<T,false>(block.c_str(),item.c_str(),TOS_Topics::MAP()[topic],index)<<std::endl;            
    else
        std::cout<< "INVALID";

    std::cout<< std::endl;
}

template<typename T>
void 
_get( int(*func)(LPCSTR, LPCSTR, LPCSTR, long, T*, pDateTimeStamp) )
{
    T d;
    DateTimeStamp dts;
    std::string block;
    std::string item;
    std::string topic;
    char dts_y_or_n;
    long index;

    PROMPT_FOR_BLOCK_ITEM_TOPIC_INDEX(block,item,topic,index);

    prompt<<"get datetime stamp?(y/n): ";    
    std::cin.get();
    prompt>> dts_y_or_n ;

    if(dts_y_or_n == 'y'){
        int ret = func(block.c_str(), item.c_str(), topic.c_str(), index, &d, &dts);
        CHECK_DISPLAY_RET_VAL_DTS(ret,d,dts);          
    }else if(dts_y_or_n == 'n'){
        int ret = func(block.c_str(), item.c_str(), topic.c_str(), index, &d, nullptr);
        CHECK_DISPLAY_RET_VAL(ret,d);             
    }else
        std::cout<< "INVALID";

    std::cout<< std::endl;
  
}

template<typename T>
void 
_get_stream_snapshot()
{
    long beg; 
    long end;
    std::string block;
    std::string item;
    std::string topic;
    char dts_y_or_n;

    PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);      

    prompt<<"beginning datastream index: ";
    prompt>>beg;
    prompt<<"ending datastream index: ";
    prompt>>end;
    prompt<<"get datetime stamp?(y/n): ";    
    std::cin.get();
    prompt>> dts_y_or_n ;

    if(dts_y_or_n == 'y')      
        std::cout<< std::endl << TOSDB_GetStreamSnapshot<T,true>(block.c_str(),item.c_str(),TOS_Topics::MAP()[topic],end,beg);
    else if(dts_y_or_n == 'n')      
        std::cout<< std::endl << TOSDB_GetStreamSnapshot<T,false>(block.c_str(),item.c_str(),TOS_Topics::MAP()[topic],end,beg);            
    else
        std::cout<< "INVALID";

    std::cout<< std::endl;
}

template<typename T>
void 
_get_stream_snapshot( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*,
                                 size_type, pDateTimeStamp, long, long))
{
    size_type len;
    long beg;
    long end;
    char dts_y_or_n;

    pDateTimeStamp dts = nullptr;
    T *dat = nullptr;

    std::string block;
    std::string item;
    std::string topic;
    
    PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);

    prompt<<"length of array to pass: ";
    prompt>>len;

    try{
        dat = new T[len];
        dts = new DateTimeStamp[len];

        prompt<<"beginning datastream index: ";
        prompt>>beg;
        prompt<<"ending datastream index: ";
        prompt>>end;
        prompt<<"get datetime stamp?(y/n): ";    
        std::cin.get();
        prompt>> dts_y_or_n ;

        int ret = func(block.c_str(), item.c_str(), topic.c_str(), dat, len,
                       (dts_y_or_n == 'y') ? dts : nullptr, end, beg);
        if(ret) 
            std::cout<<"error: "<<ret<<std::endl;
        else 
            display_stream_data(len, dat, (dts_y_or_n == 'y' ? dts : nullptr));
 
        if(dat)
            delete[] dat;
        if(dts)
            delete[] dts;

    }catch(...){
        if(dat)
            delete[] dat;
        if(dts)
            delete[] dts;
    }
}


template<typename T>
void 
_get_stream_snapshot_from_marker( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*,
                                             size_type, pDateTimeStamp, long, long*))
{
    size_type len;
    long beg;
    long get_size;
    char dts_y_or_n;

    pDateTimeStamp dts = nullptr;
    T *dat = nullptr;

    std::string block;
    std::string item;
    std::string topic;

    PROMPT_FOR_BLOCK_ITEM_TOPIC(block,item,topic);

    prompt<<"length of array to pass: ";
    prompt>>len;

    try{
        dat = new T[len];
        dts = new DateTimeStamp[len];

        prompt<<"beginning datastream index: ";
        prompt>>beg;
        prompt<<"get datetime stamp?(y/n): ";    
        std::cin.get();
        prompt>> dts_y_or_n ;

        int ret = func(block.c_str(), item.c_str(), topic.c_str(), dat, len,
                       (dts_y_or_n == 'y') ? dts : nullptr, beg, &get_size);
        if(ret) 
            std::cout<<"error: "<<ret<<std::endl;
        else 
            display_stream_data(len, dat, (dts_y_or_n == 'y') ? dts : nullptr);

        if(dat)
            delete[] dat;
        if(dts)
            delete[] dts;
    }catch(...){
        if(dat)
            delete[] dat;
        if(dts)
            delete[] dts;
    }
}


template<typename T>
void 
_get_item_frame( int(*func)(LPCSTR, LPCSTR, T*, size_type,
                            LPSTR*, size_type, pDateTimeStamp) )
{
    std::string block;
    std::string topic;
    size_type nitems;

    unsigned int has_dt;
    pDateTimeStamp dts= nullptr;
    T *dat = nullptr;
    char** lab = nullptr;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"topic: ";
    prompt>> topic;    

    TOSDB_GetItemCount(block.c_str(), &nitems);
    TOSDB_IsUsingDateTime(block.c_str(), &has_dt);
    try{
        dat = new T[nitems];
        dts = new DateTimeStamp[nitems];  
        lab = NewStrings(nitems, TOSDB_MAX_STR_SZ);

        int ret = func(block.c_str(), topic.c_str() ,dat , nitems, lab, 
                       TOSDB_MAX_STR_SZ, (has_dt ? dts : nullptr));
        if(ret) 
            std::cout<< "error:  "<< ret << std::endl;
        else{
            std::cout<< std::endl;
            for(size_type i = 0; i < nitems; ++i)
                std::cout<< lab[i] << ' ' << dat[i] << ' ' << (has_dt ? &dts[i] : nullptr) << std::endl;
            std::cout<< std::endl;
        }

        DeleteStrings(lab, nitems);
        if(dat)
            delete[] dat;
        if(dts)
            delete[] dts;
    }catch(...){
        DeleteStrings(lab, nitems);
        if(dat)
            delete[] dat;
        if(dts)
            delete[] dts;
    }

}

};
