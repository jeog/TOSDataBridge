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

#include <algorithm>
#include <ostream> //dbg
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include "tos_databridge.h"
#include "ipc.hpp"
#include "concurrency.hpp"

namespace {

const unsigned int COMM_BUFFER_SIZE = 4;
const unsigned int ACL_SIZE         = 96;
const unsigned int UPDATE_PERIOD    = 2000;  
const unsigned int MAX_ARG_SIZE     = 20;
    
LPSTR   SERVICE_NAME      = "TOSDataBridge"; 
LPCSTR  LOG_NAME          = "service-log.log";
LPCSTR  ENGINE_BASE_NAME  = "tos-databridge-engine";

std::string engine_path;
std::string integrity_level; 

char engine_cmd[12];

PROCESS_INFORMATION   engine_pinfo;      
SYSTEM_INFO           sys_info;
SERVICE_STATUS        service_status;
SERVICE_STATUS_HANDLE service_status_hndl;
HINSTANCE             hinstance = NULL;
    
volatile bool shutdown_flag =  false;  
volatile bool pause_flag    =  false;
volatile bool is_service    =  true;

int custom_session = -1; 

typedef std::unique_ptr<DynamicIPCMaster> mstr_ptr_ty;
mstr_ptr_ty master;


void 
UpdateStatus(int status, int check_point)
{
    if(check_point < 0)
        ++service_status.dwCheckPoint;
    else 
        service_status.dwCheckPoint = check_point;

    if(status >= 0) 
        service_status.dwCurrentState = status; 

    if( !SetServiceStatus(service_status_hndl, &service_status) )
    {
        TOSDB_LogH("ADMIN","error setting status");
        service_status.dwCurrentState = SERVICE_STOPPED;
        service_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        service_status.dwServiceSpecificExitCode = 2;
        ++service_status.dwCheckPoint;

        if( !SetServiceStatus(service_status_hndl, &service_status) ){
            TOSDB_LogH("ADMIN", "fatal error handling service error");
            TerminateProcess(engine_pinfo.hProcess, EXIT_FAILURE);            
            ExitProcess(EXIT_FAILURE);
        }
    }
}


long 
SendMsgWaitForResponse(long msg)
{
    long i;
    unsigned int lcount = 0;

    if(!master){
        master = mstr_ptr_ty( new DynamicIPCMaster(TOSDB_COMM_CHANNEL) );
        master->try_for_slave();
        // ERROR CHECK
        IPC_TIMED_WAIT( master->grab_pipe() <= 0,
                        "SendMsgWaitForResponse timed out",
                        -1 );     
    }    
    master->send(msg);
    master->send(0); 
    master->recv(i);
    return i;
}

VOID WINAPI 
ServiceController(DWORD cntrl)
{
    switch(cntrl){ 
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP :
    {            
        shutdown_flag = true;

        TOSDB_Log("STATE","SERVICE_STOP_PENDING");
        UpdateStatus(SERVICE_STOP_PENDING, -1); 

        if(pause_flag){ /* if we're paused... get it to continue silently */                
            if( SendMsgWaitForResponse(TOSDB_SIG_CONTINUE) != TOSDB_SIG_GOOD )            
                TOSDB_Log("ADMIN", "error resuming paused thread to stop it");              
        }
        if( SendMsgWaitForResponse(TOSDB_SIG_STOP) != TOSDB_SIG_GOOD )        
            TOSDB_Log("ADMIN","BAD_SIG returned from core process");   

        break;
    }     
    case SERVICE_CONTROL_PAUSE:
    {        
        if(pause_flag)
            break;
        pause_flag = true;

        TOSDB_Log("STATE","SERVICE_PAUSE_PENDING");
        UpdateStatus(SERVICE_PAUSE_PENDING, -1);
             
        if( SendMsgWaitForResponse(TOSDB_SIG_PAUSE) == TOSDB_SIG_GOOD ){
            TOSDB_Log("STATE","SERVICE_PAUSED");
            UpdateStatus(SERVICE_PAUSED, -1);
        }else{            
            TOSDB_LogH("ADMIN", "engine failed to confirm pause msg");                        
        }

        break;
    }     
    case SERVICE_CONTROL_CONTINUE:
    {
        if(!pause_flag)
            break;    
        
        TOSDB_Log("STATE","SERVICE_CONTINUE_PENDING");
        UpdateStatus(SERVICE_CONTINUE_PENDING, -1);

        if(!master){
            TOSDB_LogH("ADMIN","we don't own the slave");
            break;
        }            

        if( SendMsgWaitForResponse(TOSDB_SIG_CONTINUE) == TOSDB_SIG_GOOD ){
            TOSDB_Log("STATE","SERVICE_RUNNING");
            UpdateStatus(SERVICE_RUNNING, -1);
            pause_flag = false;
        }else{
            TOSDB_Log("ADMIN","BAD_SIG returned from core process");
        }

        master->release_pipe();
        master.reset();

        break;
    }    
    default: 
        break;
    }    
}

#define SPAWN_LOG_EX(msg) TOSDB_LogEx("SPAWN",msg,GetLastError())

bool 
SpawnRestrictedProcess(int session = -1)
{            
    STARTUPINFO  startup_info;            
    SID_NAME_USE dummy;

    HANDLE tkn_hndl  = NULL; 
    HANDLE ctkn_hndl = NULL;
    DWORD session_id = 0;    
    DWORD sid_sz     = SECURITY_MAX_SID_SIZE;
    DWORD dom_sz     = 256;
    bool  ret        = false;

    SmartBuffer<VOID>    sid_buf(sid_sz);
    SmartBuffer<CHAR>    dom_buf(dom_sz);            

    if( !OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tkn_hndl) ){
        SPAWN_LOG_EX("(1) failed to open token handle");                    
        return false;     
    }

