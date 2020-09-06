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

#include "tos_databridge.h"
#include "initializer_chain.hpp"

#include <fstream>
#include <memory>
#include <string>
#include <algorithm>
#include <set>

/* this will be defined/exported for ALL (implemenation) modules; */                             
/* extern */ char TOSDB_LOG_PATH[MAX_PATH+40];

namespace{

const std::set<char> ITEM_VALID_START_CHARS = // 2020-09-06 'const char' -> 'char'
    InitializerChain<std::set<char>>
        ('-')
        ('+')
        ('.')
        ('$')       
        ('/')
        ('#');

const std::set<char> ITEM_VALID_MID_CHARS = // 2020-09-06 'const char' -> 'char'
    InitializerChain<std::set<char>>
        ('-')
        ('.')
        ('*')
        ('+')
        (':')
        ('$')  
        ('/')
        ('#');

const std::set<char> ITEM_VALID_END_CHARS = // 2020-09-06 'const char' -> 'char'
    InitializerChain<std::set<char>>
        ('.')
        ('$') 
        ('#');

const std::unordered_map<char, std::string> ITEM_SYMBOL_BUFFER_MAP = 
    InitializerChain<std::unordered_map<char, std::string>>
        ('-', "(1)")
        ('.', "(2)")
        ('$', "(3)")
        ('/', "(4)")
        ('*', "(5)")
        ('+', "(6)")
        ('#', "(7)")
        (':', "(8)");

bool
_isValidItemChar(const char c, const std::set<char>& valid_other_chars=std::set<char>()) // 2020-09-06 'const char' -> 'char'
{
    return (isalnum(c) || (valid_other_chars.find(c) != valid_other_chars.end())); 
}

}; /* namespace */


BOOL WINAPI 
DllMain(HINSTANCE mod, DWORD why, LPVOID res)
{    
    /* NOTE: change HANDLE to HINSTANCE to avoid passing NULL to 
             GetModuleFileName which returns path of calling module
             instead of *this* module, screwing up the relative log path.
             
             ***IF SOMETHING FUNDAMENTAL BREAKS LOOK HERE*** (Nov 10 2016)
    */
    switch(why){
    case DLL_PROCESS_ATTACH:
    {  
     /* 
        either use a relative(local) log path or create a log folder in 
        appdata; define TOSDB_LOG_PATH for all other implemenation modules 
      */
#ifdef LOCAL_LOG_PATH	/* add error checks */
        GetModuleFileName(mod, TOSDB_LOG_PATH, MAX_PATH);
        std::string lp(TOSDB_LOG_PATH);
        std::string nlp(lp.begin(),lp.begin() + lp.find_last_of('\\'));
        nlp.append(LOCAL_LOG_PATH);
        memset(TOSDB_LOG_PATH,0,(MAX_PATH+40)* sizeof(*TOSDB_LOG_PATH));
        strcpy(TOSDB_LOG_PATH, nlp.c_str());    		
#else
        GetEnvironmentVariable("APPDATA", TOSDB_LOG_PATH, MAX_PATH);         
        strcat_s(TOSDB_LOG_PATH, "\\tos-databridge\\"); /* needs to be < 40 char */
#endif	        
        CreateDirectory(TOSDB_LOG_PATH, NULL);
        break;
    } 
    case DLL_THREAD_ATTACH: /* no break */
    case DLL_THREAD_DETACH: /* no break */
    case DLL_PROCESS_DETACH:/* no break */    
    default: break;
    }

    return TRUE;
}

/* IMPLEMENTATION ONLY */

std::string 
CreateBufferName(std::string topic_str, std::string item)
{     /* 
      * name of mapping is of form: "TOSDB_Buffer__[topic name]_[item_name]"  
      * replacing reserved chars w/ ITEM_SYMBOL_BUFFER_MAP strings
      */
      std::stringstream bname;
      std::string str = "TOSDB_Buffer__" + topic_str + "_" + item;

      for( char c : str ){
          auto f = ITEM_SYMBOL_BUFFER_MAP.find(c);
          bname << ((f == ITEM_SYMBOL_BUFFER_MAP.end()) ? std::string(1,c) : f->second);
      }
     
#ifdef NO_KGBLNS
      return bname.str();
#else
      return std::string("Global\\").append(bname.str());
#endif
}

std::string
BuildLogPath(std::string name)
{
    return std::string(TOSDB_LOG_PATH) + name;
}


void 
ParseArgs(std::vector<std::string>& vec, std::string str)
{
    std::string::size_type i = str.find_first_of(' '); 

    if( str.empty() ){ /* done */
        return;
    }else if(i == std::string::npos){ /* only 1 str */
        vec.push_back(str);
        return;
    }else if(i == 0){ /* trim initial space(s) */
        ParseArgs(vec, str.substr(1,-1));
        return;
    }else{ /* atleast 2 strings */
        vec.push_back(str.substr(0,i));
        ParseArgs(vec, str.substr(i+1,str.size()));
    }
}


