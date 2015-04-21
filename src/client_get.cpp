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

#include "raw_data_block.hpp"
#include "client.hpp"
#include <ctime>
#include <fstream>
#include <iostream>
#include <iomanip>

/* 
    there is alot of redundancy here in order to support 
    both C and C++ interfaces, and string versions
*/

size_type TOSDB_GetBlockLimit()
{
    our_rlock_guard_type _lock_(*global_rmutex);
    return TOSDB_RawDataBlock::max_block_count();
}

size_type TOSDB_SetBlockLimit( size_type sz )
{
    our_rlock_guard_type _lock_(*global_rmutex);
    return TOSDB_RawDataBlock::max_block_count( sz );
}

size_type TOSDB_GetBlockCount()
{
    our_rlock_guard_type _lock_(*global_rmutex);
    return TOSDB_RawDataBlock::block_count();
}

int TOSDB_GetBlockSize( LPCSTR id, size_type* pSize )
{
    try{     
        if( !CheckIDLength( id ) )
            return -1;

        our_rlock_guard_type _lock_(*global_rmutex);
        *pSize = GetBlockOrThrow(id)->block->block_size();
        return 0;
    }catch( ... ){
        return -2;
    }
}

int TOSDB_SetBlockSize( LPCSTR id, size_type sz )
{
    try{
        if( !CheckIDLength( id ) )
            return -1;

        our_rlock_guard_type _lock_(*global_rmutex);
        GetBlockOrThrow(id)->block->block_size(sz);
        return 0;
    }catch( ... ){ 
        return -2;
    }
}



int TOSDB_GetItemCount( LPCSTR id, size_type* count )
{
    try{
        if( !CheckIDLength( id ) )
            return -1;

        our_rlock_guard_type _lock_(*global_rmutex);
        *count = GetBlockOrThrow(id)->block->item_count();
        return 0;
    }catch( ... ){
        return -2;
    }
}

int TOSDB_GetTopicCount( LPCSTR id, size_type* count )
{
    try{
        if( !CheckIDLength( id ) )
            return -1;

        our_rlock_guard_type _lock_(*global_rmutex);
        *count = GetBlockOrThrow(id)->block->topic_count();
        return 0;
    }catch( ... ){
        return -2;
    }
}

/*
int TOSDB_GetPreCachedTopicNames( LPCSTR id, 
                                  LPSTR* dest, 
                                  size_type str_len, 
                                  size_type array_len )
{
    const TOSDBlock *db;
    topic_set_type topS;
    int i, errVal;

    i = errVal = 0;

    if( !CheckIDLength(id) )
        return -1;

    db = GetBlockPtr(id);
    if ( !db ) 
        return -1;

    topS = db->topic_precache;    
    if ( array_len < topS.size() ) 
        return -1;    

    for( auto & topic : topS ){
        if( errVal = strcpy_s( dest[i++], str_len,
                               TOS_Topics::map[topic].c_str() )){
           return errVal;         
        }
    }

    return errVal;
}

int TOSDB_GetPreCachedItemNames( LPCSTR id, 
                                 LPSTR* dest, 
                                 size_type str_len, 
                                 size_type array_len )
{
    const TOSDBlock *db;
    str_set_type itemS;
    int i, errVal;

    i = errVal = 0;

    if( !CheckIDLength( id ) )
        return -1;

    db = GetBlockPtr(id);
    if ( !db ) 
        return -1;

    itemS = db->item_precache;     
    if ( array_len < itemS.size() ) 
        return -1;    

    for( auto & item : itemS )
        if( errVal = strcpy_s( dest[i++], str_len, item.c_str() )) 
            return errVal;
    
    return errVal;
}
*/

int TOSDB_GetTopicNames( LPCSTR id, 
                         LPSTR* dest, 
                         size_type array_len, 
                         size_type str_len )
{
    const TOSDBlock *db;
    topic_set_type topS;
    int i, errVal;

    if( !CheckIDLength(id) )
        return -1;

    our_rlock_guard_type _lock_(*global_rmutex);

    db = GetBlockPtr(id);
    if ( !db ) 
        return -2;

    topS = db->block->topics();    
    if ( array_len < topS.size() ) 
        return -3;    

    i = errVal = 0;

    for( auto & topic : topS ){
        errVal = strcpy_s( dest[i++], str_len,
                           TOS_Topics::map[topic].c_str() );
        if( errVal )
            return errVal;         
    }

    return errVal;
}

int TOSDB_GetItemNames( LPCSTR id, 
                        LPSTR* dest,  
                        size_type array_len,  
                        size_type str_len )
{
    const TOSDBlock *db;
    str_set_type itemS;
    int i, errVal;

    if( !CheckIDLength( id ) )
        return -1;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockPtr(id);
    if ( !db ) 
        return -2;

    itemS = db->block->items();     
    if ( array_len < itemS.size() ) 
        return -3;   
 
    i = errVal = 0;

    for( auto & item : itemS ){
        errVal = strcpy_s( dest[i++], str_len, item.c_str() );
        if( errVal )
            return errVal;
    }

    return errVal;
}

int TOSDB_GetTypeBits( LPCSTR sTopic, type_bits_type* type_bits )
{
    TOS_Topics::TOPICS t;

    if( !CheckStringLength( sTopic ) )
        return -1;

    if( (t = GetTopicEnum(sTopic)) == TOS_Topics::TOPICS::NULL_TOPIC )
        return -2;

    try{
        *type_bits = TOSDB_GetTypeBits( t );
        return 0;
    }catch( ... ){
        return -3;
    }
}

