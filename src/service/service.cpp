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
const unsigned int ACL_SIZE = 96;
const unsigned int UPDATE_PERIOD = 2000;  
const unsigned int MAX_ARG_SIZE = 20;
    
LPSTR SERVICE_NAME = "TOSDataBridge"; 
LPCSTR ENGINE_BASE_NAME = "tos-databridge-engine";

#ifdef LOG_BACKEND_USE_SINGLE_FILE
LPCSTR LOG_NAME = LOG_BACKEND_SINGLE_FILE_NAME;
#else
LPCSTR LOG_NAME = "service-log.log";
#endif 

#ifdef REDIRECT_STDERR_TO_LOG
LPCSTR ERR_LOG_NAME = "service-stderr.log";
#endif

std::string engine_path;
std::string integrity_level; 

std::unique_ptr<IPCMaster> master;

PROCESS_INFORMATION engine_pinfo;      
SYSTEM_INFO sys_info;
SERVICE_STATUS service_status;
SERVICE_STATUS_HANDLE service_status_hndl;

HINSTANCE hinstance = NULL;
    
volatile bool shutdown_flag = false;  
volatile bool pause_flag = false;
volatile bool is_service = true;

int custom_session = -1; 


void 
UpdateStatus(int status, int check_point)
{
    BOOL ret;

    if(check_point < 0)
        ++service_status.dwCheckPoint;
    else 
        service_status.dwCheckPoint = check_point;

    if(status >= 0) 
        service_status.dwCurrentState = status; 

    ret = SetServiceStatus(service_status_hndl, &service_status);
    if(!ret){
        TOSDB_LogH("ADMIN","error setting service status");
        service_status.dwCurrentState = SERVICE_STOPPED;
        service_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        service_status.dwServiceSpecificExitCode = 2;
        ++service_status.dwCheckPoint;

        shutdown_flag = true;
        TerminateProcess(engine_pinfo.hProcess, EXIT_FAILURE);
        TOSDB_LogH("SHUTDOWN","UpdateStatus terminating engine"); 

        UpdateStatus(SERVICE_STOPPED, -1);
        TOSDB_Log("STATE","SERVICE_STOPPED"); 
    }
}


bool 
SendMsgWaitForResponse(long msg)
{
    long ret;
    
    std::string ipc_msg = std::to_string(msg);

    if(!master){
        master = std::unique_ptr<IPCMaster>( new IPCMaster(TOSDB_COMM_CHANNEL) );
        TOSDB_LogDebug("***IPC*** SERVICE - CHECK CONNECTED (IN)");
        if( !master->connected(TOSDB_DEF_TIMEOUT) ){
            TOSDB_LogH("IPC", "created IPCMaster is not connected");
            return false;
        }
        TOSDB_LogDebug("***IPC*** SERVICE - CHECK CONNECTED (OUT)");
    }    

    TOSDB_LogDebug("***IPC*** SERVICE - CALL (IN)");
    if( !master->call(&ipc_msg, TOSDB_DEF_TIMEOUT) ){
         TOSDB_LogH("IPC",("master.call failed in SendMsgWaitForResponse, msg:" + ipc_msg).c_str());
         return false;
    }
    TOSDB_LogDebug("***IPC*** SERVICE - CALL (OUT)");

    try{
        ret = std::stol(ipc_msg);
        return (ret == TOSDB_SIG_GOOD);
    }catch(...){
        TOSDB_LogH("IPC", "failed to convert return message to long in SendMsgWaitForResponse");
        return false;
    }    
}


