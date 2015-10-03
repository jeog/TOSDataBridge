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
#include "engine.hpp"
#include "dynamic_ipc.hpp"
#include "concurrency.hpp"

namespace {

  LPCSTR  CLASS_NAME = "DDE_CLIENT_WINDOW";
  LPCSTR  LOG_NAME   = "engine-log.log";  
  
  const unsigned int COMM_BUFFER_SIZE = 5; /* opcode + 3 args + {0,0} */
  const unsigned int ACL_SIZE         = 96;
  const unsigned int UPDATE_PERIOD    = 2000;

  /* not guaranteed to be unique; need them at compile time */
  const unsigned int LINK_DDE_ITEM      = 0x0500;
  const unsigned int REQUEST_DDE_ITEM   = 0x0501;
  const unsigned int DELINK_DDE_ITEM    = 0x0502;
  const unsigned int CLOSE_CONVERSATION = 0x0503;  
    
  HINSTANCE hinstance = NULL;
  SYSTEM_INFO sys_info;

  SECURITY_ATTRIBUTES sec_attr[2];
  SECURITY_DESCRIPTOR sec_desc[2];

  SmartBuffer<void> sids[2] = { 
    SmartBuffer<void>(SECURITY_MAX_SID_SIZE), 
    SmartBuffer<void>(SECURITY_MAX_SID_SIZE) 
  };
  SmartBuffer<ACL> acls[2] = { 
    SmartBuffer<ACL>(ACL_SIZE), 
    SmartBuffer<ACL>(ACL_SIZE) 
  };   
  
  buffers_type buffers;  
  topics_type  topics; 
  convos_types convos; 

  LightWeightMutex topic_mtx;
  LightWeightMutex buffer_mtx;
  SignalManager    ack_signals;

  HANDLE init_event      =  NULL;
  HANDLE msg_thrd        =  NULL;
  HWND   msg_window      =  NULL;
  DWORD  msg_thrd_id     =  0;
  LPCSTR msg_window_name =  "TOSDB_ENGINE_MSG_WNDW";

  volatile bool shutdown_flag =  false;  
  volatile bool pause_flag    =  false;
  volatile bool is_service    =  true;

  template<typename T> 
  int  RouteToBuffer(DDE_Data<T> data); // remove rvalue 9/30/15
  int  MainCommLoop(); 
  void TearDownTopic(TOS_Topics::TOPICS tTopic, size_type timeout);
  bool DestroyBuffer(TOS_Topics::TOPICS tTopic, std::string sItem);
  void HandleData(UINT msg, WPARAM wparam, LPARAM lparam);
  void DeAllocKernResources();
  int  CleanUpMain(int ret_code);
  void DumpBufferStatus();
  void CloseAllStreams(size_type timeout);
  int  SetSecurityPolicy();   
  int  AddStream(TOS_Topics::TOPICS tTopic,std::string sItem,size_type timeout);
  bool RemoveStream(TOS_Topics::TOPICS tTopic,std::string sItem,
                    size_type timeout);
  bool PostItem(std::string sItem,TOS_Topics::TOPICS tTopic,size_type timeout);
  bool PostCloseItem(std::string sItem,TOS_Topics::TOPICS tTopic,
                     size_type timeout);   
  bool CreateBuffer(TOS_Topics::TOPICS tTopic, std::string sItem, 
                    unsigned int buffer_sz = TOSDB_SHEM_BUF_SZ);

  DWORD WINAPI     Threaded_Init(LPVOID lParam);
  LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);   

};

const TOS_Topics::topic_map_type& TOS_Topics::map = TOS_Topics::_map;

