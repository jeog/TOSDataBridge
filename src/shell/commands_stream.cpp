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

#include <algorithm>

#include "shell.hpp"

namespace{
  
void GetStreamSnapshotDoubles(CommandCtx *ctx);
void GetStreamSnapshotFloats(CommandCtx *ctx); 
void GetStreamSnapshotLongLongs(CommandCtx *ctx);
void GetStreamSnapshotLongs(CommandCtx *ctx);
void GetStreamSnapshotStrings(CommandCtx *ctx);
void GetStreamSnapshotGenerics(CommandCtx *ctx);
void GetStreamSnapshotDoublesFromMarker(CommandCtx *ctx);
void GetStreamSnapshotFloatsFromMarker(CommandCtx *ctx); 
void GetStreamSnapshotLongLongsFromMarker(CommandCtx *ctx);
void GetStreamSnapshotLongsFromMarker(CommandCtx *ctx);
void GetStreamSnapshotStringsFromMarker(CommandCtx *ctx);

}; /* namespace */

CommandsMap commands_stream(
    CommandsMap::InitChain("GetStreamSnapshotDoubles",GetStreamSnapshotDoubles) 
                          ("GetStreamSnapshotFloats",GetStreamSnapshotFloats) 
                          ("GetStreamSnapshotLongLongs",GetStreamSnapshotLongLongs) 
                          ("GetStreamSnapshotLongs",GetStreamSnapshotLongs) 
                          ("GetStreamSnapshotStrings",GetStreamSnapshotStrings) 
                          ("GetStreamSnapshotGenerics",GetStreamSnapshotGenerics) 
                          ("GetStreamSnapshotDoublesFromMarker",GetStreamSnapshotDoublesFromMarker) 
                          ("GetStreamSnapshotFloatsFromMarker",GetStreamSnapshotFloatsFromMarker) 
                          ("GetStreamSnapshotLongLongsFromMarker",GetStreamSnapshotLongLongsFromMarker) 
                          ("GetStreamSnapshotLongsFromMarker",GetStreamSnapshotLongsFromMarker) 
                          ("GetStreamSnapshotStringsFromMarker",GetStreamSnapshotStringsFromMarker)
);


namespace{

template<typename T>
void 
_get_stream_snapshot( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*, 
                                 size_type, pDateTimeStamp, long, long),
                      CommandCtx *ctx );

template<typename T>
void _get_stream_snapshot(CommandCtx *ctx);

template<typename T>
void 
_get_stream_snapshot_from_marker( int(*func)(LPCSTR, LPCSTR, LPCSTR, T*,
                                             size_type, pDateTimeStamp, long, long*),
                                  CommandCtx *ctx );

template<typename T>
void 
_check_display_ret(int r, T *v, size_type n, pDateTimeStamp d);

template<typename T>
void 
_display_stream_data(size_type len, T* dat, pDateTimeStamp dts);

size_type
_min_stream_len(std::string block, long beg, long end, size_type len);


void
GetStreamSnapshotDoubles(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) 
        ? _get_stream_snapshot<double>(ctx) 
        : _get_stream_snapshot<double>(TOSDB_GetStreamSnapshotDoubles, ctx);
}


void
GetStreamSnapshotFloats(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) 
        ? _get_stream_snapshot<float>(ctx) 
        : _get_stream_snapshot<float>(TOSDB_GetStreamSnapshotFloats, ctx);
}


void
GetStreamSnapshotLongLongs(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) 
        ? _get_stream_snapshot<long long>(ctx) 
        : _get_stream_snapshot<long long>(TOSDB_GetStreamSnapshotLongLongs, ctx);
}


void
GetStreamSnapshotLongs(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) 
        ? _get_stream_snapshot<long>(ctx) 
        : _get_stream_snapshot<long>(TOSDB_GetStreamSnapshotLongs, ctx);
}