int TOSDB_GetTypeString( LPCSTR sTopic, LPSTR dest, size_type str_len )
{
    TOS_Topics::TOPICS t;
    std::string str;

    if( !CheckStringLength( sTopic ) )
        return -1;

    if( (t = GetTopicEnum(sTopic)) == TOS_Topics::TOPICS::NULL_TOPIC )
        return -1;

    str = TOS_Topics::TypeString( t );
    return strcpy_s( dest, str_len, str.c_str() );
}

int TOSDB_IsUsingDateTime( LPCSTR id, unsigned int* is_datetime )
{    
    try{
        if( !CheckIDLength( id ) )
            return -1;

        our_rlock_guard_type _lock_(*global_rmutex);
        *is_datetime = GetBlockOrThrow(id)->block->uses_dtstamp();
        return 0;
    }catch( ... ){
        return -2;
    }
}

topic_set_type TOSDB_GetTopicEnums( std::string id )
{
    const TOSDBlock *db;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);

    return db->block->topics();
}

str_set_type TOSDB_GetTopicNames( std::string id )
{
    const TOSDBlock *db;    

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);

    return str_set_type( db->block->topics(), 
                         [&]( TOS_Topics::TOPICS t )
                           {
                             return TOS_Topics::map[ t ]; 
                           } 
                        );
}

str_set_type TOSDB_GetItemNames( std::string id )
{
    const TOSDBlock* db;    

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);

    return db->block->items();
}

topic_set_type TOSDB_GetPreCachedTopicEnums( std::string id )
{
    const TOSDBlock *db;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);

    return db->topic_precache;
}

str_set_type TOSDB_GetPreCachedTopicNames( std::string id )
{
    const TOSDBlock *db;    

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);

    return str_set_type( db->topic_precache,
                         [=](TOS_Topics::TOPICS top)
                           { 
                             return TOS_Topics::map[top]; 
                           } 
                        );
}

str_set_type TOSDB_GetPreCachedItemNames( std::string id )
{
    const TOSDBlock *db;    

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);
    return db->item_precache;
}

type_bits_type TOSDB_GetTypeBits( TOS_Topics::TOPICS tTopic )
{
    if( tTopic == TOS_Topics::TOPICS::NULL_TOPIC )
        throw std::invalid_argument( "can not accept NULL_TOPIC" );

    return TOS_Topics::TypeBits( tTopic );                    
}

std::string TOSDB_GetTypeString( TOS_Topics::TOPICS tTopic )
{
    if( tTopic == TOS_Topics::TOPICS::NULL_TOPIC )
        throw std::invalid_argument( "can not accept NULL_TOPIC" );

    return TOS_Topics::TypeString( tTopic );
}

size_type TOSDB_GetItemCount( std::string id )
{
    our_rlock_guard_type _lock_(*global_rmutex);
    return  GetBlockOrThrow(id)->block->item_count();
}

size_type TOSDB_GetTopicCount( std::string id )
{
    our_rlock_guard_type _lock_(*global_rmutex);
    return GetBlockOrThrow(id)->block->topic_count();
}

void TOSDB_SetBlockSize( std::string id, size_type sz )
{
    our_rlock_guard_type _lock_(*global_rmutex);
    GetBlockOrThrow(id)->block->block_size(sz);    
}

size_type TOSDB_GetBlockSize( std::string id )
{
    our_rlock_guard_type _lock_(*global_rmutex);
    return GetBlockOrThrow(id)->block->block_size();    
}

bool TOSDB_IsUsingDateTime( std::string id )
{    
    our_rlock_guard_type _lock_(*global_rmutex);
    return GetBlockOrThrow(id)->block->uses_dtstamp();
}

int TOSDB_GetStreamOccupancy( LPCSTR id,
                              LPCSTR sItem, 
                              LPCSTR sTopic, 
                              size_type* sz )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;
    TOS_Topics::TOPICS t;

    if( !CheckIDLength( id ) || !CheckStringLength( sItem, sTopic ) )
        return -1;    

    t = GetTopicEnum(sTopic);

    try{
        our_rlock_guard_type _lock_(*global_rmutex);
        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, t);
        *sz = (size_type)(dat->size());
        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetStreamOccupancy()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }    
}

size_type TOSDB_GetStreamOccupancy( std::string id, 
                                    std::string sItem, 
                                    TOS_Topics::TOPICS tTopic )
{
    const TOSDBlock *db;    
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);
    dat = db->block->raw_stream_ptr( sItem, tTopic );    
    try{
        return (size_type)(dat->size());
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "TOSDB_GetStreamOccupancy()" );
    }catch( ... ){
        throw;
    }
}

int TOSDB_GetMarkerPosition( LPCSTR id,
                             LPCSTR sItem, 
                             LPCSTR sTopic, 
                             long long* pos )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;
    TOS_Topics::TOPICS t;

    if( !CheckIDLength( id ) || !CheckStringLength( sItem, sTopic ) )
        return -1;    

    t = GetTopicEnum(sTopic);

    try{
        our_rlock_guard_type _lock_(*global_rmutex);
        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, t);
        *pos = (dat->marker_position());
        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetMarkerPosition()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }    
}

long long TOSDB_GetMarkerPosition( std::string id, 
                                   std::string sItem, 
                                   TOS_Topics::TOPICS tTopic )
{
    const TOSDBlock *db;    
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);
    dat = db->block->raw_stream_ptr( sItem, tTopic );    
    try{
        return (dat->marker_position());
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "TOSDB_GetMarkerPosition()" );
    }catch( ... ){
        throw;
    }
}