void /* in place 'safe'*/
str_to_lower(char* str, size_t max)
{
    while(str && max--){
        str[0] = tolower(str[0]);
        ++str;
    }
}


void /* in place 'safe' */
str_to_upper(char* str, size_t max)
{
    while(str && max--){
        str[0] = toupper(str[0]);
        ++str;
    }
}

std::string 
str_to_lower(std::string str)
{
    std::transform( str.begin(), str.end(), str.begin(), 
                    [](unsigned char c){ return tolower(c); } );

    return str;
}


std::string  
str_to_upper(std::string str)
{
    std::transform( str.begin(), str.end(), str.begin(),
                    [](unsigned char c){ return toupper(c); } );

    return str;
}


/* IMPLEMENTATION AND INTERFACE */

char** 
NewStrings(size_t num_strs, size_t strs_len)
{
    char** strs = new char*[num_strs];

    for(size_t i = 0; i < num_strs; ++i)
        strs[i] = new char[strs_len + 1];

    return strs;
}

void 
DeleteStrings(char** str_array, size_t num_strs)
{
    if(!str_array)
      return;

    while(num_strs--){  
        if(str_array[num_strs])
          delete[] str_array[num_strs];    
    }

    delete[] str_array;
}


unsigned int 
CheckStringLength(LPCSTR str)
{
    size_t slen = strnlen_s(str, TOSDB_MAX_STR_SZ+1);
    if( slen == (TOSDB_MAX_STR_SZ+1) ){
        TOSDB_LogH("INPUT", "string length > TOSDB_MAX_STR_SZ");
        return 0;
    }

    return 1;
}


unsigned int 
CheckStringLengths(LPCSTR* str, size_type items_len)
{
    size_t slen;

    if(items_len > TOSDB_MAX_NSTRS){
        TOSDB_LogH("INPUT", "# of strings > TOSDB_MAX_NSTRS");
        return 0;
    }

    while(items_len--){
        slen = strnlen_s(str[items_len], TOSDB_MAX_STR_SZ + 1);
        if( slen == (TOSDB_MAX_STR_SZ+1) ){ 
            TOSDB_LogH("INPUT", "string length > TOSDB_MAX_STR_SZ");
            return 0;
        }
    }

  return 1;
}


unsigned int 
CheckIDLength(LPCSTR id)
{
    size_t slen = strnlen_s(id, TOSDB_BLOCK_ID_SZ + 1);
    if( slen == (TOSDB_BLOCK_ID_SZ + 1) ){
        TOSDB_LogH("INPUT", "block name length > TOSDB_BLOCK_ID_SZ");
        return 0;
    }

  return 1;
}


unsigned int
IsValidBlockSize(size_type sz)
{
    if(sz < 1){
        TOSDB_LogH("BLOCK", "block size < 1");
        return 0; 
    }        

    if(sz > TOSDB_MAX_BLOCK_SZ){
        TOSDB_LogH("BLOCK", ("block size (" + std::to_string(sz) + ") > TOSDB_MAX_BLOCK_SZ ("
                             + std::to_string(TOSDB_MAX_BLOCK_SZ) + ")").c_str());
        return 0; 
    } 

    return 1;
}


unsigned int
IsValidBlockID(const char* id)
{
    /* check length */
    if( !CheckIDLength(id) )
        return 0;

    /* check isn't reserved */
    if(std::string(id) == std::string(TOSDB_RESERVED_BLOCK_NAME)){        
        TOSDB_LogH("BLOCK", ("block name '" + std::string(id) + "' is reserved").c_str());
        return 0; 
    }

    return 1;
}


unsigned int
IsValidItemString(const char* item)
{
    if( !item || !CheckStringLength(item)  )
        return 0;

    size_t slen = strlen(item);

    /* first char */
    if( !_isValidItemChar(item[0], ITEM_VALID_START_CHARS) ){
          TOSDB_LogH("INPUT", ("invalid item string: " + std::string(item)).c_str() );
          return 0;       
    }
        
    /* middle chars */
    for(int i = 1; i < slen -1; ++i){
        if( !_isValidItemChar(item[i], ITEM_VALID_MID_CHARS) ){
            TOSDB_LogH("INPUT", ("invalid item string: " + std::string(item)).c_str() );
            return 0;           
        }       
    }    

    /* last char*/
    if( !_isValidItemChar(item[slen-1], ITEM_VALID_END_CHARS) ){
        TOSDB_LogH("INPUT", ("invalid item string: " + std::string(item)).c_str() );
        return 0;
    }

    return 1;
}