int WINAPI WinMain(HINSTANCE hInst, 
                   HINSTANCE hPrevInst, 
                   LPSTR lpCmdLn, 
                   int nShowCmd)
{  
  TOSDB_StartLogging(
    std::string(std::string(TOSDB_LOG_PATH) + std::string(LOG_NAME)).c_str());     

  bool res;    
  int err = 0;
  size_t indx = 0;  

  void *parg1 = nullptr;
  void *parg2 = nullptr;
  void *parg3 = nullptr;

  WNDCLASS clss =  {};

  DynamicIPCSlave slave(TOSDB_COMM_CHANNEL, TOSDB_SHEM_BUF_SZ);
  DynamicIPCSlave::shem_chunk shem_buf[COMM_BUFFER_SIZE];
    
  if(!strcmp(lpCmdLn,"--noservice"))
    is_service = false;

  err = SetSecurityPolicy();
  if(err){
    TOSDB_LogH("STARTUP", "engine failed to initialize security objects");
    return -1;
  }
  
  hinstance = GetModuleHandle(NULL);
  clss.hInstance = hinstance;
  clss.lpfnWndProc = WndProc; 
  clss.lpszClassName = CLASS_NAME; 

  RegisterClass(&clss); 
    
  msg_thrd = CreateThread(NULL, 0, Threaded_Init, NULL, 0, &msg_thrd_id);
  if(!msg_thrd)
    return -1;  

  if(WaitForSingleObject(init_event,TOSDB_DEF_TIMEOUT) == WAIT_TIMEOUT)
  {
    TOSDB_LogH("STARTUP","core thread did not receive signal from msg thread");
    return CleanUpMain(-1);
  }    
  
  GetSystemInfo(&sys_info);    

  while(!shutdown_flag)
  {     
    /* BLOCK UNTIL MASTER IS READY */
    if(!slave.wait_for_master()){      
      TOSDB_LogH("IPC", "wait_for_master() failed");
      return CleanUpMain(-1);
    }
    while(!shutdown_flag 
          && (res = slave.recv(shem_buf[indx])))
        { /* ! slave.recv() should follow evaluation of shutdown_flag ! */
          if(shem_buf[indx].sz == 0 
             && shem_buf[indx].offset == 0)
            {  /* shem_buf needs to be {0,0} */
              int i;
              switch(shem_buf[0].offset){
              case TOSDB_SIG_ADD:
              {
                parg1 = slave.shem_ptr(shem_buf[1]); 
                parg2 = slave.shem_ptr(shem_buf[2]); 
                parg3 = slave.shem_ptr(shem_buf[3]);
                if(!parg1 || !parg2 || !parg3)
                {            
                  TOSDB_LogH("IPC","invalid shem_chunk passed to slave");
                  break;
                }         
                i = AddStream(*(TOS_Topics::TOPICS*)parg1, 
                              std::string((char*)parg2), *(size_type*)parg3);           
                slave.send(i);      
              } break;
              case TOSDB_SIG_REMOVE:
              {
                parg1 = slave.shem_ptr(shem_buf[1]); 
                parg2 = slave.shem_ptr(shem_buf[2]); 
                parg3 = slave.shem_ptr(shem_buf[3]);
                if(!parg1 || !parg2 || !parg3)
                { 
                  TOSDB_LogH("IPC","invalid shem_chunk passed to slave");
                  break;
                  }
                i = RemoveStream(*(TOS_Topics::TOPICS*)parg1, 
                                 std::string((char*)parg2), 
                                 *(size_type*)parg3) 
                                   ? 0 
                                   : 1;
                slave.send(i);            
              } break;
              case TOSDB_SIG_PAUSE:
              {
                TOSDB_Log("IPC", "TOSDB_SIG_PAUSE message received");
                if(is_service){      
                  pause_flag = true;
                  slave.send(TOSDB_SIG_GOOD);              
                }            
              } break;
              case TOSDB_SIG_CONTINUE:
              {           
                TOSDB_Log("IPC", "TOSDB_SIG_CONTINUE message received");          
                if(is_service){
                  pause_flag = false;              
                  slave.send(TOSDB_SIG_GOOD);            
                }
              } break;
              case TOSDB_SIG_STOP:
              {           
                TOSDB_Log("IPC", "TOSDB_SIG_STOP message received");         
                if(is_service){
                  shutdown_flag = true;
                  TOSDB_Log("IPC", "shutdown flag set");
                  slave.send(TOSDB_SIG_GOOD);
                }
              } break;
              case TOSDB_SIG_DUMP:
              {
                TOSDB_Log("IPC", "TOSDB_SIG_DUMP message received");
                DumpBufferStatus();
                slave.send(0);
              } break;
              default:
                TOSDB_LogH("IPC","invalid opCode");
              } 
/* SWITCH */
              indx = 0;    
              parg1 = parg2 = parg3 = nullptr;
            }
          else if(++indx >= COMM_BUFFER_SIZE)
            {
              TOSDB_LogH("IPC","shem_chunk buffer full, reseting msg loop");
              indx = 0;        
            }      
        }  
  }  
  CloseAllStreams(TOSDB_DEF_TIMEOUT);
  return CleanUpMain(0);      
}

