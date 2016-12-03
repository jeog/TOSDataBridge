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
  
void GetDouble(void *ctx=nullptr);
void GetFloat(void *ctx=nullptr);
void GetLongLong(void *ctx=nullptr);
void GetLong(void *ctx=nullptr);
void GetString(void *ctx=nullptr);
void GetGeneric(void *ctx=nullptr);

commands_map_ty
build_commands_map_get()
{
    commands_map_ty m;

    m.insert( commands_map_elem_ty("GetDouble",GetDouble) );
    m.insert( commands_map_elem_ty("GetFloat",GetFloat) );
    m.insert( commands_map_elem_ty("GetLongLong",GetLongLong) );
    m.insert( commands_map_elem_ty("GetLong",GetLong) );
    m.insert( commands_map_elem_ty("GetString",GetString) );
    m.insert( commands_map_elem_ty("GetGeneric",GetGeneric) );

    return m;
}

};

commands_map_ty commands_get = build_commands_map_get();



namespace {

template<typename T>
void 
_get();

template<typename T>
void 
_get( int(*func)(LPCSTR, LPCSTR, LPCSTR, long, T*, pDateTimeStamp) );


void
GetDouble(void *ctx)
{
    prompt_for_cpp() ? _get<double>() : _get(TOSDB_GetDouble);       
}


void
GetFloat(void *ctx)
{
    prompt_for_cpp() ? _get<float>() : _get(TOSDB_GetFloat);
}
       

void
GetLongLong(void *ctx)
{
    prompt_for_cpp() ? _get<long long>() : _get(TOSDB_GetLongLong);
}
       

void
GetLong(void *ctx)
{
    prompt_for_cpp() ? _get<long>() : _get(TOSDB_GetLong);
}


void
GetString(void *ctx)
{
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
}

void
GetGeneric(void *ctx)
{
    _get<generic_type >();    
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

};