int TOSDB_IsMarkerDirty( LPCSTR id,
                         LPCSTR sItem, 
                         LPCSTR sTopic, 
                         unsigned int* is_dirty )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;
    TOS_Topics::TOPICS t;

    if( !CheckIDLength( id ) || !CheckStringLength( sItem, sTopic ) )
        return -1;    

    t = GetTopicEnum(sTopic);

    try{
        our_rlock_guard_type _lock_(*global_rmutex);
        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, t);
        *is_dirty = (unsigned int)(dat->is_marker_dirty());
        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_IsMarkerDirty()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }    
}

bool TOSDB_IsMarkerDirty( std::string id, 
                          std::string sItem, 
                          TOS_Topics::TOPICS tTopic )
{
    const TOSDBlock *db;    
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow(id);
    dat = db->block->raw_stream_ptr( sItem, tTopic );    
    try{
        return dat->is_marker_dirty();
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "TOSDB_IsMarkerDirty()" );
    }catch( ... ){
        throw;
    }
}

template<> 
generic_type TOSDB_Get< generic_type, false >( std::string id, 
                                               std::string sItem, 
                                               TOS_Topics::TOPICS tTopic, 
                                               long indx  )
{    
    const TOSDBlock *db;    
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);

    db = GetBlockOrThrow(id);
    dat = db->block->raw_stream_ptr( sItem, tTopic );    

    try{
        return dat->operator[](indx);
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "TOSDB_Get<generic_type,false>()" );
    }catch( ... ){
        throw;
    }
}

template<> 
generic_dts_type TOSDB_Get< generic_type, true >( std::string id, 
                                                  std::string sItem, 
                                                  TOS_Topics::TOPICS tTopic, 
                                                  long indx )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);

    db = GetBlockOrThrow(id);
    dat = db->block->raw_stream_ptr( sItem, tTopic );

    try{
        return dat->both(indx);
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in "
                                       "TOSDB_Get<generic_type,true>()" );
    }catch( ... ){
        throw;
    }
}

/* use functors of partially specialized object to overload by explicit call */
template< typename T, bool b> struct GRetType;
template< typename T >
struct GRetType< T, true>{
    std::pair< T, DateTimeStamp > operator()( T val, DateTimeStamp&& dts )
    {            
        return std::pair< T, DateTimeStamp>( val, std::move(dts) );
    }
};

template< typename T >
struct GRetType< T, false>{
    T operator()( T val, DateTimeStamp&& dts )
    {    
        return val;
    }
};

template< typename T, bool b > 
auto TOSDB_Get( std::string id, 
                std::string sItem, 
                TOS_Topics::TOPICS tTopic, 
                long indx ) 
    -> typename std::conditional< b, std::pair< T, DateTimeStamp >, T >::type
{
    T tmp;
    DateTimeStamp datetime;

    if ( TOSDB_Get_( id, sItem, tTopic, indx, &tmp, &datetime ) )        
        throw TOSDB_DataStreamError( "TOSDB_Get_ returned error code" );

    return GRetType<T,b>()( tmp, std::move( datetime ) );    
}

template< typename T > 
int TOSDB_Get_( std::string id, 
                std::string sItem, 
                TOS_Topics::TOPICS tTopic, 
                long indx, 
                T* dest, 
                pDateTimeStamp datetime )
{     
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat; 
   
    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, tTopic);     

        dat->copy(dest, 1,indx, indx, datetime);

        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_Get<T>", e.what() );
        return -1;
    }catch( ... ){
        return -1;    
    }
}

template< typename T > 
int TOSDB_Get_( LPCSTR id, 
                LPCSTR sItem, 
                LPCSTR sTopic, 
                long indx, 
                T* dest, 
                pDateTimeStamp datetime )
{ 
    if( !CheckIDLength( id ) || !CheckStringLength( sTopic, sTopic ) )
        return -1;  
  
    return TOSDB_Get_(id, sItem, GetTopicEnum( sTopic ), indx, dest, datetime);
}

int TOSDB_GetDouble( LPCSTR id, 
                     LPCSTR sItem, 
                     LPCSTR sTopic, 
                     long indx, 
                     ext_price_type* dest, 
                     pDateTimeStamp datetime )
{    
    return TOSDB_Get_( id, sItem, sTopic , indx, dest, datetime );
}

int TOSDB_GetFloat( LPCSTR id, 
                    LPCSTR sItem, 
                    LPCSTR sTopic, 
                    long indx, 
                    def_price_type* dest, 
                    pDateTimeStamp datetime )
{
    return TOSDB_Get_( id, sItem, sTopic , indx, dest, datetime );
}

int TOSDB_GetLongLong( LPCSTR id, 
                       LPCSTR sItem, 
                       LPCSTR sTopic, 
                       long indx, 
                       ext_size_type* dest, 
                       pDateTimeStamp datetime )
{
    return TOSDB_Get_( id, sItem, sTopic , indx, dest, datetime );
}

int TOSDB_GetLong( LPCSTR id, 
                   LPCSTR sItem, 
                   LPCSTR sTopic, 
                   long indx, 
                   def_size_type* dest, 
                   pDateTimeStamp datetime )
{
    return TOSDB_Get_( id, sItem, sTopic , indx, dest, datetime );
}

