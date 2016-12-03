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
  
void GetStreamSnapshotDoubles(void *ctx=nullptr);
void GetStreamSnapshotFloats(void *ctx=nullptr); 
void GetStreamSnapshotLongLongs(void *ctx=nullptr);
void GetStreamSnapshotLongs(void *ctx=nullptr);
void GetStreamSnapshotStrings(void *ctx=nullptr);
void GetStreamSnapshotGenerics(void *ctx=nullptr);
void GetStreamSnapshotDoublesFromMarker(void *ctx=nullptr);
void GetStreamSnapshotFloatsFromMarker(void *ctx=nullptr); 
void GetStreamSnapshotLongLongsFromMarker(void *ctx=nullptr);
void GetStreamSnapshotLongsFromMarker(void *ctx=nullptr);
void GetStreamSnapshotStringsFromMarker(void *ctx=nullptr);


commands_map_ty
build_commands_map_stream()
{
    commands_map_ty m;

    m.insert( commands_map_elem_ty("GetStreamSnapshotDoubles",GetStreamSnapshotDoubles) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotFloats",GetStreamSnapshotFloats) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotLongLongs",GetStreamSnapshotLongLongs) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotLongs",GetStreamSnapshotLongs) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotStrings",GetStreamSnapshotStrings) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotGenerics",GetStreamSnapshotGenerics) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotDoublesFromMarker",GetStreamSnapshotDoublesFromMarker) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotFloatsFromMarker",GetStreamSnapshotFloatsFromMarker) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotLongLongsFromMarker",GetStreamSnapshotLongLongsFromMarker) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotLongsFromMarker",GetStreamSnapshotLongsFromMarker) );
    m.insert( commands_map_elem_ty("GetStreamSnapshotStringsFromMarker",GetStreamSnapshotStringsFromMarker) );
    
    return m;
}

};


commands_map_ty commands_stream = build_commands_map_stream();


namespace{

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
GetStreamSnapshotDoubles(void *ctx)
{
    prompt_for_cpp() ? _get_stream_snapshot<double>() 
                      : _get_stream_snapshot<double>(TOSDB_GetStreamSnapshotDoubles);
}

void
GetStreamSnapshotFloats(void *ctx)
{
    prompt_for_cpp() ? _get_stream_snapshot<float>() 
                      : _get_stream_snapshot<float>(TOSDB_GetStreamSnapshotFloats);
}

void
GetStreamSnapshotLongLongs(void *ctx)
{
    prompt_for_cpp() ? _get_stream_snapshot<long long>() 
                      : _get_stream_snapshot<long long>(TOSDB_GetStreamSnapshotLongLongs);
}

void
GetStreamSnapshotLongs(void *ctx)
{
    prompt_for_cpp() ? _get_stream_snapshot<long>() 
                      : _get_stream_snapshot<long>(TOSDB_GetStreamSnapshotLongs);
}

void
GetStreamSnapshotStrings(void *ctx)
{
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
}


void
GetStreamSnapshotGenerics(void *ctx)
{
    _get_stream_snapshot<generic_type>();
}

void
GetStreamSnapshotDoublesFromMarker(void *ctx)
{
    _get_stream_snapshot_from_marker<double>(TOSDB_GetStreamSnapshotDoublesFromMarker);
}


void
GetStreamSnapshotFloatsFromMarker(void *ctx)
{
    _get_stream_snapshot_from_marker<float>(TOSDB_GetStreamSnapshotFloatsFromMarker);
}

void
GetStreamSnapshotLongLongsFromMarker(void *ctx)
{
    _get_stream_snapshot_from_marker<long long>(TOSDB_GetStreamSnapshotLongLongsFromMarker);
}


void
GetStreamSnapshotLongsFromMarker(void *ctx)
{
    _get_stream_snapshot_from_marker<long>(TOSDB_GetStreamSnapshotLongsFromMarker);
}


void
GetStreamSnapshotStringsFromMarker(void *ctx)
{
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


};