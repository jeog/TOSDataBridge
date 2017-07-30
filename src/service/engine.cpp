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
#include <ostream> /* debug */
#include <thread>
#include <future>
#include <memory>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <cctype>

#include "tos_databridge.h"
#include "ipc.hpp"
#include "concurrency.hpp"

namespace { 

typedef struct{
    void*        hfile;    /* handle to mapping */
    void*        raw_addr; /* physical location in our process space */
    unsigned int raw_sz;   /* physical size of the buffer */
    void*        hmtx;  
} StreamBuffer, *pStreamBuffer;

typedef std::map<std::string, size_t>  item_refcounts_ty;
typedef std::pair<std::string, TOS_Topics::TOPICS>  buffer_id_ty;

typedef TwoWayHashMap<TOS_Topics::TOPICS, HWND, true,
                      std::hash<TOS_Topics::TOPICS>, std::hash<HWND>,
                      std::equal_to<TOS_Topics::TOPICS>, std::equal_to<HWND>>  convos_ty;

LPCSTR CLASS_NAME = "DDE_CLIENT_WINDOW"; 
LPCSTR APP_NAME = "TOS";

#ifdef LOG_BACKEND_USE_SINGLE_FILE
LPCSTR LOG_NAME = LOG_BACKEND_SINGLE_FILE_NAME;
#else
LPCSTR LOG_NAME = "engine-log.log";
#endif 
  
#ifdef REDIRECT_STDERR_TO_LOG
LPCSTR ERR_LOG_NAME = "engine-stderr.log";
#endif

const system_clock_type  system_clock;

const unsigned int ACL_SIZE = 96;
const int NSECURABLE = 2;

/* our 'private' messages; OK between 0x0400 and 0x7fff */
const unsigned int LINK_DDE_ITEM = 0x0500;
const unsigned int REQUEST_DDE_ITEM = 0x0501;
const unsigned int DELINK_DDE_ITEM = 0x0502;
const unsigned int CLOSE_CONVERSATION = 0x0503;  
    
HINSTANCE hinstance = NULL;
SYSTEM_INFO sys_info;  
SECURITY_ATTRIBUTES sec_attr[2];
SECURITY_DESCRIPTOR sec_desc[2];

SmartBuffer<void> sids[] = { 
    SmartBuffer<void>(SECURITY_MAX_SID_SIZE), 
    SmartBuffer<void>(SECURITY_MAX_SID_SIZE) 
};

SmartBuffer<ACL> acls[] = { 
    SmartBuffer<ACL>(ACL_SIZE), 
    SmartBuffer<ACL>(ACL_SIZE) 
};   

std::map<buffer_id_ty, StreamBuffer> buffers;  
std::map<TOS_Topics::TOPICS, item_refcounts_ty> topic_refcounts; 

convos_ty convos; 

LightWeightMutex topic_mtx;
LightWeightMutex buffer_mtx;
SignalManager ack_signals;

/* !!! 'buffer_lock_guard_' is reserved inside this namespace !!! */
#define BUFFER_LOCK_GUARD WinLockGuard buffer_lock_guard_(buffer_mtx)

HANDLE init_event = NULL;
HANDLE msg_thrd = NULL;
HWND msg_window = NULL;
DWORD msg_thrd_id = 0;
LPCSTR msg_window_name = "TOSDB_ENGINE_MSG_WNDW";
  
volatile bool pause_flag = false;
volatile bool shutdown_flag = false;

/* forward decl */
template<typename T>
class DDE_Data;

bool
CheckExecType(LPSTR cmd);

DWORD WINAPI     
ThreadedWinInit(LPVOID lParam);

bool
RunMainCommLoop(IPCSlave *pslave);

bool
ParseIPCMessage(std::string msg, 
                unsigned int *op, 
                TOS_Topics::TOPICS *topic, 
                std::string *item, 
                unsigned long *timeout);

int
HandleGoodIPCMessage(unsigned int op, 
                     TOS_Topics::TOPICS topic, 
                     std::string item, 
                     unsigned long timeout);

int  
CleanUpMain(int ret_code);

int  
AddStream(TOS_Topics::TOPICS topic_t,std::string item, unsigned long timeout, bool log=true);

int 
RemoveStream(TOS_Topics::TOPICS topic_t,std::string item, unsigned long timeout);

void 
RemoveAllStreams(unsigned long timeout);

int 
TestStream(TOS_Topics::TOPICS topic_t,std::string item, unsigned long timeout);

int
CreateTopic(TOS_Topics::TOPICS topic_t, std::string item, unsigned long timeout);

int
CreateItem(TOS_Topics::TOPICS topic_t, std::string item, unsigned long timeout);

void 
CloseTopic(TOS_Topics::TOPICS topic_t, unsigned long timeout);

int
CloseItem(TOS_Topics::TOPICS topic_t, std::string item, unsigned long timeout);

bool 
PostItem(std::string item,TOS_Topics::TOPICS topic_t, unsigned long timeout);

bool 
PostCloseItem(std::string item,TOS_Topics::TOPICS topic_t, unsigned long timeout);   

bool 
CreateBuffer(TOS_Topics::TOPICS topic_t, 
             std::string item, 
             unsigned int buffer_sz = TOSDB_SHEM_BUF_SZ);

bool 
DestroyBuffer(TOS_Topics::TOPICS topic_t, std::string item);

template<typename T> 
void 
RouteToBuffer(DDE_Data<T> data); 

LRESULT CALLBACK 
WndProc(HWND, UINT, WPARAM, LPARAM);  

void 
HandleData(UINT msg, WPARAM wparam, LPARAM lparam);

void 
DumpBufferStatus();

bool
SetSecurityPolicy();   

};