int TOSDB_GetString( LPCSTR id, 
                     LPCSTR sItem, 
                     LPCSTR sTopic, 
                     long indx, 
                     LPSTR dest, 
                     size_type str_len, 
                     pDateTimeStamp datetime )
{    
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;
    TOS_Topics::TOPICS t;

    if( !CheckIDLength( id ) || !CheckStringLength( sItem, sTopic ) )
        return -1;    

    t = GetTopicEnum(sTopic);

    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, t);

        dat->copy(&dest, 1, str_len, indx,indx,datetime);

        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetString()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }    
}

template<> 
generic_vector_type 
TOSDB_GetStreamSnapshot< generic_type, false >( std::string id, 
                                                std::string sItem, 
                                                TOS_Topics::TOPICS tTopic, 
                                                long end, 
                                                long beg )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);

    db = GetBlockOrThrow( id );    
    dat = db->block->raw_stream_ptr( sItem, tTopic);    

    try{
        return dat->vector(end, beg); 
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in TOSDB_GetStreamSnapshot"
                                       "<generic_type,false>()" );
    }catch( ... ){
        throw;
    }
}

template<> 
std::pair< std::vector<generic_type>, 
           std::vector<DateTimeStamp> >         
TOSDB_GetStreamSnapshot< generic_type, true >( std::string id, 
                                               std::string sItem, 
                                               TOS_Topics::TOPICS tTopic, 
                                               long end, 
                                               long beg )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);

    db = GetBlockOrThrow( id );    
    dat = db->block->raw_stream_ptr( sItem, tTopic);

    try{
        return std::pair< std::vector<generic_type>, 
                          std::vector<DateTimeStamp>>(
                             dat->vector(end, beg), 
                             dat->secondary_vector(end,beg) );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                       "tosdb_data_stream error caught and "
                                       "encapsulated in TOSDB_GetStreamSnapshot"
                                       "<generic_type,true>()" );
    }catch( ... ){
        throw;
    }
}

/* 
   use functors of partially specialized object to overload by explicit call
   note: data_stream takes raw pointers so we need to pass the addr of the 
   dereferenced first element of the vector 
*/
template< typename T, bool b> struct GSSRetType;
template< typename T >
struct GSSRetType< T, true>{

    std::pair< std::vector<T>, std::vector<DateTimeStamp> > 
    operator()( TOSDB_RawDataBlock::stream_const_ptr_type dat, 
                long end, 
                long beg, 
                size_t diff )
    {                
        std::vector<T> tmpV(diff); /* adjust for [ ) -> [ ] */
        std::vector< DateTimeStamp > tmpDTSV(diff); /* adjust for [ ) -> [ ] */
    
        if( diff > 0 )
            dat->copy( &(*(tmpV.begin())), diff, end, beg, 
                       &(*(tmpDTSV.begin())) );            

        return std::pair< std::vector<T>, 
                          std::vector<DateTimeStamp> >( tmpV, tmpDTSV );
    }

};

template< typename T >
struct GSSRetType< T, false>{
    std::vector<T> operator()( TOSDB_RawDataBlock::stream_const_ptr_type dat, 
                               long end, 
                               long beg, 
                               size_t diff )
    {    
        std::vector<T> tmpV(diff); /* adjust for [ ) -> [ ] */

        if( diff > 0 )
            dat->copy( &(*(tmpV.begin())), diff, end, beg, nullptr );            

        return tmpV;
    }

};

template< typename T, bool b > 
auto TOSDB_GetStreamSnapshot( std::string id, 
                              std::string sItem, 
                              TOS_Topics::TOPICS tTopic, 
                              long end, 
                              long beg ) 
    -> typename std::conditional< b, 
                                  std::pair< std::vector< T >, 
                                             std::vector< DateTimeStamp> >, 
                                             std::vector< T > >::type
{ /* this should be cleaned up; 
     bound to be a type/cast issue in here somewhere 
   */
    size_type sz;
    long diff;    
    long long minDiff;
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    our_rlock_guard_type _lock_(*global_rmutex);

    db = GetBlockOrThrow( id );    
    dat = db->block->raw_stream_ptr(sItem, tTopic); /* get stream size */

    sz = (size_type)(dat->bound_size());
    if( end < 0 ) 
        end += sz;
    if( beg < 0 ) 
        beg += sz;

    diff = end - beg;
    if( diff < 0 )
        throw std::invalid_argument( " TOSDB_GetStreamSnapshot(...): "
                                     "invalid indexes( end < beg ) ");

    minDiff = (long long)std::min< size_t >((size_t)end + 1, dat->size()) - beg;
    if( minDiff < 0 )
        minDiff = 0;

    try{ 
        return GSSRetType<T,b>()( dat, end, beg, (size_t)minDiff );
    }catch( const tosdb_data_stream::error& e ){
        throw TOSDB_DataStreamError( e, 
                                      "tosdb_data_stream error caught and "
                                      "encapsulated in "
                                      "TOSDB_GetStreamSnapshot<T,b>()" );
    }catch( ... ){
        throw; 
    }    
}

template< typename T > 
int TOSDB_GetStreamSnapshot_( LPCSTR id,
                              LPCSTR sItem, 
                              TOS_Topics::TOPICS tTopic, 
                              T* dest, 
                              size_type array_len, 
                              pDateTimeStamp datetime, 
                              long end, 
                              long beg )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    if( !CheckIDLength( id ) || !CheckStringLength( sItem ))
        return -1;

    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, tTopic);

        dat->copy(dest,array_len,end,beg,datetime);

        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "GetStreamSnapshot<T>", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }
}

