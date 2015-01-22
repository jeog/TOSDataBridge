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

#ifndef JO_TOSDB_DATABRIDGE
#define JO_TOSDB_DATABRIDGE

/* internal objects of exported impl classes not exported;
    look here if link errors on change of_tos-databridge... mods */
#pragma warning( once : 4251 )
/* decorated name length exceeded warning */
#pragma warning( disable : 4503 )
/* chkd iterator warning: we check internally */
#pragma warning( disable : 4996 )

#ifdef __cplusplus
#define CDCR_ "C"
#define EXT_SPEC_ extern CDCR_
#define NO_THROW_ __declspec(nothrow)
#else /* default == nothing */
#define CDCR_ 
#define EXT_SPEC_ 
#define NO_THROW_
#endif

#if defined(THIS_EXPORTS_INTERFACE)
#define DLL_SPEC_IFACE_ __declspec (dllexport)
/*#define TMPL_EXP__( _tmpl, _prot, _body ) \
    _tmpl DLL_SPEC_IFACE_ _prot ;*/
#elif defined(THIS_DOESNT_IMPORT_INTERFACE)
#define DLL_SPEC_IFACE_ 
//#define TMPL_EXP__
#else /* default interface directive should be to import */
#define DLL_SPEC_IFACE_ __declspec (dllimport)
/*#define TMPL_EXP__( _tmpl, _prot, _body ) \
    _tmpl \
    inline _prot \
    { \
    _body \
    }  */
#endif 

#if defined(THIS_EXPORTS_IMPLEMENTATION)
#define DLL_SPEC_IMPL_ __declspec (dllexport)
#elif defined(THIS_IMPORTS_IMPLEMENTATION)
#define DLL_SPEC_IMPL_ __declspec (dllimport)
#else /* default implementation directive should be nothing */
#define DLL_SPEC_IMPL_
#endif

#ifndef _DEBUG
#define KERNEL_GLOBAL_NAMESPACE
#endif
/* if we need to prefix our inter-Session kernel objects with 'Global\' */
#ifdef KERNEL_GLOBAL_NAMESPACE
#define KGBLNS_
#endif
/* virtual leak detector */
#ifdef _DEBUG 
#ifdef VLD_
#include <vld.h>
#endif
#endif
/* define to use portable sync objects built on std::condition_variable,
    (doesn't work properly in VS2012: throws access violation)        */
#ifdef CPP_COND_VAR
#define CPP_COND_VAR_
#endif
/* define to supress internal strlen checks on input, not recommended */
#ifdef SPPRSS_INPT_CHCK
#define SPPRSS_INPT_CHCK_
#endif

#ifdef __cplusplus
namespace JO{ /* forward declaration for generic.hpp */
class DLL_SPEC_IFACE_ Generic;
};
/* forward declarations for _tos-databridge-shared.dll */
#ifdef CPP_COND_VAR_
// class DLL_SPEC_IMPL_ BoundedSemaphore; 
// class DLL_SPEC_IMPL_ CyclicCountDownLatch;
class DLL_SPEC_IMPL_ SignalManager
#else
class DLL_SPEC_IMPL_ LightWeightMutex;
class DLL_SPEC_IMPL_ WinLockGuard;
class DLL_SPEC_IMPL_ SignalManager;
#endif
class DLL_SPEC_IMPL_ ExplicitHeap;
class DLL_SPEC_IMPL_ DynamicIPCBase;
class DLL_SPEC_IMPL_ DynamicIPCMaster;
class DLL_SPEC_IMPL_ DynamicIPCSlave;
#endif
/* externally: limiting use of Win typedefs to LPCSTR / LPSTR, when possible 
   internally: WinAPI facing / relevant code will use all    */
#include <windows.h> 
#include <time.h>
#ifdef __cplusplus
#include <map>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "containers.hpp"/*custom client-facing containers */
#include "generic.hpp"    /* our 'generic' type */
#endif

/* the core types implemented by the data engine: engine-core.cpp */
typedef long          def_size_type; 
typedef long long     ext_size_type;
typedef float         def_price_type;
typedef double        ext_price_type; 
/* arch independent guarantees for the Python wrapper */
typedef unsigned long size_type;
typedef unsigned char type_bits_type;