int WINAPI 
WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLn, int nShowCmd)
{   
    std::string logpath(TOSDB_LOG_PATH); 

#ifdef REDIRECT_STDERR_TO_LOG
    freopen( (logpath + ERR_LOG_NAME).c_str(), "a", stderr);
#endif

    /* start logging */
    logpath.append(std::string(LOG_NAME));
    StartLogging( logpath.c_str() );

    /* check/log type of execution (service v. pure, spawned v. direct) */
    if( !CheckExecType(lpCmdLn) ){
        return 0; // only exit if user agrees (no error state)
    }

    /* the other side of our IPC channel (see client_admin.cpp) */
    IPCSlave slave(TOSDB_COMM_CHANNEL);         

    /* initialize our security objects for IPC */
    if( !SetSecurityPolicy() ){
        TOSDB_LogH("STARTUP", "engine failed to initialize security objects");
        return TOSDB_ERROR_IPC;
    }
  
    /* setup our windows class that will run the engine */
    WNDCLASS clss =  {};
    hinstance = GetModuleHandle(NULL);
    clss.hInstance = hinstance;
    clss.lpfnWndProc = WndProc; 
    clss.lpszClassName = CLASS_NAME; 
    RegisterClass(&clss); 
    
    /* spin-off the windows msg loop that we'll communicate w/ directly via 
       private messages in the main communication loop below; it will also 
       respond to return DDE messages from the TOS platform */
    msg_thrd = CreateThread(NULL, 0, ThreadedWinInit, NULL, 0, &msg_thrd_id);
    if(!msg_thrd)
        return TOSDB_ERROR_CONCURRENCY;  
    
    init_event = CreateEvent(NULL, FALSE, FALSE, NULL); 
    
    /* block until its ready */
    switch( WaitForSingleObject(init_event,TOSDB_DEF_TIMEOUT) ){
    case WAIT_TIMEOUT:
       TOSDB_LogH("STARTUP","main thread timed out waiting for signal from msg thread");        
       return CleanUpMain(TOSDB_ERROR_CONCURRENCY);  
    case WAIT_FAILED:
       TOSDB_LogH("STARTUP","main thread failed to receive signal from msg thread");        
       return CleanUpMain(TOSDB_ERROR_CONCURRENCY); 
    }  
  
    GetSystemInfo(&sys_info);     

    /* Start the main communciation loop that client code and service will 
       use to communicate with the back-end; this will block until:
           1) the slave's wait_for_master call returns false(IPC ERROR), OR
           2) TOSDB_SIG_STOP signal is received from client/service  */
    int err = RunMainCommLoop(&slave) ? 0 : TOSDB_ERROR_IPC;

    TOSDB_LogH("CONTROL","out of run loop (remove all streams, clean up)");    
    RemoveAllStreams(TOSDB_DEF_TIMEOUT);
    err = CleanUpMain(err);      

    StopLogging();  

#ifdef REDIRECT_STDERR_TO_LOG
    fclose(stderr);
#endif

    return err;
}


