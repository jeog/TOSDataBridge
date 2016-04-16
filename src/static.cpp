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
#include <iomanip>
#include <fstream>
#include <memory>
#include <string>
#include <algorithm>


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

char** AllocStrArray(size_t num_strs, size_t strs_len)
{
  char** mat = new char*[num_strs];

  for(size_t i = 0; i < num_strs; ++i)
    mat[i] = new char[strs_len + 1];

  return mat;
}

void DeallocStrArray(const char* const* str_array, size_t num_strs)
{
  if(!str_array)
    return;

  while(num_strs--)  
    if(str_array[num_strs])
      delete[] (char*)(str_array[num_strs]);    

  delete[] (char**)str_array;
}

namespace {
  const system_clock_type  sys_clock;
  const size_type          log_col_width[5] = {30, 12, 12, 20, 12};
  const LPCSTR             severity_str[2] = {"low","high"};
  std::mutex               fout_mtx;   
  std::ofstream            lout;  
};

std::string SysTimeString()
{
  std::unique_ptr<char> buf(new char[log_col_width[0]]);

  time_t now = sys_clock.to_time_t(sys_clock.now());    
  ctime_s(buf.get(), log_col_width[0], &now);

  std::string tmp(buf.get());
  tmp.pop_back();
  return tmp;
}

void TOSDB_StartLogging(LPCSTR fname)
{
  if(lout.is_open())
    return;

  lout.open(fname , std::ios::out | std::ios::app);    

  if(lout.seekp(0,std::ios::end).tellp() == std::ios::pos_type(0))
  {
    lout << std::setw(log_col_width[0]) << std::left << "DATE / TIME"
         << std::setw(log_col_width[1]) << std::left << "Process ID"
         << std::setw(log_col_width[2]) << std::left << "Thread ID"
         << std::setw(log_col_width[3]) << std::left << "Log TAG"
         << std::setw(log_col_width[4]) << std::left << "Severity"
                                        << std::left << "Description"
                                        << std::endl << std::endl;    
  }
}

inline void TOSDB_StopLogging() { lout.close(); }
inline void TOSDB_ClearLog() { lout.clear(); }

void TOSDB_Log_(DWORD pid, 
                DWORD tid, 
                Severity sevr, 
                LPCSTR tag,  
                LPCSTR description) 
{  
  std::lock_guard<std::mutex> lock(fout_mtx);
  /* --- CRITICAL SECTION */
  std::string now_str = SysTimeString();
  if(!lout.is_open())
  {
    if(sevr > 0)
      std::cerr << std::setw(log_col_width[0])<< std::left
                << now_str.substr(0,30)
                << std::setw(log_col_width[1])<< std::left<< pid
                << std::setw(log_col_width[2])<< std::left<< tid
                << std::setw(log_col_width[3])<< std::left
                << std::string(tag).substr(0,19)      
                << std::left<< description << std::endl;    
  }
  else
  {
    lout << std::setw(log_col_width[0])<< std::left<< now_str.substr(0,30)
         << std::setw(log_col_width[1])<< std::left<< pid
         << std::setw(log_col_width[2])<< std::left<< tid
         << std::setw(log_col_width[3])<< std::left
         << std::string(tag).substr(0,19)
         << std::setw(log_col_width[4])<< std::left<< severity_str[sevr]
                         << std::left<< description 
                         << std::endl;
  }
  /* --- CRITICAL SECTION */
}

void TOSDB_LogEx_(DWORD pid, 
                  DWORD tid, 
                  Severity sevr, 
                  LPCSTR tag,  
                  LPCSTR description, 
                  DWORD error) 
{  
  TOSDB_Log_(pid, tid, sevr, tag, std::string(description)
                                     .append(" ERROR# ")
                                     .append(std::to_string(error)).c_str());
}

void TOSDB_Log_Raw_(LPCSTR description) 
{
  if(!lout.is_open())
  {
    std::cerr<<SysTimeString()<<'\t'<<description<<std::endl;
    return;
  }    
  lout<< std::left<<SysTimeString()<<'\t'<< description << std::endl;
}