namespace {    

int CleanUpMain(int ret_code)
{
  ret_code 
    ? TOSDB_LogEx("SHUTDOWN", "engine exiting because of error", ret_code)
    : TOSDB_Log("SHUTDOWN", "engine exiting");

  if(msg_window)
    DestroyWindow(msg_window);

  if(!hinstance)
    hinstance = GetModuleHandle(NULL);

  UnregisterClass(CLASS_NAME, hinstance);
  return ret_code;
}
  
int AddStream(TOS_Topics::TOPICS tTopic, 
              std::string sItem, 
              unsigned long timeout)
{    
  int err = 0;

  if(!(TOS_Topics::enum_type)tTopic)
    return -1;

  topics_type::iterator topic_iter = topics.find(tTopic);  
  if(topic_iter == topics.end()){ 
    /* if topic isn't in our global mapping */   
    ATOM topic_atom;
    ATOM app_atom;
    std::string sTopic;

    init_event = CreateEvent(NULL, FALSE, FALSE, NULL);          
    sTopic = TOS_Topics::map[ tTopic ];  
    topic_atom = GlobalAddAtom(sTopic.c_str());
    app_atom = GlobalAddAtom(TOSDB_APP_NAME);

    ack_signals.set_signal_ID(TOS_Topics::map[ tTopic ]); 

    if(topic_atom){
      SendMessageTimeout((HWND)HWND_BROADCAST,WM_DDE_INITIATE,(WPARAM)msg_window, 
                         MAKELONG(app_atom,topic_atom), SMTO_NORMAL, 500, NULL);  
    }

    if(app_atom) 
      GlobalDeleteAtom(app_atom);

    if(topic_atom) 
      GlobalDeleteAtom(topic_atom);

     /* wait for ack from DDE server */
    if(!ack_signals.wait_for(TOS_Topics::map[ tTopic ], timeout))
      err = -2;       
     
    if(!err){
      topics[tTopic] = refcount_type();
      if(PostItem(sItem, tTopic, timeout))
      {
        topics[tTopic][sItem] = 1;
        if(!CreateBuffer(tTopic, sItem))
          err = -4;     
      }
      else       
        err = -3;       
    }
  }else{ /* if topic IS already in our global mapping */   
    refcount_type::iterator item_iter = topic_iter->second.find(sItem); 
    if(item_iter == topic_iter->second.end())
    { /* and it doesn't have that item yet */
      if(PostItem(sItem, tTopic, timeout))
      {
        topic_iter->second[sItem] = 1;
        if(!CreateBuffer(tTopic, sItem))
          err = -4;      
      }
      else
        err = -3;
    }
    else /* if both already there, increment the ref-count */
      item_iter->second++;    
  }  
  
  switch(err){ /* unwind if it fails during creation */   
  case -4:    
    PostCloseItem(sItem, tTopic, timeout);
    topics[tTopic].erase(sItem);
  case -3:
    if(!topics[tTopic].empty()) 
      break;        
  case -2:        
    TearDownTopic(tTopic, timeout);      
  };

  return err;
}


bool RemoveStream(TOS_Topics::TOPICS tTopic, 
                  std::string sItem, 
                  unsigned long timeout)
{  
  if(!(TOS_Topics::enum_type)tTopic)
    return false;

  topics_type::iterator topic_iter = topics.find(tTopic);  
  if(topic_iter != topics.end()){ 
    /* if topic is in our global mapping */  
    refcount_type::iterator item_iter = topic_iter->second.find(sItem);
    if(item_iter != topic_iter->second.end())
    { /* if it has that item */      
      if(!(--(item_iter->second)))
      { /* if ref-count hits zero */
        if(!PostCloseItem(sItem, tTopic, timeout))
          return false; 
        DestroyBuffer(tTopic, sItem);
        topic_iter->second.erase(item_iter);       
      }
    } 
    if(topic_iter->second.empty()) 
      /* if no items close the convo */
      TearDownTopic(tTopic, timeout);  
    
    return true;  
  }  
  return false;
}

void CloseAllStreams(unsigned long timeout)
{ /* need to iterate through copies */  
  topics_type t_copy;
  refcount_type rc_copy;

  std::copy(topics.begin(), topics.end(),
            std::insert_iterator<topics_type>(t_copy, t_copy.begin()));

  for(const auto& topic: t_copy)
  {
    std::copy(topic.second.begin(), topic.second.end(),
              std::insert_iterator<refcount_type>(rc_copy,rc_copy.begin()));
    for(const auto& item : rc_copy){
      RemoveStream(topic.first, item.first, timeout);
    }
    rc_copy.clear();
  }
}

int SetSecurityPolicy() 
{    
  SID_NAME_USE dummy;

  DWORD dom_sz = 128;
  DWORD sid_sz = SECURITY_MAX_SID_SIZE;

  SmartBuffer<char>  dom_buf(dom_sz);
  SmartBuffer<void>  sid(sid_sz);  

  sec_attr[SHEM1].nLength = 
    sec_attr[MUTEX1].nLength = sizeof(SECURITY_ATTRIBUTES);

  sec_attr[SHEM1].bInheritHandle = sec_attr[MUTEX1].bInheritHandle = FALSE;

  sec_attr[SHEM1].lpSecurityDescriptor = &sec_desc[SHEM1];
  sec_attr[MUTEX1].lpSecurityDescriptor = &sec_desc[MUTEX1];

  if(!LookupAccountName(NULL, "Everyone", sid.get(), &sid_sz, dom_buf.get(), 
                        &dom_sz, &dummy))
    {
      return -1;
    }

  if(memcpy_s(sids[SHEM1].get(), SECURITY_MAX_SID_SIZE, sid.get(), 
              SECURITY_MAX_SID_SIZE) 
     || memcpy_s(sids[MUTEX1].get(), SECURITY_MAX_SID_SIZE, sid.get(), 
                 SECURITY_MAX_SID_SIZE))
    {  
      return -2;
    }

  if(!InitializeSecurityDescriptor(&sec_desc[SHEM1],
                                   SECURITY_DESCRIPTOR_REVISION) 
     || !InitializeSecurityDescriptor(&sec_desc[MUTEX1],
                                      SECURITY_DESCRIPTOR_REVISION))
    {    
      return -3; 
    }

  if(!SetSecurityDescriptorGroup(&sec_desc[SHEM1],sids[SHEM1].get(),FALSE) 
     || !SetSecurityDescriptorGroup(&sec_desc[MUTEX1],sids[MUTEX1].get(),FALSE))
    {
      return -4;
    }

  if(!InitializeAcl(acls[SHEM1].get(), ACL_SIZE, ACL_REVISION) 
     || !InitializeAcl(acls[MUTEX1].get(), ACL_SIZE, ACL_REVISION))
    {
      return -5;
    }

  if(!AddAccessDeniedAce(acls[SHEM1].get(), ACL_REVISION, FILE_MAP_WRITE, 
                         sids[SHEM1].get()) 
     || !AddAccessAllowedAce(acls[SHEM1].get(), ACL_REVISION, FILE_MAP_READ, 
                             sids[SHEM1].get()) 
     || !AddAccessAllowedAce(acls[MUTEX1].get(), ACL_REVISION, SYNCHRONIZE, 
                             sids[MUTEX1].get()))
    {     
      return -6;
    } 

  if(!SetSecurityDescriptorDacl(&sec_desc[SHEM1], TRUE, acls[SHEM1].get(), FALSE) 
     || !SetSecurityDescriptorDacl(&sec_desc[MUTEX1], TRUE, acls[MUTEX1].get(), 
                                   FALSE))
    {
      return -7;
    }

  return 0;
}

DWORD WINAPI Threaded_Init(LPVOID lParam)
{
  if(!hinstance)
    hinstance = GetModuleHandle(NULL);

  msg_window = CreateWindow(CLASS_NAME, msg_window_name, WS_OVERLAPPEDWINDOW, 
                            0, 0, 0, 0, NULL, NULL, hinstance, NULL);  
  if(!msg_window) 
    return 1; 

  SetEvent(init_event);

  MSG msg = {};   
  while(GetMessage(&msg, NULL, 0, 0))
  {  
    TranslateMessage(&msg);
    DispatchMessage(&msg);    
  }   
     
  return 0;
}
  
bool PostItem(std::string sItem, 
              TOS_Topics::TOPICS tTopic, 
              unsigned long timeout)
{  
  HWND convo = convos[tTopic];
  std::string sid_id = std::to_string((size_t)convo) + sItem;

  ack_signals.set_signal_ID(sid_id);
  PostMessage(msg_window,REQUEST_DDE_ITEM,(WPARAM)convo,(LPARAM)(sItem.c_str())); 
  /* 
   *  for whatever reason a bad item gets a posive ack from an attempt 
   *  to link it, so that message must post second to give the request 
   *  a chance to preempt it 
   */    
  PostMessage(msg_window,LINK_DDE_ITEM,(WPARAM)convo,(LPARAM)(sItem.c_str()));    

  return ack_signals.wait_for(sid_id , timeout);
}

bool PostCloseItem(std::string sItem, 
                   TOS_Topics::TOPICS tTopic, 
                   unsigned long timeout)
{  
  HWND convo = convos[tTopic];
  std::string sid_id = std::to_string((size_t)convo) + sItem;

  ack_signals.set_signal_ID(sid_id);
  PostMessage(msg_window, DELINK_DDE_ITEM,(WPARAM)convo,(LPARAM)(sItem.c_str()));  

  return ack_signals.wait_for(sid_id, timeout);
}

void TearDownTopic(TOS_Topics::TOPICS tTopic, unsigned long timeout)
{  
  PostMessage(msg_window, CLOSE_CONVERSATION, (WPARAM)convos[tTopic], NULL);        
  topics.erase(tTopic); 
  convos.remove(tTopic);  
}

bool CreateBuffer(TOS_Topics::TOPICS tTopic, 
                  std::string sItem, 
                  unsigned int buffer_sz)
{    
  type_bits_type tbits;
  StreamBuffer buf;
  std::string name;

  id_type id(sItem, tTopic);  
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

  tbits = TOS_Topics::TypeBits(tTopic);    
  /* cast mem-map to our header and fill values */
  pBufferHead ptmp = (pBufferHead)(buf.raw_addr); 
  ptmp->loop_seq = 0;
  ptmp->next_offset = ptmp->beg_offset = sizeof(BufferHead);

  ptmp->elem_size = ((tbits & TOSDB_STRING_BIT) 
                      ? TOSDB_STR_DATA_SZ 
                      : (tbits & TOSDB_QUAD_BIT) ? 8 : 4) 
                    + sizeof(DateTimeStamp);

  ptmp->end_offset = ptmp->beg_offset 
                   + ((buf.raw_sz - ptmp->beg_offset) / ptmp->elem_size) 
                   * ptmp->elem_size ;

  buffers.insert(buffers_type::value_type(id,buf));
  return true;
}

bool DestroyBuffer(TOS_Topics::TOPICS tTopic, std::string sItem)
{   
  WinLockGuard lock(buffer_mtx);
  /* ---CRITICAL SECTION --- */
  /* don't allow buffer to be destroyed while we're writing to it */
  buffers_type::iterator buf_iter = buffers.find(id_type(sItem,tTopic));
  if(buf_iter != buffers.end())
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

void DeAllocKernResources() 
{ /* 
   * only called if we're forced to exit abruptly 
   */ 
  WinLockGuard lock(buffer_mtx);
  /* ---CRITICAL SECTION --- */
  for(buffers_type::value_type & buf : buffers)
    DestroyBuffer(buf.first.second, buf.first.first);
  CloseHandle(init_event);
   /* ---CRITICAL SECTION --- */
}

template<typename T> 
inline void ValToBuf(void* pos, T val) { *(T*)pos = val; }

template<> 
inline void ValToBuf(void* pos, std::string val) 
{ /* copy the string, truncate if necessary */
  strncpy_s((char*)pos, TOSDB_STR_DATA_SZ, val.c_str(), TOSDB_STR_DATA_SZ-1);
}

template<typename T>
int RouteToBuffer(DDE_Data<T> data)
{  
  pBufferHead head;  
  int err = 0;

  WinLockGuard _lock_(buffer_mtx);
  /* ---(INTRA-PROCESS) CRITICAL SECTION --- */
  buffers_type::iterator bufIter = buffers.find(id_type(data.item,data.topic));
  if(bufIter == buffers.end())  
    return -1;

  head = (pBufferHead)(bufIter->second.raw_addr);
  
  WaitForSingleObject(bufIter->second.hmtx, INFINITE);
  /* ---(INTER-PROCESS) CRITICAL SECTION --- */

  ValToBuf((void*)((char*)head + head->next_offset), data.data);

  *(pDateTimeStamp)
  ((char*)head + head->next_offset + (head->elem_size - sizeof(DateTimeStamp))) 
    = *data.time; 

  if((head->next_offset + head->elem_size) >= head->end_offset)
  {
    head->next_offset = head->beg_offset; 
    ++(head->loop_seq);
  }
  else
    head->next_offset += head->elem_size;

  /* ---(INTER-PROCESS) CRITICAL SECTION --- */
  ReleaseMutex(bufIter->second.hmtx);
  return err;  
  /* ---(INTRA-PROCESS) CRITICAL SECTION --- */
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{  
  switch (message){
  case WM_DDE_DATA: 
  { /*
     * ideally we should de-link it all and store state, then re-init 
     * on continue for now lets just not handle the data from the server 
     */
    if(!pause_flag) 
      HandleData(message, wParam, lParam);
  } break;     
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
    
    if(!PostMessage((HWND)wParam, WM_DDE_ADVISE, (WPARAM)msg_window, lp))
    {
      GlobalDeleteAtom(item);
      GlobalFree(hoptions);
      FreeDDElParam(WM_DDE_ADVISE, lp);
    }
  } break;
  case REQUEST_DDE_ITEM:
  {      
    ATOM item = GlobalAddAtom((LPCSTR)lParam);          
    if(!item) 
      break;
  
    if(!PostMessage((HWND)wParam, WM_DDE_REQUEST, (WPARAM)(msg_window), 
                     PackDDElParam(WM_DDE_REQUEST, CF_TEXT, item)))
      {
        GlobalDeleteAtom(item); 
      }
  } break; 
  case DELINK_DDE_ITEM:
  {      
    ATOM item = GlobalAddAtom((LPCSTR)lParam);      
    LPARAM lp = PackDDElParam(WM_DDE_UNADVISE, 0, item);
      
    if(!item) 
      break;

    if(!PostMessage((HWND)wParam, WM_DDE_UNADVISE, (WPARAM)(msg_window), lp))
    {
      GlobalDeleteAtom(item);      
      FreeDDElParam(WM_DDE_UNADVISE, lp);
    }    
  } break;
  case CLOSE_CONVERSATION:         
    PostMessage((HWND)wParam, WM_DDE_TERMINATE, (WPARAM)msg_window, NULL);          
    break;  
  case WM_DESTROY:        
    PostQuitMessage(0);  
    break;
  case WM_DDE_ACK:  
  {      
    char topic_atom[TOSDB_MAX_STR_SZ + 1];
    char app_atom[TOSDB_MAX_STR_SZ + 1];
    char app_str_lower[TOSDB_MAX_STR_SZ + 1];
    char item_atom[TOSDB_MAX_STR_SZ + 1];  
    
    UINT_PTR pho = 0; 
    UINT_PTR plo = 0;   
    
    if (lParam <= 0)
    {      
      pho = HIWORD(lParam); /*topic*/
      plo = LOWORD(lParam); /*app*/

      GlobalGetAtomName((ATOM)(pho), topic_atom, (TOSDB_MAX_STR_SZ + 1)); 
      GlobalGetAtomName((ATOM)(plo), app_atom, (TOSDB_MAX_STR_SZ + 1));

      strcpy_s(app_str_lower, TOSDB_APP_NAME);      
      _strlwr_s(app_atom, TOSDB_MAX_STR_SZ); 
      _strlwr_s(app_str_lower, TOSDB_MAX_STR_SZ);       
    
      if(strcmp(app_atom, app_str_lower))
        break;   

      convos.insert(
        convos_types::pair1_type(TOS_Topics::map[topic_atom],(HWND)wParam));

      ack_signals.signal(topic_atom, true);
    }
    else
    {
      UnpackDDElParam(message,lParam, (PUINT_PTR)&plo, (PUINT_PTR)&pho); 
      GlobalGetAtomName((ATOM)(pho), item_atom, (TOSDB_MAX_STR_SZ + 1));  
      if(plo == 0x0000)
      {
        ack_signals.signal(
          std::to_string((size_t)(HWND)wParam) + std::string(item_atom), false);
        TOSDB_LogH("DDE", std::string("NEG ACK from server for item: ")
                             .append(item_atom).c_str());
      }
      else if(plo == 0x8000)
      {
        ack_signals.signal(
          std::to_string((size_t)(HWND)wParam) + std::string(item_atom), true);   
      }       
    }        
   } break;    
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }  
  /* SWITCH */
  return 0;
}

void HandleData(UINT msg, WPARAM wparam, LPARAM lparam)
{  
  DDEDATA FAR* dde_data;
  BOOL clnt_rel;
  PVOID data;  
  UINT_PTR atom;
  char cp_data[TOSDB_STR_DATA_SZ+1]; /* include CR LF, excluded added \0 */
  char item_atom[TOSDB_MAX_STR_SZ + 1];
  LPARAM lParamNeg;

  UnpackDDElParam(msg, lparam, (PUINT_PTR)&data, &atom);   
  
  if(!(dde_data = (DDEDATA FAR*) GlobalLock(data)) 
     || (dde_data->cfFormat != CF_TEXT))
    { /* if we can't lock the data or its not expected frmt */
      lParamNeg = PackDDElParam(WM_DDE_ACK, 0, (UINT_PTR)atom); 
      /* SEND NEG ACK TO SERVER  - convo already destroyed if NULL */
      PostMessage((HWND)wparam, WM_DDE_ACK, (WPARAM)msg_window, lParamNeg);
      GlobalDeleteAtom((WORD)atom);
      FreeDDElParam(WM_DDE_ACK, lParamNeg);
      return; 
    }  

  if(strncpy_s(cp_data, (LPCSTR)(dde_data->Value), TOSDB_STR_DATA_SZ))
    TOSDB_LogH("DDE", "error copying data->value[] string");   

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
      str.erase(std::remove_if(str.begin(),str.end(),[](char c){return c < 32;}), 
                str.end());
      RouteToBuffer(DDE_Data<std::string>(tTopic, item_atom, str, true));  
    } 
    break;
    case TOSDB_INTGR_BIT : /* LONG */   
    {
      /* remove commas */
      str.erase(std::remove_if(str.begin(), str.end(),
                               [](char c){ return std::isdigit(c) == 0; }), 
                str.end());

      RouteToBuffer(
        DDE_Data<def_size_type>(tTopic, item_atom, std::stol(str), true));  
    } 
    break;          
    case TOSDB_QUAD_BIT : /* DOUBLE */
    {
      RouteToBuffer(
        DDE_Data<ext_price_type>(tTopic, item_atom, std::stod(str), true));      
    } 
    break;
    case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT :/* LONG LONG */
    {
      /* remove commas */
      str.erase(std::remove_if(str.begin(), str.end(),
                               [](char c){ return std::isdigit(c) == 0; }), 
                str.end());

      RouteToBuffer(
        DDE_Data<ext_size_type>(tTopic, item_atom, std::stoll(str), true));      
    } 
    break;
    case 0 : /* FLOAT */
    {
      RouteToBuffer(
        DDE_Data<def_price_type>(tTopic, item_atom, std::stof(str), true)); 
    } 
    break;
    };   
    /* SWITCH */   
  }catch(const std::out_of_range& e){      
    TOSDB_LogH("DDE", e.what());
  }catch(const std::invalid_argument& e){    
    TOSDB_LogH("DDE", std::string(e.what()).append(" Value:: ")
                                           .append(cp_data).c_str());
  }catch(...){    
    throw TOSDB_DDE_Error("unexpected error handling dde data");
  }  
  return /* 0 */; 
}

