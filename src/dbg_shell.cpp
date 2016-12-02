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
#include <algorithm>

#include "tos_databridge.h"

/* 
    NOTE: this shell is for testing/querying/debugging interface and engine. 

   *** It DOES NOT check input, format all output etc. etc.***
 */

#define MAX_DISPLAY_WIDTH 80
#define LEFT_INDENT_SIZE 10
#define CMD_OUT_PER_PAGE 10

namespace{

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
    //stream_prompt&
    operator<<(const T& val)
    {
        std::cout<<_prmpt<<' '<<val; 
        return std::cout;
    }

    template< typename T >
    std::istream& 
    //stream_prompt&
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

bool
on_cmd_commands();

void
on_cmd_topics();

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

size_type
min_stream_len(std::string block, long beg, long end, size_type len);

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

void 
check_display_ret(int r);

template<typename T>
void 
check_display_ret(int r, T v);

template<typename T>
void 
check_display_ret(int r, T v, pDateTimeStamp d);

template<>
void 
check_display_ret(int r, bool v);

template<typename T>
void 
check_display_ret(int r, T *v, size_type n);

template<typename T>
void 
check_display_ret(int r, T *v, size_type n, pDateTimeStamp d);

template<typename T>
void 
check_display_ret(int r, T *v, char **l, size_type n, pDateTimeStamp d);

template<int W, const char FILL>
void
display_header_line(std::string pre, std::string post, std::string text);

template<int W, int INDENT, char H>
void
display_header(std::string hpre, std::string hpost);

};

 

int main(int argc, char* argv[])
{         
    std::string cmd;

    display_header<MAX_DISPLAY_WIDTH, LEFT_INDENT_SIZE, '-'>("[--", "--]");

    while(1){    
        try{
            prompt>> cmd;   
            if(cmd == "exit" || cmd == "quit" || cmd == "close")
                break;
            else if(cmd == "commands"){
                if( !on_cmd_commands() )
                    break;
            }else if(cmd == "topics"){
                on_cmd_topics();
            }else{
                if( on_cmd_admin(cmd) 
                    || on_cmd_get(cmd)
                    || on_cmd_stream_snapshot(cmd)
                    || on_cmd_frame(cmd) )
                {
                    continue;
                }
                std::cout<< std::endl << "BAD COMMAND" << std::endl << std::endl;
            }
        }catch(TOSDB_Error& e){ 
            std::string cont;
            std::cerr<< std::endl << "*** TOSDB_Error caught by shell **" << std::endl
                     << std::setw(15) << "    Process ID: "<< e.processID() << std::endl
                     << std::setw(15) << "    Thread ID: "<< e.threadID() << std::endl
                     << std::setw(15) << "    tag: " << e.tag() << std::endl
                     << std::setw(15) << "    info: " << e.info() << std::endl
                     << std::setw(15) << "    what: " << e.what() << std::endl << std::endl;

            /* use goto so we can cleanly break out of main while loop */
            exit_prompt: 
                std::cout<< std::endl << "Exit shell? (y/n) ";
                prompt>> cont;
                std::cout<< std::endl;
                if(cont == "y")
                    break;
                else if(cont != "n"){
                    std::cout<< "INVALID - enter 'y' or 'n'" << std::endl;
                    goto exit_prompt;
                }
            

        }
    }

    return 0; 
}


