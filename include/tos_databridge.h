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

#ifndef JO_TOSDB_DATABRIDGE
#define JO_TOSDB_DATABRIDGE

#ifndef _WIN32
#error tos-databridge core libs require Windows !
#endif

/* internal objects of exported impl classes not exported;
  look here if link errors on change of_tos-databridge... mods */
#pragma warning(once : 4251)
/* decorated name length exceeded warning */
#pragma warning(disable : 4503)
/* chkd iterator warning: we check internally */
#pragma warning(disable : 4996)
/* BOOL to bool warn */
#pragma warning(disable : 4800)

#ifdef __cplusplus
#define EXT_C_SPEC extern "C"
/* make things a bit clearer */
#define NO_THROW __declspec(nothrow) 
#else 
#define EXT_C_SPEC 
#define NO_THROW
#endif

/* the following manages what is imported/exported from various modules, 
   check the preprocessor options for each module to see what is defined*/

#if defined(THIS_EXPORTS_INTERFACE)
#define DLL_SPEC_IFACE __declspec (dllexport)
#elif defined(THIS_DOESNT_IMPORT_INTERFACE)
#define DLL_SPEC_IFACE 
#else 
#define DLL_SPEC_IFACE __declspec (dllimport) /* default == import */
#endif /* INTERFACE */

#if defined(THIS_EXPORTS_IMPLEMENTATION)
#define DLL_SPEC_IMPL __declspec (dllexport)
#elif defined(THIS_IMPORTS_IMPLEMENTATION)
#define DLL_SPEC_IMPL __declspec (dllimport)
#else 
#define DLL_SPEC_IMPL /* default == nothing */
#endif /* IMPLEMENTATION */


#include <windows.h> 
/* C API uses Window's string typedefs:
     typedef const char* LPCSTR
     typedef char*       LPSTR          */
#include <time.h>
#include <limits.h>
#include <stdint.h>

#ifdef __cplusplus

#include <map>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>

/* forward declaration for generic.hpp */
//class DLL_SPEC_IFACE TOSDB_Generic;

#include "containers.hpp"/*custom client-facing containers */
#include "generic.hpp"  /* our 'generic' type */
#include "exceptions.hpp" /* our global exceptions */

#endif /* __cplusplus */


#ifdef NO_KERNEL_GLOBAL_NAMESPACE
/* because the service operates in session #0 and the engine/client code 
   will run in session #1 (or higher) we, in most cases, need to use the 
   global namespace for mutexes and file-mapping kernel objects  
   
   in rare cases(debugging service in same session) we should
   define NO_KERNEL_GLOBAL_NAMESPACE to use the local namespace */
#define NO_KGBLNS
/***CRASH WARNING *** 

   if you try to run tos-databridge-engine directly (not recommended)
   without elevated privileges you will probably crash from an uncaught
   exception in the IPCSlave constructor caused by an attempt to 
   create kernel objects in the global namespace w/o the appropriate 
   privileges.

   1) you probably should only run the engine via the service 

   2) if for some reason you absolutley have to, recompile with 
      NO_KERNEL_GLOBAL_NAMESPACE defined or use eleveated privileges
      to run the .exe

 ***CRASH WARNING ***/
#endif

#ifdef CUSTOM_COMM_CHANNEL
/* generally we want a single, static comm channel so client can't accidentaly run 
   multiple instances of the engine and/or break the underlying IPC mechanism
   
   If a user tries to create a second IPCSlave (maybe forgeting to close an old
   engine before starting a new one) it will crash during construction as it tries
   to create kernel objects that already exist, causing a runtime_exception to be thrown.
   (this will generate log messages w/ the IPC-Slave tag in log/engine.log)

   Defining CUSTOM_COMM_CHANNEL with the name of the channel to use allows us to override 
   this behavior. Obviously YOU SHOULD BE VERY CAREFUL DOING THIS. */
#define TOSDB_COMM_CHANNEL CUSTOM_COMM_CHANNEL
#else
#define TOSDB_COMM_CHANNEL "TOSDB_channel_1"
#endif

/* the core types implemented by the data engine: engine-core.cpp 

   when using large blocks the size difference between 
   4 and 8 byte scalars can make a big difference */
typedef long       def_size_type; 
typedef long long  ext_size_type;
typedef float      def_price_type;
typedef double     ext_price_type;

/* guarantees for the Python wrapper */
typedef uint32_t size_type;
typedef uint8_t type_bits_type;

struct TypeSizeAsserts_{ 
    /* sanity checks */
    char ASSERT_def_size_type_atleast_4bytes[sizeof(def_size_type) >= 4 ? 1 : -1];
    char ASSERT_ext_size_type_is_8bytes[sizeof(ext_size_type) == 8 ? 1 : -1];
    char ASSERT_def_price_type_atleast_4bytes[sizeof(def_price_type) >= 4 ? 1 : -1];
    char ASSERT_ext_price_type_is_8bytes[sizeof(ext_price_type) == 8 ? 1 : -1];
    char ASSERT_size_type_is_4bytes[sizeof(size_type) == 4 ? 1 : -1];
    char ASSERT_type_bits_type_is_1byte[sizeof(type_bits_type) == 1 ? 1 : -1];
    /* sanity checks */
};

