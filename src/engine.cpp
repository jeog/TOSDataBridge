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

  /* !!! 'buffer_lock_guard_' is reserved inside this namespace !!! */
  /* !!! 'topic_lock_guard_' is reserved inside this namespace !!! */

typedef struct{
    void*        hfile;    /* handle to mapping */
    void*        raw_addr; /* physical location in our process space */
    unsigned int raw_sz;   /* physical size of the buffer */
    void*        hmtx;  
} StreamBuffer, *pStreamBuffer;

typedef std::map<std::string, size_t>                refcount_ty;
typedef std::pair<std::string , TOS_Topics::TOPICS>  id_ty;
typedef std::map<TOS_Topics::TOPICS, refcount_ty>    topics_ref_ty;
typedef std::map<id_ty, StreamBuffer>                buffers_ty;

typedef TwoWayHashMap<TOS_Topics::TOPICS, HWND, true,
                      std::hash<TOS_Topics::TOPICS>,
                      std::hash<HWND>,
                      std::equal_to<TOS_Topics::TOPICS>,
                      std::equal_to<HWND> >               convos_ty;


LPCSTR  CLASS_NAME = "DDE_CLIENT_WINDOW";
LPCSTR  LOG_NAME   = "engine-log.log";  
LPCSTR  APP_NAME   = "TOS";
  
const system_clock_type  system_clock;

const unsigned int COMM_BUFFER_SIZE = 5; /* opcode + 3 args + {0,0} */
const unsigned int ACL_SIZE         = 96;
const unsigned int UPDATE_PERIOD    = 2000;
const int NSECURABLE = 2;

/* our 'private' messages; OK between 0x0400 and 0x7fff */
const unsigned int LINK_DDE_ITEM      = 0x0500;
const unsigned int REQUEST_DDE_ITEM   = 0x0501;
const unsigned int DELINK_DDE_ITEM    = 0x0502;
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

buffers_ty    buffers;  
topics_ref_ty topic_refs; 
convos_ty     convos; 

LightWeightMutex topic_mtx;
LightWeightMutex buffer_mtx;
SignalManager    ack_signals;

#define BUFFER_LOCK_GUARD WinLockGuard buffer_lock_guard_(buffer_mtx)
#define TOPIC_LOCK_GUARD WinLockGuard topic_lock_guard_(topic_mtx)

HANDLE init_event      =  NULL;
HANDLE msg_thrd        =  NULL;
HWND   msg_window      =  NULL;
DWORD  msg_thrd_id     =  0;
LPCSTR msg_window_name =  "TOSDB_ENGINE_MSG_WNDW";

volatile bool shutdown_flag =  false;  
volatile bool pause_flag    =  false;
volatile bool is_service    =  true;

/* forward decl (see end of source) */
template<typename T>
class DDE_Data;

template<typename T> 
int  
RouteToBuffer(DDE_Data<T> data); 

int  
MainCommLoop(); 

void 
TearDownTopic(TOS_Topics::TOPICS tTopic, unsigned long timeout);

bool 
DestroyBuffer(TOS_Topics::TOPICS tTopic, std::string sItem);

void 
HandleData(UINT msg, WPARAM wparam, LPARAM lparam);

void 
DeAllocKernResources();

int  
CleanUpMain(int ret_code);

void 
DumpBufferStatus();

void 
CloseAllStreams(unsigned long timeout);

int  
SetSecurityPolicy();   

int  
AddStream(TOS_Topics::TOPICS tTopic,std::string sItem, unsigned long timeout);

bool 
RemoveStream(TOS_Topics::TOPICS tTopic,std::string sItem, unsigned long timeout);
  
bool 
PostItem(std::string sItem,TOS_Topics::TOPICS tTopic, unsigned long timeout);

bool 
PostCloseItem(std::string sItem,TOS_Topics::TOPICS tTopic, unsigned long timeout);   

bool 
CreateBuffer(TOS_Topics::TOPICS tTopic, 
             std::string sItem, 
             unsigned int buffer_sz = TOSDB_SHEM_BUF_SZ);

DWORD WINAPI     
Threaded_Init(LPVOID lParam);

LRESULT CALLBACK 
WndProc(HWND, UINT, WPARAM, LPARAM);   

};


