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
#include <fstream>
#include <memory>
#include <string>
#include <algorithm>

/* this will be defined/exported for ALL (implemenation) modules; */                             
/* extern */ char TOSDB_LOG_PATH[MAX_PATH+40];

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

std::string 
CreateBufferName(std::string sTopic, std::string sItem)
{     /* 
      * name of mapping is of form: "TOSDB_[topic name]_[item_name]"  
      * only alpha-numeric characters (except under-score) 
      */
      std::string str("TOSDB_");
      str.append( sTopic.append("_"+sItem) );

      auto f = [](char x){ return !isalnum(x) && x != '_'; };
      str.erase(std::remove_if(str.begin(), str.end(), f), str.end());
     
#ifdef NO_KGBLNS
      return str;
#else
      return std::string("Global\\").append(str);
#endif
}

std::string
BuildLogPath(std::string name)
{
    return std::string(TOSDB_LOG_PATH) + name;
}

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
        TOSDB_LogH("User Input", "string length > TOSDB_MAX_STR_SZ");
        return 0;
    }

    return 1;
}


unsigned int 
CheckStringLengths(LPCSTR* str, size_type items_len)
{
    size_t slen;
    while(items_len--){
        slen = strnlen_s(str[items_len], TOSDB_MAX_STR_SZ + 1);
        if( slen == (TOSDB_MAX_STR_SZ+1) ){ 
            TOSDB_LogH("User Input", "string length > TOSDB_MAX_STR_SZ");
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
        TOSDB_LogH("Strings", "name/id length > TOSDB_BLOCK_ID_SZ");
        return 0;
    }

  return 1;
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