    if( !DuplicateTokenEx(tkn_hndl, 0, NULL, SecurityImpersonation, 
                          TokenPrimary, &ctkn_hndl) ){        
        if(tkn_hndl) 
             CloseHandle(tkn_hndl);   
        SPAWN_LOG_EX("(2) failed to duplicate token");
        return false; 
    }         
    
    /* try to drop our integrity level from System */
    if( LookupAccountName(NULL, integrity_level.c_str(), sid_buf.get(), 
                          &sid_sz, dom_buf.get(), &dom_sz, &dummy) )
    {
        TOKEN_MANDATORY_LABEL tl;
        tl.Label.Attributes = SE_GROUP_INTEGRITY;
        tl.Label.Sid = sid_buf.get();
        DWORD sz = sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(sid_buf.get());

        if( !SetTokenInformation(ctkn_hndl, TokenIntegrityLevel, &tl, sz) )
            SPAWN_LOG_EX("(3) failed to set Integrity Level");        
    }else{ 
        SPAWN_LOG_EX("(3) failed to lookup Mandatory Level group");
    }    
    
    session_id = (session < 0) ? WTSGetActiveConsoleSessionId() : session;         
    
     /* try to get out of Session 0 isolation if we need to */
    if( is_service    
        && !SetTokenInformation(ctkn_hndl,TokenSessionId,&session_id,sizeof(DWORD)) )
    { 
        SPAWN_LOG_EX("(4) failed to set session ID");    
    }
    else{ /* remove privileges */                        
        DWORD ret_len;
        PTOKEN_PRIVILEGES tpriv;         
        LUID cg_id;    /*    <-- THE PRIVILEGE(S) WE NEED TO RETAIN    */

        LookupPrivilegeValue(NULL, SE_CREATE_GLOBAL_NAME, &cg_id);                         
        GetTokenInformation(ctkn_hndl, TokenPrivileges, NULL, 0, &ret_len);

        SmartBuffer<TOKEN_PRIVILEGES> priv_buf(ret_len);    
        GetTokenInformation(ctkn_hndl, TokenPrivileges, priv_buf.get(), 
                            ret_len, &ret_len);        

        tpriv = priv_buf.get();        
        for(DWORD i = 0; i < tpriv->PrivilegeCount; ++i){ 
            /* set the removed attribute on all but the one(s) to retain */
            if( tpriv->Privileges[i].Luid.LowPart != cg_id.LowPart 
                || tpriv->Privileges[i].Luid.HighPart != cg_id.HighPart)
            {             
                tpriv->Privileges[i].Attributes = SE_PRIVILEGE_REMOVED;             
            }
        }
        
        if( !AdjustTokenPrivileges(ctkn_hndl, FALSE, priv_buf.get(),0, NULL, NULL) )
            SPAWN_LOG_EX("(5) failed to adjust token privileges");    
           

        GetStartupInfo(&startup_info);       
        /* try to create the process with the new token */
        if( !CreateProcessAsUser(ctkn_hndl, engine_path.c_str(), engine_cmd, NULL, NULL, 
                                 FALSE, 0, NULL, NULL, &startup_info, &engine_pinfo) ){  
            SPAWN_LOG_EX("(6) failed to create core process");
        }else{ 
            ret = true; 
        }
    }   
         