int WINAPI 
WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLn, int nShowCmd)
{    
    void *parg1, *parg2, *parg3;
    int err;
    size_t indx;    

	/* start logging */
    std::string logpath(TOSDB_LOG_PATH);    
    logpath.append(std::string(LOG_NAME));
    StartLogging( logpath.c_str() ); 

    /* the other side of our IPC channel (see client_admin.cpp) */
    DynamicIPCSlave slave(TOSDB_COMM_CHANNEL, TOSDB_SHEM_BUF_SZ);
    DynamicIPCSlave::shem_chunk shem_buf[COMM_BUFFER_SIZE];   


    /* if we get the noservice arg we will run as a pure executable */
    if(!strcmp(lpCmdLn,"--noservice"))
        is_service = false;

    /* initialize our security objects */
    err = SetSecurityPolicy();
    if(err){
        TOSDB_LogH("STARTUP", "engine failed to initialize security objects");
        return -1;
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
    msg_thrd = CreateThread(NULL, 0, Threaded_Init, NULL, 0, &msg_thrd_id);
    if(!msg_thrd)
        return -1;  

    /* block until its ready */
    if(WaitForSingleObject(init_event,TOSDB_DEF_TIMEOUT) == WAIT_TIMEOUT){
        TOSDB_LogH("STARTUP","core thread did not receive signal from msg thread");
        return CleanUpMain(-1);
    }    
  
    GetSystemInfo(&sys_info);    

    parg1 = nullptr;
    parg2 = nullptr;
    parg3 = nullptr;
    indx = 0;

    /* main communication loop */
    while(!shutdown_flag){     

        /* BLOCK until master is ready */
        if( !slave.wait_for_master() ){      
            TOSDB_LogH("IPC", "wait_for_master failed");
            return CleanUpMain(-1);
        }

        /* slave.recv() MUST follow evaluation of shutdown_flag; 
           it will block until master sends a shem_chunk obj      */
        while( !shutdown_flag && slave.recv(shem_buf[indx]) )
        { 
            /* 'tail' shem_buf needs to be {0,0} */
            if(shem_buf[indx].sz == 0 && shem_buf[indx].offset == 0){  
                long i;
                switch(shem_buf[0].offset){
                case TOSDB_SIG_ADD:
                {
                    parg1 = slave.shem_ptr(shem_buf[1]); 
                    parg2 = slave.shem_ptr(shem_buf[2]); 
                    parg3 = slave.shem_ptr(shem_buf[3]);
                    if(!parg1 || !parg2 || !parg3){            
                        TOSDB_LogH("IPC","invalid shem_chunk passed to slave");
                        break;
                    }         
                    i = (long)AddStream( *(TOS_Topics::TOPICS*)parg1, 
                                         std::string((char*)parg2), 
                                         *(unsigned long*)parg3 );           
                    slave.send(i);      
                    break;
                } 
                case TOSDB_SIG_REMOVE:
                {
                    parg1 = slave.shem_ptr(shem_buf[1]); 
                    parg2 = slave.shem_ptr(shem_buf[2]); 
                    parg3 = slave.shem_ptr(shem_buf[3]);
                    if(!parg1 || !parg2 || !parg3){ 
                        TOSDB_LogH("IPC","invalid shem_chunk passed to slave");
                        break;
                    }
                    i = RemoveStream( *(TOS_Topics::TOPICS*)parg1, 
                                      std::string((char*)parg2), 
                                      *(unsigned long*)parg3)  ?  0  :  1;
                    slave.send(i);            
                    break;
                } 
                case TOSDB_SIG_PAUSE:
                {
                    TOSDB_Log("IPC", "TOSDB_SIG_PAUSE message received");
                    if(is_service){      
                        pause_flag = true;
                        slave.send(TOSDB_SIG_GOOD);              
                    }            
                    break;
                } 
                case TOSDB_SIG_CONTINUE:
                {           
                    TOSDB_Log("IPC", "TOSDB_SIG_CONTINUE message received");          
                    if(is_service){
                        pause_flag = false;              
                        slave.send(TOSDB_SIG_GOOD);            
                    }
                    break;
                } 
                case TOSDB_SIG_STOP:
                {           
                    TOSDB_Log("IPC", "TOSDB_SIG_STOP message received");         
                    if(is_service){
                        shutdown_flag = true;
                        TOSDB_Log("IPC", "shutdown flag set");
                        slave.send(TOSDB_SIG_GOOD);
                    }
                    break;
                } 
                case TOSDB_SIG_DUMP:
                {
                    TOSDB_Log("IPC", "TOSDB_SIG_DUMP message received");
                    DumpBufferStatus();
                    slave.send(0);
                    break;
                } 
                default:
                    TOSDB_LogH("IPC","invalid opcode");
                } /* switch(shem_buf[0].offset) */
            
                indx = 0;    
                parg1 = nullptr;
                parg2 = nullptr;
                parg3 = nullptr;

            /* if we overflow the message buffer: reset */
            }else if(++indx >= COMM_BUFFER_SIZE){ /* <-- we're incrementing indx */
                TOSDB_LogH("IPC","shem_chunk buffer full, reseting msg loop");
                indx = 0;        
            }  

        }  
    }  
    CloseAllStreams(TOSDB_DEF_TIMEOUT);
	StopLogging();
    return CleanUpMain(0);      
}

namespace {    

  
DWORD WINAPI 
Threaded_Init(LPVOID lParam)
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


int 
CleanUpMain(int ret_code)
{
    if(ret_code) 
        TOSDB_LogEx("SHUTDOWN", "engine exiting because of error", ret_code);
    else
        TOSDB_Log("SHUTDOWN", "engine exiting");

    if(msg_window)
        DestroyWindow(msg_window);

    if(!hinstance)
        hinstance = GetModuleHandle(NULL);

    UnregisterClass(CLASS_NAME, hinstance);
    return ret_code;
}
  

int 
AddStream(TOS_Topics::TOPICS tTopic, 
          std::string sItem, 
          unsigned long timeout)
{    
    int err = 0;

    if( !(TOS_Topics::enum_value_type)tTopic )
        return -1;

    auto topic_iter = topic_refs.find(tTopic);  
    if(topic_iter == topic_refs.end()){ /* if topic isn't in our global mapping */ 

        ATOM topic_atom;
        ATOM app_atom;
        std::string sTopic;

        init_event = CreateEvent(NULL, FALSE, FALSE, NULL);          
        sTopic = TOS_Topics::map[ tTopic ];  
        topic_atom = GlobalAddAtom(sTopic.c_str());
        app_atom = GlobalAddAtom(APP_NAME);

        ack_signals.set_signal_ID(TOS_Topics::map[ tTopic ]); 

        if(topic_atom){
            SendMessageTimeout((HWND)HWND_BROADCAST, WM_DDE_INITIATE,
                               (WPARAM)msg_window, MAKELONG(app_atom,topic_atom), 
                               SMTO_NORMAL, 500, NULL);  
        }

        if(app_atom) 
            GlobalDeleteAtom(app_atom);

        if(topic_atom) 
            GlobalDeleteAtom(topic_atom);

        /* wait for ack from DDE server */
        if( !ack_signals.wait_for(TOS_Topics::map[ tTopic ], timeout) )
            err = -2;       
     
        if(!err){
            topic_refs[tTopic] = refcount_ty();
            if( PostItem(sItem, tTopic, timeout) ){
                topic_refs[tTopic][sItem] = 1;
                if(!CreateBuffer(tTopic, sItem))
                    err = -4;     
            }else{       
                err = -3; 
            }
        }
    }else{ /* if topic IS already in our global mapping */   

        auto item_iter = topic_iter->second.find(sItem); 
        if( item_iter == topic_iter->second.end() ) /* and it doesn't have that item yet */
        { 
            if( PostItem(sItem, tTopic, timeout) ){
                topic_iter->second[sItem] = 1;
                if( !CreateBuffer(tTopic, sItem) )
                    err = -4;      
            }else{
                err = -3;
            }
        }else{ /* if both already there, increment the ref-count */
            item_iter->second++;    
        }
    }  
  
    switch(err){ /* unwind if it fails during creation */   
    case -4:    
        PostCloseItem(sItem, tTopic, timeout);
        topic_refs[tTopic].erase(sItem);
    case -3:
        if( !topic_refs[tTopic].empty() ) 
            break;        
    case -2:        
        TearDownTopic(tTopic, timeout);      
    }

   return err;
}


bool 
RemoveStream(TOS_Topics::TOPICS tTopic, 
             std::string sItem, 
             unsigned long timeout)
{  
    if( !(TOS_Topics::enum_value_type)tTopic )
        return false;

    auto topic_iter = topic_refs.find(tTopic);  
    if( topic_iter == topic_refs.end() ) /* if topic is in our global mapping */    
        return false;    

    auto item_iter = topic_iter->second.find(sItem);
    if( item_iter != topic_iter->second.end() ) /* if it has that item */   
    {
        if( !(--(item_iter->second)) ) /* if ref-count hits zero */
        {
            if(!PostCloseItem(sItem, tTopic, timeout))
                return false; 
            DestroyBuffer(tTopic, sItem);
            topic_iter->second.erase(item_iter);       
        }
    } 

    if( topic_iter->second.empty() ) /* if no items close the convo */      
         TearDownTopic(tTopic, timeout);  
    
    return true;        
}


void 
CloseAllStreams(unsigned long timeout)
{ /* need to iterate through copies */  
    topics_ref_ty t_copy;
    refcount_ty rc_copy;

    std::insert_iterator<topics_ref_ty> tii(t_copy, t_copy.begin());
    std::copy(topic_refs.begin(), topic_refs.end(), tii);

    for(const auto& topic: t_copy){
        std::insert_iterator<refcount_ty> rii(rc_copy,rc_copy.begin());
        std::copy(topic.second.begin(), topic.second.end(), rii);
        for(const auto& item : rc_copy){
            RemoveStream(topic.first, item.first, timeout);    
        }
        rc_copy.clear();
    }
}


int 
SetSecurityPolicy() 
{    
    SID_NAME_USE dummy;
    BOOL ret; /* int */

    DWORD dom_sz = 128;
    DWORD sid_sz = SECURITY_MAX_SID_SIZE;

    SmartBuffer<char>  dom_buf(dom_sz);
    SmartBuffer<void>  sid(sid_sz);  

    ret = LookupAccountName(NULL, "Everyone", sid.get(), &sid_sz, 
                            dom_buf.get(), &dom_sz, &dummy);
    if(!ret)    
        return -1;
   
    for(int i = 0; i < NSECURABLE; ++i){
        sec_attr[(Securable)i].nLength = sizeof(SECURITY_ATTRIBUTES);
        sec_attr[(Securable)i].bInheritHandle = FALSE;
        sec_attr[(Securable)i].lpSecurityDescriptor = &sec_desc[(Securable)i];

        /* memcpy 'TRUE' is error */
        ret = memcpy_s( sids[(Securable)i].get(), SECURITY_MAX_SID_SIZE, 
                        sid.get(), SECURITY_MAX_SID_SIZE );
        if(ret)
            return -2;

        ret = InitializeSecurityDescriptor( &sec_desc[(Securable)i], 
                                            SECURITY_DESCRIPTOR_REVISION );
        if(!ret)
            return -3;

        ret = SetSecurityDescriptorGroup( &sec_desc[(Securable)i], 
                                          sids[(Securable)i].get(), FALSE );
        if(!ret)
            return -4;     

        ret = InitializeAcl(acls[(Securable)i].get(), ACL_SIZE, ACL_REVISION);
        if(!ret)
            return -5;
    }
 
    /* add ACEs individually */

    ret = AddAccessDeniedAce( acls[SHEM1].get(), ACL_REVISION, 
                              FILE_MAP_WRITE, sids[SHEM1].get() );
    if(!ret)
        return -6;

    ret = AddAccessAllowedAce( acls[SHEM1].get(), ACL_REVISION, 
                               FILE_MAP_READ, sids[SHEM1].get() );
    if(!ret)
        return -6;

    ret = AddAccessAllowedAce( acls[MUTEX1].get(), ACL_REVISION, 
                               SYNCHRONIZE, sids[MUTEX1].get() );
    if(!ret)
        return -6;

    for(int i = 0; i < NSECURABLE; ++i){
        ret = SetSecurityDescriptorDacl( &sec_desc[(Securable)i], TRUE, 
                                         acls[(Securable)i].get(), FALSE );
        if(!ret)
            return -7;
    }

    return 0;
}
  

bool 
PostItem(std::string sItem, 
         TOS_Topics::TOPICS tTopic, 
         unsigned long timeout)
{  
    HWND convo = convos[tTopic];
    std::string sid_id = std::to_string((size_t)convo) + sItem;

    ack_signals.set_signal_ID(sid_id);
    PostMessage(msg_window, REQUEST_DDE_ITEM, 
                (WPARAM)convo, (LPARAM)(sItem.c_str())); 
    /* 
      for whatever reason a bad item gets a posive ack from an attempt 
      to link it, so that message must post second to give the request 
      a chance to preempt it 
     */    
    PostMessage(msg_window, LINK_DDE_ITEM, 
                (WPARAM)convo, (LPARAM)(sItem.c_str()));    

    return ack_signals.wait_for(sid_id , timeout);
}


bool 
PostCloseItem(std::string sItem, 
              TOS_Topics::TOPICS tTopic, 
              unsigned long timeout)
{  
    HWND convo = convos[tTopic];
    std::string sid_id = std::to_string((size_t)convo) + sItem;

    ack_signals.set_signal_ID(sid_id);
    PostMessage(msg_window, DELINK_DDE_ITEM,
                (WPARAM)convo, (LPARAM)(sItem.c_str()));  

    return ack_signals.wait_for(sid_id, timeout);
}


void 
TearDownTopic(TOS_Topics::TOPICS tTopic, unsigned long timeout)
{  
    PostMessage(msg_window, CLOSE_CONVERSATION, (WPARAM)convos[tTopic], NULL);        
    topic_refs.erase(tTopic); 
    convos.remove(tTopic);  
}


bool 
CreateBuffer(TOS_Topics::TOPICS tTopic, 
             std::string sItem, 
             unsigned int buffer_sz)
{  
    StreamBuffer buf;
    std::string name;

    id_ty id(sItem, tTopic);  

    if(buffers.find(id) != buffers.end())
        return false;

    name = CreateBufferName(TOS_Topics::map[tTopic], sItem);

    buf.raw_sz = (buffer_sz < sys_info.dwPageSize) 
                  ? sys_info.dwPageSize 
                  : buffer_sz;

    buf.hfile = CreateFileMapping(INVALID_HANDLE_VALUE, &sec_attr[SHEM1],
                                  PAGE_READWRITE, 0, buf.raw_sz, name.c_str()); 
    if(!buf.hfile)
        return false;

    buf.raw_addr = MapViewOfFile(buf.hfile, FILE_MAP_ALL_ACCESS, 0, 0, 0);     
    if(!buf.raw_addr){
        CloseHandle(buf.hfile);
        return false;   
    }

    std::string mtx_name = std::string(name).append("_mtx");

    buf.hmtx = CreateMutex(&sec_attr[MUTEX1], FALSE, mtx_name.c_str());
    if(!buf.hmtx){
        CloseHandle(buf.hfile);
        UnmapViewOfFile(buf.raw_addr); 
        return false;
    }
 
    /* cast mem-map to our header and fill values */
    pBufferHead ptmp = (pBufferHead)(buf.raw_addr); 
    ptmp->loop_seq = 0;
    ptmp->next_offset = ptmp->beg_offset = sizeof(BufferHead);
    ptmp->elem_size = TOS_Topics::TypeSize(tTopic) + sizeof(DateTimeStamp);
    ptmp->end_offset = ptmp->beg_offset 
                     + ((buf.raw_sz - ptmp->beg_offset) / ptmp->elem_size) 
                     * ptmp->elem_size ;

    buffers.insert(buffers_ty::value_type(id,buf));
    return true;
}


bool 
DestroyBuffer(TOS_Topics::TOPICS tTopic, std::string sItem)
{   
    BUFFER_LOCK_GUARD;
    /* ---CRITICAL SECTION --- */
    /* don't allow buffer to be destroyed while we're writing to it */
    auto buf_iter = buffers.find(id_ty(sItem,tTopic));
    if( buf_iter != buffers.end() )
    { 
        UnmapViewOfFile(buf_iter->second.raw_addr);
        CloseHandle(buf_iter->second.hfile);
        CloseHandle(buf_iter->second.hmtx);
        buffers.erase(buf_iter);
        return true;
    }
    return false;
    /* ---CRITICAL SECTION --- */
}


void 
DeAllocKernResources() /* only called if we're forced to exit abruptly */
{  
    BUFFER_LOCK_GUARD;
    /* ---CRITICAL SECTION --- */
    for(buffers_ty::value_type & buf : buffers){
        DestroyBuffer(buf.first.second, buf.first.first);
    }
    CloseHandle(init_event);
    /* ---CRITICAL SECTION --- */
}


template<typename T> 
inline void 
ValToBuf(void* pos, T val) { *(T*)pos = val; }

template<> 
inline void 
ValToBuf(void* pos, std::string val) /* copy the string, truncate if necessary */
{ 
    strncpy_s((char*)pos, TOSDB_STR_DATA_SZ, val.c_str(), TOSDB_STR_DATA_SZ-1);
}

template<typename T>
int 
RouteToBuffer(DDE_Data<T> data)
{  
    pBufferHead head;  
    int err = 0;

    BUFFER_LOCK_GUARD;
    /* ---(INTRA-PROCESS) CRITICAL SECTION --- */
    auto buf_iter = buffers.find(id_ty(data.item,data.topic));
    if(buf_iter == buffers.end())  
        return -1;

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
    return err;  
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
        if (!hoptions)
            break;

        lp_options = (DDEADVISE FAR*)GlobalLock(hoptions);
        if (!lp_options){
            GlobalFree(hoptions);
            break;
        }
        lp_options->cfFormat = CF_TEXT;
        lp_options->fAckReq = FALSE;   
        lp_options->fDeferUpd = FALSE; 

        GlobalUnlock(hoptions);

        item = GlobalAddAtom((LPCSTR)lParam);
        if(!item)
            break;

        lp = PackDDElParam(WM_DDE_ADVISE, (UINT)hoptions, item);      
    
        if( !PostMessage((HWND)wParam, WM_DDE_ADVISE, (WPARAM)msg_window, lp) )
        {
            GlobalDeleteAtom(item);
            GlobalFree(hoptions);
            FreeDDElParam(WM_DDE_ADVISE, lp);
        }
        break;
    } 
    case REQUEST_DDE_ITEM:
    {      
        ATOM item = GlobalAddAtom((LPCSTR)lParam);          
        if(!item) 
            break;
  
        if( !PostMessage((HWND)wParam, WM_DDE_REQUEST, (WPARAM)(msg_window), 
                         PackDDElParam(WM_DDE_REQUEST, CF_TEXT, item)) )
        {
            GlobalDeleteAtom(item); 
        }

        break; 
    } 
    case DELINK_DDE_ITEM:
    {      
        ATOM item = GlobalAddAtom((LPCSTR)lParam);      
        LPARAM lp = PackDDElParam(WM_DDE_UNADVISE, 0, item);
      
        if(!item) 
            break;

        if( !PostMessage((HWND)wParam, WM_DDE_UNADVISE, (WPARAM)(msg_window), lp) )
        {
            GlobalDeleteAtom(item);      
            FreeDDElParam(WM_DDE_UNADVISE, lp);
        }   

        break;
    } 
    case CLOSE_CONVERSATION: 
    {
        PostMessage((HWND)wParam, WM_DDE_TERMINATE, (WPARAM)msg_window, NULL);          
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
    
        if (lParam <= 0){      
            pho = HIWORD(lParam); /*topic*/
            plo = LOWORD(lParam); /*app*/

            GlobalGetAtomName((ATOM)(pho), topic_atom, (TOSDB_MAX_STR_SZ + 1)); 
            GlobalGetAtomName((ATOM)(plo), app_atom, (TOSDB_MAX_STR_SZ + 1));

            strcpy_s(app_str_lower, APP_NAME);       

           /* not sure why we were using these
            _strlwr_s(app_atom, TOSDB_MAX_STR_SZ); 
            _strlwr_s(app_str_lower, TOSDB_MAX_STR_SZ); */ 

            str_to_lower(app_atom, TOSDB_MAX_STR_SZ); 
            str_to_lower(app_str_lower, TOSDB_MAX_STR_SZ); 

            if( strcmp(app_atom, app_str_lower) )
                break;   

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
            if(plo == 0x0000){
                std::string sarg(std::to_string((size_t)(HWND)wParam)); 
                ack_signals.signal(sarg + std::string(item_atom), false);
                std::string serr("NEG ACK from server for item: ");
                TOSDB_LogH("DDE", serr.append(item_atom).c_str());
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
        this->topic = d.topic;
        this->item = d.item;
        this->data = d.data;
        this->valid_datetime = d.valid_datetime;
        this->time = d.time;
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
    DDEDATA FAR* dde_data;
    BOOL clnt_rel;
    PVOID data;  
    UINT_PTR atom;
    LPARAM lpneg;
    int cpret;

    char cp_data[TOSDB_STR_DATA_SZ+1]; /* include CR LF, exclude added \0 */
    char item_atom[TOSDB_MAX_STR_SZ + 1];    

    UnpackDDElParam(msg, lparam, (PUINT_PTR)&data, &atom);   
  
    /* if we can't lock the data or its not expected frmt */
    if( !(dde_data = (DDEDATA FAR*) GlobalLock(data)) 
        || (dde_data->cfFormat != CF_TEXT))
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
  
    GlobalGetAtomName((WORD)atom, item_atom, TOSDB_MAX_STR_SZ + 1);
    clnt_rel = dde_data->fRelease;
    GlobalUnlock(data); 
    if(clnt_rel)
        GlobalFree(data);
    GlobalDeleteAtom((WORD)atom); 
    /* need to free lParam, as well, or we leak (not documented well on MSDN) */  
    FreeDDElParam(WM_DDE_DATA, lparam);

    TOS_Topics::TOPICS tTopic = convos[(HWND)(wparam)];
    try{
        std::string str(cp_data); 
        switch(TOS_Topics::TypeBits(tTopic)){
        case TOSDB_STRING_BIT : /* STRING */   
        {
            /* clean up problem chars */         
            auto f = [](char c){return c < 32;};
            auto r = std::remove_if(str.begin(),str.end(),f);
            str.erase(r, str.end());
            RouteToBuffer(DDE_Data<std::string>(tTopic, item_atom, str, true));  
            break;
        }     
        case TOSDB_INTGR_BIT : /* LONG */   
        {
            /* remove commas */
            auto f = [](char c){ return std::isdigit(c) == 0; };
            auto r = std::remove_if(str.begin(), str.end(), f);
            str.erase(r,str.end());
            RouteToBuffer( DDE_Data<def_size_type>(tTopic, item_atom, 
                                                   std::stol(str), true) );  
            break;
        }               
        case TOSDB_QUAD_BIT : /* DOUBLE */
        {
            RouteToBuffer( DDE_Data<ext_price_type>(tTopic, item_atom, 
                                                    std::stod(str), true) ); 
            break;
        }     
        case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :/* LONG LONG */
        {
            /* remove commas */
            auto f = [](char c){ return std::isdigit(c) == 0; };
            auto r = std::remove_if(str.begin(), str.end(), f);
            str.erase(r,str.end());
            RouteToBuffer( DDE_Data<ext_size_type>(tTopic, item_atom, 
                                                   std::stoll(str), true) );  
            break;
        }     
        case 0 : /* FLOAT */
        {
            RouteToBuffer( DDE_Data<def_price_type>(tTopic, item_atom, 
                                                    std::stof(str), true) ); 
            break;
        }     
        };
    }catch(const std::out_of_range& e){      
        TOSDB_LogH("DDE", e.what());
    }catch(const std::invalid_argument& e){    
        std::string serr(e.what());
        TOSDB_LogH("DDE", serr.append(" Value:: ").append(cp_data).c_str());
    }catch(...){    
        throw TOSDB_DDE_Error("unexpected error handling dde data");
    }  

    return /* 0 */; 
}

void DumpBufferStatus()
{  
    const size_t log_col_width[5] = { 30, 30, 10, 60, 16};  
   
    std::string   time_now(SysTimeString());  
    std::string   lpath(TOSDB_LOG_PATH);
    std::string   name("buffer-status-");
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

    {
        TOPIC_LOCK_GUARD;
        /* --- CRITICAL SECTION --- */
        for(const auto & t : topic_refs)
        {
            for(const auto & i : t.second){
                lout << std::setw(log_col_width[0]) << std::left 
                     << TOS_Topics::map[t.first]
                     << std::setw(log_col_width[1]) << std::left << i.first
                     << std::setw(log_col_width[2]) << std::left << i.second       
                     << std::endl;  
            }
        }
        /* --- CRITICAL SECTION --- */
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
