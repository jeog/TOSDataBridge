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

/* NOTE: this 'shell' was thrown together to test the interface of 
tos-databridge. It does not check input, format all output etc. etc. */

namespace{

std::string cmmnd, block, item, topic;
size_type size, nItems, nTopics;
char dtsB;
long indx;
pDateTimeStamp dtsRay;
char** itemArray;
char** topicArray;

class stream_prompt 
{
    std::string _prmpt;
public:
    stream_prompt( std::string prompt)
        :            
        _prmpt( prompt )
    {
    }
    template< typename T >
    std::ostream& operator<<(const T& val )
    {
        std::cout<<_prmpt<<' '<<val; 
        return std::cout;
    }
    template< typename T >
    std::istream& operator>>( T& val )
    {
        std::cout<<_prmpt<<' ';
        std::cin>>val;
        return std::cin;
    }
} prompt("[-->");

void get_block_item_topic();
void get_block_item_topic_indx();
void get_cstr_items();
void get_cstr_topics();
void del_cstr_items();
void del_cstr_topics();
template<typename T >
void deal_with_stream_data(size_type len, T* dataRay, bool show_dts);
template<typename T>
void _get();
template<typename T>
void _get( int(*func)(LPCSTR,LPCSTR,LPCSTR,long,T*,pDateTimeStamp) );
template<typename T>
void _getItemFrame( int(*func)(LPCSTR,LPCSTR,T*,size_type,LPSTR*,size_type,pDateTimeStamp) );
template<typename T>
void _getStreamSnapshot( int(*func)(LPCSTR,LPCSTR,LPCSTR,T*,size_type,pDateTimeStamp,long,long) );
template<typename T>
void _getStreamSnapshot();
template<typename T>
void _getStreamSnapshotFromMarker( int(*func)(LPCSTR,LPCSTR,LPCSTR,T*,size_type,pDateTimeStamp,long,long*) );

std::string admin_commands[] = 
{
    "AddDefault",
    "Connect", "Disconnect", "IsConnected",
    "CreateBlock", "CloseBlock", "CloseBlocks",
    "GetBlockLimit","SetBlockLimit",     
    "GetBlockCount",
    "GetBlockIDs","GetBlockIDsCPP",
    "GetBlockSize", "GetBlockSizeCPP", "SetBlockSize",
    "GetLatency", "SetLatency",
    "Add","AddTopic","AddItem","AddTopics","AddItems","AddCPP","AddTopicCPP","AddTopicsCPP","AddItemsCPP",
    "RemoveTopic","RemoveItem","RemoveTopicCPP",
    "GetItemCount","GetItemCountCPP","GetTopicCount","GetTopicCountCPP",
    "GetTopicNames","GetItemNames","GetTopicNamesCPP","GetItemNamesCPP","GetTopicEnumsCPP",
    "GetPreCachedItemNamesCPP","GetPreCachedTopicNamesCPP","GetPreCachedTopicEnums",
    "GetTypeBits","GetTypeString","IsUsingDateTime","IsUsingDateTimeCPP",
    "GetStreamOccupancy","GetStreamOccupancyCPP",
    "GetMarkerPosition","GetMarkerPositionCPP",
    "IsMarkerDirty","IsMarkerDirty",
    "DumpBufferStatus"
};
std::string get_val_commands[] = 
{ 
    "GetDouble","GetFloat","GetLongLong","GetLong","GetString",
    "Get<generic>", "Get<double>","Get<float>","Get<longlong>","Get<long>","Get<std::string>"
};
std::string get_ds_commands[] = 
{
    "GetStreamSnapshotDoubles",
    "GetStreamSnapshotFloats", 
    "GetStreamSnapshotLongLongs",
    "GetStreamSnapshotLongs",
    "GetStreamSnapshotStrings",
    "GetStreamSnapshot<generic>",
    "GetStreamSnapshot<double>",
    "GetStreamSnapshot<float>",
    "GetStreamSnapshot<long_long>",
    "GetStreamSnapshot<long>",
    "GetStreamSnapshot<std::string>",
    "GetStreamSnapshotDoublesFromMarker",
    "GetStreamSnapshotFloatsFromMarker", 
    "GetStreamSnapshotLongLongsFromMarker",
    "GetStreamSnapshotLongsFromMarker",
    "GetStreamSnapshotStringsFromMarker",
};
std::string get_frame_commands[] = 
{
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
};

int main( int argc, char* argv[])
{    
    std::cout<<"[-------------------------------------------------------------]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[--       Welcome to the TOS-DataBridge Debug Shell         --]" <<std::endl;
    std::cout<<"[--           Copyright (C) 2014 Jonathon Ogden             --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-------------------------------------------------------------]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-- This program is distributed WITHOUT ANY WARRANTY.       --]" <<std::endl;
    std::cout<<"[-- See the GNU General Public License for more details.    --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-- Use the 'Connect' command to connect to the Service.    --]" <<std::endl;
    std::cout<<"[-- Type 'commands' for a list of commands; 'exit' to exit. --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-- NOTE: Topics/Items are case sensitive; use upper-case   --]" <<std::endl;
    std::cout<<"[--                                                         --]" <<std::endl;
    std::cout<<"[-------------------------------------------------------------]" <<std::endl;
    std::cout<<std::endl;
    while(1)
    {        
        try
        {
            prompt>> cmmnd ;        
            if( cmmnd == "commands" )
            {
                std::string i_cmmnd;
                int count = 0;                    
                while(1)
                {
                    prompt<<"What type of command?"<<std::endl<<std::endl;;
                    std::cout<<"- admin"<<std::endl;
                    std::cout<<"- get"<<std::endl;
                    std::cout<<"- stream-snapshot"<<std::endl;
                    std::cout<<"- frame"<<std::endl;
                    std::cout<<"- back"<<std::endl<<std::endl;
                    prompt>> i_cmmnd;
                    std::cin.get();
                    if(i_cmmnd == "admin")
                    {                
                        std::cout<< "ADMINISTRATIVE COMMANDS:" <<std::endl<<std::endl;
                        for( auto & x : ILSet< std::string>(admin_commands) )
                        {            
                            std::cout<<"- "<< x << std::endl;
                            if( !(++count % 20) )
                            {                        
                                std::cout<<"(Press Enter)";
                                std::cin.get();
                                std::cout<< std::endl;
                            }                    
                        }
                        count = 0;
                        std::cout<< std::endl;
                        continue;
                    }
                    if(i_cmmnd == "get")
                    {    
                        std::cout<< "GET COMMANDS:" <<std::endl<<std::endl;
                        for( auto & x : ILSet< std::string>(get_val_commands) )                                
                            std::cout<<"- "<< x << std::endl;                
                        std::cout<< std::endl;
                        continue;
                    }
                    if(i_cmmnd == "stream-snapshot")
                    {    
                        std::cout<< "STREAM SNAPSHOT COMMANDS:" <<std::endl<<std::endl;
                        for( auto & x : ILSet< std::string>(get_ds_commands) )                                
                            std::cout<<"- "<< x << std::endl;                        
                        std::cout<< std::endl;
                        continue;
                    }
                    if(i_cmmnd == "frame")
                    {    
                        std::cout<< "FRAME COMMANDS:" <<std::endl<<std::endl;
                        for( auto & x : ILSet< std::string>(get_frame_commands) )                                
                            std::cout<<"- "<< x << std::endl;                    
                        std::cout<< std::endl;
                        continue;
                    }
                    if(i_cmmnd == "back")
                    {
                        break;
                    }
                    std::cout<< "BAD COMMAND!"<<std::endl;
                }            
                continue;
            }
            if( cmmnd == "Connect")
            {            
                int ret = TOSDB_Connect();
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<<"Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "Disconnect")
            {
                int ret = TOSDB_Disconnect();
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<<"Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "IsConnected")
            {
                std::cout<< std::boolalpha << ((TOSDB_IsConnected() == 1) ? true : false) << std::endl;
                continue;
            }
        
            if( cmmnd == "CreateBlock" )
            {
                unsigned long timeout;
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"block size: ";
                prompt>> size;
                prompt<<"use datetime stamp?(y/n): ";        
                std::cin.get();
                prompt>> dtsB ;
                prompt<<"timeout: ";
                prompt>> timeout;
                int ret =  TOSDB_CreateBlock( block.c_str(), size, dtsB == 'y' ? true : false, timeout );            
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<<"Success!"<<std::endl;
                continue;            
            }
            if( cmmnd == "CloseBlock" )
            {
                prompt<<"block id: ";
                prompt>> block;                    
                int ret =  TOSDB_CloseBlock( block.c_str() );
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<<"Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "CloseBlocks" )
            {
                int ret =  TOSDB_CloseBlocks();
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<<"Success!"<<std::endl;
                continue;
            }
        
            if( cmmnd == "GetBlockLimit" )
            {            
                std::cout<< TOSDB_GetBlockLimit() <<std::endl;
                continue;
            }
            if( cmmnd == "SetBlockLimit" )
            {        
                prompt<<"block limit: ";
                prompt>> size;
                std::cout<< TOSDB_SetBlockLimit( size )<<std::endl;
                continue;
            }
            if( cmmnd == "GetBlockCount" )
            {        
                std::cout<< TOSDB_GetBlockCount() <<std::endl;
                continue;
            }
            if( cmmnd == "GetBlockIDs" )
            {                            
                size_type bCount = TOSDB_GetBlockCount();
                char** strArray = AllocStrArray( bCount, TOSDB_BLOCK_ID_SZ );
                int ret = TOSDB_GetBlockIDs(strArray,bCount,TOSDB_BLOCK_ID_SZ);
                if(ret) std::cout<< "error: "<<ret<<std::endl;
                else
                    for(size_type i = 0; i < bCount; ++i)
                        std::cout<< strArray[i] <<std::endl;
                DeallocStrArray( strArray, bCount);
                continue;
            }
            if( cmmnd == "GetBlockIDsCPP" )
            {                                
                str_set_type ids = TOSDB_GetBlockIDs();
                for( const auto& str : ids )
                    std::cout<< str << std::endl;
                continue;
            }

            if( cmmnd == "GetBlockSize" )
            {
                prompt<<"block id: ";
                prompt>> block;
                size_type sz;
                int ret = TOSDB_GetBlockSize( block.c_str(), &sz );
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< sz <<std::endl;            
                continue;
            }
            if( cmmnd == "GetBlockSizeCPP" )
            {
                prompt<<"block id: ";
                prompt>> block;
                std::cout<< TOSDB_GetBlockSize( block ) <<std::endl;
                continue;
            }
            if( cmmnd == "SetBlockSize" )
            {
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"block size: ";
                prompt>> size;
                int ret = TOSDB_SetBlockSize( block.c_str(), size );
                if(ret)    std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< "Success!" <<std::endl;
                continue;
            }
            if( cmmnd == "GetLatency" )
            {            
                std::cout<< TOSDB_GetLatency() <<std::endl;
                continue;
            }
            if( cmmnd == "SetLatency" )
            {
                int lat;
                prompt<<"enter latency value(milleseconds)";
                prompt>> lat;                
                TOSDB_SetLatency((UpdateLatency)lat);
                continue;
            }
            if( cmmnd == "Add" )
            {            
                prompt<<"block id: "; 
                prompt>> block;        
                get_cstr_items();
                get_cstr_topics();            
                int ret = TOSDB_Add( block.c_str(), (const char**)itemArray, nItems, (const char**)topicArray, nTopics );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                del_cstr_items();
                del_cstr_topics();            
                continue;
            }
            if( cmmnd == "AddTopic" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"topic: ";
                prompt>> topic;        
                int ret =  TOSDB_AddTopic( block.c_str(), topic.c_str() );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "AddItem" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"item: ";
                prompt>> item;        
                int ret =  TOSDB_AddItem( block.c_str(), item.c_str() );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "AddTopics" )
            {            
                prompt<<"block id: ";
                prompt>> block;
                get_cstr_topics();            
                int ret = TOSDB_AddTopics( block.c_str(), (const char**)topicArray, nTopics );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                del_cstr_topics();
                continue;
            }
            if( cmmnd == "AddItems" )
            {                
                prompt<<"block id: ";
                prompt>> block;
                get_cstr_items();        
                int ret =  TOSDB_AddItems( block.c_str(), (const char**)itemArray, nItems );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                del_cstr_items();            
                continue;
            }
            if( cmmnd == "AddCPP" )
            {            
                prompt<<"block id: ";
                prompt>> block;        
                get_cstr_items();
                get_cstr_topics();            
                int ret =  TOSDB_Add( 
                            block, 
                            str_set_type(itemArray,nItems), 
                            topic_set_type(topicArray,nTopics,[=](LPCSTR str){ return TOS_Topics::map[str];}) );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                del_cstr_items();
                del_cstr_topics();            
                continue;
            }
            if( cmmnd == "AddTopicCPP" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"topic: ";
                prompt>> topic;        
                int ret =  TOSDB_AddTopic( block, TOS_Topics::map[topic] );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "AddTopicsCPP" )
            {            
                prompt<<"block id: ";
                prompt>> block;
                get_cstr_topics();            
                int ret = TOSDB_AddTopics( 
                            block, 
                            topic_set_type(topicArray, nTopics, 
                            [=](LPCSTR str){ return TOS_Topics::map[str]; }) );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                del_cstr_topics();
                continue;
            }
            if( cmmnd == "AddItemsCPP" )
            {                
                prompt<<"block id: ";
                prompt>> block;
                get_cstr_items();        
                int ret = TOSDB_AddItems(
                            block, 
                            str_set_type(itemArray, nItems) );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                del_cstr_items();            
                continue;
            }

            if( cmmnd == "RemoveTopic" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"topic: ";
                prompt>> topic;        
                int ret =  TOSDB_RemoveTopic( block.c_str(), topic.c_str() );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "RemoveItem" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"item: ";
                prompt>> item;        
                int ret =  TOSDB_RemoveItem( block.c_str(), item.c_str());
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                continue;
            }
            if( cmmnd == "RemoveTopicCPP" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"topic: ";
                prompt>> topic;        
                int ret =  TOSDB_RemoveTopic( block, TOS_Topics::map[topic] );
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< "Success!"<<std::endl;
                continue;
            }

            if( cmmnd == "GetItemCount" )
            {
                prompt<<"block id: ";
                prompt>> block;
                size_type itemLen;
                int ret = TOSDB_GetItemCount(block.c_str(), &itemLen);
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< itemLen<<std::endl;
                continue;
            }
            if( cmmnd == "GetItemCountCPP" )
            {
                prompt<<"block id: ";
                prompt>> block;
                std::cout<< TOSDB_GetItemCount(block) <<std::endl;
                continue;
            }
            if( cmmnd == "GetTopicCount" )
            {
                prompt<<"block id: ";
                prompt>> block;
                size_type topicLen;
                int ret = TOSDB_GetTopicCount(block.c_str(), &topicLen);
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else std::cout<< topicLen<<std::endl;
                continue;
            }
            if( cmmnd == "GetTopicCountCPP" )
            {
                prompt<<"block id: ";
                prompt>> block;
                std::cout<< TOSDB_GetTopicCount(block) <<std::endl;
                continue;
            }

            if( cmmnd == "GetTopicNames" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                size_type topicLen = TOSDB_GetTopicCount(block);
                char** strArray = AllocStrArray( topicLen, TOSDB_MAX_STR_SZ );
                int ret = TOSDB_GetTopicNames(block.c_str(),strArray,topicLen,TOSDB_MAX_STR_SZ);
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else 
                    for(size_type i = 0; i < topicLen; ++i)
                        std::cout<< strArray[i] <<std::endl;
                DeallocStrArray( strArray, topicLen);
                continue;
            }
            if( cmmnd == "GetItemNames" )
            {
                prompt<<"block id: ";
                prompt>> block;
                size_type itemLen = TOSDB_GetItemCount(block);
                char** strArray = AllocStrArray( itemLen, TOSDB_MAX_STR_SZ );
                int ret = TOSDB_GetItemNames(block.c_str(),strArray,itemLen,TOSDB_MAX_STR_SZ);
                if(ret) std::cout<< "error: "<< ret <<std::endl;
                else 
                    for(size_type i = 0; i < itemLen; ++i)
                        std::cout<< strArray[i] <<std::endl;
                DeallocStrArray( strArray, itemLen);
                continue;
            }
            if( cmmnd == "GetTopicNamesCPP" )
            {
                str_set_type topicSet;
                prompt<<"block id: ";
                prompt>> block;
                topicSet = TOSDB_GetTopicNames(block);
                for( auto & t : topicSet)
                    std::cout<< t << std::endl;
                continue;
            }
            if( cmmnd == "GetItemNamesCPP" )
            {        
                str_set_type itemSet;
                prompt<<"block id: ";
                prompt>> block;
                itemSet = TOSDB_GetItemNames(block);
                for( auto & i : itemSet)
                    std::cout<< i << std::endl;
                continue;
            }
            if( cmmnd == "GetTopicEnumsCPP" )
            {        
                prompt<<"block id: ";
                prompt>> block;            
                topic_set_type topSet = TOSDB_GetTopicEnums(block);
                for( auto & t : topSet)
                    std::cout<< (TOS_Topics::enum_type)t <<' '<< TOS_Topics::map[t] << std::endl;
                continue;
            }
            if( cmmnd == "GetPreCachedTopicNamesCPP" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                str_set_type topSet = TOSDB_GetPreCachedTopicNames(block);
                for( auto & t : topSet)
                    std::cout<< t << std::endl;
                continue;
            }
            if( cmmnd == "GetPreCachedItemNamesCPP" )
            {        
                prompt<<"block id: ";
                prompt>> block;
                str_set_type itemSet = TOSDB_GetPreCachedItemNames(block);
                for( auto & t : itemSet)
                    std::cout<< t << std::endl;
                continue;
            }
            if( cmmnd == "GetPreCachedTopicEnumsCPP" )
            {        
                prompt<<"block id: ";
                prompt>> block;            
                topic_set_type topSet = TOSDB_GetPreCachedTopicEnums(block);
                for( auto & t : topSet)
                    std::cout<< (TOS_Topics::enum_type)t << TOS_Topics::map[t] << std::endl;
                continue;
            }        
            if( cmmnd == "GetTypeBits" )
            {
                prompt<<"topic: ";
                prompt>> topic;
                type_bits_type bits;
                int ret = TOSDB_GetTypeBits( topic.c_str(), &bits); 
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< bits<<std::endl;
                continue;
            }
            if( cmmnd == "GetTypeString" )
            {
                char str[256];
                prompt<<"topic: ";
                prompt>> topic;
                int ret = TOSDB_GetTypeString( topic.c_str(), str, 256 );
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< str << std::endl;
                continue;
            }
            if( cmmnd == "GetTypeBitsCPP" )
            {
                prompt<<"topic: ";
                prompt>> topic;            
                std::cout<< TOSDB_GetTypeBits( TOS_Topics::map[topic] )<<std::endl;             
                continue;
            }
            if( cmmnd == "GetTypeStringCPP" )
            {            
                prompt<<"topic: ";
                prompt>> topic;
                std::cout<< TOSDB_GetTypeString( TOS_Topics::map[topic] )<<std::endl;            
                continue;
            }

            if( cmmnd == "IsUsingDateTime" )
            {            
                prompt<<"block id: ";
                prompt>> block;
                unsigned int b;
                int ret = TOSDB_IsUsingDateTime( block.c_str(), &b);
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< std::boolalpha << ((b==1) ? true : false) <<std::endl;
                continue;
            }
            if( cmmnd == "IsUsingDateTimeCPP" )
            {            
                prompt<<"block id: ";
                prompt>> block;
                std::cout<< std::boolalpha << TOSDB_IsUsingDateTime( block) <<std::endl;
                continue;
            }
            if( cmmnd == "GetStreamOccupancy" )
            {
                get_block_item_topic();        
                size_type sz;
                int ret = TOSDB_GetStreamOccupancy( block.c_str(), item.c_str(), topic.c_str(), &sz );
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< sz <<std::endl;
                continue;
            }
            if( cmmnd == "GetStreamOccupancyCPP" )
            {
                get_block_item_topic();        
                std::cout<< TOSDB_GetStreamOccupancy( block, item, TOS_Topics::map[topic] )<<std::endl;        
                continue;
            }
            if( cmmnd == "GetMarkerPosition" )
            {
                get_block_item_topic();        
                long long sz;
                int ret = TOSDB_GetMarkerPosition( block.c_str(), item.c_str(), topic.c_str(), &sz );
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< sz <<std::endl;
                continue;
            }
            if( cmmnd == "GetMarkerPositionCPP" )
            {
                get_block_item_topic();        
                std::cout<< TOSDB_GetMarkerPosition( block, item, TOS_Topics::map[topic] )<<std::endl;        
                continue;
            }
            if( cmmnd == "IsMarkerDirty" )
            {            
                get_block_item_topic(); 
                unsigned int b;
                int ret = TOSDB_IsMarkerDirty( block.c_str(), item.c_str(),topic.c_str(), &b);
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< std::boolalpha << ((b==1) ? true : false) <<std::endl;
                continue;
            }
            if( cmmnd == "IsMarkerDirty" )
            {            
                get_block_item_topic(); 
                std::cout<< std::boolalpha << TOSDB_IsMarkerDirty(block, item,TOS_Topics::map[topic]) <<std::endl;
                continue;
            }
            if( cmmnd == "DumpBufferStatus" )
            {                
                TOSDB_DumpSharedBufferStatus();
                continue;
            }

            if( cmmnd == "GetDouble")
            {
                _get( TOSDB_GetDouble );
                continue;
            }
            if( cmmnd == "GetFloat")
            {
                _get( TOSDB_GetFloat );
                continue;
            }
            if( cmmnd == "GetLongLong")
            {
                _get( TOSDB_GetLongLong );
                continue;
            }
            if( cmmnd == "GetLong")
            {
                _get( TOSDB_GetLong );
                continue;
            }
            if( cmmnd == "GetString")
            {
                char str[100];
                DateTimeStamp dts;
                get_block_item_topic_indx();
                int ret = TOSDB_GetString( block.c_str(),item.c_str(),topic.c_str(),indx,str,100,&dts);
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else std::cout<< str<<' '<<dts<<std::endl;
                continue;
            }

            if( cmmnd == "Get<generic>")
            {            
                _get< generic_type >();        
                continue; 
            }
            if( cmmnd == "Get<double>")
            {            
                _get< double >();        
                continue; 
            }
            if( cmmnd == "Get<float>")
            {            
                _get< float >();        
                continue; 
            }
            if( cmmnd == "Get<longlong>")
            {            
                _get< long long >();        
                continue; 
            }
            if( cmmnd == "Get<long>")
            {            
                _get< long >();        
                continue; 
            }
            if( cmmnd == "Get<std::string>")
            {            
                _get< std::string >();        
                continue; 
            }
            if( cmmnd == "GetStreamSnapshotDoubles" )
            {
                _getStreamSnapshot<double>( TOSDB_GetStreamSnapshotDoubles );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotFloats" )
            {
                _getStreamSnapshot<float>( TOSDB_GetStreamSnapshotFloats );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotLongLongs" )
            {
                _getStreamSnapshot<long long>( TOSDB_GetStreamSnapshotLongLongs );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotLongs" )
            {
                _getStreamSnapshot<long>( TOSDB_GetStreamSnapshotLongs );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotStrings" )
            {
                size_type len;
                long beg, end;
                get_block_item_topic();
                prompt<<"length of array to pass: ";
                prompt>>len;
                char** dataRay = AllocStrArray( len, TOSDB_STR_DATA_SZ - 1 );
                dtsRay = new DateTimeStamp[len];
                prompt<<"beginning datastream index: ";
                prompt>>beg;
                prompt<<"ending datastream index: ";
                prompt>>end;
                int ret = TOSDB_GetStreamSnapshotStrings( block.c_str(),item.c_str(),topic.c_str(),dataRay,len,TOSDB_STR_DATA_SZ,dtsRay,end,beg);
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else deal_with_stream_data(len, dataRay,false);
                DeallocStrArray( dataRay, len );
                delete dtsRay;
                continue;
            }

            if( cmmnd == "GetStreamSnapshot<double>" ) 
            {            
                _getStreamSnapshot< double >();
                continue;
            }        
            if( cmmnd == "GetStreamSnapshot<float>" ) 
            {            
                _getStreamSnapshot< float >();
                continue;
            }
            if( cmmnd == "GetStreamSnapshot<long_long>" ) 
            {            
                _getStreamSnapshot< long long >();
                continue;
            }
            if( cmmnd == "GetStreamSnapshot<long>" ) 
            {            
                _getStreamSnapshot< long >();
                continue;
            }
            if( cmmnd == "GetStreamSnapshot<std::string>" ) 
            {            
                _getStreamSnapshot< std::string >();
                continue;
            }
            if( cmmnd == "GetStreamSnapshot<generic>" ) 
            {            
                _getStreamSnapshot< generic_type >();
                continue;
            }

            /********/

            if( cmmnd == "GetStreamSnapshotDoublesFromMarker" )
            {
                _getStreamSnapshotFromMarker<double>( 
                    TOSDB_GetStreamSnapshotDoublesFromMarker );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotFloatsFromMarker" )
            {
                _getStreamSnapshotFromMarker<float>( 
                    TOSDB_GetStreamSnapshotFloatsFromMarker );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotLongLongsFromMarker" )
            {
                _getStreamSnapshotFromMarker<long long>( 
                    TOSDB_GetStreamSnapshotLongLongsFromMarker );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotLongsFromMarker" )
            {
                _getStreamSnapshotFromMarker<long>( 
                    TOSDB_GetStreamSnapshotLongsFromMarker );
                continue;
            }
            if( cmmnd == "GetStreamSnapshotStringsFromMarker" )
            {
                size_type len;
                long beg, get_size;
                get_block_item_topic();
                prompt<<"length of array to pass: ";
                prompt>>len;
                char** dataRay = AllocStrArray( len, TOSDB_STR_DATA_SZ - 1 );
                dtsRay = new DateTimeStamp[len];
                prompt<<"beginning datastream index: ";
                prompt>>beg;               
                int ret = TOSDB_GetStreamSnapshotStringsFromMarker( 
                    block.c_str(),item.c_str(),topic.c_str(),dataRay,len,
                    TOSDB_STR_DATA_SZ,dtsRay,beg, &get_size );
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else deal_with_stream_data(len, dataRay,false);
                DeallocStrArray( dataRay, len );
                delete dtsRay;
                continue;
            }

            /*******/

            if( cmmnd == "GetItemFrame" ) 
            {            
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"topic: ";
                prompt>> topic;    
                prompt<<"get datetime stamp?(y/n): ";        
                std::cin.get();
                prompt>> dtsB ;
                if( dtsB == 'y')
                    std::cout<< TOSDB_GetItemFrame<true>( block,TOS_Topics::map[topic]);
                else
                    std::cout<< TOSDB_GetItemFrame<false>( block,TOS_Topics::map[topic]);                    
                std::cout<<std::endl;
                continue;
            }
            if( cmmnd == "GetItemFrameDoubles" )
            {    
                _getItemFrame( TOSDB_GetItemFrameLongs );            
                continue;
            }
            if( cmmnd == "GetItemFrameFloats" )
            {    
                _getItemFrame( TOSDB_GetItemFrameLongs );            
                continue;
            }
            if( cmmnd == "GetItemFrameLongLongs" )
            {    
                _getItemFrame( TOSDB_GetItemFrameLongs );            
                continue;
            }
            if( cmmnd == "GetItemFrameLongs" )
            {    
                _getItemFrame( TOSDB_GetItemFrameLongs );            
                continue;
            }
            if( cmmnd == "GetItemFrameStrings" )
            {    
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"topic: ";
                prompt>> topic;        
                size_type nItems;
                TOSDB_GetItemCount(block.c_str(), &nItems);
                char** dataRay = AllocStrArray( nItems, TOSDB_STR_DATA_SZ - 1);
                char** labelRay = AllocStrArray( nItems, TOSDB_STR_DATA_SZ - 1);        
                dtsRay = new DateTimeStamp[ nItems ];
                int ret = TOSDB_GetItemFrameStrings( block.c_str(),topic.c_str(),dataRay,nItems,TOSDB_STR_DATA_SZ,labelRay,TOSDB_STR_DATA_SZ,dtsRay);
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else 
                    for(size_type i = 0; i < nItems; ++i)
                        std::cout<<' '<<labelRay[i]<<' '<<dataRay[i]<<' '<<dtsRay[i]<<std::endl;
                DeallocStrArray( dataRay, nItems );
                DeallocStrArray( labelRay, nItems );
                delete dtsRay;
                continue;            
            }    

            if( cmmnd == "GetTopicFrameStrings" )
            {                
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"item: ";
                prompt>> item;        
                size_type nItems;
                TOSDB_GetItemCount(block.c_str(), &nItems);
                char** dataRay = AllocStrArray( nTopics, TOSDB_STR_DATA_SZ -1 );
                char** labelRay = AllocStrArray( nTopics, TOSDB_STR_DATA_SZ - 1 );    
                dtsRay = new DateTimeStamp[ nTopics ];
                int ret = TOSDB_GetTopicFrameStrings( block.c_str(),item.c_str(),dataRay,nTopics,TOSDB_STR_DATA_SZ,labelRay,TOSDB_STR_DATA_SZ,dtsRay);
                if(ret) std::cout<<"error: "<<ret<<std::endl;
                else
                    for(size_type i = 0; i < nTopics; ++i)
                        std::cout<<labelRay[i]<<' '<<dataRay[i]<<' '<<dtsRay[i]<<std::endl;
                DeallocStrArray( dataRay, nTopics );
                DeallocStrArray( labelRay, nTopics );
                delete dtsRay;
                continue;            
            }

            if( cmmnd == "GetTopicFrame" ) 
            {            
                prompt<<"block id: ";
                prompt>> block;
                prompt<<"item: ";
                prompt>> item;    
                prompt<<"get datetime stamp?(y/n): ";        
                std::cin.get();
                prompt>> dtsB ;
                if( dtsB == 'y')            
                    std::cout<< TOSDB_GetTopicFrame<true>( block,item) <<std::endl;            
                else
                    std::cout<< TOSDB_GetTopicFrame<false>( block,item) <<std::endl;                                    
            
                continue;
            }
            if( cmmnd == "GetTotalFrame" )
            {
                prompt<<"block id:";
                prompt>>block;
                prompt<<"get datetime stamp?(y/n): ";
                std::cin.get();
                prompt>> dtsB;
                if( dtsB == 'y')
                    std::cout<< TOSDB_GetTotalFrame<true>( block ) <<std::endl;
                else
                    std::cout<< TOSDB_GetTotalFrame<false>( block )<<std::endl;
                continue;
            }
        
            if( cmmnd == "exit" || cmmnd == "quit" || cmmnd == "close" )
            {
                break;
            }
            std::cout<< "BAD COMMAND!"<<std::endl;
        }
        catch(const TOSDB_Error& e)
        {            
            std::cerr<< "!!! TOSDB_Error caught by shell !!!" << std::endl;
            std::cerr<< "Process ID: "<< e.processID() <<std::endl;
            std::cerr<< "Thread ID: "<< e.threadID() <<std::endl;
            std::cerr<< "tag: " << e.tag() <<std::endl;
            std::cerr<< "info: " << e.info() <<std::endl;
            std::cerr<< "what: " << e.what() <<std::endl;
            prompt<<"try to continue?(y/n): ";
            std::cin.get();
            prompt>> dtsB;
            if( dtsB == 'y')
                continue;
            else
                throw;
        }
        catch(...)
        { //
            throw;
        }
    }        
    return 0;
}

namespace{

void get_block_item_topic()
{
    prompt<<"block id: ";
    prompt>> block;
    prompt<<"item: ";
    prompt>> item;
    prompt<<"topic: ";
    prompt>> topic;
}
void get_block_item_topic_indx()
{
    get_block_item_topic();
    prompt<<"indx: ";
    prompt>> indx;
}
template<typename T >
void deal_with_stream_data(size_type len, T* dataRay, bool show_dts)
{
    std::string indx_inpt;
    char loopA = 'n';
    do
    {
        prompt<< "Show data from what index value?('all' to show all): ";
        prompt>> indx_inpt;
        if( indx_inpt == "all")
        {
            for(size_type i = 0; i < len; ++i){
                if( show_dts )
                    std::cout<<dataRay[i]<<' '<< dtsRay[i] <<std::endl;
                else
                    std::cout<<dataRay[i]<<' ' <<std::endl;
            }
            break;
        }
        else
        {
            try
            {
                long long ind1 = std::stoll( indx_inpt);
                if( show_dts )
                    std::cout<<dataRay[ind1]<<' '<< dtsRay[ind1] <<std::endl;
                else
                    std::cout<<dataRay[ind1]<<' ' <<std::endl;
            }
            catch(...)
            {
                std::cerr<<"BAD INPUT";
                continue;
            }            
        }
        std::cout<<std::endl;
        prompt<<"Show more data?(y/n): ";
        prompt>>loopA;
    }while( loopA == 'y' );
}
void get_cstr_items()
{    
    prompt<<"how many items? ";
    prompt>> nItems;
    itemArray = AllocStrArray( nItems, TOSDB_MAX_STR_SZ );
    for(size_type i = 0; i < nItems; ++i)
    {
        prompt<<"item #"<<i+1<<": ";
        prompt>> item;        
        strcpy_s( itemArray[i],item.length() + 1,item.c_str() );
    }
}
void get_cstr_topics()
{    
    prompt<<"how many topics? ";
    prompt>> nTopics;
    topicArray = AllocStrArray( nTopics, TOSDB_MAX_STR_SZ );
    for(size_type i = 0; i < nTopics; ++i)
    {
        prompt<<"topic #"<<i+1<<": ";
        prompt>> topic;    
        strcpy_s(topicArray[i],topic.length() + 1, topic.c_str());
    }
}
void del_cstr_items()
{
    DeallocStrArray( itemArray, nItems );
}
void del_cstr_topics()
{
    DeallocStrArray( topicArray, nTopics );
}

template<typename T>
void _get()
{    
    get_block_item_topic_indx();        
    prompt<<"get datetime stamp?(y/n): ";        
    std::cin.get();
    prompt>> dtsB ;    
    if( dtsB == 'y')
        std::cout<< TOSDB_Get<T,true>( block, item, TOS_Topics::map[topic], indx)<<std::endl;        
    else
        std::cout<< TOSDB_Get<T,false>( block.c_str(),item.c_str(),TOS_Topics::map[topic],indx)<<std::endl;                        
}
template<typename T>
void _get( int(*func)(LPCSTR,LPCSTR,LPCSTR,long,T*,pDateTimeStamp) )
{
    T d;
    DateTimeStamp dts;
    get_block_item_topic_indx();
    prompt<<"get datetime stamp?(y/n): ";        
    std::cin.get();
    prompt>> dtsB ;
    if( dtsB == 'y')
    {
        int ret = func( block.c_str(),item.c_str(),topic.c_str(),indx,&d,&dts);
        if(ret) std::cout<<"error: "<<ret<<std::endl;
        else std::cout<<d<<' '<<dts<<std::endl;                    
    }
    else
    {
        int ret = func( block.c_str(),item.c_str(),topic.c_str(),indx,&d,nullptr);
        if(ret) std::cout<<"error: "<<ret<<std::endl;
        else std::cout<<d<<std::endl;        
    }
    
}

template<typename T>
void _getStreamSnapshot()
{
    long beg, end;
    get_block_item_topic();            
    prompt<<"beginning datastream index: ";
    prompt>>beg;
    prompt<<"ending datastream index: ";
    prompt>>end;
    prompt<<"get datetime stamp?(y/n): ";        
    std::cin.get();
    prompt>> dtsB ;
    if( dtsB == 'y')            
        std::cout<<TOSDB_GetStreamSnapshot<T,true>( block.c_str(),item.c_str(),TOS_Topics::map[topic],end,beg);
    else
        std::cout<<TOSDB_GetStreamSnapshot<T,false>( block.c_str(),item.c_str(),TOS_Topics::map[topic],end,beg);                        
    std::cout<<std::endl;
}
template<typename T>
void _getStreamSnapshot( int(*func)(LPCSTR,LPCSTR,LPCSTR,T*,size_type,pDateTimeStamp,long,long) )
{
    size_type len;
    long beg, end;
    get_block_item_topic();
    prompt<<"length of array to pass: ";
    prompt>>len;
    T* dataRay = new T[len];
    dtsRay = new DateTimeStamp[len];
    prompt<<"beginning datastream index: ";
    prompt>>beg;
    prompt<<"ending datastream index: ";
    prompt>>end;
    prompt<<"get datetime stamp?(y/n): ";        
    std::cin.get();
    prompt>> dtsB ;
    if( dtsB == 'y' )
    {
        int ret = func( block.c_str(),item.c_str(),topic.c_str(),dataRay,len,dtsRay,end,beg);
        if(ret) std::cout<<"error: "<<ret<<std::endl;
        else deal_with_stream_data(len, dataRay, true);
    }
    else
    {
        int ret = func( block.c_str(),item.c_str(),topic.c_str(),dataRay,len,nullptr,end,beg);
        if(ret) std::cout<<"error: "<<ret<<std::endl;
        else deal_with_stream_data(len, dataRay, false);
    }    
    delete[] dataRay;
    delete[] dtsRay;
}

/******/

template<typename T>
void _getStreamSnapshotFromMarker( int(*func)(LPCSTR,LPCSTR,LPCSTR,T*,size_type,pDateTimeStamp,long,long*) )
{
    size_type len;
    long beg, get_size;
    get_block_item_topic();
    prompt<<"length of array to pass: ";
    prompt>>len;
    T* dataRay = new T[len];
    dtsRay = new DateTimeStamp[len];
    prompt<<"beginning datastream index: ";
    prompt>>beg;
    prompt<<"get datetime stamp?(y/n): ";        
    std::cin.get();
    prompt>> dtsB ;
    if( dtsB == 'y' )
    {
        int ret = func( block.c_str(),item.c_str(),topic.c_str(),dataRay,len,dtsRay,beg,&get_size);
        if(ret) std::cout<<"error: "<<ret<<std::endl;
        else deal_with_stream_data(len, dataRay, true);
    }
    else
    {
        int ret = func( block.c_str(),item.c_str(),topic.c_str(),dataRay,len,nullptr,beg,&get_size);
        if(ret) std::cout<<"error: "<<ret<<std::endl;
        else deal_with_stream_data(len, dataRay, false);
    }    
    delete[] dataRay;
    delete[] dtsRay;
}

/*****/


template<typename T>
void _getItemFrame( int(*func)(LPCSTR,LPCSTR,T*,size_type,LPSTR*,size_type,pDateTimeStamp) )
{
    prompt<<"block id: ";
    prompt>> block;
    prompt<<"topic: ";
    prompt>> topic;        
    size_type nItems;
    TOSDB_GetItemCount(block.c_str(), &nItems);
    T* dataRay = new T[ nItems ];
    char** labelRay = new char*[nItems];
    for(size_type i = 0; i < nItems; ++i)
    {            
        labelRay[i] = new char[TOSDB_MAX_STR_SZ];
    }
    dtsRay = new DateTimeStamp[ nItems ];
    int ret = func( block.c_str(),topic.c_str(),dataRay,nItems,labelRay,TOSDB_MAX_STR_SZ,dtsRay);
    if(ret) std::cout<<"error: "<<ret<<std::endl;
    else
        for(size_type i = 0; i < nItems; ++i)
            std::cout<<labelRay[i]<<' '<<dataRay[i]<<' '<<dtsRay[i]<<std::endl;
    while(nItems--)                
        delete[] labelRay[nItems];
    delete[] dataRay;
    delete[] labelRay;

}
};
