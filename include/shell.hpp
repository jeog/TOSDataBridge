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

#ifndef JO_TOSDB_SHELL
#define JO_TOSDB_SHELL

#include <iostream>
#include <map>

#include "tos_databridge.h"


#define MAX_DISPLAY_WIDTH 80
#define LEFT_INDENT_SIZE 10
#define CMD_OUT_PER_PAGE 10


/* forward decl */
class stream_prompt;

class stream_prompt_base {   
protected:
    std::string _prmpt;  

    explicit stream_prompt_base(std::string prompt)
        :      
            _prmpt(prompt)        
        {
        }

    virtual
    ~stream_prompt_base()       
        {
        }

public:
    /* return the base */
    template<typename T>
    stream_prompt_base& 
    operator<<(const T& val)
    {
        std::cout<< val; 
        return *this;
    }

    /* return the base */
    template< typename T >
    stream_prompt_base& 
    operator>>(T& val)
    {        
        std::cout<< _prmpt << ' ';        
        std::cin>> val;      
        return *this;
    }

    /* return the derived by casting from base
    this is safe if done from outside since we protect constructor */
    static stream_prompt& 
    endl(stream_prompt_base& p)
    {
        std::cout<< std::endl;
        return (stream_prompt&)(p);
    }

    inline stream_prompt& 
    operator<<(decltype(stream_prompt_base::endl) val)
    {
        return (*val)(*this);                    
    }   

    inline stream_prompt& 
    operator>>(decltype(stream_prompt_base::endl) val)
    {
        return (*val)(*this);                    
    } 
};
 
class stream_prompt
        : public stream_prompt_base {
public:
    explicit stream_prompt(std::string prompt)
        :      
            stream_prompt_base(prompt)
        {
        }

    /* RETURN THE BASE */
    template<typename T>
    stream_prompt_base& 
    operator<<(const T& val)
    {
        std::cout<< _prmpt << ' ' << val << ' '; 
        return *this;
    }  
};

class stream_prompt_basic
        : public stream_prompt_base {
public:
    explicit stream_prompt_basic(std::string prompt)
        :      
            stream_prompt_base(prompt)
        {
        }

    /* DONT RETURN THE BASE */
    template<typename T>
    stream_prompt_basic& 
    operator<<(const T& val)
    {
        std::cout<< _prmpt << ' ' << val << ' '; 
        return *this;
    }  
};

extern stream_prompt prompt;
extern stream_prompt_basic prompt_b;

/* fwrd decl */
struct CommandCtx;

typedef void(*commands_func_ty)(CommandCtx*);

struct CommandCtx{
    std::string name;
    commands_func_ty func;
    bool exit;
};

typedef std::map<std::string, CommandCtx> commands_map_ty;

commands_map_ty::value_type
build_commands_map_elem(std::string name, commands_func_ty);

extern commands_map_ty commands_admin; 
extern commands_map_ty commands_get;
extern commands_map_ty commands_stream;
extern commands_map_ty commands_frame;
extern commands_map_ty commands_local;

typedef std::pair<std::string,commands_map_ty> command_display_pair;
typedef std::unordered_map<std::string, command_display_pair> commands_map_of_maps_ty;

/* should be const but we can't use init lists so manually instantiate in util.cpp */
extern commands_map_of_maps_ty commands;


size_type 
get_cstr_entries(std::string label, char ***pstrs, CommandCtx *ctx);


inline size_type 
get_cstr_items(char ***p, CommandCtx *ctx)
{  
    return get_cstr_entries("item",p,ctx); 
}


inline size_type
get_cstr_topics(char ***p, CommandCtx *ctx)
{  
    return get_cstr_entries("topic",p,ctx);
}


inline void 
del_cstr_items(char **items, size_type nitems)
{
    DeleteStrings(items, nitems);
}


inline void 
del_cstr_topics(char **topics, size_type ntopics)
{
    DeleteStrings(topics, ntopics);
}


size_type
min_stream_len(std::string block, long beg, long end, size_type len);


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

bool
decr_page_latch_and_wait(int *n, std::function<void(void)> cb_on_wait );

inline void 
prompt_for(std::string sout, std::string* sin, CommandCtx *ctx)
{
    prompt_b<< ctx->name << sout >> *sin;
}

bool 
prompt_for_cpp(CommandCtx *ctx, int recurse=2);

bool 
prompt_for_datetime(std::string block, CommandCtx *ctx);

void
prompt_for_block_item_topic(std::string *pblock, 
                            std::string *pitem, 
                            std::string *ptopic,
                            CommandCtx *ctx);

void
prompt_for_block_item_topic_index(std::string *pblock, 
                                  std::string *pitem, 
                                  std::string *ptopic, 
                                  std::string *pindex, 
                                  CommandCtx *ctx);

void 
check_display_ret(int r);

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
check_display_ret(int r, bool v);


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


#endif /* JO_TOSDB_SHELL */