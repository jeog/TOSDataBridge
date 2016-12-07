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
  
void GetDouble(CommandCtx *ctx);
void GetFloat(CommandCtx *ctx);
void GetLongLong(CommandCtx *ctx);
void GetLong(CommandCtx *ctx);
void GetString(CommandCtx *ctx);
void GetGeneric(CommandCtx *ctx);

}; /* namespace */

const CommandsMap commands_get(
    CommandsMap::InitChain("GetDouble",GetDouble)
                          ("GetFloat",GetFloat)
                          ("GetLongLong",GetLongLong)
                          ("GetLong",GetLong)
                          ("GetString",GetString)
                          ("GetGeneric",GetGeneric) 
);

namespace {

template<typename T>
void 
_get(CommandCtx *ctx);

template<typename T>
void 
_get( int(*func)(LPCSTR, LPCSTR, LPCSTR, long, T*, pDateTimeStamp), CommandCtx *ctx );

template<typename T>
void 
_check_display_ret(int r, T v, pDateTimeStamp d);


void
GetDouble(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) ? _get<double>(ctx) : _get(TOSDB_GetDouble, ctx);       
}


void
GetFloat(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) ? _get<float>(ctx) : _get(TOSDB_GetFloat, ctx);
}
       

void
GetLongLong(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) ? _get<long long>(ctx) : _get(TOSDB_GetLongLong, ctx);
}
       

void
GetLong(CommandCtx *ctx)
{
    prompt_for_cpp(ctx) ? _get<long>(ctx) : _get(TOSDB_GetLong, ctx);
}


void
GetString(CommandCtx *ctx)
{
    if(prompt_for_cpp(ctx)){
        _get<std::string>(ctx);
    }else{
        char s[TOSDB_STR_DATA_SZ];
        DateTimeStamp dts;
        std::string block;
        std::string item;
        std::string topic;
        std::string index_s;
        int ret;
        bool get_dts;

        prompt_for_block_item_topic_index(&block, &item, &topic, &index_s, ctx);

        long index = 0;
        try{         
            index = std::stol(index_s);            
        }catch(...){
            std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
            return;
        }

        get_dts = prompt_for_datetime(block, ctx);

        ret = TOSDB_GetString(block.c_str(), item.c_str(), topic.c_str(), index, 
                              s, TOSDB_STR_DATA_SZ, (get_dts ? &dts : nullptr));

        _check_display_ret(ret, s, (get_dts ? &dts : nullptr));
    }
}


void
GetGeneric(CommandCtx *ctx)
{
    _get<generic_type>(ctx);    
}


template<typename T>
void 
_get(CommandCtx *ctx)
{  
    std::string block;
    std::string item;
    std::string topic;    
    std::string index_s;  
    bool get_dts;

    prompt_for_block_item_topic_index(&block, &item, &topic, &index_s, ctx);   
      
    long index = 0;
    try{         
        index = std::stol(index_s);            
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        return;
    }

    get_dts = prompt_for_datetime(block, ctx); 
 
    if(get_dts)
        std::cout<< std::endl 
                 << TOSDB_Get<T,true>(block, item, TOS_Topics::MAP()[topic], index)
                 << std::endl << std::endl;    
    else 
        std::cout<< std::endl 
                 << TOSDB_Get<T,false>(block,item,TOS_Topics::MAP()[topic], index)
                 <<std::endl << std::endl;            
}


template<typename T>
void 
_get( int(*func)(LPCSTR, LPCSTR, LPCSTR, long, T*, pDateTimeStamp), CommandCtx *ctx )
{
    T d;
    DateTimeStamp dts;
    std::string block;
    std::string item;
    std::string topic;
    std::string index_s;
    int ret;
    bool get_dts;

    prompt_for_block_item_topic_index(&block, &item, &topic, &index_s, ctx);

    long index = 0;
    try{         
        index = std::stol(index_s);            
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        return;
    }

    get_dts = prompt_for_datetime(block, ctx);

    ret = func(block.c_str(), item.c_str(), topic.c_str(), index, 
               &d, (get_dts ? &dts : nullptr));
    _check_display_ret(ret, d, (get_dts ? &dts : nullptr));  
}


template<typename T>
void 
_check_display_ret(int r, T v, pDateTimeStamp d)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << v << ' ' << d << std::endl << std::endl; 
}

}; /* namespace */