/* need to #define; can't define consts in header because of C,
    can't define at link-time because of switches / arrays */
#define TOSDB_SIG_ADD 1
#define TOSDB_SIG_REMOVE 2
#define TOSDB_SIG_PAUSE 3
#define TOSDB_SIG_CONTINUE 4
#define TOSDB_SIG_STOP 5
#define TOSDB_SIG_DUMP 6
#define TOSDB_SIG_GOOD ((int)INT_MAX)
#define TOSDB_SIG_BAD ((int)(INT_MAX-1))

#define TOSDB_INTGR_BIT ((type_bits_type)0x80)
#define TOSDB_QUAD_BIT ((type_bits_type)0x40)
#define TOSDB_STRING_BIT ((type_bits_type)0x20)
#define TOSDB_TOPIC_BITMASK ((type_bits_type)0xE0)

#ifdef STR_DATA_SZ /* defined in data-stream.hpp */
#define TOSDB_STR_DATA_SZ STR_DATA_SZ
#else
#define TOSDB_STR_DATA_SZ ((size_type)40)
#endif

#define TOSDB_MAX_STR_SZ ((unsigned long)0xFF)

extern char    DLL_SPEC_IMPL_  TOSDB_LOG_PATH[ MAX_PATH+20 ]; 
/* consts NOT exported from tos-databridge-0.1[].dll */    
extern LPCSTR  TOSDB_APP_NAME;        
extern LPCSTR  TOSDB_COMM_CHANNEL;
/* consts exported from tos-databridge-0.1[].dll, 
   must use /export:[func name] during link */
extern CDCR_ const size_type DLL_SPEC_IFACE_  TOSDB_DEF_TIMEOUT;    // 2000
extern CDCR_ const size_type DLL_SPEC_IFACE_  TOSDB_MIN_TIMEOUT;    // 1500
extern CDCR_ const size_type DLL_SPEC_IFACE_  TOSDB_SHEM_BUF_SZ;    // 4096
extern CDCR_ const size_type DLL_SPEC_IFACE_  TOSDB_BLOCK_ID_SZ;    // 63

typedef const enum{ 
    SHEM1 = 0, 
    MUTEX1, 
    PIPE1 
}Securable;

typedef const enum{ /*milliseconds*/
    Fastest = 0, 
    VeryFast = 3, 
    Fast = 30, 
    Moderate = 300, 
    Slow = 3000, 
    Glacial = 30000 /* for debuging, generally useless for real-time apps */
}UpdateLatency;
#define TOSDB_DEF_LATENCY Fast

typedef struct{
    struct tm  ctime_struct;
    long       micro_second;
} DateTimeStamp, *pDateTimeStamp;

/* a header that will be placed at the front(offset 0) of the mem mapping */
typedef struct{ 
    volatile unsigned int loop_seq;   /* # of times buffer has looped around */
    volatile unsigned int elem_size;  /* size of elements in the buffer */
    volatile unsigned int beg_offset; /* logical location (after header) */
    /* 
        logical location: beg_offset + ( (raw_size - beg_offset) // elem_size ) 
    */
    volatile unsigned int end_offset; 
    volatile unsigned int  next_offset; /* logical location of next write */
} BufferHead, *pBufferHead; 

#define TOSDB_BIT_SHIFT_LEFT(T,val) (((T)val)<<((sizeof(T)-sizeof(type_bits_type))*8))
#define TOSDB_BIT_SHIFT_RIGHT(T,val) (((T)val)>>((sizeof(T)-sizeof(type_bits_type))*8))

#ifdef __cplusplus
/* for C code: create a string of form: "TOSDB_[topic name]_[item name]"  
    only alpha-numeric characters (except under-score)                */
std::string CreateBufferName( std::string sTopic, std::string sItem );
std::string SysTimeString();

typedef std::chrono::steady_clock                  steady_clock_type;
typedef std::chrono::system_clock                  system_clock_type;
typedef std::chrono::microseconds                  micro_sec_type; 
typedef std::chrono::duration< long, std::milli >  milli_sec_type;

