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

void GetItemFrame(void *ctx=nullptr);
void GetItemFrameStrings(void *ctx=nullptr);
void GetItemFrameDoubles(void *ctx=nullptr);
void GetItemFrameFloats(void *ctx=nullptr);
void GetItemFrameLongLongs(void *ctx=nullptr);
void GetItemFrameLongs(void *ctx=nullptr);
void GetTopicFrame(void *ctx=nullptr);
void GetTopicFrameStrings(void *ctx=nullptr);
void GetTotalFrame(void *ctx=nullptr);

commands_map_ty
build_commands_map_frame()
{
    commands_map_ty m;

    m.insert( commands_map_elem_ty("GetItemFrame",GetItemFrame) );
    m.insert( commands_map_elem_ty("GetItemFrameStrings",GetItemFrameStrings) );
    m.insert( commands_map_elem_ty("GetItemFrameDoubles",GetItemFrameDoubles) );
    m.insert( commands_map_elem_ty("GetItemFrameFloats",GetItemFrameFloats) );
    m.insert( commands_map_elem_ty("GetItemFrameLongLongs",GetItemFrameLongLongs) );
    m.insert( commands_map_elem_ty("GetItemFrameLongs",GetItemFrameLongs) );
    m.insert( commands_map_elem_ty("GetTopicFrame",GetTopicFrame) );
    m.insert( commands_map_elem_ty("GetTopicFrameStrings",GetTopicFrameStrings) );
    m.insert( commands_map_elem_ty("GetTotalFrame",GetTotalFrame) );

    return m;
}

};


commands_map_ty commands_frame = build_commands_map_frame();


namespace{

template<typename T>
void 
_get_item_frame( int(*func)(LPCSTR, LPCSTR, T*, size_type,
                            LPSTR*,size_type,pDateTimeStamp) );


void
GetItemFrame(void *ctx)
{      
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
}


void
GetItemFrameDoubles(void *ctx)
{
    _get_item_frame(TOSDB_GetItemFrameDoubles);      
}

       
void
GetItemFrameFloats(void *ctx)
{
    _get_item_frame(TOSDB_GetItemFrameFloats);      
}


void
GetItemFrameLongLongs(void *ctx)
{  
    _get_item_frame(TOSDB_GetItemFrameLongLongs);      
}


void
GetItemFrameLongs(void *ctx)
{
    _get_item_frame(TOSDB_GetItemFrameLongs);      
}


void
GetItemFrameStrings(void *ctx)
{
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
}


void
GetTopicFrameStrings(void *ctx)
{        
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
}


void
GetTopicFrame(void *ctx)
{
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
}


void
GetTotalFrame(void *ctx)
{
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

};