typedef const enum{ /*milliseconds*/
    Fastest = 0, 
    VeryFast = 3, 
    Fast = 30, 
    Moderate = 300, 
    Slow = 3000, 
    Glacial = 30000 /* <-- DEBUG ONLY */
}UpdateLatency;

typedef struct{
    struct tm  ctime_struct;
    long       micro_second;
} DateTimeStamp, *pDateTimeStamp;

/* reserve a block name for the implementation */
#define TOSDB_RESERVED_BLOCK_NAME "___RESERVED_BLOCK_NAME___"

/* the connected states we can be in; see TOSDB_ConnectedState() */
#define TOSDB_CONN_NONE 0
#define TOSDB_CONN_ENGINE 1
#define TOSDB_CONN_ENGINE_TOS 2

/* NEED for tosdb/setup.py !!! DO NOT REMOVE !!! */
#define TOSDB_INTGR_BIT ((type_bits_type)0x80)
#define TOSDB_QUAD_BIT ((type_bits_type)0x40)
#define TOSDB_STRING_BIT ((type_bits_type)0x20)
#define TOSDB_TOPIC_BITMASK ((type_bits_type)0xE0)
#define TOSDB_STR_DATA_SZ ((size_type)40)
#define TOSDB_MAX_STR_SZ ((size_type)0xFF)
#define TOSDB_DEF_TIMEOUT  2000
#define TOSDB_DEF_PAUSE    (TOSDB_DEF_TIMEOUT / 10)
#define TOSDB_MIN_TIMEOUT  1500
#define TOSDB_SHEM_BUF_SZ  4096
#define TOSDB_BLOCK_ID_SZ  63 
#define TOSDB_MAX_BLOCK_SZ INT_MAX 
#define TOSDB_DEF_LATENCY Fast
/* NEED for tosdb/setup.py !!! DO NOT REMOVE !!! */ 

/* following block will contain back-end stuff shared by various mods;
   to access you should define 'THIS_IMPORTS_IMPLEMENTATION'  */
#if defined(THIS_EXPORTS_IMPLEMENTATION) || defined(THIS_IMPORTS_IMPLEMENTATION)

#ifdef __cplusplus
/* forward declarations for _tos-databridge.dll */

/* CONCURRENCY - concurrency.cpp / concurrency.hpp - define CPP_COND_VAR to use 
   portable sync objects built on std::condition_variable. (doesn't work 
   properly in VS2012: throws access violation)    */
#ifdef CPP_COND_VAR
class DLL_SPEC_IMPL SignalManager
#else
class DLL_SPEC_IMPL LightWeightMutex;
class DLL_SPEC_IMPL WinLockGuard;
class DLL_SPEC_IMPL SignalManager;
#endif  /* CPP_COND_VAR */

class DLL_SPEC_IMPL IPCNamedMutexClient;

/* IPC - ipc.cpp / ipc.hpp */
class DLL_SPEC_IMPL IPCBase;
class DLL_SPEC_IMPL IPCMaster;
class DLL_SPEC_IMPL IPCSlave;

/* for C code: create a string of form: "TOSDB_[topic name]_[item name]"  
   only alpha-numerics */
DLL_SPEC_IMPL std::string 
CreateBufferName(std::string topic_str, std::string item);

DLL_SPEC_IMPL std::string
BuildLogPath(std::string name);

DLL_SPEC_IMPL std::string 
SysTimeString();

DLL_SPEC_IMPL void 
ParseArgs(std::vector<std::string>& vec, std::string str);

#endif /*__cplusplus */

#define TOSDB_SIG_ADD 1
#define TOSDB_SIG_REMOVE 2
#define TOSDB_SIG_PAUSE 3
#define TOSDB_SIG_CONTINUE 4
#define TOSDB_SIG_STOP 5
#define TOSDB_SIG_DUMP 6
#define TOSDB_SIG_GOOD 7 
#define TOSDB_SIG_BAD 8 
#define TOSDB_SIG_TEST 9

typedef const enum{ 
    SHEM1 = 0, 
    MUTEX1, 
    PIPE1 
}Securable;

typedef struct{ 
    /* 
      header that will be placed at the front(offset 0) of the mem mapping 
      logical location: beg_offset + ((raw_size - beg_offset) // elem_size) 
     */
    volatile unsigned int loop_seq;    /* # of times buffer has looped around */
    volatile unsigned int elem_size;   /* size of elements in the buffer */
    volatile unsigned int beg_offset;  /* logical location (after header) */  
    volatile unsigned int end_offset;  /* logical location (after header) */ 
    volatile unsigned int next_offset; /* logical location of next write */  
} BufferHead, *pBufferHead; 

/* we still need to export this for DumpBufferStatus in engine.cpp */
extern char  DLL_SPEC_IMPL  TOSDB_LOG_PATH[ MAX_PATH+40 ]; 

/* we'll log to a relative directory - REQUIRES A STABLE DIRECTORY TREE 
   this will be relative to the back-end lib: _tos-databridge-[].dll */
#define LOCAL_LOG_PATH "\\..\\..\\..\\log\\" /* needs to be < 40 char */

/* LOGGING - logging.cpp */