VOID WINAPI 
ServiceController(DWORD cntrl)
{
    switch(cntrl){ 
    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:     

        shutdown_flag = true;

        TOSDB_Log("STATE","SERVICE_STOP_PENDING");
        UpdateStatus(SERVICE_STOP_PENDING, -1); 

        if(pause_flag){ 
            /* if we're paused... get it to continue silently */              
            if(!SendMsgWaitForResponse(TOSDB_SIG_CONTINUE))            
                TOSDB_Log("ADMIN", "error resuming paused service stop it");              
        }
               
        if(!SendMsgWaitForResponse(TOSDB_SIG_STOP)){        
            TOSDB_Log("ADMIN","failed to send stop signal to engine");                  
            TerminateProcess(engine_pinfo.hProcess, EXIT_FAILURE);
        }        

        break;
       
    case SERVICE_CONTROL_PAUSE: 

        if(pause_flag)
            break;       

        TOSDB_Log("STATE","SERVICE_PAUSE_PENDING");
        UpdateStatus(SERVICE_PAUSE_PENDING, -1);    
      
        if(SendMsgWaitForResponse(TOSDB_SIG_PAUSE)){
            TOSDB_Log("STATE","SERVICE_PAUSED");
            UpdateStatus(SERVICE_PAUSED, -1);
            pause_flag = true;
        }else{            
            TOSDB_LogH("ADMIN", "failed to send pause signal to engine");                        
        }        

        break;
         
    case SERVICE_CONTROL_CONTINUE: 

        if(!pause_flag)
            break;    
        
        TOSDB_Log("STATE","SERVICE_CONTINUE_PENDING");
        UpdateStatus(SERVICE_CONTINUE_PENDING, -1);        
              
        if(SendMsgWaitForResponse(TOSDB_SIG_CONTINUE)){
            TOSDB_Log("STATE","SERVICE_RUNNING");
            UpdateStatus(SERVICE_RUNNING, -1);
            pause_flag = false;
        }else{
            TOSDB_Log("ADMIN","failed to send continue signal to engine");
        }

        break;
    }    
}


#define SPAWN_ERROR_CHECK(ret, msg) do{ \
    if(!ret){ \
        errno_t e = GetLastError(); \
        TOSDB_LogEx("SPAWN", msg, e); \
        goto cleanup_and_exit; \
    } \
}while(0)    