/* Generic STL Types returned by the interface(below) */ 
typedef JO::Generic                                 generic_type;
typedef std::pair< generic_type, DateTimeStamp >    generic_dts_type; 
typedef std::vector< generic_type >                 generic_vector_type;
typedef std::vector< DateTimeStamp >                dts_vector_type;
typedef std::pair< generic_vector_type, 
                   dts_vector_type >                generic_dts_vectors_type;    
typedef std::map< std::string, generic_type >       generic_map_type;
typedef std::map< std::string, generic_map_type >   generic_matrix_type;    
typedef std::map< std::string, 
                  std::pair< generic_type, 
                             DateTimeStamp> >       generic_dts_map_type;
typedef std::map< std::string, 
                  generic_dts_map_type >            generic_dts_matrix_type;

template< typename T > 
class Topic_Enum_Wrapper {

    static_assert( 
        std::is_integral<T>::value && !std::is_same<T,bool>::value, 
        "Invalid Topic_Enum_Wrapper<T> Type"
        );

    Topic_Enum_Wrapper()
    { 
    }    

    static const T ADJ_INTGR_BIT  = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_INTGR_BIT);
    static const T ADJ_QUAD_BIT   = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_QUAD_BIT);
    static const T ADJ_STRING_BIT = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_STRING_BIT);
    static const T ADJ_FULL_MASK  = TOSDB_BIT_SHIFT_LEFT(T,TOSDB_TOPIC_BITMASK);

public: /* pack type info into HO nibble of scoped Enum  */   
 
    typedef T enum_type;

    enum class TOPICS /* gaps represent the 'reserved' exported TOS/DDE fields*/ 
        : T {         /* [ see topics.cpp ] */
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
        CLOSE = 0x2a | ADJ_QUAD_BIT,
        //
        DELTA = 0x46 | ADJ_QUAD_BIT,
        //
        DESCRIPTION = 0x48 | ADJ_STRING_BIT,
        //
        DIV = 0x4b,
        DIV_FREQ = 0x4c | ADJ_STRING_BIT,
        //
        DT = 0x52 | ADJ_STRING_BIT,
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
    };

    template< TOPICS topic >
    struct Type{ /* get the type at compile-time */
        typedef typename std::conditional< 
            ((T)topic & ADJ_STRING_BIT ), 
            std::string, 
            typename std::conditional< 
                (T)topic & ADJ_INTGR_BIT,                     
                typename std::conditional< 
                    (T)topic & ADJ_QUAD_BIT, 
                    ext_size_type, 
                    def_size_type >::type,
                typename std::conditional< 
                    (T)topic & ADJ_QUAD_BIT, 
                    ext_price_type, 
                    def_price_type >::type >::type >::type  type;
    };
    
    static type_bits_type  /* type bits at run-time */
    TypeBits( typename Topic_Enum_Wrapper<T>::TOPICS tTopic )
    { 
        return ( (type_bits_type)( TOSDB_BIT_SHIFT_RIGHT(T, (T)tTopic )) 
                 & TOSDB_TOPIC_BITMASK ); 
    }
    
    static std::string  /* platform-dependent type strings at run-time */
    TypeString( typename Topic_Enum_Wrapper<T>::TOPICS tTopic )
    { 
        switch( TOS_Topics::TypeBits(tTopic) ){
        case TOSDB_STRING_BIT :                 
            return typeid(std::string).name();
        case TOSDB_INTGR_BIT :                  
            return typeid(def_size_type).name();
        case TOSDB_QUAD_BIT :                   
            return typeid(ext_price_type).name();
        case TOSDB_INTGR_BIT | TOSDB_QUAD_BIT : 
            return typeid(ext_size_type).name();
        default :                               
            return typeid(def_price_type).name();
        }; 
    }

    struct top_less 
    { 
        bool operator()( const TOPICS& left, const TOPICS& right)
        {
            return ( globalTopicMap[left] < globalTopicMap[right] );            
        }
    };

    typedef TwoWayHashMap< TOPICS, std::string >  topic_map_type;
    typedef typename topic_map_type::pair1_type   topic_map_entry_type;  
  
    /* export ref from imported defs */
    /* note: need to define the ref in each module */
    static DLL_SPEC_IFACE_ const topic_map_type&  globalTopicMap;