DLL_SPEC_IMPL void    /* TODO make same frmt as all other log funcs */
TOSDB_Log_Raw(LPCSTR); /* impl only (doesn't sync/block) */

EXT_C_SPEC  DLL_SPEC_IMPL  void  
StartLogging(LPCSTR fname);

EXT_C_SPEC  DLL_SPEC_IMPL  void  
StopLogging();

EXT_C_SPEC  DLL_SPEC_IMPL  void  
ClearLog();

#endif /* THIS_EXPORTS_IMPLEMENTATION || THIS_IMPORTS_IMPLEMENTATION */

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int
TOSDB_GetClientLogPath(char* path, size_type sz);

#ifdef __cplusplus

typedef std::chrono::steady_clock  steady_clock_type;
typedef std::chrono::system_clock  system_clock_type;
typedef std::chrono::microseconds  micro_sec_type; 

/* Generic STL Types returned by the interface(below) */ 
typedef TOSDB_Generic                                     generic_type;
typedef std::pair<generic_type,DateTimeStamp>             generic_dts_type; 
typedef std::vector<generic_type>                         generic_vector_type;
typedef std::vector<DateTimeStamp>                        dts_vector_type;
typedef std::pair<generic_vector_type,dts_vector_type>    generic_dts_vectors_type;  
typedef std::map<std::string, generic_type>               generic_map_type;
typedef std::map<std::string, generic_map_type>           generic_matrix_type;  
typedef std::map<std::string, generic_dts_type>           generic_dts_map_type;
typedef std::map<std::string, generic_dts_map_type>       generic_dts_matrix_type;

#define TOSDB_BIT_SHIFT_LEFT(T,val) (((T)val)<<((sizeof(T)-sizeof(type_bits_type))*8))
#define TOSDB_BIT_SHIFT_RIGHT(T,val) (((T)val)>>((sizeof(T)-sizeof(type_bits_type))*8))

template<typename T> 
class Topic_Enum_Wrapper {
   /*
      The TOPICS enum and utility functions/templates inside the wrapper will end 
      up being defined in each module but trying to export it from both the backend 
      and the client-side libs creates all kinds of problems.

      The enum-string mapping is stored in the static TwoWayHashMap 'map' and 
      is used internally. Client code should use the exported MAP() static function
      which returns a const reference to 'map'.
    */ 
    static_assert(std::is_integral<T>::value && !std::is_same<T,bool>::value, 
                  "Invalid Topic_Enum_Wrapper<T> Type");

    Topic_Enum_Wrapper() 
        {
        }  

    static const T ADJ_INTGR_BIT  = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_INTGR_BIT);
    static const T ADJ_QUAD_BIT   = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_QUAD_BIT);
    static const T ADJ_STRING_BIT = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_STRING_BIT);
    static const T ADJ_FULL_MASK  = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_TOPIC_BITMASK);