namespace {        

bool
CheckExecType(LPSTR cmd)
{   
    bool is_service = false;
    bool is_spawned = false;
    
    std::vector<std::string> args;
    ParseArgs(args, cmd);    

    /* check whether we were spawned by service... */
    if(!args.empty() && args[0] == "--spawned")
        is_spawned=true; 

    /* ...if not, warn */
    if(!is_spawned){
        std::string warn_msg("Running tos-databridge-engine directly is not recommended. "
            "Inadequate privileges can cause connection issues with TOS or fatal errors. \n\n"
            "Using 'tos-databridge-serv --noservice' to spawn the engine is recommended.\n\n");
          
#ifndef NO_KERNEL_GLOBAL_NAMESPACE
        warn_msg.append(           
            "NO_KERNEL_GLOBAL_NAMESPACE is not defined and you may not have the "
            "necessary privileges to create global kernel objects. This may result "
            " in a CRASH.\n\n"
         );

        TOSDB_LogH("STARTUP", "engine being run directly without NO_KERNEL_GLOBAL_NAMESPACE");
#endif
        if(MessageBox(NULL,warn_msg.c_str(),"Warning",MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL)
        {        
            TOSDB_LogH("STARTUP", "Warning Box - Cancel; aborting startup");
            return false;                
        }
    };
    
    std::stringstream ss_args;
    ss_args<< "lpCmdLn args: ";    
    for(auto & a : args){ 
        ss_args<< a << ' ';
        /* if we get the service arg run as service, pure executable otherwise*/
        if(a == "--service")
            is_service = true;
    }

    if(is_service && !is_spawned){
        TOSDB_LogH("STARTUP", "can not run as 'unspawned' service; default to --noservice");
        is_service = false;
    }

    TOSDB_Log("STARTUP", (is_service ? "is_service == true" : "is_service == false"));   
    TOSDB_Log("STARTUP", ss_args.str().c_str());

    return true;
}


DWORD WINAPI 
ThreadedWinInit(LPVOID lParam)
{
    if(!hinstance)
        hinstance = GetModuleHandle(NULL);

    msg_window = CreateWindow(CLASS_NAME, msg_window_name, WS_OVERLAPPEDWINDOW, 
                              0, 0, 0, 0, NULL, NULL, hinstance, NULL);  

    if(!msg_window) 
        return 1; 

    SetEvent(init_event);

    MSG msg = {};   
    while( GetMessage(&msg, NULL, 0, 0) )
    {  
        TranslateMessage(&msg);
        DispatchMessage(&msg);    
    }   
     
    return 0;
}


bool
RunMainCommLoop(IPCSlave *pslave)
{
    TOS_Topics::TOPICS cli_topic;
    std::string cli_item;
    std::string ipc_msg;
    unsigned long cli_timeout; 
    unsigned int cli_op;  
    bool good_msg;
    int resp;        
    
    TOSDB_Log("STARTUP", "entering RunMainCommLoop");
    while(!shutdown_flag){     
                      
        /* BLOCK until master is ready */ 
        if( !pslave->wait_for_master() ){           
            shutdown_flag = true;
            TOSDB_LogH("IPC", "wait_for_master failed");            
            return false;
        }     
                          
        /* BLOCK until we get a message */ 
        if( !pslave->recv(&ipc_msg) ){       
            pslave->drop_master();
            TOSDB_LogH("IPC", "recv failed");            
            continue;          
        }

        /* parse the received msg */     
        good_msg = ParseIPCMessage(ipc_msg, &cli_op, &cli_topic, &cli_item, &cli_timeout);
        if(good_msg){            
            resp = HandleGoodIPCMessage(cli_op, cli_topic, cli_item, cli_timeout);   
        }else{
            resp = TOSDB_ERROR_IPC_MSG;
            TOSDB_LogH("IPC", ("failed to parse message: " + ipc_msg).c_str());            
        }
                            
        /* reply to MASTER */
        if( !pslave->send(std::to_string(resp)) ){
            TOSDB_LogH("IPC", "send/reply failed in main comm loop");                       
        }                  
         
        pslave->drop_master();
    }

    TOSDB_Log("SHUTDOWN", "exiting MainCommLoop");
    return true;
}


#define STREAM_CHECK_LOG_ERROR(e,call,topic,item,tout) do{ \
if(e){ \
    std::stringstream err_s; \
    err_s << "Error (" << std::to_string(e) << ") in " << call << " (" \
          << TOS_Topics::map[topic] << ',' << item << ',' \
          << std::to_string(tout) << ')'; \
    TOSDB_LogH("STREAM-OP", err_s.str().c_str()); \
} \
}while(0)


int
HandleGoodIPCMessage( unsigned int op, 
                      TOS_Topics::TOPICS topic, 
                      std::string item, 
                      unsigned long timeout )
{
    int ret = TOSDB_SIG_BAD;

    switch(op){
    case TOSDB_SIG_ADD:                              
        ret = AddStream(topic, item, timeout);
        STREAM_CHECK_LOG_ERROR(ret, "AddStream", topic, item, timeout);                                                  
        break;
                
    case TOSDB_SIG_REMOVE:                
        ret = RemoveStream(topic, item, timeout);
        STREAM_CHECK_LOG_ERROR(ret, "RemoveStream", topic, item, timeout);                             
        break;
                
    case TOSDB_SIG_TEST:
        ret = TestStream(topic, item, timeout);                                                      
        break;

    case TOSDB_SIG_PAUSE:                                    
        TOSDB_Log("SERVICE-MSG", "TOSDB_SIG_PAUSE message received");                         
        pause_flag = true;
        ret = TOSDB_SIG_GOOD;                                              
        break;
                
    case TOSDB_SIG_CONTINUE:                           
        TOSDB_Log("SERVICE-MSG", "TOSDB_SIG_CONTINUE message received");                              
        pause_flag = false;              
        ret = TOSDB_SIG_GOOD;                                
        break;
                
    case TOSDB_SIG_STOP:                           
        TOSDB_Log("SERVICE-MSG", "TOSDB_SIG_STOP message received");        
        shutdown_flag = true;                    
        ret = TOSDB_SIG_GOOD;                  
        break;
                
    case TOSDB_SIG_DUMP:               
        TOSDB_Log("SERVICE-MSG", "TOSDB_SIG_DUMP message received");
        DumpBufferStatus();
        ret = TOSDB_SIG_GOOD;
        break;
              
    default:                
        TOSDB_LogH("IPC", ("invalid opcode: " + std::to_string(op)).c_str());
    }    

    return ret;
}

#undef STREAM_CHECK_LOG_ERROR


bool
ParseIPCMessage( std::string msg, 
                 unsigned int *op, 
                 TOS_Topics::TOPICS *topic, 
                 std::string *item, 
                 unsigned long *timeout )
{  
    size_t mlen = msg.length();
    if(mlen > IPCBase::MAX_MESSAGE_SZ)
    {      
        TOSDB_LogH("IPC", ("message length (" + std::to_string(mlen) + ") > MAX_MESSAGE_SZ (" 
                           + std::to_string(IPCBase::MAX_MESSAGE_SZ) + ')').c_str());
        return false;
    }
     
    std::vector<std::string> args;    
    ParseArgs(args, msg);

    size_t nargs = args.size();
    if(nargs == 0){        
        TOSDB_LogH("IPC", ("ParseArgs returned 0 args, msg: " + msg).c_str());
        return false;
    }
        
    try{ /* first, get the opcode */
        *op = (unsigned int)std::stoul(args[0]); 
    }catch(...){
        TOSDB_LogH("IPC", ("failed to get 'op' arg from msg, args[0]: " + args[0]).c_str());
        return false;
    }

    switch(*op){ /* break out if we just need an opcode */    
    case TOSDB_SIG_PAUSE: 
    case TOSDB_SIG_CONTINUE: 
    case TOSDB_SIG_STOP: 
    case TOSDB_SIG_DUMP: 
        return true;        
    };
        
    if(nargs != 4){ /* if we we have a stream op check we have 4 args */       
        TOSDB_LogH("IPC", ("ParseArgs didn't return 4 args (" 
                           + std::to_string(nargs) + "), msg: " + msg).c_str());
        return false;
    }    
    
    try{ /* second, get the topic */
        *topic = TOS_Topics::MAP()[args[1]];
    }catch(...){
        TOSDB_LogH("IPC", ("failed to get 'topic' arg from msg, args[1]: " + args[1]).c_str());
        return false;
    }

    /* third, the item */
    *item = args[2];
    
    try{ /* fourth, the timeout */
        *timeout = std::stoul(args[3]);
    }catch(...){
        TOSDB_LogH("IPC", ("failed to get 'timeout' arg from msg, args[3]: " + args[3]).c_str());
        return false;
    }

    return true;
}


int 
CleanUpMain(int ret_code)
{
    if(ret_code) 
        TOSDB_LogEx("SHUTDOWN", "engine exiting because of error", ret_code);
    else
        TOSDB_LogH("SHUTDOWN", "engine exiting");

    if(msg_window)
        DestroyWindow(msg_window);

    if(!hinstance)
        hinstance = GetModuleHandle(NULL);

    UnregisterClass(CLASS_NAME, hinstance);
    return ret_code;
}


int 
AddStream( TOS_Topics::TOPICS topic_t, 
           std::string item, 
           unsigned long timeout,
           bool log )
{    
    bool ret;
    int err = 0;

    if(topic_t == TOS_Topics::TOPICS::NULL_TOPIC){
        return TOSDB_ERROR_BAD_TOPIC;
    }

    auto topic_iter = topic_refcounts.find(topic_t);    
    if(topic_iter == topic_refcounts.end()) /* if topic isn't in our global mapping */    
    { 
        err = CreateTopic(topic_t, item, timeout);
        if(err && log){
            TOSDB_LogEx("STREAM", "error creating new topic stream", err);        
        }
    }
    else /* if it already is */            
    { 
        auto item_iter = topic_iter->second.find(item); 
        if(item_iter == topic_iter->second.end()) /* and it doesn't have that item yet */                        
        { 
            err = CreateItem(topic_t, item, timeout);           
            if(err && log){
                TOSDB_LogEx("STREAM", "error creating new item stream", err);  
            }
        }
        else /* if it does, simply increment the ref-count */        
        { 
            ++(item_iter->second);            
        }
    }  
  
    /* unwind if it fails during creation */
    switch(err){   
    case TOSDB_ERROR_SHEM_BUFFER:    
        PostCloseItem(item, topic_t, timeout);
        topic_refcounts[topic_t].erase(item);
        /* no break */
    case TOSDB_ERROR_DDE_POST:
        if( !topic_refcounts[topic_t].empty() ) 
            break;        
        /* else 
            no break */
    case TOSDB_ERROR_DDE_NO_ACK:        
        CloseTopic(topic_t, timeout);   
    }    

    return err;
}


int 
RemoveStream( TOS_Topics::TOPICS topic_t, 
              std::string item, 
              unsigned long timeout )
{      
    int err = 0;

    if(topic_t == TOS_Topics::TOPICS::NULL_TOPIC){
        return TOSDB_ERROR_BAD_TOPIC;
    }

    auto topic_iter = topic_refcounts.find(topic_t);      
    /* if topic is not in our global mapping */
    if(topic_iter == topic_refcounts.end())   
        return TOSDB_ERROR_ENGINE_NO_TOPIC;    

    auto item_iter = topic_iter->second.find(item);    
    /* if it has that item */
    if(item_iter != topic_iter->second.end()){ 
        /* decr the ref count */
        --(item_iter->second);
        /* if ref-count hits zero post close msg and destroy the buffer*/
        if(item_iter->second == 0){            
            err = CloseItem(topic_t, item, timeout);
            topic_iter->second.erase(item_iter);                 
        }
    }else{
        err = TOSDB_ERROR_ENGINE_NO_ITEM;
    }

    /* if no items close the convo */
    if(topic_iter->second.empty())       
         CloseTopic(topic_t, timeout);  

    return err;      
}


void 
RemoveAllStreams(unsigned long timeout)
{ /* need to iterate through copies */  
    std::map<TOS_Topics::TOPICS, item_refcounts_ty> t_copy;
    item_refcounts_ty rc_copy;
        
    auto tii = std::inserter(t_copy, t_copy.begin());
    std::copy(topic_refcounts.begin(), topic_refcounts.end(), tii);

    for(const auto& topic: t_copy){
        auto rii = std::inserter(rc_copy,rc_copy.begin());
        std::copy(topic.second.begin(), topic.second.end(), rii);
        for(const auto& item : rc_copy){
            RemoveStream(topic.first, item.first, timeout);    
        }
        rc_copy.clear();
    }
}


int 
TestStream(TOS_Topics::TOPICS topic_t,std::string item, unsigned long timeout)
{
    int a = AddStream(topic_t, item, timeout, false);
    if(a)
        return a;

    int r = RemoveStream(topic_t, item, timeout);
    if(r){
        TOSDB_LogEx("STREAM-OP", "TestStream failed to remove stream (stream leaked)", r);
        return r;
    }

    return 0;
}


int
CreateItem(TOS_Topics::TOPICS topic_t, std::string item, unsigned long timeout)
{
    /* consider creating buffer before posting to avoid getting data messages 
       before we have a data buffer to put them in */

    if( !PostItem(item, topic_t, timeout) ){
        return TOSDB_ERROR_DDE_POST;
    }

    topic_refcounts[topic_t][item] = 1;         

    if( !CreateBuffer(topic_t, item) ){
        return TOSDB_ERROR_SHEM_BUFFER;
    }
    
    return 0;
}


int
CreateTopic(TOS_Topics::TOPICS topic_t, std::string item, unsigned long timeout)
{     
    std::string topic_str;
    ATOM topic_atom;
    ATOM app_atom;
    bool ret;         

    topic_str = TOS_Topics::map[topic_t];  
    topic_atom = GlobalAddAtom(topic_str.c_str());
    app_atom = GlobalAddAtom(APP_NAME);

    ack_signals.set_signal_ID(TOS_Topics::map[topic_t]); 

    if(topic_atom){
        SendMessageTimeout( (HWND)HWND_BROADCAST, 
                            WM_DDE_INITIATE,
                            (WPARAM)msg_window, 
                            MAKELONG(app_atom,topic_atom), 
                            SMTO_NORMAL, 500, NULL );  
    }

    if(app_atom) 
        GlobalDeleteAtom(app_atom);

    if(topic_atom) 
        GlobalDeleteAtom(topic_atom);

    /* wait for ack from DDE server */
    ret = ack_signals.wait_for(TOS_Topics::map[topic_t], timeout);
    if(!ret){ /* are we sure about this? error unwind will call CloseTopic 
                 - whats the purpose if we never got the 'ack'? (maybe a late ack)
                 - deadlock or corrupt 'convos' on sending WM_DDE_TERMINATE in this state?*/
        return TOSDB_ERROR_DDE_NO_ACK;       
    }

    /* once we get our ack set up/initialize our refcounts
       we do it here because CloseTopic will remove if we have error below */
    topic_refcounts[topic_t] = item_refcounts_ty();

    ret = PostItem(item, topic_t, timeout);
    if(!ret)
        return TOSDB_ERROR_DDE_POST;    

    /* because of how AddStream unwinds errors order is important; 
       this MUST come after PostItem, before CreateBuffer */
    topic_refcounts[topic_t][item] = 1;

    ret = CreateBuffer(topic_t, item);
    if(!ret)
        return TOSDB_ERROR_SHEM_BUFFER;             

    return 0;
}


int
CloseItem(TOS_Topics::TOPICS topic_t, std::string item, unsigned long timeout)
{
    int err = 0;

    /* if we return error, continue with remove but log it */
    if( !PostCloseItem(item, topic_t, timeout) )
    {   
        err = TOSDB_ERROR_DDE_POST;
        TOSDB_LogH("ENGINE", "PostCloseItem failed, continue with CloseItem");                
    }

    /* previous err code takes priority */
    if( !DestroyBuffer(topic_t, item) ){        
        err = (err ? err : TOSDB_ERROR_SHEM_BUFFER);                             
    }

    return err;
}


void 
CloseTopic(TOS_Topics::TOPICS topic_t, unsigned long timeout)
{  
    if( !PostMessage(msg_window, CLOSE_CONVERSATION, (WPARAM)convos[topic_t], NULL) )
    {
        TOSDB_LogEx("ENGINE", "CloseTopic::PostMessage::CLOSE_CONVERSATION failed", GetLastError());
    }
    topic_refcounts.erase(topic_t); 
    convos.remove(topic_t);  
}


bool 
PostItem(std::string item, 
         TOS_Topics::TOPICS topic_t, 
         unsigned long timeout)
{ 
    HWND convo = convos[topic_t];
    std::string sid_id = std::to_string((size_t)convo) + item;

    ack_signals.set_signal_ID(sid_id);
    if( !PostMessage(msg_window, REQUEST_DDE_ITEM, (WPARAM)convo, (LPARAM)(item.c_str())) )
    {        
        TOSDB_LogEx("ENGINE", "PostItem::PostMessage::REQUEST_DDE_ITEM failed", GetLastError());
    }

    /* for whatever reason a bad item gets a posive ack from an attempt 
       to link it, so that message must post second to give the request 
       a chance to preempt it */    

    if( !PostMessage(msg_window, LINK_DDE_ITEM, (WPARAM)convo, (LPARAM)(item.c_str())) )
    {      
        TOSDB_LogEx("ENGINE", "PostItem::PostMessage::LINK_DDE_ITEM failed", GetLastError());
    }

    return ack_signals.wait_for(sid_id , timeout);
}


bool 
PostCloseItem(std::string item, 
              TOS_Topics::TOPICS topic_t, 
              unsigned long timeout)
{  
    HWND convo = convos[topic_t];
    std::string sid_id = std::to_string((size_t)convo) + item;

    ack_signals.set_signal_ID(sid_id);
    if( !PostMessage(msg_window, DELINK_DDE_ITEM, (WPARAM)convo, (LPARAM)(item.c_str())) )
    {
        TOSDB_LogEx("ENGINE", "PostCloseItem::PostMessage::DELINK_DDE_ITEM failed", GetLastError());
    }

    return ack_signals.wait_for(sid_id, timeout);
}


bool 
CreateBuffer(TOS_Topics::TOPICS topic_t, 
             std::string item, 
             unsigned int buffer_sz)
{  
    StreamBuffer buf;
    std::string name;

    buffer_id_ty id(item, topic_t);  
       
    {/* Feb-15-2017 - protect the buffers map; write thread may try to access */
        BUFFER_LOCK_GUARD;
        /* --- CRITICAL SECTION --- */
        if(buffers.find(id) != buffers.end())
            return false;
    }

    name = CreateBufferName(TOS_Topics::map[topic_t], item);

    buf.raw_sz = (buffer_sz < sys_info.dwPageSize) ? sys_info.dwPageSize : buffer_sz;

    buf.hfile = CreateFileMapping( INVALID_HANDLE_VALUE, 
                                   &sec_attr[SHEM1],
                                   PAGE_READWRITE, 0, 
                                   buf.raw_sz, 
                                   name.c_str() ); 
    if(!buf.hfile){
        TOSDB_LogEx("BUFFER", "CreateFileMapping failed", GetLastError());
        return false;
    }

    buf.raw_addr = MapViewOfFile(buf.hfile, FILE_MAP_ALL_ACCESS, 0, 0, 0);     
    if(!buf.raw_addr){
        TOSDB_LogEx("BUFFER", "MapViewOfFile failed", GetLastError());
        CloseHandle(buf.hfile);
        return false;   
    }

    std::string mtx_name = std::string(name).append("_mtx");

    buf.hmtx = CreateMutex(&sec_attr[MUTEX1], FALSE, mtx_name.c_str());
    if(!buf.hmtx){
        TOSDB_LogEx("BUFFER", "CreateMutex failed", GetLastError());
        CloseHandle(buf.hfile);
        UnmapViewOfFile(buf.raw_addr); 
        return false;
    }
 
    /* cast mem-map to our header and fill values */
    pBufferHead ptmp = (pBufferHead)(buf.raw_addr); 
    ptmp->loop_seq = 0;
    ptmp->next_offset = ptmp->beg_offset = sizeof(BufferHead);
    ptmp->elem_size = TOS_Topics::TypeSize(topic_t) + sizeof(DateTimeStamp);
    ptmp->end_offset = ptmp->beg_offset 
                     + ((buf.raw_sz - ptmp->beg_offset) / ptmp->elem_size) 
                     * ptmp->elem_size ;

    /* Feb-15-2017 - protect the buffers map; write thread may try to access */
    BUFFER_LOCK_GUARD;
    /* --- CRITICAL SECTION --- */
    buffers.insert( std::make_pair(id,buf) );
    return true;
    /* --- CRITICAL SECTION --- */
}


bool 
DestroyBuffer(TOS_Topics::TOPICS topic_t, std::string item)
{   
    bool b = true; 

    /* don't allow write attempt while buffer is being destroyed */
    BUFFER_LOCK_GUARD;
    /* ---CRITICAL SECTION --- */    

    auto buf_iter = buffers.find( buffer_id_ty(item,topic_t) );
    if(buf_iter == buffers.end())
        return false;
        
    if( !UnmapViewOfFile(buf_iter->second.raw_addr) )
    {        
        TOSDB_LogEx("BUFFER", "UnmapViewOfFile failed", GetLastError());        
        b = false;
    }

    if( !CloseHandle(buf_iter->second.hfile) )
    {        
        TOSDB_LogEx("BUFFER", "CloseHandle(hfile) failed", GetLastError());          
        b = false;
    }

    if( !CloseHandle(buf_iter->second.hmtx) )
    {        
        TOSDB_LogEx("BUFFER", "CloseHanlde(hmtx) failed", GetLastError());         
        b = false;
    }

    buffers.erase(buf_iter);
    return b; 
    /* ---CRITICAL SECTION --- */
}


template<typename T> 
inline void 
ValToBuf(void* pos, T val) 
{ 
    *(T*)pos = val; 
}

template<> 
inline void 
ValToBuf(void* pos, std::string val) /* copy the string, truncate if necessary */
{ 
    strncpy_s((char*)pos, TOSDB_STR_DATA_SZ, val.c_str(), TOSDB_STR_DATA_SZ-1);
}

template<typename T>
void
RouteToBuffer(DDE_Data<T> data)
{  
    pBufferHead head;  

    BUFFER_LOCK_GUARD;
    /* ---(INTRA-PROCESS) CRITICAL SECTION --- */

    auto buf_iter = buffers.find(buffer_id_ty(data.item,data.topic));
    if(buf_iter == buffers.end()){
        /* simply aborting avoids the need for sync between the thread that 
           creates/destroys buffers and the thread (this one) that writes to them */
        return;    
    }

    head = (pBufferHead)(buf_iter->second.raw_addr);
  
    WaitForSingleObject(buf_iter->second.hmtx, INFINITE);
    /* ---(INTER-PROCESS) CRITICAL SECTION --- */
    ValToBuf((void*)((char*)head + head->next_offset), data.data);

    *(pDateTimeStamp)((char*)head + head->next_offset 
                      + (head->elem_size - sizeof(DateTimeStamp))) = *data.time; 

    if((head->next_offset + head->elem_size) >= head->end_offset){
        head->next_offset = head->beg_offset; 
        ++(head->loop_seq);
    }else{
        head->next_offset += head->elem_size;
    }

    /* ---(INTER-PROCESS) CRITICAL SECTION --- */
    ReleaseMutex(buf_iter->second.hmtx);
    /* ---(INTRA-PROCESS) CRITICAL SECTION --- */
}

void /* in place 'safe' strlwr */
str_to_lower(char* str, size_t max)
{
    while(str && max--){
        str[0] = tolower(str[0]);
        ++str;
    }
}

LRESULT CALLBACK 
WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{  
    switch (message){
    case WM_DDE_DATA: 
    { /* TODO:  de-link it all and store state, then re-init on continue */      
        if(!pause_flag) 
            HandleData(message, wParam, lParam);
        break;     
    } 
    case LINK_DDE_ITEM:
    {      
        DDEADVISE FAR* lp_options;
        LPARAM lp;      
        ATOM item;       

        HGLOBAL hoptions = GlobalAlloc(GMEM_MOVEABLE, sizeof(DDEADVISE));       
        if (!hoptions){           
            TOSDB_LogEx("DDE", "LINK_DDE_ITEM::GlobalAlloc failed", GetLastError());  
            break;
        }

        lp_options = (DDEADVISE FAR*)GlobalLock(hoptions);
        if (!lp_options){           
            TOSDB_LogEx("DDE", "LINK_DDE_ITEM::GlobalLock failed", GetLastError());  
            GlobalFree(hoptions);
            break;
        }
        lp_options->cfFormat = CF_TEXT;
        lp_options->fAckReq = FALSE;   
        lp_options->fDeferUpd = FALSE; 

        GlobalUnlock(hoptions);

        item = GlobalAddAtom((LPCSTR)lParam);
        if(!item){           
            TOSDB_LogEx("DDE", "LINK_DDE_ITEM::GlobalAddAtom failed", GetLastError());  
            break;
        }

        lp = PackDDElParam(WM_DDE_ADVISE, (UINT_PTR)hoptions, item);        

        if( !PostMessage((HWND)wParam, WM_DDE_ADVISE, (WPARAM)msg_window, lp) )
        {      
            TOSDB_LogEx("DDE", "LINK_DDE_ITEM::PostMessage failed", GetLastError());    
            GlobalDeleteAtom(item);
            GlobalFree(hoptions);
            FreeDDElParam(WM_DDE_ADVISE, lp);
        }
        break;
    } 
    case REQUEST_DDE_ITEM:
    {     
        LPARAM lp;   

        ATOM item = GlobalAddAtom((LPCSTR)lParam);          
        if(!item){       
            TOSDB_LogEx("DDE", "REQUEST_DDE_ITEM::GlobalAddAtom failed", GetLastError());  
            break;
        }
  
        lp = PackDDElParam(WM_DDE_REQUEST, CF_TEXT, item);
       
        if( !PostMessage((HWND)wParam, WM_DDE_REQUEST, (WPARAM)(msg_window), lp) )
        {         
            TOSDB_LogEx("DDE", "REQUEST_DDE_ITEM::PostMessage failed", GetLastError());   
            GlobalDeleteAtom(item); 
        }

        break; 
    } 
    case DELINK_DDE_ITEM:
    {     
        LPARAM lp;

        ATOM item = GlobalAddAtom((LPCSTR)lParam);     
        if(!item){            
            TOSDB_LogEx("DDE", "DELINK_DDE_ITEM::GlobalAddAtom failed", GetLastError()); 
            break;
        }

        lp = PackDDElParam(WM_DDE_UNADVISE, 0, item);

        if( !PostMessage((HWND)wParam, WM_DDE_UNADVISE, (WPARAM)(msg_window), lp) )
        {           
            TOSDB_LogEx("DDE", "DELINK_DDE_ITEM::PostMessage failed", GetLastError());   
            GlobalDeleteAtom(item);      
            FreeDDElParam(WM_DDE_UNADVISE, lp);
        }   

        break;
    } 
    case CLOSE_CONVERSATION: 
    {
        if( !PostMessage((HWND)wParam, WM_DDE_TERMINATE, (WPARAM)msg_window, NULL) )
        {   
            TOSDB_LogEx("DDE", "CLOSE_CONVERSATION::PostMessage failed", GetLastError());  
        }        
        break;  
    }
    case WM_DESTROY:        
    {
        PostQuitMessage(0);  
        break;
    }
    case WM_DDE_ACK:  
    {     
        char topic_atom[TOSDB_MAX_STR_SZ + 1];
        char app_atom[TOSDB_MAX_STR_SZ + 1];
        char app_str_lower[TOSDB_MAX_STR_SZ + 1];
        char item_atom[TOSDB_MAX_STR_SZ + 1];          
    
        UINT_PTR pho = 0; 
        UINT_PTR plo = 0;   

        TOSDB_LogDebug( ("ACK RECEIVED - lParam: " + std::to_string(lParam) + 
                         ", wParam: " + std::to_string(wParam)).c_str() );
    
        if (lParam <= 0){      
            pho = HIWORD(lParam); /*topic*/
            plo = LOWORD(lParam); /*app*/

            GlobalGetAtomName((ATOM)(pho), topic_atom, (TOSDB_MAX_STR_SZ + 1)); 
            GlobalGetAtomName((ATOM)(plo), app_atom, (TOSDB_MAX_STR_SZ + 1));

            strcpy_s(app_str_lower, APP_NAME);       
            str_to_lower(app_atom, TOSDB_MAX_STR_SZ); 
            str_to_lower(app_str_lower, TOSDB_MAX_STR_SZ); 

            TOSDB_LogDebug( ("lParam <= 0 - app_atom: " + std::string(app_atom) + 
                             ", topic_atom: " + std::string(topic_atom)).c_str() );

            if( strcmp(app_atom, app_str_lower) ){
                TOSDB_LogH("DDE", (app_atom + std::string(" != ") + app_str_lower).c_str() );
                break;   
            }

            convos_ty::pair1_type cp(TOS_Topics::map[topic_atom],(HWND)wParam);
            convos.insert(std::move(cp));       

            ack_signals.signal(topic_atom, true);
        }else{
            /* IF SOMETHING FUNDAMENTAL SEEMS BROKEN LOOK HERE ...

               The logic in here is a bit... tenuous.  The DDE behavior 
               doesn't exactly follow the MSDN docs. We had to more-or-less
               go trial-and-error to see how to handle communication 
               with the server. Whether or not this relies on impl details
               is left to be seen.           
            */
            UnpackDDElParam(message,lParam, (PUINT_PTR)&plo, (PUINT_PTR)&pho); 
            GlobalGetAtomName((ATOM)(pho), item_atom, (TOSDB_MAX_STR_SZ + 1)); 

            TOSDB_LogDebug( ("lParam > 0 - plo: " + std::to_string(plo) + ", pho: " + 
                             std::to_string(pho) + ", item_atom: " + 
                             std::string(item_atom)).c_str() );

            if(plo == 0x0000){
                std::string sarg(std::to_string((size_t)(HWND)wParam)); 
                ack_signals.signal(sarg + std::string(item_atom), false);                
                TOSDB_LogH("DDE", ("NEG ACK for item: " + std::string(item_atom)).c_str());
            }else if(plo == 0x8000){
                std::string sarg(std::to_string((size_t)(HWND)wParam));
                ack_signals.signal(sarg + std::string(item_atom), true);   
            }       
        }   
        break;
    }     
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }  

    return 0;
}


template<typename T>
class DDE_Data {
    static const system_clock_type::time_point EPOCH_TP;
  