private: 
    /* import defs from _tos-databridge.dll */
    static DLL_SPEC_IMPL_ const topic_map_type   _globalTopicMap; 
};

typedef Topic_Enum_Wrapper<unsigned short>  TOS_Topics;

typedef ILSet< std::string >               str_set_type;
typedef ILSet< const TOS_Topics::TOPICS, 
               TOS_Topics::top_less >     topic_set_type;

#endif
/* NOTE int return types indicate an error value is returned */
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_Connect();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_Disconnect();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ unsigned int    TOSDB_IsConnected();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_CreateBlock( LPCSTR id, size_type sz, BOOL fDTStamp, size_type timeout ) ;
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_CloseBlock( LPCSTR id );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_CloseBlocks();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ size_type       TOSDB_GetBlockLimit();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ size_type       TOSDB_SetBlockLimit( size_type sz );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ size_type       TOSDB_GetBlockCount();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetBlockIDs( LPSTR* dest, size_type arrLen, size_type strLen );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetBlockSize( LPCSTR id, size_type* pSize );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_SetBlockSize( LPCSTR id, size_type sz );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ unsigned short  TOSDB_GetLatency();
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ unsigned short  TOSDB_SetLatency( UpdateLatency latency );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_Add( LPCSTR id, LPCSTR* sItems, size_type szItems, LPCSTR* sTopics , size_type szTopics );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_AddTopic( LPCSTR id, LPCSTR sTopic );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_AddItem( LPCSTR id, LPCSTR sItem );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_AddTopics( LPCSTR id, LPCSTR* sTopics, size_type szTopics );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_AddItems( LPCSTR id, LPCSTR* sItems, size_type szItems );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_RemoveTopic( LPCSTR id, LPCSTR sTopic ); 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_RemoveItem( LPCSTR id, LPCSTR sItem );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemCount( LPCSTR id, size_type* pCount );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetTopicCount( LPCSTR id, size_type* pCount );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetTopicNames( LPCSTR id, LPSTR* dest, size_type arrLen, size_type strLen  );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemNames( LPCSTR id, LPSTR* dest, size_type arrLen, size_type strLen );
//EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_GetPreCachedTopicNames( LPCSTR id, LPSTR* dest, size_type strLen, size_type arrLen );
//EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_GetPreCachedItemNames( LPCSTR id, LPSTR* dest, size_type strLen, size_type arrLen );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetTypeBits( LPCSTR sTopic, type_bits_type* pTypeBits );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetTypeString( LPCSTR sTopic, LPSTR dest, size_type strLen );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_IsUsingDateTime( LPCSTR id, unsigned int* pBoolInt );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetStreamOccupancy( LPCSTR id,LPCSTR sItem, LPCSTR sTopic, size_type* sz );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_DumpSharedBufferStatus();
#ifdef __cplusplus
DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_Add( std::string id, str_set_type sItems, topic_set_type tTopics );
DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_AddTopic( std::string id, TOS_Topics::TOPICS tTopic );
DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_AddTopics( std::string id, topic_set_type tTopics );
DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_AddItems( std::string id, str_set_type sItems );
DLL_SPEC_IFACE_ NO_THROW_ int           TOSDB_RemoveTopic( std::string id, TOS_Topics::TOPICS tTopic );                                                    
DLL_SPEC_IFACE_ str_set_type            TOSDB_GetBlockIDs();
DLL_SPEC_IFACE_ topic_set_type          TOSDB_GetTopicEnums( std::string id );
DLL_SPEC_IFACE_ str_set_type            TOSDB_GetTopicNames( std::string id );
DLL_SPEC_IFACE_ str_set_type            TOSDB_GetItemNames( std::string id );
DLL_SPEC_IFACE_ topic_set_type          TOSDB_GetPreCachedTopicEnums( std::string id );
DLL_SPEC_IFACE_ str_set_type            TOSDB_GetPreCachedTopicNames( std::string id );
DLL_SPEC_IFACE_ str_set_type            TOSDB_GetPreCachedItemNames( std::string id );
DLL_SPEC_IFACE_ type_bits_type          TOSDB_GetTypeBits( TOS_Topics::TOPICS tTopic);
DLL_SPEC_IFACE_ std::string             TOSDB_GetTypeString( TOS_Topics::TOPICS tTopic );
DLL_SPEC_IFACE_ size_type               TOSDB_GetItemCount( std::string id);
DLL_SPEC_IFACE_ size_type               TOSDB_GetTopicCount( std::string id);
DLL_SPEC_IFACE_ bool                    TOSDB_IsUsingDateTime( std::string id );
DLL_SPEC_IFACE_ size_type               TOSDB_GetBlockSize( std::string id );
DLL_SPEC_IFACE_ size_type               TOSDB_GetStreamOccupancy( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic );
/* Get individual data points */
template< typename T, bool b > auto            TOSDB_Get( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx = 0 ) 
                                               -> typename std::conditional< b, std::pair< T, DateTimeStamp> , T >::type;
                                               // Generic fastest without DateTimeStamp < false >
