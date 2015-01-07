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

#include <algorithm>
#include <ostream> //dbg
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include "tos_databridge.h"
#include "engine.hpp"
#include "dynamic_ipc.hpp"
#include "concurrency.hpp"

namespace {
        
    LPCSTR    LOG_NAME        = "service-log.log";
#ifdef _WIN64
    LPCSTR    CORE_PROC_NAME  = "tos-databridge-engine-x64.exe";
#else
    LPCSTR    CORE_PROC_NAME  = "tos-databridge-engine-x86.exe";
#endif
    std::string coreProcPath;
    std::string integrityLevel;
    
    LPSTR serviceName = "TOSDataBridge";    
    char  coreProcCmd[12];

    PROCESS_INFORMATION coreProcInfo;
         
    const unsigned int COMM_BUFFER_SIZE = 4;
    const unsigned int ACL_SIZE         = 96;
    const unsigned int UPDATE_PERIOD    = 2000;    
    const unsigned int MAX_ARG_SIZE     = 20;
        
    HINSTANCE hInstance = NULL;
    SYSTEM_INFO sysInfo;
        
    volatile bool shutdownFlag = false;    
    volatile bool pauseFlag = false;
    volatile bool isService = true;

    SERVICE_STATUS serviceStatus;
    SERVICE_STATUS_HANDLE hServiceStatus; 

    std::unique_ptr<DynamicIPCMaster> globalIPCMaster;

void UpdateStatus( int status, int check_point )
{
    if( check_point < 0 )
        ++serviceStatus.dwCheckPoint;
    else 
        serviceStatus.dwCheckPoint = check_point;

    if( status >= 0) 
        serviceStatus.dwCurrentState = status; 

    if( !SetServiceStatus( hServiceStatus, &serviceStatus ) )
    {
        TOSDB_LogH("SERVICE-UPDATE","error setting status");
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        serviceStatus.dwServiceSpecificExitCode = 2;
        ++serviceStatus.dwCheckPoint;
        if( !SetServiceStatus( hServiceStatus, &serviceStatus) )
        {
            TOSDB_LogH( "SERVICE-UPDATE",
                        "fatal error handling service error, "
                        "terminating process");
            TerminateProcess( coreProcInfo.hProcess, EXIT_FAILURE );            
            ExitProcess( EXIT_FAILURE );
        }
    }
}

int SendMsgWaitForResponse( const unsigned int msg )
{
    int i;
    unsigned int lCount = 0;

    if( !globalIPCMaster )
    {
        globalIPCMaster = 
            std::unique_ptr<DynamicIPCMaster>(
                std::move( new DynamicIPCMaster( TOSDB_COMM_CHANNEL )));

        globalIPCMaster->try_for_slave();

        while( globalIPCMaster->grab_pipe() <= 0 )
        {
            Sleep(TOSDB_DEF_TIMEOUT/10);            
            if( (lCount+=(TOSDB_DEF_TIMEOUT/10)) > TOSDB_DEF_TIMEOUT )
            {
                TOSDB_LogH( "IPC",
                            "SendMsgWaitForResponse timed out "
                            "trying to grab pipe" );
                return -1;
            }
        }
    }    
 
    globalIPCMaster->send(msg);
    globalIPCMaster->send(0); 
    globalIPCMaster->recv(i);

    return i;
}

VOID WINAPI ServiceController( DWORD cntrl )
{
    switch( cntrl )
    { 
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP :
        {            
            shutdownFlag = true;
            TOSDB_Log("SERVICE-CNTRL","SERVICE_STOP_PENDING");
            UpdateStatus( SERVICE_STOP_PENDING, -1); 
            /* if we're paused */
            if( pauseFlag )
            {    /* get it to continue silently */                
                if( SendMsgWaitForResponse( TOSDB_SIG_CONTINUE ) 
                    != TOSDB_SIG_GOOD)
                        TOSDB_Log( "SERVICE-ADMIN", 
                                   "error trying to resume paused "
                                   "thread to stop it" );                
            }            
            if( SendMsgWaitForResponse( TOSDB_SIG_STOP ) != TOSDB_SIG_GOOD)
                TOSDB_Log( "SERVICE-ADMIN",
                           "BAD_SIG returned from core process" );            
        }
        break;
    case SERVICE_CONTROL_PAUSE:
        {        
            if( pauseFlag )
                break;

            pauseFlag = true;
            TOSDB_Log("SERVICE-CNTRL","SERVICE_PAUSE_PENDING");
            UpdateStatus( SERVICE_PAUSE_PENDING, -1); 
           
            if( SendMsgWaitForResponse( TOSDB_SIG_PAUSE ) == TOSDB_SIG_GOOD )
            {
                TOSDB_Log("SERVICE-CNTRL","SERVICE_PAUSED");
                UpdateStatus( SERVICE_PAUSED, -1);
            }
            else            
                TOSDB_LogH( "SERVICE-ADMIN",
                            "core process failed to confirm pause request" );                        
        }
        break;
    case SERVICE_CONTROL_CONTINUE:
        {
            if( !pauseFlag )
                break;    
        
            TOSDB_Log("SERVICE-CNTRL","SERVICE_CONTINUE_PENDING");
            UpdateStatus( SERVICE_CONTINUE_PENDING, -1);

            if( !globalIPCMaster )
            {
                TOSDB_LogH("SERVICE-ADMIN","we don't own the slave");
                break;
            }            
            if( SendMsgWaitForResponse( TOSDB_SIG_CONTINUE ) == TOSDB_SIG_GOOD)
            {
                TOSDB_Log("SERVICE-CNTRL","SERVICE_RUNNING");
                UpdateStatus( SERVICE_RUNNING, -1);
                pauseFlag = false;
            }
            else
                TOSDB_Log("SERVICE-ADMIN","BAD_SIG returned from core process");

            globalIPCMaster->release_pipe();
            globalIPCMaster.reset();
        }
        break;
    default:
        break;
    }
}

bool SpawnRestrictedProcess()
{            
    STARTUPINFO  stupInfo;            
    SID_NAME_USE sidUseDummy;
    HANDLE       hToken      = NULL; 
    HANDLE       hChildToken = NULL;
    DWORD        sessionID   = 0;
    bool         retVal      = false;
    DWORD        sidSz       = SECURITY_MAX_SID_SIZE;
    DWORD        domSz       = 256;

    SmartBuffer<VOID>  sidBuffer(sidSz);
    SmartBuffer<CHAR>  domBuffer(domSz);            

    if( !OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken) ) 
    {
        TOSDB_LogEx( "SpawnRestrictedProcess", 
                     "(1) failed to open token handle", 
                     GetLastError());
        return false;     
    }