bool 
SpawnRestrictedProcess(std::string engine_cmd, int session = -1)
{            
    TOKEN_MANDATORY_LABEL tml;
    STARTUPINFO  startup_info;            
    SID_NAME_USE dummy;
    PTOKEN_PRIVILEGES tpriv;    
    LUID cg_id;    
    BOOL ret;

    HANDLE tkn_hndl = NULL; 
    HANDLE ctkn_hndl = NULL;
    DWORD tpriv_len = 0;
    DWORD session_id = 0;    
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;
    DWORD dom_sz = 256;
    bool success = false;

    SmartBuffer<VOID> sid_buf(sid_sz);
    SmartBuffer<CHAR> dom_buf(dom_sz);       
    SmartBuffer<CHAR> cmd_buf(engine_path.size() + engine_cmd.size() + 3);
    SmartBuffer<TOKEN_PRIVILEGES> tpriv_buf;

    ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tkn_hndl);
    SPAWN_ERROR_CHECK(ret,"(1) failed to open token handle");                    
 
    ret = DuplicateTokenEx(tkn_hndl, 0, NULL, SecurityImpersonation, TokenPrimary, &ctkn_hndl);
    SPAWN_ERROR_CHECK(ret,"(2) failed to duplicate token");

    /* try to drop our integrity level from System */ 
    ret = LookupAccountName(NULL, integrity_level.c_str(), sid_buf.get(),  
                            &sid_sz, dom_buf.get(), &dom_sz, &dummy);
    SPAWN_ERROR_CHECK(ret,"(3a) failed to lookup Mandatory Level group");       
    
    tml.Label.Attributes = SE_GROUP_INTEGRITY;
    tml.Label.Sid = sid_buf.get();    

    ret = SetTokenInformation(ctkn_hndl, TokenIntegrityLevel, &tml, 
                              sizeof(TOKEN_MANDATORY_LABEL) + GetLengthSid(sid_buf.get()));
    SPAWN_ERROR_CHECK(ret,"(3b) failed to set Integrity Level");
    
    session_id = (session < 0) ? WTSGetActiveConsoleSessionId() : session;         
        
    /* try to get out of Session 0 isolation if we need to */
    if(is_service){        
        std::string sess_id_msg = std::string("(4) set session id: ") 
                                + std::to_string(session_id)
                                + std::string(" (this may may fail during (6) 'CreateProcessAsUser')"); 
        TOSDB_Log("SPAWN", sess_id_msg.c_str());

        ret = SetTokenInformation(ctkn_hndl,TokenSessionId,&session_id,sizeof(DWORD));
        SPAWN_ERROR_CHECK(ret,"(4) failed to set session ID");    
    }                     
    
    /* LUID of the privilege we need to retain for creating global kernel objects*/
    ret = LookupPrivilegeValue(NULL, SE_CREATE_GLOBAL_NAME, &cg_id);
    SPAWN_ERROR_CHECK(ret,"(5a) failed to lookup privilege value: SE_CREATE_GLOBAL_NAME");
  
    /* get size needed for privileges buffer - NO ERROR CHECK (this fails on success)*/
    GetTokenInformation(ctkn_hndl, TokenPrivileges, NULL, 0, &tpriv_len);
             
    /* fill buffer with privileges info */
    tpriv_buf =  SmartBuffer<TOKEN_PRIVILEGES>(tpriv_len); 
    ret = GetTokenInformation(ctkn_hndl, TokenPrivileges, tpriv_buf.get(), tpriv_len, &tpriv_len);
    SPAWN_ERROR_CHECK(ret,"(5b) failed to get TokenPrivileges information");
 
    tpriv = tpriv_buf.get();        
    for(DWORD i = 0; i < tpriv->PrivilegeCount; ++i){ 
        /* set the removed attribute on all but the one(s) to retain */
        if( tpriv->Privileges[i].Luid.LowPart != cg_id.LowPart 
            || tpriv->Privileges[i].Luid.HighPart != cg_id.HighPart)
        {             
            tpriv->Privileges[i].Attributes = SE_PRIVILEGE_REMOVED;             
        }
    }
       
    /* remove the privileges from the token */
    ret = AdjustTokenPrivileges(ctkn_hndl, FALSE, tpriv_buf.get(),0, NULL, NULL);
    SPAWN_ERROR_CHECK(ret,"(5c) failed to adjust token privileges");

    GetStartupInfo(&startup_info);       
        
    /*prepend path to command string*/
    strcpy(cmd_buf.get(), (engine_path + " " + engine_cmd).c_str());

    /* try to create the process with the new token */
    ret = CreateProcessAsUser(ctkn_hndl, engine_path.c_str(), cmd_buf.get(), NULL, NULL, 
                              FALSE, 0, NULL, NULL, &startup_info, &engine_pinfo);
    SPAWN_ERROR_CHECK(ret,"(6) failed to create engine process with new token");

    success = true;  

    cleanup_and_exit:
        if(tkn_hndl) 
            CloseHandle(tkn_hndl);
        if(ctkn_hndl) 
            CloseHandle(ctkn_hndl);     

    return success;
}    

};


