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

/* [Jan 16 2017] in order to synchronize 'atomic' write operations we use 
   a named mutex via NamedMutexLockGuard (if LOG_BACKEND_USE_SINGLE_FILE is 
   #defined and we need inter-process sync) or std::lock_guard<std::mutex>        
     
   (ideally we should use an async producer/consumer queue for better performance) */

namespace {

const system_clock_type SYSTEM_CLOCK;
const size_type COL_WIDTHS[5] = {36, 12, 12, 20, 12};
const LPCSTR  SEVERITY[2] = {"LOW","HIGH"};

#ifndef LOG_BACKEND_USE_SINGLE_FILE
std::mutex log_file_mtx;
#endif

std::ofstream log_file;

void
_write_log(std::ofstream& fout, 
           std::string now, 
           Severity sevr, 
           std::string tag, 
           std::string desc)
{
    fout << std::setw(COL_WIDTHS[0]) << std::left << now.substr(0,COL_WIDTHS[0]-1)
         << std::setw(COL_WIDTHS[1]) << std::left << GetCurrentProcessId()
         << std::setw(COL_WIDTHS[2]) << std::left << GetCurrentThreadId()
         << std::setw(COL_WIDTHS[3]) << std::left << std::string(tag).substr(0,COL_WIDTHS[3]-1)
         << std::setw(COL_WIDTHS[4]) << std::left << SEVERITY[sevr] 
                                     << std::left << desc << std::endl; 
}

void
_write_err(std::string now, std::string tag, std::string desc)
{
    std::cerr << std::setw(COL_WIDTHS[0]) << std::left << now.substr(0,COL_WIDTHS[0]-1)
              << std::setw(COL_WIDTHS[1]) << std::left << GetCurrentProcessId()
              << std::setw(COL_WIDTHS[2]) << std::left << GetCurrentThreadId()
              << std::setw(COL_WIDTHS[3]) << std::left << std::string(tag).substr(0,COL_WIDTHS[3]-1)      
                                          << std::left << desc << std::endl;     
}

};


void 
StartLogging(LPCSTR path)
{
/* need to sync admin ops if we are sharing log between processes */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#endif

    if(log_file.is_open())
        return;

    log_file.open(path , std::ios::out | std::ios::app);    

    auto pos = log_file.seekp(0,std::ios::end).tellp();
    if(pos == std::ios::pos_type(0)){
        /* write header if necessary */
        log_file << std::setw(COL_WIDTHS[0]) << std::left << "DATE / TIME"
                 << std::setw(COL_WIDTHS[1]) << std::left << "Process ID"
                 << std::setw(COL_WIDTHS[2]) << std::left << "Thread ID"
                 << std::setw(COL_WIDTHS[3]) << std::left << "Log TAG"
                 << std::setw(COL_WIDTHS[4]) << std::left << "Severity"
                                             << std::left << "Description"
                                             << std::endl << std::endl;    
    }      
}


void 
StopLogging() 
{ 
/* need to sync admin ops if we are sharing log between processes */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#endif

    log_file.close(); 
}


void 
ClearLog() 
{ 
/* need to sync admin ops if we are sharing log between processes */
#ifdef LOG_BACKEND_USE_SINGLE_FILE
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#endif

    if(!log_file.is_open())
        return; 

    log_file.clear();  
}


void 
TOSDB_Log_(Severity sevr, LPCSTR tag, LPCSTR description) 
{   

#ifdef LOG_BACKEND_USE_SINGLE_FILE
    /* if we share back end logs we need to sync across processes */ 
    NamedMutexLockGuard lock(LOG_BACKEND_MUTEX_NAME);
#else 
    /* if not just use a typical mutex to sync across intra-process threads  */
    std::lock_guard<std::mutex> lock(log_file_mtx);
#endif

    /* --- CRITICAL SECTION --- */
    std::string now = SysTimeString();
      
    if(log_file.is_open())   
        _write_log(log_file, now, sevr, tag, description); 
    else if(sevr > 0)
        _write_err(now, tag, description);         
  /* --- CRITICAL SECTION --- */
}


void 
TOSDB_LogEx_(Severity sevr, LPCSTR tag, LPCSTR description, int error) 
{  
    std::string desc = std::string(description) + ", ERROR# " + std::to_string(error);
    TOSDB_Log_(sevr, tag, desc.c_str());
}


void 
TOSDB_LogRaw_(Severity sevr, LPCSTR tag, LPCSTR description) 
{
    std::string now = SysTimeString();

    if(log_file.is_open())  
        _write_log(log_file, now, sevr, tag, description); 
    else if(sevr > 0)
        _write_err(now, tag, description);     
}


std::string 
SysTimeString(bool use_msec)
{
    using namespace std::chrono;

    time_t now;
    long long ms_rem = 0;
    int madj = use_msec ? 10 : 0; // 2 + 6 + 1 + 1(pad) = 10 char

    SmartBuffer<char> buf(COL_WIDTHS[0] - madj);    

    auto tp = SYSTEM_CLOCK.now();  

    if(use_msec){
        auto ms = duration_cast<microseconds>(tp.time_since_epoch());
        ms_rem = ms.count() % 1000000; 
        /* remove the msec so we don't accidentally round up */
        now = SYSTEM_CLOCK.to_time_t(tp - microseconds(ms_rem));  
    }else{
        now = SYSTEM_CLOCK.to_time_t(tp);  
    }
  
    ctime_s(buf.get(), COL_WIDTHS[0] - madj, &now); 

    std::string tmp(buf.get());
    tmp.pop_back();

    if(use_msec)        
        tmp += " [" + std::to_string(ms_rem) + ']';     

    return tmp;
}