template< typename T > 
int TOSDB_GetStreamSnapshot_( LPCSTR id,
                              LPCSTR sItem, 
                              LPCSTR sTopic, 
                              T* dest, 
                              size_type array_len, 
                              pDateTimeStamp datetime, 
                              long end, 
                              long beg )
{    
    if( !CheckStringLength( sTopic ) ) /* let this go thru std::string ? */
        return -1;     
   
    return TOSDB_GetStreamSnapshot_( id, sItem, GetTopicEnum( sTopic ), dest, 
                                     array_len, datetime, end, beg );
}

int TOSDB_GetStreamSnapshotDoubles( LPCSTR id,
                                    LPCSTR sItem, 
                                    LPCSTR sTopic, 
                                    ext_price_type* dest, 
                                    size_type array_len, 
                                    pDateTimeStamp datetime, 
                                    long end, 
                                    long beg)
{
    return TOSDB_GetStreamSnapshot_( id, sItem, sTopic, dest, array_len, 
                                     datetime, end, beg );
}

int TOSDB_GetStreamSnapshotFloats( LPCSTR id, 
                                   LPCSTR sItem, 
                                   LPCSTR sTopic, 
                                   def_price_type* dest, 
                                   size_type array_len, 
                                   pDateTimeStamp datetime, 
                                   long end, 
                                   long beg )
{
    return TOSDB_GetStreamSnapshot_( id, sItem, sTopic, dest, array_len, 
                                     datetime, end, beg);
}

int TOSDB_GetStreamSnapshotLongLongs( LPCSTR id, 
                                      LPCSTR sItem, 
                                      LPCSTR sTopic, 
                                      ext_size_type* dest, 
                                      size_type array_len, 
                                      pDateTimeStamp datetime, 
                                      long end, 
                                      long beg )
{
    return TOSDB_GetStreamSnapshot_( id, sItem, sTopic, dest, array_len, 
                                     datetime, end, beg);    
}

int TOSDB_GetStreamSnapshotLongs( LPCSTR id, 
                                  LPCSTR sItem, 
                                  LPCSTR sTopic, 
                                  def_size_type* dest, 
                                  size_type array_len, 
                                  pDateTimeStamp datetime, 
                                  long end, 
                                  long beg)
{
    return TOSDB_GetStreamSnapshot_( id, sItem, sTopic, dest, array_len, 
                                     datetime, end, beg);    
}

int TOSDB_GetStreamSnapshotStrings( LPCSTR id, 
                                    LPCSTR sItem, 
                                    LPCSTR sTopic, 
                                    LPSTR* dest, 
                                    size_type array_len, 
                                    size_type str_len, 
                                    pDateTimeStamp datetime, 
                                    long end, 
                                    long beg )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;
    TOS_Topics::TOPICS tTopic;

    if( !CheckIDLength(id) || !CheckStringLength( sItem, sTopic ) )
        return -1;

    tTopic = GetTopicEnum( sTopic );    
    
    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, tTopic);

        dat->copy(dest,array_len,str_len,end,beg,datetime);

        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetStreamSnapshotStrings()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }
}

/**************************
**** data-stream-marker ***
**************************/

template< typename T > 
int TOSDB_GetStreamSnapshotFromMarker_( LPCSTR id,
                                        LPCSTR sItem, 
                                        TOS_Topics::TOPICS tTopic, 
                                        T* dest, 
                                        size_type array_len, 
                                        pDateTimeStamp datetime,                                         
                                        long beg,
                                        long *get_size )
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;

    if( !CheckIDLength( id ) || !CheckStringLength( sItem ))
        return -1;

    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, tTopic);
                /* O.K. as long as data_stream::MAX_BOUND_SIZE == INT_MAX */
        *get_size = (long)(dat->copy_from_marker(dest,array_len,beg,datetime));
        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "GetStreamSnapshotFromMarker<T>", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }
}

template< typename T > 
int TOSDB_GetStreamSnapshotFromMarker_( LPCSTR id,
                                        LPCSTR sItem, 
                                        LPCSTR sTopic, 
                                        T* dest, 
                                        size_type array_len, 
                                        pDateTimeStamp datetime,                             
                                        long beg,
                                        long *get_size )
{    
    if( !CheckStringLength( sTopic ) ) /* let this go thru std::string ? */
        return 0;     
   
    return TOSDB_GetStreamSnapshotFromMarker_( id, sItem, GetTopicEnum( sTopic ), 
                                               dest, array_len, datetime, beg, 
                                               get_size );
}

int TOSDB_GetStreamSnapshotDoublesFromMarker( LPCSTR id,
                                              LPCSTR sItem, 
                                              LPCSTR sTopic, 
                                              ext_price_type* dest, 
                                              size_type array_len, 
                                              pDateTimeStamp datetime,                                                 
                                              long beg,
                                              long *get_size )
{
    return TOSDB_GetStreamSnapshotFromMarker_( id, sItem, sTopic, dest, array_len, 
                                               datetime, beg, get_size );
}

int TOSDB_GetStreamSnapshotFloatsFromMarker( LPCSTR id, 
                                             LPCSTR sItem, 
                                             LPCSTR sTopic, 
                                             def_price_type* dest, 
                                             size_type array_len, 
                                             pDateTimeStamp datetime,                                                
                                             long beg,
                                             long *get_size )
{
    return TOSDB_GetStreamSnapshotFromMarker_( id, sItem, sTopic, dest, array_len, 
                                               datetime, beg, get_size);
}

int TOSDB_GetStreamSnapshotLongLongsFromMarker( LPCSTR id, 
                                                LPCSTR sItem, 
                                                LPCSTR sTopic, 
                                                ext_size_type* dest, 
                                                size_type array_len, 
                                                pDateTimeStamp datetime,                                                 
                                                long beg,
                                                long *get_size )
{
    return TOSDB_GetStreamSnapshotFromMarker_( id, sItem, sTopic, dest, array_len, 
                                               datetime, beg, get_size);    
}