    DDE_Data(const DDE_Data<T>&);
    DDE_Data& operator=(const DDE_Data<T>&);

    void 
    _init_datetime();  

public:
    TOS_Topics::TOPICS topic;
    pDateTimeStamp time;
    bool valid_datetime;
    std::string item;  
    T data;
    
    DDE_Data(const TOS_Topics::TOPICS topic, 
             std::string item, 
             const T d, 
             bool datetime) 
      :
          topic(topic),
          item(item),
          data(d), 
          time(new DateTimeStamp),
          valid_datetime(datetime)
      {
          if(datetime) 
              _init_datetime();      
      }

    ~DDE_Data() 
        { 
            if(time) 
                delete time; 
        }

    DDE_Data(DDE_Data<T>&& d)
        :
            topic(d.topic),
            item(d.item),
            data(d.data),
            time(d.time),
            valid_datetime(d.valid_datetime)
        {    
            d.time = nullptr;
        }

    DDE_Data& 
    operator=(DDE_Data<T>&& d)
    {
        topic = d.topic;
        item = d.item;
        data = d.data;
        valid_datetime = d.valid_datetime;
        time = d.time;
        d.time = nullptr;

        return *this;
    }
}; 

template<typename T> 
const system_clock_type::time_point  
DDE_Data<T>::EPOCH_TP; 

template<typename T>
void 
DDE_Data<T>::_init_datetime()
{
    system_clock_type::time_point now;  
    std::chrono::seconds sec;
    micro_sec_type ms;    
    time_t t;

    /* current timepoint*/
    now = system_clock.now(); 
    /* number of ms since epoch */
    ms = std::chrono::duration_cast<micro_sec_type, 
                                    system_clock_type::rep, 
                                    system_clock_type::period>(now - EPOCH_TP);
    /* this is necessary to avoid issues w/ conversions to C time */
    sec = std::chrono::duration_cast< std::chrono::seconds >(ms);
    /* ms since last second */
    time->micro_second = (long)((ms % micro_sec_type::period::den).count());  
    /* get the ctime by adjusting epoch by seconds since */
    t = system_clock.to_time_t(EPOCH_TP + sec);  
    localtime_s(&time->ctime_struct, &t);      
}


void 
HandleData(UINT msg, WPARAM wparam, LPARAM lparam)
{  
    DDEDATA FAR *dde_data;
    BOOL clnt_rel;
    PVOID data;  
    UINT_PTR atom;
    LPARAM lpneg;
    int cpret;

    char cp_data[TOSDB_STR_DATA_SZ+1]; /* include CR LF, exclude added \0 */
    char item_atom[TOSDB_MAX_STR_SZ + 1];    

    UnpackDDElParam(msg, lparam, (PUINT_PTR)&data, &atom);   
      
    dde_data = (DDEDATA FAR*)GlobalLock(data);
    /* if we can't lock the data or its not expected frmt */
    if(!dde_data || dde_data->cfFormat != CF_TEXT)
    {   
        /* SEND NEG ACK TO SERVER  - convo already destroyed if NULL */
        lpneg = PackDDElParam(WM_DDE_ACK, 0, (UINT_PTR)atom);         
        PostMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)msg_window, lpneg);