    if(tkn_hndl) 
        CloseHandle(tkn_hndl);

    if(ctkn_hndl) 
        CloseHandle(ctkn_hndl);     

    return ret;
}    
};

void WINAPI 
ServiceMain(DWORD argc, LPSTR argv[])
{
    service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    service_status.dwCurrentState = SERVICE_START_PENDING;
    service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP 
                                        | SERVICE_ACCEPT_PAUSE_CONTINUE
                                        | SERVICE_ACCEPT_SHUTDOWN;
    service_status.dwWin32ExitCode  = NO_ERROR; 
    service_status.dwServiceSpecificExitCode = 0;
    service_status.dwCheckPoint = 0;
    service_status.dwWaitHint = (DWORD)(2.5 * UPDATE_PERIOD); 

    service_status_hndl = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceController); 
    if(!service_status_hndl){
        TOSDB_LogH("ADMIN","failed to register control handler, exiting");
        service_status.dwCurrentState = SERVICE_STOPPED;
        service_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        service_status.dwServiceSpecificExitCode = 1;
        UpdateStatus(SERVICE_STOPPED, -1);
        return;
    }
    TOSDB_Log("ADMIN","successfully registered control handler");

    SetServiceStatus(service_status_hndl, &service_status);
    TOSDB_Log("STATE","SERVICE_START_PENDING");
    TOSDB_Log("ADMIN","starting service update loop on its own thread");
        
    std::async( std::launch::async, /* spin off the basic service update loop */
                [&]{
                    while(!shutdown_flag){
                        Sleep(UPDATE_PERIOD);
                        UpdateStatus(-1, -1); 
                    } 
                } );

    /* create new process that can communicate with our interface */
    if(!SpawnRestrictedProcess(custom_session)){      
        std::string serr("failed to spawn ");
        serr.append(engine_path);
        TOSDB_LogH("ADMIN", serr.c_str());         
    }else{
        /* on success, update and block */
        UpdateStatus(SERVICE_RUNNING, -1);
        TOSDB_Log("STATE","SERVICE_RUNNING");
        WaitForSingleObject(engine_pinfo.hProcess, INFINITE);
    }

    /* !! IF WE GET HERE WE WILL SHUTDOWN !! */
    UpdateStatus(SERVICE_STOPPED, 0);
    /* !! IF WE GET HERE WE WILL SHUTDOWN !! */

    TOSDB_Log("STATE","SERVICE_STOPPED");    
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