    if( !DuplicateTokenEx( hToken, 
                           0, 
                           NULL, 
                           SecurityImpersonation, 
                           TokenPrimary, 
                           &hChildToken ) )
        {        
            if( hToken ) 
                CloseHandle(hToken);  
            TOSDB_LogEx( "SpawnRestrictedProcess", 
                         "(2) failed to duplicate token", 
                         GetLastError() );
            return false; 
        }  
   
    /* try to drop our integrity level from System */
    if( LookupAccountName( NULL, 
                           integrityLevel.c_str(),
                           sidBuffer.get(), 
                           &sidSz, 
                           domBuffer.get(), 
                           &domSz, 
                           &sidUseDummy) )
        {     
            TOKEN_MANDATORY_LABEL tl;

            tl.Label.Attributes = SE_GROUP_INTEGRITY;
            tl.Label.Sid = sidBuffer.get();
            DWORD sz = 
                sizeof(TOKEN_MANDATORY_LABEL) + 
                GetLengthSid( sidBuffer.get() );

            if( !SetTokenInformation( hChildToken, 
                                      TokenIntegrityLevel, 
                                      &tl, 
                                      sz ) )
                TOSDB_LogEx( "SpawnRestrictedProcess", 
                             "(3) failed to set Integrity Level", 
                             GetLastError());
        }
    else 
        TOSDB_LogEx("SpawnRestrictedProcess", 
                    "(3) failed to lookup Mandatory Level group", 
                    GetLastError() );
    