void WINAPI 
ServiceMain(DWORD argc, LPSTR argv[])
{
    bool good_engine;

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
                      
    TOSDB_Log("STARTUP", "try to start tos-databridge-engine.exe (AS A SERVICE)"); 

    /* create new process that can communicate with our interface */
    good_engine = SpawnRestrictedProcess("--spawned --service", custom_session);

    if(!good_engine){
        /* FAILURE */
        shutdown_flag = true;
        std::string serr("failed to spawn ");
        serr.append(engine_path).append(" --spawned --service");
        TOSDB_LogH("STARTUP", serr.c_str());         
    }else{
        /* SUCCESS */
        UpdateStatus(SERVICE_RUNNING, -1);
        TOSDB_Log("STATE","SERVICE_RUNNING");

        /* MAIN UPDATE LOOP */           
        do{
            /* first check if engine is running */
            if( WaitForSingleObject(engine_pinfo.hProcess, 0) != WAIT_TIMEOUT){
                TOSDB_LogH("SHUTDOWN", "service believes engine closed unexpectedly");
                shutdown_flag = true;
                break;
            }
            Sleep(UPDATE_PERIOD);
            UpdateStatus(-1, -1);            
        }while(!shutdown_flag);

        if( WaitForSingleObject(engine_pinfo.hProcess, TOSDB_DEF_TIMEOUT * 2) == WAIT_TIMEOUT){                    
           /* forcefully close the engine */
           TOSDB_LogH("SHUTDOWN", "engine took too long to shutdown, terminated");
           TerminateProcess(engine_pinfo.hProcess, EXIT_FAILURE);     
        }
       
        TOSDB_LogH("SHUTDOWN", "service believes engine has shutdown");
    }  

    /*** IF WE GET HERE WE WILL SHUTDOWN ***/

    UpdateStatus(SERVICE_STOPPED, 0);    
    TOSDB_Log("STATE","SERVICE_STOPPED");    
}


int WINAPI 
WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLn, int nShowCmd)
{     
    bool good_engine;    
    std::vector<std::string> args;    

    std::string cmd_str(lpCmdLn);    
    std::string logpath(TOSDB_LOG_PATH);

#ifdef REDIRECT_STDERR_TO_LOG
    freopen( (logpath + ERR_LOG_NAME).c_str(), "a", stderr);
#endif

    /* start  logging */
    logpath.append(LOG_NAME);
    StartLogging(logpath.c_str()); 

    /* get 'our' module name */
    SmartBuffer<CHAR> module_buf(MAX_PATH);
    GetModuleFileName(NULL, module_buf.get(), MAX_PATH); 

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
    ss_args << "argc: " << std::to_string(argc) 
            << " custom_session: " << std::to_string(custom_session) 
            << " admin_pos: " << std::to_string(admin_pos) 
            << " no_service_pos: " << std::to_string(no_service_pos);
		
    TOSDB_Log("STARTUP", std::string("lpCmdLn: ").append(cmd_str).c_str() );
    TOSDB_Log("STARTUP", ss_args.str().c_str() );

    integrity_level = admin_pos > 0 ? "High Mandatory Level" : "Medium Mandatory Level"; 
    
    /* populate the engine command and if --noservice is passed jump right into
       the engine via SpawnRestrictedProcess; otherwise Start the service which
       will handle that for us 
       
       prepend '--spawned' so engine knows *we* called it; if someone
       else passes '--spawned' they deserve what they get */

    if(no_service_pos > 0){       
        is_service = false;

        TOSDB_Log("STARTUP", "starting tos-databridge-engine.exe directly(NOT A SERVICE)");

        good_engine = SpawnRestrictedProcess("--spawned --noservice", custom_session); 
        if(!good_engine){      
            std::string serr("failed to spawn ");
            serr.append(engine_path).append(" --spawned --noservice");
            TOSDB_LogH("STARTUP", serr.c_str());         
        }
    }else{    
        SERVICE_TABLE_ENTRY dTable[] = {
            {SERVICE_NAME,ServiceMain},
            {NULL,NULL}
        };        

        /* START SERVICE */	
        if( !StartServiceCtrlDispatcher(dTable) ){
            TOSDB_LogH("STARTUP", "StartServiceCtrlDispatcher() failed. "  
                                  "(Be sure to use an appropriate Window's tool to start the service "
                                  "(e.g SC.exe, Services.msc) or pass '--noservice' to run directly.)");
        }
    }

    TOSDB_Log("SHUTDOWN","service exiting");
    StopLogging();

#ifdef REDIRECT_STDERR_TO_LOG
    fclose(stderr);
#endif

    return 0;
}
    