int TOSDB_GetStreamSnapshotLongsFromMarker( LPCSTR id, 
                                            LPCSTR sItem, 
                                            LPCSTR sTopic, 
                                            def_size_type* dest, 
                                            size_type array_len, 
                                            pDateTimeStamp datetime,                                              
                                            long beg,
                                            long *get_size )
{
    return TOSDB_GetStreamSnapshotFromMarker_( id, sItem, sTopic, dest, array_len, 
                                               datetime, beg, get_size);    
}

int TOSDB_GetStreamSnapshotStringsFromMarker( LPCSTR id, 
                                              LPCSTR sItem, 
                                              LPCSTR sTopic, 
                                              LPSTR* dest, 
                                              size_type array_len, 
                                              size_type str_len, 
                                              pDateTimeStamp datetime,                                                 
                                              long beg,
                                              long *get_size)
{
    const TOSDBlock *db;
    TOSDB_RawDataBlock::stream_const_ptr_type dat;
    TOS_Topics::TOPICS tTopic;

    if( !CheckIDLength(id) || !CheckStringLength( sItem, sTopic ) )
        return -1;

    tTopic = GetTopicEnum( sTopic );    
    
    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        dat = db->block->raw_stream_ptr(sItem, tTopic);
                /* O.K. as long as data_stream::MAX_BOUND_SIZE == INT_MAX */
        *get_size = (long)(dat->copy_from_marker( dest, array_len, str_len, beg,
                                                  datetime ));     
        return 0;
    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetStreamSnapshotStringsFromMarker()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }
}

/**************************
**** data-stream-marker ***
**************************/

template<> 
generic_map_type TOSDB_GetItemFrame< false >( std::string id, 
                                              TOS_Topics::TOPICS tTopic )
{
    const TOSDBlock *db; 

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow( id );
    return db->block->map_of_frame_items( tTopic );
}

template<> 
generic_dts_map_type TOSDB_GetItemFrame< true >( std::string id, 
                                                 TOS_Topics::TOPICS tTopic )
{
    const TOSDBlock *db; 

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow( id );
    return db->block->pair_map_of_frame_items( tTopic );
}

template< typename T> 
int TOSDB_GetItemFrame_( LPCSTR id, 
                         TOS_Topics::TOPICS tTopic, 
                         T* dest, 
                         size_type array_len, 
                         LPSTR* dest2, 
                         size_type strLen2, 
                         pDateTimeStamp datetime )
{
    const TOSDBlock *db;
    int errVal = 0;

    if( !CheckIDLength(id) )
        return -1;

    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        if( datetime ){

            generic_dts_map_type::const_iterator bIter, eIter;
            generic_dts_map_type tmpMDT = 
                db->block->pair_map_of_frame_items(tTopic);
            bIter = tmpMDT.cbegin();
            eIter = tmpMDT.cend();

            if( !dest2 ){    
        
                for( size_type i = 0; 
                     (i < array_len) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        dest[i] = (T)bIter->second.first;
                        datetime[i] = bIter->second.second;
                    }

            }else{    
            
                for( size_type i = 0; 
                     ( i < array_len ) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        dest[i] = (T)bIter->second.first;
                        datetime[i] = bIter->second.second;

                        errVal = strcpy_s( dest2[i], strLen2, 
                                           (bIter->first).c_str() );
                        if( errVal ) 
                            return errVal; 
                    }
            }

        }else{

            generic_map_type::const_iterator bIter, eIter;             
            generic_map_type tmpM = db->block->map_of_frame_items(tTopic);
            bIter = tmpM.cbegin();
            eIter = tmpM.cend();

            if( !dest2 ){   
             
                for( size_type i = 0; 
                     (i < array_len) && (bIter != eIter); 
                     ++bIter, ++i )    
                    {            
                         dest[i] = (T)bIter->second;                
                    }

            }else{                

                for( size_type i = 0; 
                     ( i < array_len ) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        dest[i] = (T)bIter->second;

                        errVal = strcpy_s( dest2[i], strLen2, 
                                           (bIter->first).c_str() );
                        if( errVal ) 
                            return errVal;
                    }                    
            }
        };
    }catch( const std::exception& e ){
        TOSDB_LogH( "GetItemFrame<T>", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }

    return 0;
}

template< typename T> 
int TOSDB_GetItemFrame_( LPCSTR id, 
                         LPCSTR sTopic, 
                         T* dest, 
                         size_type array_len, 
                         LPSTR* dest2, 
                         size_type strLen2, 
                         pDateTimeStamp datetime )
{    
    if( !CheckStringLength( sTopic ) )
        return -1;   
     
    return TOSDB_GetItemFrame_( id, GetTopicEnum( sTopic ), dest, array_len, 
                                dest2, strLen2, datetime);    
}

int TOSDB_GetItemFrameDoubles( LPCSTR id, 
                               LPCSTR sTopic, 
                               ext_price_type* dest, 
                               size_type array_len, 
                               LPSTR* label_dest, 
                               size_type label_str_len, 
                               pDateTimeStamp datetime )
{
    return TOSDB_GetItemFrame_( id, sTopic, dest, array_len, label_dest, 
                                label_str_len, datetime);        
}