public:
    enum class TOPICS 
        : T { /* 
               * pack type info into HO nibble of scoped Enum
               * gaps represent the 'reserved' exported TOS/DDE fields
               * [ see topics.cpp ] 
               */
    /* BEGIN TOPICS ksxaw9834hr84hf;esij?><  DO NOT EDIT !!! */
    NULL_TOPIC = 0x00, 
    HIGH52 = 0x1,
    LOW52 = 0x2,
    //
    ASK = 0x6 | ADJ_QUAD_BIT,
    ASKX = 0x7 | ADJ_STRING_BIT,
    ASK_SIZE = 0x8 | ADJ_INTGR_BIT,
    //
    AX = 0xa | ADJ_STRING_BIT,
    //
    BACK_EX_MOVE = 0x15 | ADJ_QUAD_BIT,
    BACK_VOL = 0x16 | ADJ_QUAD_BIT,
    BA_SIZE = 0x17 | ADJ_STRING_BIT,
    BETA = 0x18 | ADJ_QUAD_BIT,
    BID = 0x19 | ADJ_QUAD_BIT,
    BIDX = 0x1a | ADJ_STRING_BIT,
    BID_SIZE = 0x1b | ADJ_INTGR_BIT,
    BX = 0x1c | ADJ_STRING_BIT,
    //
    CALL_VOLUME_INDEX = 0x27,
    //
    //CLOSE = 0x2a | ADJ_QUAD_BIT,
    //
    DELTA = 0x46 | ADJ_QUAD_BIT,
    //
    DESCRIPTION = 0x48 | ADJ_STRING_BIT,
    //
    DIV = 0x4b,
    DIV_FREQ = 0x4c | ADJ_STRING_BIT,
    //
    //DT = 0x52 | ADJ_STRING_BIT,
    //
    EPS = 0x61,
    EXCHANGE = 0x62 | ADJ_STRING_BIT,
    EXPIRATION = 0x63 | ADJ_STRING_BIT,
    EXTRINSIC = 0x64 | ADJ_QUAD_BIT,
    EX_DIV_DATE = 0x65 | ADJ_STRING_BIT,
    EX_MOVE_DIFF = 0x66 | ADJ_QUAD_BIT,
    //
    FRONT_EX_MOVE = 0x73 | ADJ_QUAD_BIT,
    FRONT_VOL = 0x74 | ADJ_QUAD_BIT,
    //
    FX_PAIR = 0x78 | ADJ_STRING_BIT,
    //
    GAMMA = 0x7d | ADJ_QUAD_BIT,
    //
    HIGH = 0x7f | ADJ_QUAD_BIT,
    HTB_ETB = 0x80 | ADJ_STRING_BIT,
    //
    IMPL_VOL = 0x8a | ADJ_QUAD_BIT,
    INTRINSIC = 0x8b,
    //
    LAST = 0x95 | ADJ_QUAD_BIT,
    LASTX = 0x96 | ADJ_STRING_BIT,
    LAST_SIZE = 0x97 | ADJ_INTGR_BIT,
    //
    LOW = 0x9b | ADJ_QUAD_BIT,
    LX = 0x9c | ADJ_STRING_BIT,
    //
    MARK = 0xa9 | ADJ_QUAD_BIT,
    MARKET_CAP = 0xaa | ADJ_STRING_BIT,
    MARK_CHANGE = 0xab | ADJ_QUAD_BIT,
    MARK_PERCENT_CHANGE = 0xac | ADJ_QUAD_BIT,
    //
    MRKT_MKR_MOVE = 0xaf,
    MT_NEWS = 0xb0 | ADJ_STRING_BIT,
    //
    NET_CHANGE = 0xc9 | ADJ_QUAD_BIT,
    //
    OPEN = 0xcb | ADJ_QUAD_BIT ,
    OPEN_INT = 0xcc | ADJ_INTGR_BIT,
    OPTION_VOLUME_INDEX = 0xcd,
    //
    PE = 0xd5,
    PERCENT_CHANGE = 0xd6 | ADJ_QUAD_BIT,
    //
    PUT_CALL_RATIO = 0xda,
    PUT_VOLUME_INDEX = 0xdb,
    //
    RHO = 0xed | ADJ_QUAD_BIT,
    //
    SHARES = 0xff | ADJ_INTGR_BIT | ADJ_QUAD_BIT,
    //
    STRENGTH_METER = 0x102 | ADJ_STRING_BIT,
    STRIKE = 0x103,
    SYMBOL = 0x104 | ADJ_STRING_BIT,
    //
    THETA = 0x11f | ADJ_QUAD_BIT,
    //
    VEGA = 0x13c | ADJ_QUAD_BIT,
    VOLUME = 0x13d | ADJ_INTGR_BIT | ADJ_QUAD_BIT,
    VOL_DIFF = 0x13e | ADJ_QUAD_BIT,
    VOL_INDEX = 0x13f | ADJ_QUAD_BIT,
    //
    WEIGHTED_BACK_VOL = 0x14b | ADJ_QUAD_BIT,
    //
    YIELD = 0x152  
    /* END TOPICS ksxaw9834hr84hf;esij?><  DO NOT EDIT !!! */
    };

    typedef T enum_value_type; 
    typedef typename Topic_Enum_Wrapper<T>::TOPICS enum_type;

    template<enum_type topic>
    struct Type{ /* type at compile-time */
        /* e.g TOS_Topics::Type<TOS_Topics::LAST>::type == ext_price_type */
        typedef typename std::conditional<
            ((T)topic & ADJ_STRING_BIT), std::string, 
            typename std::conditional<
                (T)topic & ADJ_INTGR_BIT,           
                typename std::conditional<
                    (T)topic & ADJ_QUAD_BIT, ext_size_type, def_size_type>::type,
                    typename std::conditional<
                        (T)topic & ADJ_QUAD_BIT, 
                        ext_price_type, def_price_type>::type>::type>::type  type;
    };
  
    static inline type_bits_type 
    TypeBits(enum_type topic) /* type bits at run-time */
    { 
        return ((type_bits_type)(TOSDB_BIT_SHIFT_RIGHT(T, (T)topic)) 
                & TOSDB_TOPIC_BITMASK); 
    }

    static size_type 
    TypeSize(enum_type topic) /* type size at run-time */
    { 
        switch(TypeBits(topic)){
        case TOSDB_STRING_BIT:                 return TOSDB_STR_DATA_SZ;
        case TOSDB_INTGR_BIT:                  return sizeof(def_size_type);
        case TOSDB_QUAD_BIT:                   return sizeof(ext_price_type);
        case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT: return sizeof(ext_size_type);
        default :                              return sizeof(def_price_type);
        }; 
    }
  
    static std::string 
    TypeString(enum_type topic) /* platform-dependent type strings at run-time */
    {     
        switch(TypeBits(topic)){
        case TOSDB_STRING_BIT:                 return typeid(std::string).name();
        case TOSDB_INTGR_BIT:                  return typeid(def_size_type).name();
        case TOSDB_QUAD_BIT:                   return typeid(ext_price_type).name();
        case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT: return typeid(ext_size_type).name();
        default :                              return typeid(def_price_type).name();
        }; 
    }

#ifdef CUSTOM_HASHER
    /* we can't guarantee that the particular STL implementation will  
       provide a default hasher like VS */
    struct hasher{
        size_t operator()(const enum_type& t) const { 
            static_assert(std::is_unsigned<enum_value_type>::value,
                          "hasher only accepts unsigned integral enum_value_types");
            return (enum_value_type)t; 
        }
    };
    typedef TwoWayHashMap<enum_type, std::string, false, hasher> topic_map_type;