void
GetStreamSnapshotStrings(CommandCtx *ctx)
{
    if(prompt_for_cpp(ctx)){
        _get_stream_snapshot<std::string>(ctx);
    }else{        
        size_type display_len;           
        int ret;
        std::string block;
        std::string item;
        std::string topic;
        std::string len_s;
        std::string beg_s;
        std::string end_s;
        bool get_dts;
                        
        char **dat = nullptr;
        pDateTimeStamp dts = nullptr;

        prompt_for_block_item_topic(&block, &item, &topic, ctx);
        prompt_for("length of array", &len_s, ctx);        
        prompt_for("beginning datastream index", &beg_s, ctx);        
        prompt_for("ending datastream index", &end_s, ctx);

        size_type len = 0;
        long beg = 0;
        long end = 0;
        try{
            len = std::stoul(len_s);
            beg = std::stol(beg_s);
            end = std::stol(end_s);
        }catch(...){
            std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
            return;
        }
        
        get_dts = prompt_for_datetime(block, ctx);

        try{
            dat = NewStrings(len, TOSDB_STR_DATA_SZ - 1);

            if(get_dts)
                dts = new DateTimeStamp[len];

            ret = TOSDB_GetStreamSnapshotStrings(block.c_str(), item.c_str(), topic.c_str(),
                                                  dat, len, TOSDB_STR_DATA_SZ, dts, end, beg);
           
            display_len = _min_stream_len(block,beg,end,len);

            _check_display_ret(ret, dat, display_len, dts);

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
}


void
GetStreamSnapshotGenerics(CommandCtx *ctx)
{
    _get_stream_snapshot<generic_type>(ctx);
}


void
GetStreamSnapshotDoublesFromMarker(CommandCtx *ctx)
{
    _get_stream_snapshot_from_marker<double>(TOSDB_GetStreamSnapshotDoublesFromMarker, ctx);
}


void
GetStreamSnapshotFloatsFromMarker(CommandCtx *ctx)
{
    _get_stream_snapshot_from_marker<float>(TOSDB_GetStreamSnapshotFloatsFromMarker, ctx);
}


void
GetStreamSnapshotLongLongsFromMarker(CommandCtx *ctx)
{
    _get_stream_snapshot_from_marker<long long>(TOSDB_GetStreamSnapshotLongLongsFromMarker, ctx);
}


void
GetStreamSnapshotLongsFromMarker(CommandCtx *ctx)
{
    _get_stream_snapshot_from_marker<long>(TOSDB_GetStreamSnapshotLongsFromMarker, ctx);
}


void
GetStreamSnapshotStringsFromMarker(CommandCtx *ctx)
{    
    long get_size;
    int ret;
    std::string block;
    std::string item;
    std::string topic;
    std::string len_s;
    std::string beg_s;
    bool get_dts;

    char **dat = nullptr;
    pDateTimeStamp dts= nullptr;

    prompt_for_block_item_topic(&block, &item, &topic, ctx);

    prompt_for("length of array", &len_s, ctx);        
    prompt_for("beginning datastream index", &beg_s, ctx);        
      
    size_type len = 0;
    long beg = 0;      
    try{
        len = std::stoul(len_s);
        beg = std::stol(beg_s);      
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        return;
    }

    get_dts = prompt_for_datetime(block, ctx);

    try{         
        dat = NewStrings(len, TOSDB_STR_DATA_SZ - 1);

        if(get_dts)
            dts = new DateTimeStamp[len];

        ret = TOSDB_GetStreamSnapshotStringsFromMarker(block.c_str(), item.c_str(), topic.c_str(), 
                                                        dat, len, TOSDB_STR_DATA_SZ, 
                                                        dts, beg, &get_size);

        _check_display_ret(ret, dat, abs(get_size), dts);

        DeleteStrings(dat, len);
        if(dts)
            delete dts;
    }catch(...){
        DeleteStrings(dat, len);
        if(dts)
            delete dts;
    }       
}



template<typename T>
void 
_get_stream_snapshot(CommandCtx *ctx)
{  
    std::string block;
    std::string item;
    std::string topic;
    std::string beg_s;
    std::string end_s;
    bool get_dts;

    prompt_for_block_item_topic(&block, &item, &topic, ctx);         
    prompt_for("beginning datastream index", &beg_s, ctx);        
    prompt_for("ending datastream index", &end_s, ctx);
      
    long beg = 0;
    long end = 0;
    try{
        beg = std::stol(beg_s);
        end = std::stol(end_s);
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        return;
    }

    get_dts = prompt_for_datetime(block, ctx);

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
                                 size_type, pDateTimeStamp, long, long),
                      CommandCtx *ctx )
{  
    size_type display_len;   
    int ret;
    std::string block;
    std::string item;
    std::string topic;
    std::string len_s;
    std::string beg_s;
    std::string end_s;
    bool get_dts;

    pDateTimeStamp dts = nullptr;
    T *dat = nullptr;
   
    prompt_for_block_item_topic(&block, &item, &topic, ctx);
    prompt_for("length of array", &len_s, ctx);        
    prompt_for("beginning datastream index", &beg_s, ctx);        
    prompt_for("ending datastream index", &end_s, ctx);

    size_type len = 0;
    long beg = 0;
    long end = 0;
    try{
        len = std::stoul(len_s);
        beg = std::stol(beg_s);
        end = std::stol(end_s);
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        return;
    }
    
    get_dts = prompt_for_datetime(block, ctx);

    try{
        dat = new T[len];

        if(get_dts)
            dts = new DateTimeStamp[len];

        ret = func(block.c_str(), item.c_str(), topic.c_str(), dat, len, dts, end, beg);

        display_len = _min_stream_len(block,beg,end,len);

        _check_display_ret(ret, dat, display_len, dts);
 
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
                                             size_type, pDateTimeStamp, long, long*),
                                  CommandCtx *ctx )
{   
    long get_size;
    int ret;
    std::string block;
    std::string item;
    std::string topic;
    std::string beg_s;
    std::string len_s;
    bool get_dts;

    pDateTimeStamp dts = nullptr;
    T *dat = nullptr;

    prompt_for_block_item_topic(&block, &item, &topic, ctx);
    prompt_for("length of array", &len_s, ctx);        
    prompt_for("beginning datastream index", &beg_s, ctx);         

    size_type len = 0;
    long beg = 0;  
    try{
        len = std::stoul(len_s);
        beg = std::stol(beg_s);          
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        return;
    }

    get_dts = prompt_for_datetime(block, ctx);

    try{
        dat = new T[len];

        if(get_dts)
            dts = new DateTimeStamp[len];

        ret = func(block.c_str(), item.c_str(), topic.c_str(), dat, len, dts, beg, &get_size);

        _check_display_ret(ret, dat, abs(get_size), dts);

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
_check_display_ret(int r, T *v, size_type n, pDateTimeStamp d)
{
    if(r) 
        std::cout<< std::endl << "error: " << r << std::endl << std::endl;
    else 
        _display_stream_data(n, v, d);
}


template<typename T>
void 
_display_stream_data(size_type len, T* dat, pDateTimeStamp dts)
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
                std::cerr<< std::endl 
                         << "BAD INPUT - index must be < min(array length, abs(end-beg))" 
                         << std::endl;
                continue;
            }
            std::cout<< std::endl << dat[i] << ' ' << (dts ? &dts[i] : nullptr) << std::endl;             
        }catch(...){
            std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
            continue;
        }   

    }while(1);

    std::cout<< std::endl;
}


size_type
_min_stream_len(std::string block, long beg, long end, size_type len)
{
    size_type block_size = TOSDB_GetBlockSize(block);
    size_type min_size = 0;

    if(end < 0)
        end += block_size;
    if(beg < 0)
        beg += block_size;
        
    min_size = std::min((size_t)(end-beg+1), len);

    if(beg < 0 || end < 0)
        throw TOSDB_Error("STREAM DATA", "negative 'beg' or 'end' index values");

    if( (size_type)beg >= block_size 
        || (size_type)end >= block_size 
        || (size_type)min_size <= 0)
        throw TOSDB_Error("STREAM DATA", "invalid beg or end index values");

    return min_size;
}

}; /* namespace */