int WINAPI 
WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLn, int nShowCmd)
{     
    std::string cmd_str(lpCmdLn);
    std::vector<std::string> args;    

    /* get 'our' module name */
    SmartBuffer<CHAR> module_buf(MAX_PATH);
    GetModuleFileName(NULL, module_buf.get(), MAX_PATH);

    /* start  logging */
    std::string logpath(TOSDB_LOG_PATH);
    logpath.append(LOG_NAME);
    StartLogging(logpath.c_str()); 

    /* add the appropriate engine name to the module path */
    std::string path(module_buf.get()); 
    std::string serv_ext(path.begin() + path.find_last_of("-"), path.end()); 
#ifdef _DEBUG
    if(serv_ext != "-x86_d.exe" && serv_ext != "-x64_d.exe"){
#else
    if(serv_ext != "-x86.exe" && serv_ext != "-x64.exe"){
#endif	
        TOSDB_LogH("STARTUP", "service path doesn't provide proper extension");
        return 1;
    }

    path.erase(path.find_last_of("\\")); 
    engine_path = path;
    engine_path.append("\\").append(ENGINE_BASE_NAME).append(serv_ext); 

    GetSystemInfo(&sys_info);    
    ParseArgs(args,cmd_str);
    
    size_t argc = args.size();
    int admin_pos = 0;
    int no_service_pos = 0;
	

    /* look for --admin and/or --noservice args */
    if(argc > 0 && args[0] == "--admin")            
        admin_pos = 1;
    else if(argc > 1 && args[1] == "--admin") 
        admin_pos = 2;
    
    if(argc > 0 && args[0] == "--noservice")            
        no_service_pos = 1;
    else if(argc > 1 && args[1] == "--noservice") 
        no_service_pos = 2;
        

    switch(argc){ /* look for custom_session arg */
    case 0:
        break;
    case 1:
        if(admin_pos == 0 && no_service_pos == 0)
            custom_session = std::stoi(args[0]);
        break;
    case 2:
        if( (admin_pos == 1 && no_service_pos != 2) 
            || (no_service_pos == 1 && admin_pos != 2))
        {
            custom_session = std::stoi(args[1]);
        }
        break;
    case 3:
        if(admin_pos > 0 && no_service_pos > 0)
            custom_session = std::stoi(args[2]);
        break;
    default:
        std::string serr("invalid # of args: ");
        serr.append(std::to_string(argc));
        TOSDB_LogH("STARTUP",serr.c_str());
        return 1;
    }     
   
    std::stringstream ss_args;
    ss_args << "cmd_str: " << cmd_str << " argc: " << std::to_string(argc) 
            << " custom_session: " << std::to_string(custom_session) 
            << " admin_pos: " << std::to_string(admin_pos) 
            << " no_service_pos: " << std::to_string(no_service_pos);
		    
    TOSDB_Log("ARGS", ss_args.str().c_str() );

    integrity_level = admin_pos > 0 
                    ? "High Mandatory Level" 
                    : "Medium Mandatory Level"; 
    
    /* populate the engine command and if --noservice is passed jump right into
       the engine via SpawnRestrictedProcess; otherwise Start the service which
       will handle that for us */

    if(no_service_pos > 0){
        strcpy_s(engine_cmd,"--noservice");
        is_service = false;
        TOSDB_Log("STARTUP", "tos-databridge-engine.exe starting(NOT AS A SERVICE)");
        SpawnRestrictedProcess(custom_session); 
    }else{
        strcpy_s(engine_cmd,"--service");
        SERVICE_TABLE_ENTRY dTable[] = {
            {SERVICE_NAME,ServiceMain},
            {NULL,NULL}
        };
        TOSDB_Log("STARTUP", "tos-databridge-engine.exe starting(AS A SERVICE)");
        /* START SERVICE */	
        if( !StartServiceCtrlDispatcher(dTable) )
            TOSDB_LogH("STARTUP", "StartServiceCtrlDispatcher() failed");
    }

    StopLogging();
    return 0;
}
    
