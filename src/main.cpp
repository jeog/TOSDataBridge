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

char TOSDB_LOG_PATH[MAX_PATH+20];

BOOL 
WINAPI DllMain(HANDLE mod, DWORD why, LPVOID res)
{    
    switch(why){
    case DLL_PROCESS_ATTACH:
    {  
        /* create our log folder in appdata */
        GetEnvironmentVariable("APPDATA", TOSDB_LOG_PATH, MAX_PATH+20);         
        strcat_s(TOSDB_LOG_PATH, TOSDB_APP_FOLDER);
        CreateDirectory(TOSDB_LOG_PATH, NULL);

        /* start logging */
        std::string lpath(TOSDB_LOG_PATH);
        lpath.append("\\tos-databridge-shared.log");
        TOSDB_StartLogging( lpath.c_str() );
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

#ifdef KGBLNS_
      return std::string("Global\\").append(str);
#else
      return str;
#endif
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