        /* free resources */
        GlobalDeleteAtom((WORD)atom);
        FreeDDElParam(WM_DDE_ACK, lpneg);
        return; 
    }  

    cpret = strncpy_s(cp_data, (LPCSTR)(dde_data->Value), TOSDB_STR_DATA_SZ);
    if(cpret)    
        TOSDB_LogH("DDE", "error copying data->Value string");       

    if(dde_data->fAckReq){
        /* SEND POS ACK TO SERVER - convo already destroyed if NULL */
        PostMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)msg_window,
                    PackDDElParam(WM_DDE_ACK, 0x8000, (UINT_PTR)atom)); 
    }
  
    /* extract before we unlock and free */
    GlobalGetAtomName((WORD)atom, item_atom, TOSDB_MAX_STR_SZ + 1);
    clnt_rel = dde_data->fRelease;
    GlobalUnlock(data); 

    if(clnt_rel)
        GlobalFree(data);

    GlobalDeleteAtom((WORD)atom); 

    /* need to free lParam, as well, or we leak (not documented well on MSDN) */  
    FreeDDElParam(WM_DDE_DATA, lparam);

    /* extract the topic from wparam */
    TOS_Topics::TOPICS topic_t = convos[(HWND)(wparam)];

    try{
        std::string str(cp_data); 

        switch(TOS_Topics::TypeBits(topic_t)){
        case TOSDB_STRING_BIT : /* STRING */   
        {
            /* clean up problem chars */                     
            auto r = std::remove_if(str.begin(), str.end(), [](char c){return c < 32;});
            str.erase(r, str.end());
            RouteToBuffer( DDE_Data<std::string>(topic_t, item_atom, str, true) );  
            break;
        }     
        case TOSDB_INTGR_BIT : /* LONG */   
        {
            /* remove commas */            
            auto r = std::remove_if(str.begin(), str.end(), [](char c){return std::isdigit(c) == 0;});
            str.erase(r,str.end());
            RouteToBuffer( DDE_Data<long>(topic_t, item_atom, std::stol(str), true) );  
            break;
        }               
        case TOSDB_QUAD_BIT : /* DOUBLE */
        {
            RouteToBuffer( DDE_Data<double>(topic_t, item_atom, std::stod(str), true) ); 
            break;
        }     
        case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :/* LONG LONG */
        {
            /* remove commas */           
            auto r = std::remove_if(str.begin(), str.end(), [](char c){return std::isdigit(c) == 0;});
            str.erase(r,str.end());
            RouteToBuffer( DDE_Data<long long>(topic_t, item_atom, std::stoll(str), true) );  
            break;
        }     
        case 0 : /* FLOAT */
        {
            RouteToBuffer( DDE_Data<float>(topic_t, item_atom, std::stof(str), true) ); 
            break;
        }     
        };

    }catch(const std::out_of_range& e){      
        TOSDB_LogH("DDE", e.what());

    }catch(const std::invalid_argument&){    
        /* Dec 20 2016 - comment out, cluttering log file */
        /*        
        std::string serr(e.what());
        TOSDB_Log("DDE", serr.append(" Value:: ").append(cp_data).c_str());       
         */

    }catch(const std::exception& e){    
        TOSDB_LogH("DDE", e.what());
        throw TOSDB_DDE_Error(e, "error handling dde data");
    }  

    return /* 0 */; 
}