void DumpBufferStatus()
{  
  const size_type log_col_width[5] = { 30, 30, 10, 60, 16};  
   
  std::string   time_now(SysTimeString());    
  std::string   name("buffer-status-");
  std::ofstream lout;
    
  std::replace_if(time_now.begin(), time_now.end(),
                  [](char x){ return std::isalnum(x) == 0; }, '-'); 

  lout.open(
    std::string(TOSDB_LOG_PATH).append(name).append(time_now).append(".log"),   
    std::ios::out | std::ios::app);

  lout <<" --- TOPIC INFO --- " << std::endl;
  lout << std::setw(log_col_width[0]) << std::left << "Topic"
       << std::setw(log_col_width[1]) << std::left << "Item"
       << std::setw(log_col_width[2]) << std::left << "Ref-Count" 
       << std::endl;

  {
    WinLockGuard lock(topic_mtx);
    /* --- CRITICAL SECTION --- */
    for(const auto & t : topics)
      for(const auto & i : t.second)
      {
        lout << std::setw(log_col_width[0]) << std::left 
             << TOS_Topics::map[t.first]
             << std::setw(log_col_width[1]) << std::left << i.first
             << std::setw(log_col_width[2]) << std::left << i.second       
             << std::endl;  
      }
    /* --- CRITICAL SECTION --- */
  }

  lout <<" --- BUFFER INFO --- " << std::endl;  
  lout << std::setw(log_col_width[3])<< std::left << "BufferName"
       << std::setw(log_col_width[4])<< std::left << "Handle" << std::endl;
  {
    WinLockGuard lock(buffer_mtx);
    /* --- CRITICAL SECTION --- */
    for(const auto & b : buffers)
    {
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