template<> DLL_SPEC_IFACE_ generic_type        TOSDB_Get< generic_type, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );    
template<> DLL_SPEC_IFACE_ generic_dts_type    TOSDB_Get< generic_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
                                               // Templatized Type fastet with DateTimeStamp < true >
template DLL_SPEC_IFACE_ std::string           TOSDB_Get< std::string, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx);
template DLL_SPEC_IFACE_ ext_price_type        TOSDB_Get< ext_price_type, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
template DLL_SPEC_IFACE_ def_price_type        TOSDB_Get< def_price_type, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
template DLL_SPEC_IFACE_ ext_size_type         TOSDB_Get< ext_size_type, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
template DLL_SPEC_IFACE_ def_size_type         TOSDB_Get< def_size_type, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );                                                                //
template DLL_SPEC_IFACE_ 
    std::pair< std::string, DateTimeStamp >    TOSDB_Get< std::string, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx);
template DLL_SPEC_IFACE_ 
    std::pair< ext_price_type, DateTimeStamp > TOSDB_Get< ext_price_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
template DLL_SPEC_IFACE_ 
    std::pair< def_price_type, DateTimeStamp > TOSDB_Get< def_price_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
template DLL_SPEC_IFACE_ 
    std::pair< ext_size_type, DateTimeStamp >  TOSDB_Get< ext_size_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
template DLL_SPEC_IFACE_ 
    std::pair< def_size_type, DateTimeStamp >  TOSDB_Get< def_size_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long indx );
#endif                                         // C calls about 20-30% slower across the board
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int        TOSDB_GetDouble( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, long indx, ext_price_type* dest, pDateTimeStamp datetime );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int        TOSDB_GetFloat( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, long indx, def_price_type* dest, pDateTimeStamp datetime );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int        TOSDB_GetLongLong( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, long indx, ext_size_type* dest, pDateTimeStamp datetime );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int        TOSDB_GetLong( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, long indx, def_size_type* dest, pDateTimeStamp datetime );
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int        TOSDB_GetString( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, long indx, LPSTR dest, size_type strLen, pDateTimeStamp datetime );
#ifdef __cplusplus   
/* Get multiple, contiguous data points in the stream*/
template< typename T, bool b > auto                             TOSDB_GetStreamSnapshot( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end = -1, long beg = 0 )
                                                                -> typename std::conditional< b, std::pair< std::vector<T>, dts_vector_type>, std::vector<T> >::type;
template<> DLL_SPEC_IFACE_ generic_vector_type                  TOSDB_GetStreamSnapshot< generic_type, false  >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );                                        
template<> DLL_SPEC_IFACE_ generic_dts_vectors_type             TOSDB_GetStreamSnapshot< generic_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );    
                                                                // these are about 20x faster than generic