bool
SetSecurityPolicy() 
{    
    SID_NAME_USE dummy;
    BOOL ret; /* int */

    DWORD dom_sz = 128;
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;

    SmartBuffer<char>  dom_buf(dom_sz);
    SmartBuffer<void>  sid(sid_sz);  

    ret = LookupAccountName(NULL, "Everyone", sid.get(), &sid_sz, dom_buf.get(), &dom_sz, &dummy);
    if(!ret){    
        TOSDB_LogEx("ENGINE-SEC", "LookupAccountName failed", GetLastError());
        return false;
    }
   
    for(int i = 0; i < NSECURABLE; ++i){
        sec_attr[(Securable)i].nLength = sizeof(SECURITY_ATTRIBUTES);
        sec_attr[(Securable)i].bInheritHandle = FALSE;
        sec_attr[(Securable)i].lpSecurityDescriptor = &sec_desc[(Securable)i];

        /* memcpy 'TRUE' is error */
        ret = memcpy_s( sids[(Securable)i].get(), SECURITY_MAX_SID_SIZE, 
                        sid.get(), SECURITY_MAX_SID_SIZE );
        if(ret){    
            TOSDB_LogEx("ENGINE-SEC", "memcpy_s(sid) failed", GetLastError());
            return false;
        }

        ret = InitializeSecurityDescriptor(&sec_desc[(Securable)i], SECURITY_DESCRIPTOR_REVISION);
        if(!ret){    
            TOSDB_LogEx("ENGINE-SEC", "InitializeSecurityDescriptor failed", GetLastError());
            return false;
        }

        ret = SetSecurityDescriptorGroup(&sec_desc[(Securable)i], sids[(Securable)i].get(), FALSE);
        if(!ret){    
            TOSDB_LogEx("ENGINE-SEC", "SetSecurityDescriptorGroup failed", GetLastError());
            return false;
        }    

        ret = InitializeAcl(acls[(Securable)i].get(), ACL_SIZE, ACL_REVISION);
        if(!ret){    
            TOSDB_LogEx("ENGINE-SEC", "InitializeAcl failed", GetLastError());
            return false;
        }
    }
 
    /* add ACEs individually */

    ret = AddAccessDeniedAce(acls[SHEM1].get(), ACL_REVISION, FILE_MAP_WRITE, sids[SHEM1].get());
    if(!ret){    
        TOSDB_LogEx("ENGINE-SEC", "AddAccessDeniedAce failed", GetLastError());
        return false;
    }

    ret = AddAccessAllowedAce(acls[SHEM1].get(), ACL_REVISION, FILE_MAP_READ, sids[SHEM1].get());
    if(!ret){    
        TOSDB_LogEx("ENGINE-SEC", "AddAccessAllowedAce(SHEM1) failed", GetLastError());
        return false;
    }

    ret = AddAccessAllowedAce(acls[MUTEX1].get(), ACL_REVISION, SYNCHRONIZE, sids[MUTEX1].get());
    if(!ret){    
        TOSDB_LogEx("ENGINE-SEC", "AddAccessAllowedAce(MUTEX1) failed", GetLastError());
        return false;
    }

    for(int i = 0; i < NSECURABLE; ++i){
        ret = SetSecurityDescriptorDacl( &sec_desc[(Securable)i], TRUE, acls[(Securable)i].get(), FALSE);
        if(!ret){    
            TOSDB_LogEx("ENGINE-SEC", "SetSecurityDescriptorDacl failed", GetLastError());
            return false;
        }
    }

    return true;
}
  