#else
    typedef TwoWayHashMap<enum_type, std::string> topic_map_type;
#endif

    typedef typename topic_map_type::pair1_type  topic_map_entry_type;  

    /* 'map' is defined in topics.cpp as part of the _tos-databrige[].dll module;
       therefore, the client facing lib (tos-databridge[].dll) and the service/engine
       need to import it, but the former ALSO needs to export it.

       In this case we define 'map' so the back-end can see it, the client-side lib
       can import it, and ALSO export the MAP() wrapper to client code.           
     
       long-story-short, just use MAP() to access the topic-string mapping    */

    /* internally we use map (link w/ _tos-databridge[].dll) */
#if defined(THIS_IMPORTS_IMPLEMENTATION) || defined(THIS_EXPORTS_IMPLEMENTATION)
    static DLL_SPEC_IMPL const topic_map_type map;
#endif

    /* externally we use the MAP() (link w/ tos-databridge-[].dll) */
    static DLL_SPEC_IFACE const topic_map_type&   
#if defined(THIS_EXPORTS_INTERFACE) || defined(THIS_DOESNT_IMPORT_INTERFACE)
    MAP(){ return map; }
#else
    MAP();
#endif 
  
    struct top_less{ /* back-end should use 'map' but attempts to #define 
                        special versions of top_less won't link w/ client code */      
        bool operator()(const enum_type& left, const enum_type& right){ 
            return (MAP()[left] < MAP()[right]); 
        }
    };

};

typedef Topic_Enum_Wrapper<unsigned short>  TOS_Topics;

typedef ILSet<std::string> str_set_type;
typedef ILSet<const typename TOS_Topics::TOPICS, typename TOS_Topics::top_less> topic_set_type;

#endif

/* 'Administrative' C API  -  client_admin.cpp

   NOTE: int return types indicate an error value is returned */

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_Connect();

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_Disconnect();

/* DEPRECATED - Jan 10 2017 */
EXT_C_SPEC DLL_SPEC_IFACE NO_THROW unsigned int   
TOSDB_IsConnected();

/* ... in favor of... */
EXT_C_SPEC DLL_SPEC_IFACE NO_THROW unsigned int 
TOSDB_IsConnectedToEngine();

/* ... and... */
EXT_C_SPEC DLL_SPEC_IFACE NO_THROW unsigned int 
TOSDB_IsConnectedToEngineAndTOS();