    sessionID = WTSGetActiveConsoleSessionId();        
    /* try to get out of Session 0 isolation if we need to */
    if( isService && 
        !SetTokenInformation( hChildToken, 
                              TokenSessionId, 
                              &sessionID,  
                              sizeof(DWORD) ) )    
        {
            TOSDB_LogEx( "SpawnRestrictedProcess", 
                         "(4) failed to set session ID", 
                         GetLastError() );    
        }
    else
    { /* remove privileges */
        DWORD retLen;
        PTOKEN_PRIVILEGES pTpriv;         
        LUID cgID;        /*  THE PRIVILEGE(S) WE NEED TO RETAIN  */

        LookupPrivilegeValue( NULL, SE_CREATE_GLOBAL_NAME, &cgID );
                        /**/
                        /**/
        GetTokenInformation( hChildToken, TokenPrivileges, NULL, 0, &retLen);

        SmartBuffer<TOKEN_PRIVILEGES> privBuf(retLen);    
        GetTokenInformation( hChildToken, 
                             TokenPrivileges, 
                             privBuf.get(), 
                             retLen, 
                             &retLen );                
        pTpriv = privBuf.get();

        /* set the removed attribute on all but the one(s) to retain */
        for( DWORD i = 0; i < pTpriv->PrivilegeCount; ++i ) 
            if( pTpriv->Privileges[i].Luid.LowPart != cgID.LowPart ||
                pTpriv->Privileges[i].Luid.HighPart != cgID.HighPart )
                    pTpriv->Privileges[i].Attributes = SE_PRIVILEGE_REMOVED;             
        
        if( !AdjustTokenPrivileges( hChildToken, 
                                    FALSE, 
                                    privBuf.get(), 
                                    0, 
                                    NULL, 
                                    NULL ) )
            {
                TOSDB_LogEx( "SpawnRestrictedProcess", 
                             "(5) failed to adjust token privileges", 
                             GetLastError() );    
            {

        GetStartupInfo(&stupInfo);

        /* try to create the process with the new token */        
        if( !CreateProcessAsUser( hChildToken, 
                                  coreProcPath.c_str(), 
                                  coreProcCmd, 
                                  NULL, 
                                  NULL, 
                                  FALSE, 
                                  0, 
                                  NULL, 
                                  NULL, 
                                  &stupInfo, 
                                  &coreProcInfo) )            
            {
                TOSDB_LogEx( "SpawnRestrictedProcess", 
                             "(6) failed to create core process", 
                             GetLastError() );
            }     
        else 
            retVal = true; 
    }
            
    if( hToken ) 
        CloseHandle(hToken);
    if( hChildToken ) 
        CloseHandle(hChildToken); 
    
    return retVal;
}    
};

void WINAPI ServiceMain( DWORD argc, LPSTR argv[] )
{
    serviceStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState            = SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted        = SERVICE_ACCEPT_STOP 
                                               | SERVICE_ACCEPT_PAUSE_CONTINUE 
                                               | SERVICE_ACCEPT_SHUTDOWN;
    serviceStatus.dwWin32ExitCode           = NO_ERROR; 
    serviceStatus.dwServiceSpecificExitCode = 0;
    serviceStatus.dwCheckPoint              = 0;
    serviceStatus.dwWaitHint                = (DWORD)(2.5 * UPDATE_PERIOD); 

    hServiceStatus = 
        RegisterServiceCtrlHandler( serviceName, ServiceController ); 
    if( !hServiceStatus )
    {
        TOSDB_LogH( "SERVICE-ADMIN",
                    "failed to register control handler, exiting" );

        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        serviceStatus.dwServiceSpecificExitCode = 1;

        UpdateStatus( SERVICE_STOPPED, -1);
        return;
    }

    TOSDB_Log("SERVICE-ADMIN","successfully registered control handler");

    SetServiceStatus(hServiceStatus, &serviceStatus);

    TOSDB_Log("SERVICE-CNTRL","SERVICE_START_PENDING");
    TOSDB_Log("SERVICE-ADMIN","starting service update loop on its own thread");

    /* simply spin off the basic service update loop */
    std::async( std::launch::async, 
                [&]{
                    while( !shutdownFlag ) {
                        Sleep(UPDATE_PERIOD);
                        UpdateStatus( -1, -1); 
                    }   
                } );

    /* create new process that can communicate with our interface */
    if( !SpawnRestrictedProcess() )
        TOSDB_LogH( "SERVICE-ADMIN", 
                    std::string("failed to fork ").append(CORE_PROC_NAME)
                                                  .c_str());        
    else
    {
        UpdateStatus( SERVICE_RUNNING, -1 );
        TOSDB_Log("SERVICE-CNTRL","SERVICE_RUNNING");
        WaitForSingleObject( coreProcInfo.hProcess, INFINITE);
    }
    /* if we get here we are shutting down*/
    UpdateStatus( SERVICE_STOPPED, 0 );
    TOSDB_Log("SERVICE-CNTRL","SERVICE_STOPPED");    
}

int WINAPI WinMain( HINSTANCE hInst, 
                    HINSTANCE hPrevInst, 
                    LPSTR lpCmdLn, 
                    int nShowCmd )
{            
    std::string cmdLnStr(lpCmdLn);
    std::string arg1, arg2;
    SmartBuffer<CHAR> modBuf(MAX_PATH);

    GetModuleFileName(NULL,modBuf.get(),MAX_PATH);

    std::string path( modBuf.get() );
    path.erase( path.find_last_of("\\") );
    coreProcPath.append(path).append("\\").append( CORE_PROC_NAME );

    TOSDB_StartLogging( std::string(std::string(TOSDB_LOG_PATH) + 
                        std::string(LOG_NAME)).c_str() );

    GetSystemInfo( &sysInfo );  
  
    size_t sIndx = cmdLnStr.find_first_of(' ');
    if( sIndx < cmdLnStr.size() )
    {
        arg1 = cmdLnStr.substr(0, sIndx);
        arg2 = cmdLnStr.substr(++sIndx, cmdLnStr.size());
    }
    else
    {
        arg1 = cmdLnStr;
        arg2 = "";
    }

    if( arg1 == "--admin" || arg2 == "--admin" )    
        integrityLevel = "High Mandatory Level";
    else
        integrityLevel = "Medium Mandatory Level";

    
    if( arg1 == "--noservice" || arg2 == "--noservice" )
    {
        strcpy_s(coreProcCmd,"--noservice");
        isService = false;
        TOSDB_Log( "STARTUP", 
                   "tos-databridge-engine.exe starting - NOT A WINDOWS SERVICE");
        SpawnRestrictedProcess(); 
    }
    else 
    {
        strcpy_s(coreProcCmd,"--service");
        SERVICE_TABLE_ENTRY dTable[] = 
            { {serviceName, ServiceMain}, 
              { NULL, NULL} };

        TOSDB_Log( "STARTUP", 
                   "tos-databridge-engine.exe starting - WINDOWS SERVICE");

        if( !StartServiceCtrlDispatcher( dTable ) )
            TOSDB_LogH("STARTUP", "StartServiceCtrlDispatcher() failed");
    }

    TOSDB_StopLogging();

    return 0;
}
    