void DumpBufferStatus()
{  
    const size_t log_col_width[5] = { 30, 30, 10, 60, 16};  
   
    std::string time_now(SysTimeString());  
    std::string lpath(TOSDB_LOG_PATH);
    std::string name("buffer-status-");
    std::ofstream lout;
    
    auto f = [](char x){ return std::isalnum(x) == 0; };
    std::replace_if(time_now.begin(), time_now.end(), f, '-'); 

    lpath.append(name).append(time_now).append(".log");
    lout.open(lpath, std::ios::out | std::ios::app);

    lout <<" --- TOPIC INFO --- " << std::endl;
    lout << std::setw(log_col_width[0]) << std::left << "Topic"
         << std::setw(log_col_width[1]) << std::left << "Item"
         << std::setw(log_col_width[2]) << std::left << "Ref-Count" 
         << std::endl;

    for(const auto & t : topic_refcounts){
        for(const auto & i : t.second){
            lout << std::setw(log_col_width[0]) << std::left 
                 << TOS_Topics::map[t.first]
                 << std::setw(log_col_width[1]) << std::left << i.first
                 << std::setw(log_col_width[2]) << std::left << i.second       
                 << std::endl;  
        }
    }
  
    lout <<" --- BUFFER INFO --- " << std::endl;  
    lout << std::setw(log_col_width[3])<< std::left << "BufferName"
         << std::setw(log_col_width[4])<< std::left << "Handle" << std::endl;

    {
        BUFFER_LOCK_GUARD;
        /* --- CRITICAL SECTION --- */
        for(const auto & b : buffers){
            lout << std::setw(log_col_width[3]) << std::left 
                 << CreateBufferName(TOS_Topics::map[b.first.second], b.first.first) 
                 << std::setw(log_col_width[4]) << std::left 
                 << (size_t)b.second.hfile << std::endl;
        }
        /* --- CRITICAL SECTION --- */
    }

    lout<< " --- END END END --- "<<std::endl;  
}



};