/* ... and... */
EXT_C_SPEC DLL_SPEC_IFACE NO_THROW unsigned int   
TOSDB_ConnectionState();

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_CreateBlock(LPCSTR id, size_type sz, BOOL is_datetime, size_type timeout) ;

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_CloseBlock(LPCSTR id);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_CloseBlocks();

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW size_type      
TOSDB_GetBlockLimit();

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW size_type      
TOSDB_SetBlockLimit(size_type sz);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW size_type      
TOSDB_GetBlockCount();

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_GetBlockIDs(LPSTR* dest, size_type array_len, size_type str_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_GetBlockSize(LPCSTR id, size_type* pSize);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_SetBlockSize(LPCSTR id, size_type sz);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW unsigned long  
TOSDB_GetLatency();

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW unsigned long 
TOSDB_SetLatency(UpdateLatency latency);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_Add(LPCSTR id, LPCSTR* items, size_type items_len, LPCSTR* topics_str , size_type topics_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_AddTopic(LPCSTR id, LPCSTR topic_str);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_AddItem(LPCSTR id, LPCSTR item);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_AddTopics(LPCSTR id, LPCSTR* topics_str, size_type topics_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_AddItems(LPCSTR id, LPCSTR* items, size_type items_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_RemoveTopic(LPCSTR id, LPCSTR topic_str); 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_RemoveItem(LPCSTR id, LPCSTR item);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_GetItemCount(LPCSTR id, size_type* count);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_GetTopicCount(LPCSTR id, size_type* count);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_GetTopicNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_GetItemNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len);


/*** --- BEGIN --- Oct 30 2016 ***/

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_GetPreCachedItemCount(LPCSTR id, size_type* count);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_GetPreCachedTopicCount(LPCSTR id, size_type* count);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int          
TOSDB_GetPreCachedTopicNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int          
TOSDB_GetPreCachedItemNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len);

/*** --- END --- Oct 30 2016 ***/


EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_GetTypeBits(LPCSTR topic_str, type_bits_type* type_bits);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_GetTypeString(LPCSTR topic_str, LPSTR dest, size_type str_len);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_IsUsingDateTime(LPCSTR id, unsigned int* is_datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_GetStreamOccupancy(LPCSTR id,LPCSTR item, LPCSTR topic_str, size_type* sz);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_GetMarkerPosition(LPCSTR id,LPCSTR item, LPCSTR topic_str, long long* pos);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int            
TOSDB_IsMarkerDirty(LPCSTR id, LPCSTR item, LPCSTR topic_str, unsigned int* is_dirty);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int           
TOSDB_DumpSharedBufferStatus();

#ifdef __cplusplus

/* (extended) 'Administrative' C++ API  -  client_admin.cpp

   NOTE: ints indicate error/success (as above) BUT these calls may also throw*/

DLL_SPEC_IFACE int   
TOSDB_Add(std::string id, str_set_type items, topic_set_type topics_t);

/* Nov 29 2016 */
DLL_SPEC_IFACE  int   
TOSDB_AddItem(std::string id, std::string item);
/* Nov 29 2016 */

DLL_SPEC_IFACE  int   
TOSDB_AddTopic(std::string id, TOS_Topics::TOPICS topic_t);

DLL_SPEC_IFACE  int   
TOSDB_AddTopics(std::string id, topic_set_type topics_t);

DLL_SPEC_IFACE  int   
TOSDB_AddItems(std::string id, str_set_type items);

DLL_SPEC_IFACE int   
TOSDB_RemoveItem(std::string id, std::string item); 

DLL_SPEC_IFACE int   
TOSDB_RemoveTopic(std::string id, TOS_Topics::TOPICS topic_t); 

DLL_SPEC_IFACE str_set_type    
TOSDB_GetBlockIDs();

DLL_SPEC_IFACE topic_set_type  
TOSDB_GetTopicEnums(std::string id);

DLL_SPEC_IFACE str_set_type   
TOSDB_GetTopicNames(std::string id);

DLL_SPEC_IFACE str_set_type    
TOSDB_GetItemNames(std::string id);

DLL_SPEC_IFACE topic_set_type  
TOSDB_GetPreCachedTopicEnums(std::string id);

DLL_SPEC_IFACE str_set_type   
TOSDB_GetPreCachedTopicNames(std::string id);

DLL_SPEC_IFACE str_set_type   
TOSDB_GetPreCachedItemNames(std::string id);

DLL_SPEC_IFACE type_bits_type  
TOSDB_GetTypeBits(TOS_Topics::TOPICS topic_t);

DLL_SPEC_IFACE std::string     
TOSDB_GetTypeString(TOS_Topics::TOPICS topic_t);

/*** --- BEGIN --- Oct 30 2016 ***/

DLL_SPEC_IFACE size_type       
TOSDB_GetPreCachedItemCount(std::string id);

DLL_SPEC_IFACE size_type       
TOSDB_GetPreCachedTopicCount(std::string id);

/*** --- END --- Oct 30 2016 ***/

DLL_SPEC_IFACE size_type       
TOSDB_GetItemCount(std::string id);

DLL_SPEC_IFACE size_type       
TOSDB_GetTopicCount(std::string id);

DLL_SPEC_IFACE bool            
TOSDB_IsUsingDateTime(std::string id);

DLL_SPEC_IFACE size_type      
TOSDB_GetBlockSize(std::string id);

DLL_SPEC_IFACE size_type       
TOSDB_GetStreamOccupancy(std::string id, std::string item, TOS_Topics::TOPICS topic_t);

DLL_SPEC_IFACE long long       
TOSDB_GetMarkerPosition(std::string id, std::string item, TOS_Topics::TOPICS topic_t);

DLL_SPEC_IFACE bool            
TOSDB_IsMarkerDirty(std::string id, std::string item, TOS_Topics::TOPICS topic_t);


/* 'Get' C/C++ API  -  client_get.cpp

   NOTE: C versions return error code while C++ versions throw; most internal errors 
         will be wrapped in TOSDB_Error or derived exceptions (exceptions.cpp) but
         this IS NOT guaranteed.                                                    */


/* individual data points */

template<typename T, bool b> 
auto           
TOSDB_Get(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx = 0) 
    -> typename std::conditional<b, std::pair<T, DateTimeStamp> , T>::type;

template<> 
DLL_SPEC_IFACE generic_type     
TOSDB_Get<generic_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);  

template<> 
DLL_SPEC_IFACE generic_dts_type 
TOSDB_Get<generic_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE std::string        
TOSDB_Get<std::string, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE ext_price_type     
TOSDB_Get<ext_price_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE def_price_type     
TOSDB_Get<def_price_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE ext_size_type      
TOSDB_Get<ext_size_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE def_size_type      
TOSDB_Get<def_size_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);      

template DLL_SPEC_IFACE std::pair<std::string, DateTimeStamp>     
TOSDB_Get<std::string, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE std::pair<ext_price_type, DateTimeStamp>  
TOSDB_Get<ext_price_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE std::pair<def_price_type, DateTimeStamp>  
TOSDB_Get<def_price_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE std::pair<ext_size_type, DateTimeStamp>   
TOSDB_Get<ext_size_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

template DLL_SPEC_IFACE std::pair<def_size_type, DateTimeStamp>   
TOSDB_Get<def_size_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx);

#endif                                     
                                            
EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int     
TOSDB_GetDouble(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, ext_price_type* dest, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int     
TOSDB_GetFloat(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, def_price_type* dest, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int     
TOSDB_GetLongLong(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, ext_size_type* dest, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int     
TOSDB_GetLong(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, def_size_type* dest, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int     
TOSDB_GetString(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, LPSTR dest, size_type str_len, pDateTimeStamp datetime);

#ifdef __cplusplus   

/* get multiple contiguous data points in the stream*/

template<typename T, bool b> 
auto                        
TOSDB_GetStreamSnapshot(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end = -1, long beg = 0)
    -> typename std::conditional<b, std::pair<std::vector<T>, dts_vector_type>, std::vector<T>>::type;

template<> 
DLL_SPEC_IFACE generic_vector_type           
TOSDB_GetStreamSnapshot<generic_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);                    

template<> 
DLL_SPEC_IFACE generic_dts_vectors_type      
TOSDB_GetStreamSnapshot<generic_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);  

/* 20x faster than generic */
template DLL_SPEC_IFACE std::vector<ext_price_type>     
TOSDB_GetStreamSnapshot<ext_price_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);    

template DLL_SPEC_IFACE std::vector<def_price_type>     
TOSDB_GetStreamSnapshot<def_price_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);  

template DLL_SPEC_IFACE std::vector<ext_size_type>      
TOSDB_GetStreamSnapshot<ext_size_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);  

template DLL_SPEC_IFACE std::vector<def_size_type>      
TOSDB_GetStreamSnapshot<def_size_type, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);  

template DLL_SPEC_IFACE std::vector<std::string>        
TOSDB_GetStreamSnapshot<std::string, false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);     


template DLL_SPEC_IFACE std::pair<std::vector<ext_price_type>,dts_vector_type> 
TOSDB_GetStreamSnapshot<ext_price_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);  

template DLL_SPEC_IFACE std::pair<std::vector<def_price_type>,dts_vector_type> 
TOSDB_GetStreamSnapshot<def_price_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);

template DLL_SPEC_IFACE std::pair<std::vector<ext_size_type>,dts_vector_type>  
TOSDB_GetStreamSnapshot<ext_size_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);

template DLL_SPEC_IFACE std::pair<std::vector<def_size_type>,dts_vector_type>  
TOSDB_GetStreamSnapshot<def_size_type, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);

template DLL_SPEC_IFACE std::pair<std::vector<std::string>, dts_vector_type>   
TOSDB_GetStreamSnapshot<std::string, true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg);

#endif                                                 

/* C calls about 30x faster than generic */
EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotDoubles(LPCSTR id,LPCSTR item, LPCSTR topic_str, ext_price_type* dest, size_type array_len, 
                               pDateTimeStamp datetime, long end, long beg); 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotFloats(LPCSTR id, LPCSTR item, LPCSTR topic_str, def_price_type* dest, size_type array_len, 
                              pDateTimeStamp datetime, long end, long beg); 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotLongLongs(LPCSTR id, LPCSTR item, LPCSTR topic_str, ext_size_type* dest, size_type array_len, 
                                 pDateTimeStamp datetime, long end, long beg); 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotLongs(LPCSTR id, LPCSTR item, LPCSTR topic_str, def_size_type* dest, size_type array_len, 
                             pDateTimeStamp datetime, long end, long beg); 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotStrings(LPCSTR id, LPCSTR item, LPCSTR topic_str, LPSTR* dest, size_type array_len, size_type str_len, 
                               pDateTimeStamp datetime, long end, long beg);


/* 'guaranteed' to be contiguous between calls (Get, GetStreamSnapshot, GetStreamSnapshot) */

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotDoublesFromMarker(LPCSTR id,LPCSTR item,LPCSTR topic_str,ext_price_type* dest,size_type array_len, 
                                         pDateTimeStamp datetime, long beg, long *get_size);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotFloatsFromMarker(LPCSTR id, LPCSTR item, LPCSTR topic_str, def_price_type* dest, size_type array_len, 
                                        pDateTimeStamp datetime, long beg, long *get_size);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotLongLongsFromMarker(LPCSTR id, LPCSTR item, LPCSTR topic_str, ext_size_type* dest, size_type array_len, 
                                           pDateTimeStamp datetime, long beg, long *get_size);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotLongsFromMarker(LPCSTR id, LPCSTR item, LPCSTR topic_str, def_size_type* dest, size_type array_len, 
                                       pDateTimeStamp datetime, long beg, long *get_size);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetStreamSnapshotStringsFromMarker(LPCSTR id, LPCSTR item, LPCSTR topic_str, LPSTR* dest, size_type array_len, size_type str_len, 
                                         pDateTimeStamp datetime, long beg, long *get_size);

#ifdef __cplusplus

/* get all the most recent item values for a particular topic */

template<bool b> 
auto                           
TOSDB_GetItemFrame(std::string id, TOS_Topics::TOPICS topic_t) 
    -> typename std::conditional<b, generic_dts_map_type, generic_map_type>::type; 

template<> 
DLL_SPEC_IFACE generic_map_type     
TOSDB_GetItemFrame<false>(std::string id, TOS_Topics::TOPICS topic_t);

template<> 
DLL_SPEC_IFACE generic_dts_map_type 
TOSDB_GetItemFrame<true>(std::string id, TOS_Topics::TOPICS topic_t);

