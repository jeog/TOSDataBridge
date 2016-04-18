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

BOOL WINAPI DllMain(HANDLE mod, DWORD why, LPVOID res)
{    
  switch(why){
  case DLL_PROCESS_ATTACH:
  {    
  LPCSTR app_folder = "\\tos-databridge";

  GetEnvironmentVariable("APPDATA", TOSDB_LOG_PATH, MAX_PATH+20); 
  strcat_s(TOSDB_LOG_PATH, app_folder);
  CreateDirectory(TOSDB_LOG_PATH, NULL);  
  strcat_s(TOSDB_LOG_PATH, "\\");
  TOSDB_StartLogging(
    std::string(TOSDB_LOG_PATH).append("tos-databridge-shared.log").c_str());

  } break;
  case DLL_THREAD_ATTACH: /* no break */
  case DLL_THREAD_DETACH: /* no break */
  case DLL_PROCESS_DETACH:/* no break */    
  default: break;
  }
  return TRUE;
}

std::string CreateBufferName(std::string sTopic, std::string sItem)
{ /* 
   * name of mapping is of form: "TOSDB_[topic name]_[item_name]"  
   * only alpha-numeric characters (except under-score) 
   */
  std::string str("TOSDB_");
  str.append(sTopic.append("_"+sItem));   
  str.erase(std::remove_if(str.begin(), str.end(), 
                           [](char x){ return !isalnum(x) && x != '_'; }), 
            str.end());
#ifdef KGBLNS_
  return std::string("Global\\").append(str);
#else
  return str;
#endif
}

char** NewStrings(size_t num_strs, size_t strs_len)
{
  char** strs = new char*[num_strs];

  for(size_t i = 0; i < num_strs; ++i)
    strs[i] = new char[strs_len + 1];

  return strs;
}

void DeleteStrings(char** str_array, size_t num_strs)
{
  if(!str_array)
    return;

  while(num_strs--){  
    if(str_array[num_strs])
      delete[] str_array[num_strs];    
  }

  delete[] str_array;
}