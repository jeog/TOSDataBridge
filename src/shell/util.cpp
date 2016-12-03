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
prompt_for_cpp(int recurse) /*recurse=2*/
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