namespace{



bool
decr_page_latch_and_wait(int *n, std::function<void(void)> cb_on_wait )
{
    std::string more_y_or_n;

    if(*n > 0){
        --(*n);       
    }else{
        std::cout<< std::endl << "  More? (y/n) ";         
        prompt>> more_y_or_n;               
        cb_on_wait();
        if(more_y_or_n != "y") /* stop on everythin but 'y' */
            return false;
        std::cout<< std::endl;
    }
    return true;
}


bool 
prompt_for_cpp(int recurse=2)
{
    std::string cpp_y_or_no;

    prompt<< "use C++ version? (y/n) " ;      
    prompt>> cpp_y_or_no;

    if(cpp_y_or_no == "y")
        return true;
    else if(cpp_y_or_no == "n")
        return false;
    else if(recurse > 0){
        std::cout<< "INVALID - enter 'y' or 'n'" << std::endl;
        return prompt_for_cpp(recurse-1);
    }else{
        std::cout<< "INVALID - defaulting to C version" << std::endl;
        return false;
    }
}


bool 
prompt_for_datetime(std::string block)
{
    std::string dts_y_or_n;
  
    if( !TOSDB_IsUsingDateTime(block) )
        return false;          
   
    prompt<<"get datetime stamp? (y/n) ";       
    prompt>> dts_y_or_n ;
                
    if(dts_y_or_n == "y")
        return true;
    else if(dts_y_or_n != "n")              
        std::cout<< "INVALID - default to NO datetime" << std::endl;

    return false;    
}


void
prompt_for_block_item_topic(std::string *pblock, std::string *pitem, std::string *ptopic)
{
    prompt<<"block id: "; 
    prompt>> *pblock; 
    prompt<<"item: "; 
    prompt>> *pitem; 
    prompt<<"topic: "; 
    prompt>> *ptopic; 
}


void
prompt_for_block_item_topic_index(std::string *pblock, 
                                  std::string *pitem, 
                                  std::string *ptopic, 
                                  long *pindex)
{
    prompt_for_block_item_topic(pblock, pitem, ptopic);
    prompt<< "index: "; 
    prompt>> *pindex; 
}


void
on_cmd_topics()
{
    const int SPC = 3;  
    const std::string TOPIC_HEAD("TOPICS:   ");

    int wc;
    
    std::vector<std::string> topics;
    
    std::transform(
        TOS_Topics::MAP().cbegin(), 
        TOS_Topics::MAP().cend(), 
        std::insert_iterator<std::vector<std::string>>(topics,topics.begin()),
        [&](const TOS_Topics::topic_map_entry_type& e){ return e.second; }
    );

    std::sort(topics.begin(), topics.end(), std::less<std::string>());

    std::cout<< std::endl << TOPIC_HEAD;   
    wc = TOPIC_HEAD.size();

    for(auto & t : topics){
        if(t == " ") /* exclude NULL Topic */
            continue;       
        if(wc + t.size() > MAX_DISPLAY_WIDTH){ /* stay within maxw chars */
            std::cout<< std::endl << std::string(TOPIC_HEAD.size(),' ');
            wc = TOPIC_HEAD.size();
        }
        std::cout<< t;
        wc += t.size();
        if(wc + SPC < MAX_DISPLAY_WIDTH){
            std::cout<< std::string(SPC,' ');
            wc += SPC;
        }        
    }
    std::cout<< std::endl << std::endl;
}


bool 
on_cmd_commands()
{
    std::string cmd;
    int count = CMD_OUT_PER_PAGE;        

    std::cout<< std::endl << "What type of command?" << std::endl << std::endl
             << "- admin" << std::endl
             << "- get" << std::endl
             << "- stream" << std::endl
             << "- frame" << std::endl << std::endl
             << "- back" << std::endl << std::endl;

    prompt>> cmd;
    std::cin.get();

    if(cmd == "exit")
        return false;

    if(cmd == "admin"){     
        std::cout<< std::endl << "ADMINISTRATIVE COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet<std::string>(admin_commands)){                  
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;
            std::cout<<"- "<< x << std::endl;
        }
    }else if(cmd == "get"){  
        std::cout<< std::endl << "GET COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet<std::string>(get_val_commands)){             
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;     
            std::cout<<"- "<< x << std::endl;
        }    
    }else if(cmd == "stream"){  
        std::cout<< std::endl << "STREAM SNAPSHOT COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet<std::string>(get_ds_commands)){                  
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;   
            std::cout<<"- "<< x << std::endl;
        }  
    }else if(cmd == "frame"){  
        std::cout<< std::endl << "FRAME COMMANDS:" << std::endl << std::endl;
        for(auto & x : ILSet<std::string>(get_frame_commands)){                 
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;       
            std::cout<<"- "<< x << std::endl;
        }                   
    }else if(cmd != "back"){
        std::cout<< std::endl << "BAD COMMAND" << std::endl ;        
    }
    
    std::cout<< std::endl;
    return true;
}

