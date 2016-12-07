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

void GetItemFrame(CommandCtx *ctx);
void GetItemFrameStrings(CommandCtx *ctx);
void GetItemFrameDoubles(CommandCtx *ctx);
void GetItemFrameFloats(CommandCtx *ctx);
void GetItemFrameLongLongs(CommandCtx *ctx);
void GetItemFrameLongs(CommandCtx *ctx);
void GetTopicFrame(CommandCtx *ctx);
void GetTopicFrameStrings(CommandCtx *ctx);
void GetTotalFrame(CommandCtx *ctx);


}; /* namespace */

const CommandsMap commands_frame(
    CommandsMap::InitChain("GetItemFrame",GetItemFrame) 
                          ("GetItemFrameStrings",GetItemFrameStrings) 
                          ("GetItemFrameDoubles",GetItemFrameDoubles) 
                          ("GetItemFrameFloats",GetItemFrameFloats) 
                          ("GetItemFrameLongLongs",GetItemFrameLongLongs) 
                          ("GetItemFrameLongs",GetItemFrameLongs) 
                          ("GetTopicFrame",GetTopicFrame) 
                          ("GetTopicFrameStrings",GetTopicFrameStrings) 
                          ("GetTotalFrame",GetTotalFrame) 
);


namespace{

template<typename T>
void 
_get_item_frame( int(*func)(LPCSTR, LPCSTR, T*, size_type,
                            LPSTR*,size_type,pDateTimeStamp),
                 CommandCtx *ctx );

template<typename T>
void 
_check_display_ret(int r, T *v, char **l, size_type n, pDateTimeStamp d);


void
GetItemFrame(CommandCtx *ctx)
{      
    std::string block;
    std::string topic;
    bool get_dts;

    prompt_for("block id", &block, ctx);
    prompt_for("topic", &topic, ctx);  

    get_dts = prompt_for_datetime(block, ctx);

    if(get_dts)
        std::cout<< std::endl << TOSDB_GetItemFrame<true>(block,TOS_Topics::MAP()[topic]);
    else
        std::cout<< std::endl << TOSDB_GetItemFrame<false>(block,TOS_Topics::MAP()[topic]);          
       
    std::cout<< std::endl;
}


void
GetItemFrameDoubles(CommandCtx *ctx)
{
    _get_item_frame(TOSDB_GetItemFrameDoubles, ctx);      
}

       
void
GetItemFrameFloats(CommandCtx *ctx)
{
    _get_item_frame(TOSDB_GetItemFrameFloats, ctx);      
}


void
GetItemFrameLongLongs(CommandCtx *ctx)
{  
    _get_item_frame(TOSDB_GetItemFrameLongLongs, ctx);      
}


void
GetItemFrameLongs(CommandCtx *ctx)
{
    _get_item_frame(TOSDB_GetItemFrameLongs, ctx);      
}


void
GetItemFrameStrings(CommandCtx *ctx)
{
    size_type nitems;
    std::string block;
    std::string topic;
    int ret;
    bool get_dts;
                
    char **dat = nullptr;
    char **lab = nullptr;
    pDateTimeStamp dts = nullptr;

    prompt_for("block id", &block, ctx);
    prompt_for("topic", &topic, ctx);   
        
    get_dts = prompt_for_datetime(block,ctx);

    TOSDB_GetItemCount(block.c_str(), &nitems);
        
    try{
        dat = NewStrings(nitems, TOSDB_STR_DATA_SZ - 1);
        lab = NewStrings(nitems, TOSDB_STR_DATA_SZ - 1);    

        if(get_dts)
            dts = new DateTimeStamp[ nitems ];

        ret = TOSDB_GetItemFrameStrings(block.c_str(), topic.c_str(), dat, nitems, 
                                        TOSDB_STR_DATA_SZ, lab, TOSDB_STR_DATA_SZ, dts);

        _check_display_ret(ret, dat, lab, nitems, dts);
                      
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
GetTopicFrameStrings(CommandCtx *ctx)
{        
    size_type ntopics;
    std::string block;
    std::string item;
    int ret;
    bool get_dts;

    char** dat = nullptr;
    char** lab = nullptr;
    pDateTimeStamp dts = nullptr;

    prompt_for("block id", &block, ctx);
    prompt_for("item", &item, ctx);  
        
    get_dts = prompt_for_datetime(block, ctx);

    TOSDB_GetTopicCount(block.c_str(), &ntopics);

    try{       
        dat = NewStrings(ntopics, TOSDB_STR_DATA_SZ -1);
        lab = NewStrings(ntopics, TOSDB_STR_DATA_SZ - 1);  

        if(get_dts)
            dts = new DateTimeStamp[ntopics];
                 
        ret = TOSDB_GetTopicFrameStrings(block.c_str(), item.c_str(), dat, ntopics, 
                                          TOSDB_STR_DATA_SZ, lab, TOSDB_STR_DATA_SZ, dts);
            
        _check_display_ret(ret, dat, lab, ntopics, dts);

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
GetTopicFrame(CommandCtx *ctx)
{
    std::string block;
    std::string item;
    bool get_dts;

    prompt_for("block id", &block, ctx);
    prompt_for("item", &item, ctx);
    
    get_dts = prompt_for_datetime(block, ctx);

    if(get_dts)      
        std::cout<< std::endl << TOSDB_GetTopicFrame<true>(block,item);      
    else
        std::cout<< std::endl << TOSDB_GetTopicFrame<false>(block,item); 
  
    std::cout<< std::endl;
}


void
GetTotalFrame(CommandCtx *ctx)
{
    std::string block; 
    bool get_dts;

    prompt_for("block id", &block, ctx);

    get_dts = prompt_for_datetime(block, ctx);

    if(get_dts)
        std::cout<< std::endl << TOSDB_GetTotalFrame<true>(block);
    else
        std::cout<< std::endl << TOSDB_GetTotalFrame<false>(block);
      
    std::cout<< std::endl;                         
}


template<typename T>
void 
_get_item_frame( int(*func)(LPCSTR, LPCSTR, T*, size_type,
                            LPSTR*, size_type, pDateTimeStamp),
                 CommandCtx *ctx )
{
    std::string block;
    std::string topic;
    size_type nitems;
    int ret;
    bool get_dts;

    pDateTimeStamp dts= nullptr;
    T *dat = nullptr;
    char** lab = nullptr;

    prompt_for("block id", &block, ctx);
    prompt_for("topic", &topic, ctx);

    get_dts = prompt_for_datetime(block,ctx);

    TOSDB_GetItemCount(block.c_str(), &nitems);  

    try{
        dat = new T[nitems];         
        lab = NewStrings(nitems, TOSDB_MAX_STR_SZ);        

        if(get_dts)
            dts = new DateTimeStamp[nitems]; 

        ret = func(block.c_str(), topic.c_str() ,dat , nitems, lab, TOSDB_MAX_STR_SZ, dts);
        
        _check_display_ret(ret, dat, lab, nitems, dts);

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


template<typename T>
void 
_check_display_ret(int r, T *v, char **l, size_type n, pDateTimeStamp d)
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

}; /* namespace */