template DLL_SPEC_IFACE_ std::vector< ext_price_type >          TOSDB_GetStreamSnapshot< ext_price_type, false >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );        
template DLL_SPEC_IFACE_ std::vector< def_price_type >          TOSDB_GetStreamSnapshot< def_price_type, false  >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );    
template DLL_SPEC_IFACE_ std::vector< ext_size_type >           TOSDB_GetStreamSnapshot< ext_size_type, false  >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );    
template DLL_SPEC_IFACE_ std::vector< def_size_type >           TOSDB_GetStreamSnapshot< def_size_type, false  >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );    
template DLL_SPEC_IFACE_ std::vector< std::string >             TOSDB_GetStreamSnapshot< std::string, false  >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );                                                                    
template DLL_SPEC_IFACE_ 
    std::pair< std::vector< ext_price_type >,dts_vector_type >  TOSDB_GetStreamSnapshot< ext_price_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );    
template DLL_SPEC_IFACE_ 
    std::pair< std::vector< def_price_type >,dts_vector_type >  TOSDB_GetStreamSnapshot< def_price_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );
template DLL_SPEC_IFACE_ 
    std::pair< std::vector< ext_size_type >,dts_vector_type >   TOSDB_GetStreamSnapshot< ext_size_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );
template DLL_SPEC_IFACE_ 
    std::pair< std::vector< def_size_type >,dts_vector_type >   TOSDB_GetStreamSnapshot< def_size_type, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );
template DLL_SPEC_IFACE_ 
    std::pair< std::vector< std::string >, dts_vector_type >    TOSDB_GetStreamSnapshot< std::string, true >( std::string id, std::string sItem, TOS_Topics::TOPICS tTopic, long end, long beg );
