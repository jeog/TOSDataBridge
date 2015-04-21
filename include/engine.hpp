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

#ifndef JO_TOSDB_ENGINE
#define JO_TOSDB_ENGINE

#include <mutex>
#include <condition_variable>
#include <atomic>
#include "tos_databridge.h"

//const steady_clock_type  steady_clock;
const system_clock_type  system_clock;

typedef struct{
    void*         hfile;   /* handle to mapping */
    void*         raw_addr; /* physical location in our process space */
    unsigned int  raw_sz;   /* physical size of the buffer */
    void*         hmtx;  
} StreamBuffer, *pStreamBuffer;

typedef std::map< std::string, size_t >                     refcount_type;
typedef std::pair< std::string , TOS_Topics::TOPICS >       id_type;
typedef std::map< TOS_Topics::TOPICS, refcount_type >       topics_type;
typedef std::map< id_type, StreamBuffer >                   buffers_type;
typedef TwoWayHashMap< TOS_Topics::TOPICS, HWND, true,
                       std::hash<TOS_Topics::TOPICS>,
                       std::hash<HWND>,
                       std::equal_to<TOS_Topics::TOPICS>,
                       std::equal_to<HWND> >                convos_types;

template < typename T >
class DDE_Data  {

    static const system_clock_type::time_point  EPOCH_TP;
    
    DDE_Data( const DDE_Data<T>& );
    DDE_Data& operator=( const DDE_Data<T>& );

    void _init_datetime();    
    
public:
    std::string         item;
    TOS_Topics::TOPICS  topic;
    T                   data;
    pDateTimeStamp      time;
    bool                valid_datetime;

    typedef struct{
        T              data;
        DateTimeStamp  time;
    } data_type;

    data_type data_strct() const
    {
        data_type d = {data, *time};
        return d;
    }
        
    DDE_Data( const TOS_Topics::TOPICS topic, 
              std::string item, 
              const T datum, 
              bool datetime ) 
        :
        topic( topic ),
        item( item ),
        data( datum ), 
        time( new DateTimeStamp ),
        valid_datetime( datetime )
        {
            if( datetime )
                _init_datetime();            
        }

    ~DDE_Data()
        {            
            delete time;
        }

    DDE_Data( DDE_Data<T>&& d )
        :
        topic( d.topic ),
        item( d.item ),
        data( d.data),
        time( d.time ),
        valid_datetime( d.valid_datetime )
        {        
            d.time = nullptr;
        }

    DDE_Data& operator=( DDE_Data<T>&& d )
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

template< typename T >
const system_clock_type::time_point  DDE_Data<T>::EPOCH_TP; 

template < typename T >
void DDE_Data<T>::_init_datetime()
{
    system_clock_type::time_point now; 
    micro_sec_type                ms;    
    std::chrono::seconds          sec;
    time_t                        t;

    /* current timepoint*/
    now = system_clock.now(); 

    /* number of ms since epoch */
    ms = std::chrono::duration_cast< micro_sec_type, 
                                     system_clock_type::rep, 
                                     system_clock_type::period>(now - EPOCH_TP);

    /* this is necessary to avoid issues w/ conversions to C time */
    sec = std::chrono::duration_cast< std::chrono::seconds >( ms );

    /* ms since last second */
    time->micro_second = (long)(( ms % micro_sec_type::period::den ).count());
    
    /* get the ctime by adjusting epoch by seconds since */
    t = system_clock.to_time_t( EPOCH_TP + sec );  
    localtime_s( &time->ctime_struct, &t );            
}

#endif