#endif 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetItemFrameDoubles(LPCSTR id, LPCSTR topic_str, ext_price_type* dest, size_type array_len, 
                          LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetItemFrameFloats(LPCSTR id, LPCSTR topic_str, def_price_type* dest, size_type array_len, 
                         LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetItemFrameLongLongs(LPCSTR id, LPCSTR topic_str, ext_size_type* dest, size_type array_len, 
                            LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime); 

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetItemFrameLongs(LPCSTR id, LPCSTR topic_str, def_size_type* dest, size_type array_len, 
                        LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime);

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetItemFrameStrings(LPCSTR id, LPCSTR topic_str, LPSTR* dest, size_type array_len, size_type str_len, 
                          LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime); 

#ifdef __cplusplus  

/* get all the most recent topic values for a particular item */

template<bool b> 
auto                           
TOSDB_GetTopicFrame(std::string id, std::string item) 
    -> typename std::conditional<b, generic_dts_map_type, generic_map_type>::type; 

template<> 
DLL_SPEC_IFACE generic_map_type     
TOSDB_GetTopicFrame<false>(std::string id, std::string item);

template<> 
DLL_SPEC_IFACE generic_dts_map_type 
TOSDB_GetTopicFrame<true>(std::string id, std::string item);

#endif

EXT_C_SPEC DLL_SPEC_IFACE NO_THROW int  
TOSDB_GetTopicFrameStrings(LPCSTR id, LPCSTR item, LPSTR* dest, size_type array_len, size_type str_len, 
                           LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime); 

#ifdef __cplusplus

/* get all the most recent item and topic values */

template<bool b> 
auto                              
TOSDB_GetTotalFrame(std::string id) 
    -> typename std::conditional<b, generic_dts_matrix_type, generic_matrix_type>::type; 

template<> 
DLL_SPEC_IFACE generic_matrix_type     
TOSDB_GetTotalFrame<false>(std::string id);

template<> 
DLL_SPEC_IFACE generic_dts_matrix_type 
TOSDB_GetTotalFrame<true>(std::string id);


/* OSTREAM OVERLOADS - client_out.cpp */

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const DateTimeStamp&); 

/* Nov 30 2016 */
DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const DateTimeStamp*); 
/* Nov 30 2016 */

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_matrix_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_map_type::value_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_map_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_vector_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const dts_vector_type&);

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_dts_matrix_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_dts_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_dts_map_type&); 

DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const generic_dts_vectors_type&); 


template<typename T> std::ostream& 
operator<<(std::ostream&, const std::pair<T,DateTimeStamp>&); 

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<ext_price_type,DateTimeStamp>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<def_price_type,DateTimeStamp>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<ext_size_type,DateTimeStamp>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<def_size_type,DateTimeStamp>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<std::string,DateTimeStamp>&);


template<typename T> std::ostream& 
operator<<(std::ostream&, const std::vector<T>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::vector<ext_price_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::vector<def_price_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::vector<ext_size_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::vector<def_size_type>&);

template DLL_SPEC_IFACE std::ostream& 
  operator<<(std::ostream&, const std::vector<std::string>&); 


template<typename T> std::ostream& 
operator<<(std::ostream&, const std::pair<std::vector<T>,dts_vector_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<std::vector<ext_price_type>,dts_vector_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<std::vector<def_price_type>,dts_vector_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<std::vector<ext_size_type>,dts_vector_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<std::vector<def_size_type>,dts_vector_type>&);

template DLL_SPEC_IFACE std::ostream& 
operator<<(std::ostream&, const std::pair<std::vector<std::string>,dts_vector_type>&);

#endif /* __cplusplus */

typedef enum{ low = 0, high }Severity;  

/* when building tos-databridge[].dll these calls need to be both imported( from _tosd-databridge-[].dll)
   and exported (from tos-databridge[].dll)  -  they must use /export:[func name] during link */ 
/* if logging is not enabled high severity events will be sent to std::cerr */ 

/* use the macros (below) instead */
EXT_C_SPEC  DLL_SPEC_IMPL void  
TOSDB_Log_(DWORD , DWORD, Severity, LPCSTR, LPCSTR); 

EXT_C_SPEC  DLL_SPEC_IMPL void  
TOSDB_LogEx_(DWORD , DWORD, Severity, LPCSTR, LPCSTR, DWORD); 

#define TOSDB_LogH(tag,desc) \
TOSDB_Log_(GetCurrentProcessId(), GetCurrentThreadId(), high, tag, desc)

#define TOSDB_Log(tag,desc) \
TOSDB_Log_(GetCurrentProcessId(), GetCurrentThreadId(), low, tag, desc)

/* DONT PASS GetLastError() INLINE to the macro as the Get...Id sys calls go first */
#define TOSDB_LogEx(tag,desc,error) \
TOSDB_LogEx_(GetCurrentProcessId(), GetCurrentThreadId(), high, tag, desc, error)

EXT_C_SPEC DLL_SPEC_IMPL char**  
NewStrings(size_t num_strs, size_t strs_len);

EXT_C_SPEC DLL_SPEC_IMPL void    
DeleteStrings(char** str_array, size_t num_strs);

EXT_C_SPEC  DLL_SPEC_IMPL unsigned int
CheckIDLength(LPCSTR id);

EXT_C_SPEC  DLL_SPEC_IMPL unsigned int
CheckStringLength(LPCSTR str);

EXT_C_SPEC  DLL_SPEC_IMPL unsigned int
CheckStringLengths(LPCSTR* str, size_type items_len);

#endif /* JO_TOSDB_DATABRIDGE */