#endif                                                          // C calls about 30x faster than generic
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int                         TOSDB_GetStreamSnapshotDoubles( LPCSTR id,LPCSTR sItem, LPCSTR sTopic, ext_price_type* dest, size_type arrLen, pDateTimeStamp datetime, long end, long beg); 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int                         TOSDB_GetStreamSnapshotFloats( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, def_price_type* dest, size_type arrLen, pDateTimeStamp datetime, long end, long beg); 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int                         TOSDB_GetStreamSnapshotLongLongs( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, ext_size_type* dest, size_type arrLen, pDateTimeStamp datetime, long end, long beg); 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int                         TOSDB_GetStreamSnapshotLongs( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, def_size_type* dest, size_type arrLen, pDateTimeStamp datetime, long end, long beg); 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int                         TOSDB_GetStreamSnapshotStrings( LPCSTR id, LPCSTR sItem, LPCSTR sTopic, LPSTR* dest, size_type arrLen, size_type strLen, pDateTimeStamp datetime, long end, long beg);
#ifdef __cplusplus
/* Get all the most recent item values for a particular topic */
template< bool b > auto                             TOSDB_GetItemFrame( std::string id, TOS_Topics::TOPICS tTopic) -> typename std::conditional< b, generic_dts_map_type, generic_map_type >::type; 
template<> DLL_SPEC_IFACE_ generic_map_type         TOSDB_GetItemFrame<false>( std::string id, TOS_Topics::TOPICS tTopic);
template<> DLL_SPEC_IFACE_ generic_dts_map_type     TOSDB_GetItemFrame<true>( std::string id, TOS_Topics::TOPICS tTopic);
#endif 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemFrameDoubles( LPCSTR id, LPCSTR sTopic, ext_price_type* dest, size_type arrLen, LPSTR* labelDest, size_type labelStrLen, pDateTimeStamp datetime);
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemFrameFloats( LPCSTR id, LPCSTR sTopic, def_price_type* dest, size_type arrLen, LPSTR* labelDest, size_type labelStrLen, pDateTimeStamp datetime);
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemFrameLongLongs( LPCSTR id, LPCSTR sTopic, ext_size_type* dest, size_type arrLen, LPSTR* labelDest, size_type labelStrLen, pDateTimeStamp datetime); 
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemFrameLongs( LPCSTR id, LPCSTR sTopic, def_size_type* dest, size_type arrLen, LPSTR* labelDest, size_type labelStrLen, pDateTimeStamp datetime);
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetItemFrameStrings( LPCSTR id, LPCSTR sTopic, LPSTR* dest, size_type arrLen, size_type strLen, LPSTR* labelDest, size_type labelStrLen, pDateTimeStamp datetime); 
#ifdef __cplusplus  
/* Get all the most recent topic values for a particular item */
template< bool b > auto                             TOSDB_GetTopicFrame( std::string id, std::string sItem) -> typename std::conditional< b, generic_dts_map_type, generic_map_type >::type; 
template<> DLL_SPEC_IFACE_ generic_map_type         TOSDB_GetTopicFrame<false>( std::string id, std::string sItem);
template<> DLL_SPEC_IFACE_ generic_dts_map_type     TOSDB_GetTopicFrame<true>( std::string id, std::string sItem);
#endif
EXT_SPEC_ DLL_SPEC_IFACE_ NO_THROW_ int             TOSDB_GetTopicFrameStrings( LPCSTR id, LPCSTR sItem, LPSTR* dest, size_type arrLen, size_type strLen, LPSTR* labelDest, size_type labelStrLen, pDateTimeStamp datetime); 
#ifdef __cplusplus
/* Get all the most recent item and topic values */
template< bool b > auto                             TOSDB_GetTotalFrame( std::string id ) -> typename std::conditional< b, generic_dts_matrix_type, generic_matrix_type >::type; 
template<> DLL_SPEC_IFACE_ generic_matrix_type      TOSDB_GetTotalFrame<false>( std::string id );
template<> DLL_SPEC_IFACE_ generic_dts_matrix_type  TOSDB_GetTotalFrame<true>( std::string id );
#endif 
/* if logging is not enabled high severity events will be sent to std::cerr */ 
typedef enum{ low = 0, high }Severity;    
/* note: these _tos-databridge-static.lib imports must use /export:[func name] during link */ 
EXT_SPEC_ DLL_SPEC_IFACE_ void         TOSDB_StartLogging( LPCSTR fName );
EXT_SPEC_ DLL_SPEC_IFACE_ void         TOSDB_StopLogging();
EXT_SPEC_ DLL_SPEC_IFACE_ void         TOSDB_ClearLog();
#define TOSDB_LogH(tag, desc)          TOSDB_Log_( GetCurrentProcessId(), GetCurrentThreadId(), high, tag, desc)
#define TOSDB_Log(tag, desc)           TOSDB_Log_( GetCurrentProcessId(), GetCurrentThreadId(), low, tag, desc)
#define TOSDB_LogEx(tag, desc, error)  TOSDB_LogEx_( GetCurrentProcessId(), GetCurrentThreadId(), high, tag, desc, error)
EXT_SPEC_ DLL_SPEC_IFACE_ void         TOSDB_Log_( DWORD , DWORD, Severity, LPCSTR, LPCSTR); 
EXT_SPEC_ DLL_SPEC_IFACE_ void         TOSDB_LogEx_( DWORD , DWORD, Severity, LPCSTR, LPCSTR, DWORD); 
void                                   TOSDB_Log_Raw_( LPCSTR );
EXT_SPEC_ DLL_SPEC_IFACE_ char**       AllocStrArray( size_t numStrs, size_t szStrs );
EXT_SPEC_ DLL_SPEC_IFACE_ void         DeallocStrArray( const char* const* strArray, size_t numStrs );

#ifdef __cplusplus
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const DateTimeStamp&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_matrix_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_map_type::value_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_map_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_vector_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const dts_vector_type&);
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_dts_matrix_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_dts_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_dts_map_type&); 
DLL_SPEC_IFACE_             std::ostream& operator<<(std::ostream&, const generic_dts_vectors_type&); 
template< typename T >      std::ostream& operator<<(std::ostream&, const std::pair<T,DateTimeStamp>&); 
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<ext_price_type,DateTimeStamp>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<def_price_type,DateTimeStamp>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<ext_size_type,DateTimeStamp>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<def_size_type,DateTimeStamp>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<std::string,DateTimeStamp>&);
template< typename T >      std::ostream& operator<<(std::ostream&, const std::vector<T>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::vector<ext_price_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::vector<def_price_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::vector<ext_size_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::vector<def_size_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::vector<std::string>&); 
template< typename T >      std::ostream& operator<<(std::ostream&, const std::pair<std::vector<T>,dts_vector_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<std::vector<ext_price_type>,dts_vector_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<std::vector<def_price_type>,dts_vector_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<std::vector<ext_size_type>,dts_vector_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<std::vector<def_size_type>,dts_vector_type>&);
template DLL_SPEC_IFACE_    std::ostream& operator<<(std::ostream&, const std::pair<std::vector<std::string>,dts_vector_type>&);

