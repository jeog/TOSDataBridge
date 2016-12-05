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

commands_map_of_maps_ty
build_commands_map_of_maps()
{
    typedef commands_map_of_maps_ty::value_type elem_ty;

    commands_map_of_maps_ty m;

    m.insert( elem_ty("admin", command_display_pair("ADMINISTRATIVE",commands_admin)) );
    m.insert( elem_ty("get", command_display_pair("GET",commands_get)) );
    m.insert( elem_ty("stream", command_display_pair("STREAM-SNAPSHOT",commands_stream)) );
    m.insert( elem_ty("frame", command_display_pair("FRAME",commands_frame)) );
    m.insert( elem_ty("local", command_display_pair("LOCAL",commands_local)) );

    return m;
}

};

commands_map_of_maps_ty commands = build_commands_map_of_maps();


commands_map_ty::value_type
build_commands_map_elem(std::string name, commands_func_ty func)
{
    CommandCtx c = {name, func, false};  
    auto v = commands_map_ty::value_type(name, std::move(c));
    return v;
}


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
prompt_for_cpp(CommandCtx *ctx, int recurse) /*recurse=2*/
{
    std::string cpp_y_or_n;

    prompt_for("use C++ version? (y/n)", &cpp_y_or_n, ctx);

    if(cpp_y_or_n == "y")
        return true;
    else if(cpp_y_or_n == "n")
        return false;
    else if(recurse > 0){
        std::cout<< "INVALID - enter 'y' or 'n'" << std::endl;
        return prompt_for_cpp(ctx, recurse-1);
    }else{
        std::cout<< "INVALID - defaulting to C version" << std::endl;
        return false;
    }
}


bool 
prompt_for_datetime(std::string block, CommandCtx *ctx)
{
    std::string dts_y_or_n;
  
    if( !TOSDB_IsUsingDateTime(block) )
        return false;          
   
    prompt_for("get datetime stamp? (y/n)", &dts_y_or_n, ctx);
                    
    if(dts_y_or_n == "y")
        return true;
    else if(dts_y_or_n != "n")              
        std::cout<< "INVALID - default to NO datetime" << std::endl;

    return false;    
}


void
prompt_for_block_item_topic(std::string *pblock, 
                            std::string *pitem, 
                            std::string *ptopic,
                            CommandCtx *ctx)
{
    prompt_for("block", pblock, ctx);
    prompt_for("item", pitem, ctx);    
    prompt_for("topic", ptopic, ctx);
}


void
prompt_for_block_item_topic_index(std::string *pblock, 
                                  std::string *pitem, 
                                  std::string *ptopic, 
                                  std::string *pindex, 
                                  CommandCtx *ctx)
{
    prompt_for_block_item_topic(pblock, pitem, ptopic, ctx);
    prompt_for("index", pindex, ctx); 
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


size_type 
get_cstr_entries(std::string label, char ***pstrs, CommandCtx *ctx)
{  
    std::string nentry;
    std::string item;

    prompt_for("# " + label + "s", &nentry, ctx);    

    unsigned long n = 0;
    try{
        n = std::stoul(nentry);
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        *pstrs = nullptr;
        return 0;
    }

    try{
        *pstrs = NewStrings(n, TOSDB_MAX_STR_SZ);

        for(size_type i = 0; i < n; ++i){
            prompt_b<< ctx->name;
            prompt<< label << " (" << (i+1) << ") " >> item;        
            strcpy_s((*pstrs)[i], item.length() + 1, item.c_str());
        }
    }catch(...){
        DeleteStrings(*pstrs, n);
        *pstrs = nullptr;
        return 0;
    }

    return n;
}

/* template overloads in shell.hpp */
void 
check_display_ret(int r)
{
    if(r) 
        std::cout<< std::endl << "error: "<< r << std::endl << std::endl; 
    else 
        std::cout<< std::endl << "SUCCESS" << std::endl << std::endl; 
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