int TOSDB_GetItemFrameFloats( LPCSTR id, 
                              LPCSTR sTopic, 
                              def_price_type* dest, 
                              size_type array_len, 
                              LPSTR* label_dest, 
                              size_type label_str_len, 
                              pDateTimeStamp datetime )
{
    return TOSDB_GetItemFrame_( id, sTopic, dest, array_len, label_dest, 
                                label_str_len, datetime);            
}

int TOSDB_GetItemFrameLongLongs( LPCSTR id, 
                                 LPCSTR sTopic, 
                                 ext_size_type* dest, 
                                 size_type array_len, 
                                 LPSTR* label_dest, 
                                 size_type label_str_len, 
                                 pDateTimeStamp datetime )
{
    return TOSDB_GetItemFrame_( id, sTopic, dest, array_len, label_dest, 
                                label_str_len, datetime);            
}

int TOSDB_GetItemFrameLongs( LPCSTR id, 
                             LPCSTR sTopic, 
                             def_size_type* dest, 
                             size_type array_len,
                             LPSTR* label_dest, 
                             size_type label_str_len, 
                             pDateTimeStamp datetime )
{
    return TOSDB_GetItemFrame_( id, sTopic, dest, array_len, label_dest, 
                                label_str_len, datetime);            
}

int TOSDB_GetItemFrameStrings( LPCSTR id, 
                               LPCSTR sTopic, 
                               LPSTR* dest,  
                               size_type array_len, 
                               size_type str_len, 
                               LPSTR* label_dest, 
                               size_type label_str_len, 
                               pDateTimeStamp datetime )
{    
    const TOSDBlock *db;
    TOS_Topics::TOPICS tTopic;

    int errVal = 0;

    if( !CheckIDLength( id ) || !CheckStringLength( sTopic ) )
        return -1;

    tTopic = GetTopicEnum( sTopic );    
    
    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        if( datetime ){

            generic_dts_map_type::const_iterator bIter, eIter; 
            generic_dts_map_type tmpMDT = 
                db->block->pair_map_of_frame_items(tTopic);
            bIter = tmpMDT.cbegin();
            eIter = tmpMDT.cend();

            if( !label_dest ){                

                for( size_type i = 0; 
                     (i < array_len) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        datetime[i] = bIter->second.second;

                        errVal = strcpy_s( dest[i], str_len, 
                                          (bIter->second.first ).as_string()
                                                                .c_str() );
                        if( errVal) 
                            return errVal;                    
                    }

            }else{                

                for( size_type i = 0; 
                     ( i < array_len ) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        datetime[i] = bIter->second.second;
                        if((errVal = strcpy_s( dest[i], str_len, 
                                               (bIter->second.first).as_string()
                                                                    .c_str() ))
                           || (errVal = strcpy_s( label_dest[i], label_str_len, 
                                                  (bIter->first).c_str() )))
                           {
                               return errVal;
                           }
                    }
            }

        }else{

            generic_map_type::const_iterator bIter, eIter;           
            generic_map_type tmpM = db->block->map_of_frame_items(tTopic);
            bIter = tmpM.cbegin();
            eIter = tmpM.cend();

            if( !label_dest ){                

                for( size_type i = 0; 
                     (i < array_len) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        errVal = strcpy_s( dest[i], str_len, 
                                           (bIter->second).as_string().c_str());
                        if( errVal )
                             return errVal;
                    }

            }else{                

                for( size_type i = 0; 
                     ( i < array_len ) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        if((errVal = strcpy_s( dest[i], str_len, 
                                               (bIter->second).as_string()
                                                              .c_str() ))
                           || (errVal = strcpy_s( label_dest[i], label_str_len, 
                                                  (bIter->first).c_str() ))) 
                           { 
                               return errVal; 
                           }
                    }     
               
            }
        };

    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetItemFrameStrings()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }

    return 0;        
} 

template<> 
generic_map_type TOSDB_GetTopicFrame<false>( std::string id, std::string sItem)
{
    const TOSDBlock *db;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow( id );
    return db->block->map_of_frame_topics( sItem );
}

template<> 
generic_dts_map_type TOSDB_GetTopicFrame<true>( std::string id, 
                                                std::string sItem )
{
    const TOSDBlock *db;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow( id );
    return db->block->pair_map_of_frame_topics( sItem );
}