bool 
on_cmd_admin(std::string cmd)
{
    if(cmd == "Connect"){  

        int ret = TOSDB_Connect();
        check_display_ret(ret);

    }else if(cmd == "Disconnect"){

        int ret = TOSDB_Disconnect();
        check_display_ret(ret);
   
    }else if(cmd == "IsConnected"){

        unsigned int ret = TOSDB_IsConnected();
        std::cout<< std::endl << std::boolalpha << (ret == 1) << std::endl << std::endl;

    }else if(cmd == "CreateBlock"){

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
            
    }else if(cmd == "CloseBlock"){

        int ret;
        std::string block;

        prompt<<"block id: ";
        prompt>> block;          

        ret = TOSDB_CloseBlock(block.c_str());
        check_display_ret(ret);

    }else if(cmd == "CloseBlocks"){

        int ret =  TOSDB_CloseBlocks();
        check_display_ret(ret);

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

    }else if(cmd == "GetBlockSize"){

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

    }else if(cmd == "SetBlockSize"){

        int ret;
        std::string block;
        std::string size;      

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"block size: ";
        prompt>> size;

        ret = TOSDB_SetBlockSize(block.c_str(), std::stoul(size));
        check_display_ret(ret);

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
       
    }else if(cmd == "AddTopic"){    

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

    }else if(cmd == "AddItem"){    

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

    }else if(cmd == "AddTopics"){      

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
    
    }else if(cmd == "AddItems"){        

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

    }else if(cmd == "RemoveTopic"){    

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

    }else if(cmd == "RemoveItem"){    

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

    }else if(cmd == "GetItemCount"){

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

    }else if(cmd == "GetTopicCount"){

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
         
    }else if(cmd == "GetTopicNames"){    
              
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

    }else if(cmd == "GetItemNames"){

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

    }else if(cmd == "GetTopicEnums"){    

        std::string block;

        prompt<<"block id: ";
        prompt>> block;      
      
        std::cout<< std::endl;
        for(auto & t : TOSDB_GetTopicEnums(block))
            std::cout<< (TOS_Topics::enum_value_type)t <<' '
                     << TOS_Topics::MAP()[t] << std::endl;
        std::cout<< std::endl;
    
    }else if(cmd == "GetPreCachedTopicNames"){

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

    }else if(cmd == "GetPreCachedItemNames"){    

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

    }else if(cmd == "GetPreCachedTopicCount"){

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

    }else if(cmd == "GetTypeBits"){

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

    }else if(cmd == "GetTypeString"){
        
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

    }else if(cmd == "IsUsingDateTime"){      

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
   
    }else if(cmd == "GetStreamOccupancy"){

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
      
    }else if(cmd == "GetMarkerPosition"){

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

    }else if(cmd == "IsMarkerDirty"){      

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

        prompt_for_cpp() ? _get<double>() : _get(TOSDB_GetDouble);       

    }else if(cmd == "GetFloat"){

        prompt_for_cpp() ? _get<float>() : _get(TOSDB_GetFloat);
       
    }else if(cmd == "GetLongLong"){

       prompt_for_cpp() ? _get<long long>() : _get(TOSDB_GetLongLong);
       
    }else if(cmd == "GetLong"){

       prompt_for_cpp() ? _get<long>() : _get(TOSDB_GetLong);
       
    }else if(cmd == "GetString"){

        if(prompt_for_cpp()){
            _get<std::string>();
        }else{
            char s[TOSDB_STR_DATA_SZ];
            DateTimeStamp dts;
            std::string block;
            std::string item;
            std::string topic;
            long index;
            int ret;
            bool get_dts;

            prompt_for_block_item_topic_index(&block, &item, &topic, &index);

            get_dts = prompt_for_datetime(block);

            ret = TOSDB_GetString(block.c_str(), item.c_str(), topic.c_str(), index, s, 
                                  TOSDB_STR_DATA_SZ, (get_dts ? &dts : nullptr));

            check_display_ret(ret, s, (get_dts ? &dts : nullptr));
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

        prompt_for_cpp() ? _get_stream_snapshot<double>() 
                         : _get_stream_snapshot<double>(TOSDB_GetStreamSnapshotDoubles);
       
    }else if(cmd == "GetStreamSnapshotFloats"){

        prompt_for_cpp() ? _get_stream_snapshot<float>() 
                         : _get_stream_snapshot<float>(TOSDB_GetStreamSnapshotFloats);
       
    }else if(cmd == "GetStreamSnapshotLongLongs"){

        prompt_for_cpp() ? _get_stream_snapshot<long long>() 
                         : _get_stream_snapshot<long long>(TOSDB_GetStreamSnapshotLongLongs);
       
    }else if(cmd == "GetStreamSnapshotLongs"){

        prompt_for_cpp() ? _get_stream_snapshot<long>() 
                         : _get_stream_snapshot<long>(TOSDB_GetStreamSnapshotLongs);
       
    }else if(cmd == "GetStreamSnapshotStrings"){

        if(prompt_for_cpp()){
            _get_stream_snapshot<std::string>();
        }else{
            size_type len;   
            size_type display_len;
            long beg;
            long end;
            int ret;
            std::string block;
            std::string item;
            std::string topic;
            bool get_dts;
                        
            char **dat = nullptr;
            pDateTimeStamp dts = nullptr;

            prompt_for_block_item_topic(&block, &item, &topic);

            prompt<<"length of array to pass: ";
            prompt>>len;
            prompt<<"beginning datastream index: ";
            prompt>>beg;
            prompt<<"ending datastream index: ";
            prompt>>end;

            get_dts = prompt_for_datetime(block);

            try{
                dat = NewStrings(len, TOSDB_STR_DATA_SZ - 1);

                if(get_dts)
                    dts = new DateTimeStamp[len];

                ret = TOSDB_GetStreamSnapshotStrings(block.c_str(), item.c_str(), topic.c_str(),
                                                     dat, len, TOSDB_STR_DATA_SZ, dts, end, beg);
           
                display_len = min_stream_len(block,beg,end,len);

                check_display_ret(ret, dat, display_len, dts);

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
        int ret;
        std::string block;
        std::string item;
        std::string topic;
        bool get_dts;

        char **dat = nullptr;
        pDateTimeStamp dts= nullptr;

        prompt_for_block_item_topic(&block, &item, &topic);

        prompt<<"length of array to pass: ";
        prompt>>len;
        prompt<<"beginning datastream index: ";
        prompt>>beg;

        get_dts = prompt_for_datetime(block);

        try{         
            dat = NewStrings(len, TOSDB_STR_DATA_SZ - 1);

            if(get_dts)
                dts = new DateTimeStamp[len];

            ret = TOSDB_GetStreamSnapshotStringsFromMarker(block.c_str(), item.c_str(), topic.c_str(), 
                                                           dat, len, TOSDB_STR_DATA_SZ, 
                                                           dts, beg, &get_size);

            check_display_ret(ret, dat, abs(get_size), dts);

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
        bool get_dts;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"topic: ";
        prompt>> topic;  

        get_dts = prompt_for_datetime(block);

        if(get_dts)
            std::cout<< std::endl << TOSDB_GetItemFrame<true>(block,TOS_Topics::MAP()[topic]);
        else
            std::cout<< std::endl << TOSDB_GetItemFrame<false>(block,TOS_Topics::MAP()[topic]);          
       
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
        int ret;
        bool get_dts;
                
        char **dat = nullptr;
        char **lab = nullptr;
        pDateTimeStamp dts = nullptr;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"topic: ";
        prompt>> topic;    
        
        get_dts = prompt_for_datetime(block);

        TOSDB_GetItemCount(block.c_str(), &nitems);
        
        try{
            dat = NewStrings(nitems, TOSDB_STR_DATA_SZ - 1);
            lab = NewStrings(nitems, TOSDB_STR_DATA_SZ - 1);    

            if(get_dts)
                dts = new DateTimeStamp[ nitems ];

            ret = TOSDB_GetItemFrameStrings(block.c_str(), topic.c_str(), dat, nitems, 
                                            TOSDB_STR_DATA_SZ, lab, TOSDB_STR_DATA_SZ, dts);

            check_display_ret(ret, dat, lab, nitems, dts);
                      
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
        int ret;
        bool get_dts;

        char** dat = nullptr;
        char** lab = nullptr;
        pDateTimeStamp dts = nullptr;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"item: ";
        prompt>> item;   
        
        get_dts = prompt_for_datetime(block);

        TOSDB_GetTopicCount(block.c_str(), &ntopics);

        try{       
            dat = NewStrings(ntopics, TOSDB_STR_DATA_SZ -1);
            lab = NewStrings(ntopics, TOSDB_STR_DATA_SZ - 1);  

            if(get_dts)
                dts = new DateTimeStamp[ntopics];
                 
            ret = TOSDB_GetTopicFrameStrings(block.c_str(), item.c_str(), dat, ntopics, 
                                             TOSDB_STR_DATA_SZ, lab, TOSDB_STR_DATA_SZ, dts);
            
            check_display_ret(ret, dat, lab, ntopics, dts);

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
        bool get_dts;

        prompt<<"block id: ";
        prompt>> block;
        prompt<<"item: ";
        prompt>> item;  

        get_dts = prompt_for_datetime(block);

        if(get_dts)      
            std::cout<< std::endl << TOSDB_GetTopicFrame<true>(block,item);      
        else
            std::cout<< std::endl << TOSDB_GetTopicFrame<false>(block,item); 
  
        std::cout<< std::endl;
                         
    }else if(cmd == "GetTotalFrame"){

        std::string block; 
        bool get_dts;

        prompt<<"block id:";
        prompt>>block;

        get_dts = prompt_for_datetime(block);

        if(get_dts)
            std::cout<< std::endl << TOSDB_GetTotalFrame<true>(block);
        else
            std::cout<< std::endl << TOSDB_GetTotalFrame<false>(block);
      
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


size_type
min_stream_len(std::string block, long beg, long end, size_type len)
{
    size_type block_size = TOSDB_GetBlockSize(block);
    size_type min_size = 0;

    if(end < 0)
        end += block_size;
    if(beg < 0)
        beg += block_size;

    min_size = min( end-beg+1, len );

    if(beg < 0 || end < 0 || beg >= block_size || end >= block_size || min_size <= 0)
        throw TOSDB_Error("STREAM DATA", "invalid beg or end index values");

    return min_size;
}


template<typename T>
void 
display_stream_data(size_type len, T* dat, pDateTimeStamp dts)
{
    std::string index;
   
    do{
        std::cout<< std::endl;
        std::cout<< "Show data from what index value?('all' to show all, 'none' to quit) ";
        prompt>> index;

        if(index == "none")
            break;

        if(index == "all"){
            std::cout<< std::endl; 
            for(size_type i = 0; i < len; ++i)
                std::cout<< dat[i] << ' ' << (dts ? &dts[i] : nullptr) << std::endl;               
            break;
        }

        try{
            long long i = std::stoll(index);
            if(i >= len){
                std::cerr<< std::endl << "BAD INPUT - index must be < array length" << std::endl;
                continue;
            }
            std::cout<< std::endl << dat[i] << ' ' << (dts ? &dts[i] : nullptr) << std::endl;             
        }catch(...){
            std::cerr<< std::endl << "BAD INPUT" << std::endl << std::endl;
            continue;
        }   

    }while(1);

    std::cout<< std::endl;
}

template<typename T>
void 
_get()
{  
    std::string block;
    std::string item;
    std::string topic;    
    long index;
    bool get_dts;

    prompt_for_block_item_topic_index(&block, &item, &topic, &index);
   
    get_dts = prompt_for_datetime(block); 
 
    if(get_dts)
        std::cout<< std::endl 
                 << TOSDB_Get<T,true>(block, item, TOS_Topics::MAP()[topic], index)
                 << std::endl << std::endl;    
    else 
        std::cout<< std::endl 
                 << TOSDB_Get<T,false>(block,item,TOS_Topics::MAP()[topic],index)
                 <<std::endl << std::endl;            
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
    long index;
    int ret;
    bool get_dts;

    prompt_for_block_item_topic_index(&block, &item, &topic, &index);

    get_dts = prompt_for_datetime(block);

    ret = func(block.c_str(), item.c_str(), topic.c_str(), index, &d, (get_dts ? &dts : nullptr));
    check_display_ret(ret, d, (get_dts ? &dts : nullptr));  

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
    bool get_dts;

    prompt_for_block_item_topic(&block, &item, &topic);      

    prompt<<"beginning datastream index: ";
    prompt>>beg;
    prompt<<"ending datastream index: ";
    prompt>>end;

    get_dts = prompt_for_datetime(block);

    if(get_dts)
        std::cout<< std::endl 
                 << TOSDB_GetStreamSnapshot<T,true>(block.c_str(), item.c_str(), 
                                                    TOS_Topics::MAP()[topic], end, beg);               
    else
        std::cout<< std::endl 
                 << TOSDB_GetStreamSnapshot<T,false>(block.c_str(), item.c_str(), 
                                                     TOS_Topics::MAP()[topic], end, beg)
                 << std::endl;

    std::cout<< std::endl;
}


template<typename T>
void 
_get_stream_snapshot( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*,
                                 size_type, pDateTimeStamp, long, long))
{
    size_type len;
    size_type display_len;
    long beg;
    long end;
    int ret;
    std::string block;
    std::string item;
    std::string topic;
    bool get_dts;

    pDateTimeStamp dts = nullptr;
    T *dat = nullptr;
   
    prompt_for_block_item_topic(&block, &item, &topic);

    prompt<<"length of array to pass: ";
    prompt>>len;
    prompt<<"beginning datastream index: ";
    prompt>>beg;
    prompt<<"ending datastream index: ";
    prompt>>end;
    
    get_dts = prompt_for_datetime(block);

    try{
        dat = new T[len];

        if(get_dts)
            dts = new DateTimeStamp[len];

        ret = func(block.c_str(), item.c_str(), topic.c_str(), dat, len, dts, end, beg);

        display_len = min_stream_len(block,beg,end,len);

        check_display_ret(ret, dat, display_len, dts);
 
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
    int ret;
    std::string block;
    std::string item;
    std::string topic;
    bool get_dts;

    pDateTimeStamp dts = nullptr;
    T *dat = nullptr;

    prompt_for_block_item_topic(&block, &item, &topic);

    prompt<<"length of array to pass: ";
    prompt>>len;
    prompt<<"beginning datastream index: ";
    prompt>>beg;

    get_dts = prompt_for_datetime(block);

    try{
        dat = new T[len];

        if(get_dts)
            dts = new DateTimeStamp[len];

        ret = func(block.c_str(), item.c_str(), topic.c_str(), dat, len, dts, beg, &get_size);

        check_display_ret(ret, dat, abs(get_size), dts);

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
    int ret;
    bool get_dts;

    pDateTimeStamp dts= nullptr;
    T *dat = nullptr;
    char** lab = nullptr;

    prompt<<"block id: ";
    prompt>> block;
    prompt<<"topic: ";
    prompt>> topic;    

    get_dts = prompt_for_datetime(block);

    TOSDB_GetItemCount(block.c_str(), &nitems);  

    try{
        dat = new T[nitems];         
        lab = NewStrings(nitems, TOSDB_MAX_STR_SZ);        

        if(get_dts)
            dts = new DateTimeStamp[nitems]; 

        ret = func(block.c_str(), topic.c_str() ,dat , nitems, lab, TOSDB_MAX_STR_SZ, dts);
        
        check_display_ret(ret, dat, lab, nitems, dts);

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


void 
check_display_ret(int r)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << "SUCCESS" << std::endl << std::endl; 
}


template<typename T>
void 
check_display_ret(int r, T v)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << v << std::endl << std::endl; 
}


template<typename T>
void 
check_display_ret(int r, T v, pDateTimeStamp d)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << v << ' ' << d << std::endl << std::endl; 
}


template<>
void 
check_display_ret(int r, bool v)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << std::boolalpha << v << std::endl << std::endl; 
}


template<typename T>
void 
check_display_ret(int r, T *v, size_type n)
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


template<typename T>
void 
check_display_ret(int r, T *v, size_type n, pDateTimeStamp d)
{
    if(r) 
        std::cout<< std::endl << "error: " << r << std::endl << std::endl;
    else 
        display_stream_data(n, v, d);
}


template<typename T>
void 
check_display_ret(int r, T *v, char **l, size_type n, pDateTimeStamp d)
{
    if(r) 
          std::cout<< std::endl << "error: " << r << std::endl << std::endl;
    else{
        std::cout<< std::endl;
        for(size_type i = 0; i < n; ++i)
            std::cout<< l[i] << ' ' << v[i] << ' ' << (d ? (d + i) : d) << std::endl;
        std::cout<< std::endl;
    }
}

template<int W, const char FILL>
void
display_header_line(std::string pre, std::string post, std::string text)
{
  int f = W - (pre.size() + post.size() + text.size()) - 1; // account for newline
  if(f < 0)
      throw std::exception("can't fit header line");
  
  std::cout<< pre << std::string(int(f/2), FILL) 
           << text << std::string(f - int(f/2), FILL) 
           << post << std::endl;
}

template<int W, int INDENT, char H>
void
display_header(std::string hpre, std::string hpost)
{
    char lpath[MAX_PATH+40+40];  
    TOSDB_GetClientLogPath(lpath,MAX_PATH+40+40);   

    display_header_line<W, H>(hpre,hpost, "");
    display_header_line<W, H>(hpre,hpost, "");
    display_header_line<W,' '>(hpre,hpost, "");
    display_header_line<W,' '>(hpre,hpost, "Welcome to the TOS-DataBridge Debug Shell");
    display_header_line<W,' '>(hpre,hpost, "Copyright (C) 2014 Jonathon Ogden");
    display_header_line<W,' '>(hpre,hpost, "");
    display_header_line<W, H>(hpre,hpost, "");
    display_header_line<W, H>(hpre,hpost, "");
    display_header_line<W,' '>(hpre,hpost, "");
    display_header_line<W,' '>(hpre,hpost, "This program is distributed WITHOUT ANY WARRANTY.");
    display_header_line<W,' '>(hpre,hpost, "See the GNU General Public License for more details.");
    display_header_line<W,' '>(hpre,hpost, "");
    display_header_line<W,' '>(hpre,hpost, "Use 'Connect' command to connect to the Service.");
    display_header_line<W,' '>(hpre,hpost, "Use 'commands' for a list of commands by category.");
    display_header_line<W,' '>(hpre,hpost, "Use 'topics' for a list of topics(fields) TOS accepts.");
    display_header_line<W,' '>(hpre,hpost, "Use 'exit' to exit.");
    display_header_line<W,' '>(hpre,hpost, "");
    display_header_line<W,' '>(hpre,hpost, "NOTE: Topics/Items are case sensitive; use upper-case");
    display_header_line<W,' '>(hpre,hpost, "");
    display_header_line<W, H>(hpre,hpost, "");           
    display_header_line<W, H>(hpre,hpost, "");
        
    int wc = INDENT;

    std::cout<< std::endl << std::setw(INDENT) << std::left << "LOG: ";    
    for(auto s : std::string(lpath)){
        if(++wc >= MAX_DISPLAY_WIDTH){
            std::cout<< std::endl << std::string(INDENT,' ') ;
            wc = INDENT;
        }
        std::cout<< s;
    }

    std::cout<< std::endl << std::endl << std::setw(INDENT) << std::left 
             << "PID: " << GetCurrentProcessId() << std::endl;

    on_cmd_topics(); 
}

};