class TOSDB_error : public std::exception {

    unsigned long _threadID;
    unsigned long _processID;
    std::string _tag;
    std::string _info;

public:

    TOSDB_error( const char* info, const char* tag )
        : 
        std::exception( info ),
        _tag( tag ),
        _info( info ),
        _threadID( GetCurrentThreadId() ),
        _processID( GetCurrentProcessId() )
        {                
        }

    TOSDB_error( const std::exception& e, const char* tag )
        : 
        std::exception( e ),
        _tag( tag ),        
        _threadID( GetCurrentThreadId() ),
        _processID( GetCurrentProcessId() )
        {                 
        }

    TOSDB_error( const std::exception& e, const char* info, const char* tag )
        : 
        std::exception( e ),
        _tag( tag ),
        _info( info ),
        _threadID( GetCurrentThreadId() ),
        _processID( GetCurrentProcessId() )
        {                
        }

    virtual ~TOSDB_error()
        {
        }

    unsigned long threadID() const 
    { 
        return _threadID; 
    }

    unsigned long processID() const 
    {
        return _processID; 
    }

    std::string tag() const 
    { 
        return _tag; 
    }

    std::string info() const 
    { 
        return _info; 
    }
};
  
class TOSDB_IPC_error : public TOSDB_error {
public:

    TOSDB_IPC_error( const char* info, const char* tag = "IPC" )
        : 
        TOSDB_error( info, tag )
        {
        }
};

class TOSDB_buffer_error : public TOSDB_IPC_error {
public:

    TOSDB_buffer_error( const char* info, const char* tag = "DATA-BUFFER" )
        : 
        TOSDB_IPC_error( info, tag )
        {
        }
};

class TOSDB_dde_error : public TOSDB_error {
public:

    TOSDB_dde_error( const char* info, const char* tag = "DDE" )
        : 
        TOSDB_error( info, tag )
        {
        }

    TOSDB_dde_error( const std::exception& e, 
                     const char* info, 
                     const char* tag = "DDE" )
        : TOSDB_error( e, info, tag )
        {
        }
};        

class TOSDB_data_block_error : public TOSDB_error {
public:

    TOSDB_data_block_error( const char* info, const char* tag = "DataBlock" )
        : 
        TOSDB_error( info, tag )
        {
        }

    TOSDB_data_block_error( const std::exception& e, 
                            const char* info, 
                            const char* tag = "DataBlock" ) 
        : TOSDB_error( e, info, tag )
        {
        }
};        

class TOSDB_data_block_limit_error :
    public TOSDB_data_block_error, public std::length_error {
public:

    const size_t limit;    

    TOSDB_data_block_limit_error( const size_t limit )
        : 
        TOSDB_data_block_error( "Attempt to create TOSDB_RawDataBlock "
                                "would exceed limit." ),
        std::length_error( "Attempt to create TOSDB_RawDataBlock "
                           "would exceed limit." ),
        limit( limit )
        {
        }

    virtual const char* what() const 
        {
            return TOSDB_data_block_error::what();
        }    
};                        

class TOSDB_data_stream_error : public TOSDB_data_block_error {
public:

    TOSDB_data_stream_error( const char* info, const char* tag = "DataStream" )
        : TOSDB_data_block_error( info, tag )        
        {
        }

    TOSDB_data_stream_error( const std::exception& e, 
                             const char* info, 
                             const char* tag = "DataStream" )
        : TOSDB_data_block_error(e, info, tag )        
        {
        }
};

#endif
#endif