int TOSDB_GetTopicFrameStrings( LPCSTR id, 
                                LPCSTR sItem, 
                                LPSTR* dest, 
                                size_type array_len, 
                                size_type str_len, 
                                LPSTR* label_dest, 
                                size_type label_str_len, 
                                pDateTimeStamp datetime )
{    
    const TOSDBlock *db;
    int errVal = 0;

    if( !CheckIDLength( id ) || !CheckStringLength(sItem) )
        return -1;    

    try{
        our_rlock_guard_type _lock_(*global_rmutex);

        db = GetBlockOrThrow( id );
        if( datetime ){

            generic_dts_map_type::const_iterator bIter, eIter;      
            generic_dts_map_type tmpMDT = 
                db->block->pair_map_of_frame_topics(sItem);
            bIter = tmpMDT.cbegin();
            eIter = tmpMDT.cend();

            if( !label_dest ){ 

                for( size_type i = 0; 
                     (i < array_len) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        datetime[i] = bIter->second.second;

                        errVal = strcpy_s( dest[i], str_len, 
                                           (bIter->second.first).as_string()
                                                                .c_str() );
                        if( errVal) 
                            return errVal;                    
                    }

            }else{                

                for( size_type i = 0; 
                     ( i < array_len ) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        datetime[i] = bIter->second.second;
                        if((errVal = strcpy_s( dest[i], str_len, 
                                               (bIter->second.first).as_string()
                                                                    .c_str() ))
                           || (errVal = strcpy_s( label_dest[i], label_str_len, 
                                                  (bIter->first).c_str() ))) 
                           {
                              return errVal;
                           }
                    }

            }

        }else{

            generic_map_type::const_iterator bIter, eIter;
            generic_map_type tmpM = db->block->map_of_frame_topics(sItem);
            bIter = tmpM.cbegin();
            eIter = tmpM.cend();

            if( !label_dest ){

                for( size_type i = 0; 
                     (i < array_len) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        errVal = strcpy_s( dest[i], str_len, 
                                           (bIter->second).as_string()
                                                          .c_str() );
                        if( errVal ) 
                            return errVal;
                    }

            }else{

                for( size_type i = 0; 
                     ( i < array_len ) && (bIter != eIter); 
                     ++bIter, ++i )
                    {
                        if(( errVal = strcpy_s( dest[i], str_len, 
                                                (bIter->second).as_string()
                                                               .c_str() ))
                           || (errVal = strcpy_s( label_dest[i], label_str_len, 
                                                  (bIter->first).c_str() )))
                           {
                              return errVal; 
                           }
                    }                    
            }

        };

    }catch( const std::exception& e ){
        TOSDB_LogH( "TOSDB_GetTopicFrameStrings()", e.what() );
        return -2;
    }catch( ... ){
        return -2;    
    }

    return 0;
}

template<> 
generic_matrix_type TOSDB_GetTotalFrame<false>( std::string id )
{
    const TOSDBlock *db;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow( id );
    return db->block->matrix_of_frame();
}

template<> 
generic_dts_matrix_type TOSDB_GetTotalFrame<true>( std::string id )
{
    const TOSDBlock *db;

    our_rlock_guard_type _lock_(*global_rmutex);
    db = GetBlockOrThrow( id );
    return db->block->pair_matrix_of_frame();
}

std::ostream& operator<<(std::ostream& out, const generic_type& gen)
{
    out<< gen.as_string();
    return out;
}

std::ostream& operator<<(std::ostream& out, const DateTimeStamp& dts)
{
    out << dts.ctime_struct.tm_mon+1 << '/'
        << dts.ctime_struct.tm_mday << '/'
        << dts.ctime_struct.tm_year+1900 <<' '
        << dts.ctime_struct.tm_hour<<':'
        << dts.ctime_struct.tm_min<<':'
        << dts.ctime_struct.tm_sec<<':'
        << dts.micro_second;

    return out;
}

std::ostream & operator<<(std::ostream& out, const generic_matrix_type & mat)
{    
    for( auto & item : mat ){

        std::cout<<item.first<<"::: ";
        for( auto & topic : item.second )        
            std::cout<<topic.first<<"[ "<<topic.second<<" ] ";
                 
        out<<std::endl; 
    }

    return out;
}

std::ostream & operator<<(std::ostream& out, const generic_dts_matrix_type & mat)
{    
    for( auto & item : mat ){

        std::cout<<item.first<<"::: ";
        for( auto & topic : item.second )        
            std::cout<<topic.first<<"[ "<<topic.second<<" ] ";
        
        out<<std::endl; 
    }

    return out;
}

std::ostream& operator<<( std::ostream& out, 
                          const generic_map_type::value_type & sv)
{
    out<<sv.first<<' '<<sv.second;
    return out;  
}

std::ostream& operator<<(std::ostream& out, const generic_dts_type & sv)
{
    out<<sv.first<<' '<<sv.second;
    return out;  
}

std::ostream& operator<<(std::ostream& out, const dts_vector_type& vec)
{
    for(const auto & v : vec) 
        out<<vec<<' '; 
    return out;
}

std::ostream & operator<<(std::ostream& out, const generic_vector_type &vec)
{
    for(const auto & v : vec) 
        out<<v<<' '; 
    return out;
}

std::ostream & operator<<( std::ostream& out, 
                           const generic_dts_vectors_type &vecs )
{
    auto iterF = vecs.first.cbegin();
    auto iterS = vecs.second.cbegin();

    for( ; 
         iterF != vecs.first.cend() && iterS != vecs.second.cend(); 
         iterF++, iterS++ )
        {
           out<< *iterF <<' '<< *iterS <<std::endl;
        }

    return out;
}

std::ostream & operator<<(std::ostream& out, const generic_map_type & dict)
{
    for(const auto & x : dict) 
        out<<x <<std::endl;
    return out;
}

std::ostream & operator<<(std::ostream& out, const generic_dts_map_type & dict)
{
    for(const auto & x : dict) 
        out<<x.first<<' '<<x.second<<std::endl;
    return out;
}

template< typename T > 
std::ostream& operator<<( std::ostream& out, 
                          const std::pair<T,DateTimeStamp>& pair )
{
    out<< pair.first <<' '<< pair.second ;
    return out;
}

template< typename T > 
std::ostream& operator<<( std::ostream& out, const std::vector<T>& vec )
{
    for(const auto & v : vec) 
        out<<v<<' '; 
    return out;
}

template< typename T > 
std::ostream& operator<<( std::ostream& out, 
                          const std::pair< std::vector<T>, 
                                           dts_vector_type>& vecs )
{
    auto iterF = vecs.first.cbegin();
    auto iterS = vecs.second.cbegin();

    for( ; 
         iterF != vecs.first.cend() && iterS != vecs.second.cend(); 
         iterF++, iterS++ )
        {
            out<< generic_type(*iterF) <<' '<< *iterS << std::endl;
        }

    return out;
}






