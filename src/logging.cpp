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
#include "concurrency.hpp"

#include <iomanip>
#include <fstream>
#include <string>

namespace {
    const system_clock_type  sys_clock;
    const size_type  log_col_width[5] = {30, 12, 12, 20, 12};
    const LPCSTR  severity_str[2] = {"low","high"};
    std::mutex  fout_mtx;   
    std::ofstream  lout;  
};


std::string 
SysTimeString()
{
    std::unique_ptr<char> buf(new char[log_col_width[0]]);

    time_t now = sys_clock.to_time_t(sys_clock.now());    
    ctime_s(buf.get(), log_col_width[0], &now);

    std::string tmp(buf.get());
    tmp.pop_back();
    return tmp;
}


void 
StartLogging(LPCSTR fname)
{
/* need to sync admin ops if we are sharing log between processes */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#endif

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


inline void 
StopLogging() 
{ 
/* need to sync admin ops if we are sharing log between processes */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#endif
    lout.close(); 
}


inline void 
ClearLog() 
{ 
/* need to sync admin ops if we are sharing log between processes */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#endif
    lout.clear(); 
}


void 
TOSDB_Log_(DWORD pid, 
           DWORD tid, 
           Severity sevr, 
           LPCSTR tag,  
           LPCSTR description) 
{ 
/* if we share back end logs we need to sync across processes 
   if not just use a typical mutex to sync across intra-process threads  */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#else 
    std::lock_guard<std::mutex> lock(fout_mtx);
#endif

    /* --- CRITICAL SECTION --- */
    std::string now_str = SysTimeString();

    if( lout.is_open() ){
        lout << std::setw(log_col_width[0])<< std::left<< now_str.substr(0,30)
             << std::setw(log_col_width[1])<< std::left<< pid
             << std::setw(log_col_width[2])<< std::left<< tid
             << std::setw(log_col_width[3])<< std::left
             << std::string(tag).substr(0,19)
             << std::setw(log_col_width[4])<< std::left<< severity_str[sevr]
                                           << std::left<< description << std::endl;
    }else if(sevr > 0){
        std::cerr << std::setw(log_col_width[0])<< std::left
                  << now_str.substr(0,30)
                  << std::setw(log_col_width[1])<< std::left<< pid
                  << std::setw(log_col_width[2])<< std::left<< tid
                  << std::setw(log_col_width[3])<< std::left
                  << std::string(tag).substr(0,19)      
                  << std::left<< description << std::endl;   

    }
    
  /* --- CRITICAL SECTION --- */
}


void 
TOSDB_LogEx_(DWORD pid, 
             DWORD tid, 
             Severity sevr, 
             LPCSTR tag,  
             LPCSTR description, 
             int error) 
{  
    std::string desc(description);
    desc.append(" ERROR# ").append(std::to_string(error));
    TOSDB_Log_(pid, tid, sevr, tag, desc.c_str());
}


void 
TOSDB_Log_Raw(LPCSTR description) 
{
    if( !lout.is_open() ){
        std::cerr<< SysTimeString() << '\t' << description << std::endl;
        return;
    }    
    lout<< std::left << SysTimeString() << '\t' << description << std::endl;    
}

