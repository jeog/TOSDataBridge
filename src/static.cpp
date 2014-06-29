/* 
Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >

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

LPCSTR    TOSDB_APP_NAME    =  "TOS";
LPCSTR    TOSDB_COMM_CHANNEL = "TOSDB_channel_1";

const size_type TOSDB_DEF_TIMEOUT = 2000;
const size_type TOSDB_MIN_TIMEOUT = 1500;
const size_type TOSDB_SHEM_BUF_SZ = 4096;
const size_type TOSDB_BLOCK_ID_SZ = 63;

std::string CreateBufferName( std::string sTopic, std::string sItem )
{      /* name of mapping is of form: "TOSDB_[topic name]_[item_name]"  
        only alpha-numeric characters (except under-score)                */
    std::string str("TOSDB_");
    str.append( sTopic.append("_"+sItem) );     
    str.erase( 
        std::remove_if( 
                str.begin(), 
                str.end(), 
                [](char x){ return !isalnum( x ) && x != '_'; } 
                ), 
        str.end() 
        );
#ifdef KGBLNS_
    return std::string("Global\\").append(str);
#else
    return str;
#endif
}

char** AllocStrArray( size_t numStrs, size_t szStrs )
{
    char** strMat = new char*[numStrs];
    for(size_t i = 0; i < numStrs; ++i)
        strMat[i] = new char[ szStrs + 1 ];
    return strMat;
}

void DeallocStrArray( const char* const* strArray, size_t numStrs )
{
    if( !strArray )
        return;
    while( numStrs--)    
        if( strArray[numStrs] )
            delete[] (char*)(strArray[numStrs]);        
    delete[] (char**)strArray;
}

namespace {
    const size_type         logColW[5] = {30, 12, 12, 20, 12};
    const LPCSTR            sevStr[2] = {"low","high"};
    std::mutex              fOutMtx;     
    std::ofstream           logOut;
    const system_clock_type sysClock;
};

std::string SysTimeString()
{
    std::unique_ptr<char> tmpBuf(new char[logColW[0]]);
    time_t now = sysClock.to_time_t( sysClock.now() );        
    ctime_s( tmpBuf.get(), logColW[0], &now);
    std::string tmpStr(tmpBuf.get());
    tmpStr.pop_back();
    return tmpStr;
}

void TOSDB_StartLogging(LPCSTR fName)
{
    if( logOut.is_open() )
        return;
    logOut.open( fName , std::ios::out | std::ios::app);        
    if( logOut.seekp(0,std::ios::end).tellp() == std::ios::pos_type(0)){        
        logOut<<std::setw(logColW[0])<< std::left << "DATE / TIME"
            <<std::setw(logColW[1])<< std::left << "Process ID"
            <<std::setw(logColW[2])<< std::left << "Thread ID"
            <<std::setw(logColW[3])<< std::left << "Log TAG"
            <<std::setw(logColW[4])<< std::left << "Severity"
            << std::left << "Description"
            << std::endl <<std::endl;        
    }
}

inline void TOSDB_StopLogging(){ logOut.close(); }

inline void TOSDB_ClearLog(){ logOut.clear(); }

void TOSDB_Log_( DWORD pid, DWORD tid, Severity sevr, LPCSTR tag,  LPCSTR description) 
{    
    std::lock_guard<std::mutex> _lck(fOutMtx);
    std::string nowTime = SysTimeString();
    if( !logOut.is_open() ){
        if (sevr > 0)
            std::cerr<<std::setw(logColW[0])<< std::left<< nowTime.substr(0,30)
                <<std::setw(logColW[1])<< std::left<< pid
                <<std::setw(logColW[2])<< std::left<< tid
                <<std::setw(logColW[3])<< std::left<< std::string(tag).substr(0,19)            
                << std::left<< description
                << std::endl;        
    }
    else
    {
        logOut<<std::setw(logColW[0])<< std::left<< nowTime.substr(0,30)
            <<std::setw(logColW[1])<< std::left<< pid
            <<std::setw(logColW[2])<< std::left<< tid
            <<std::setw(logColW[3])<< std::left<< std::string(tag).substr(0,19)
            <<std::setw(logColW[4])<< std::left<< sevStr[sevr]
            << std::left<< description
            << std::endl;
    }
}

void TOSDB_LogEx_( DWORD pid, DWORD tid, Severity sevr, LPCSTR tag,  LPCSTR description, DWORD error) 
{    
    TOSDB_Log_( pid, tid, sevr, tag, std::string(description).append(" ERROR# ").append(std::to_string(error)).c_str());
}

void TOSDB_Log_Raw_( LPCSTR description ) 
{
    if( !logOut.is_open() ){
        std::cerr<<SysTimeString()<<'\t'<<description<<std::endl;
        return;
    }        
    logOut<< std::left<<SysTimeString()<<'\t'<< description << std::endl;
}
