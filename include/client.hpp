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

#ifndef JO_TOSDB_CLIENT
#define JO_TOSDB_CLIENT

#include "tos_databridge.h"
#include  <mutex>

typedef std::lock_guard< std::recursive_mutex > rGuardTy;

extern std::recursive_mutex* const globalMutex;

template< typename T,typename T2> class RawDataBlock; 

typedef RawDataBlock< generic_type, DateTimeStamp> TOSDB_RawDataBlock;

typedef struct {
    TOSDB_RawDataBlock*  block;
    str_set_type         itemPreCache;
    topic_set_type       topicPreCache;    
    unsigned long        timeout;
} TOSDBlock; /* no ptr or const typedefs; force code to state explicitly */

bool                ChceckIDLength(LPCSTR id);
bool                ChceckStringLength(LPCSTR str);
bool                ChceckStringLength(LPCSTR str, LPCSTR str2 );
bool                ChceckStringLengths(LPCSTR* str, size_type szItems);
TOS_Topics::TOPICS  GetTopicEnum( std::string sTopic); 
const TOSDBlock*    GetBlockPtr( std::string id );
const TOSDBlock*    GetBlockOrThrow( std::string id );

#endif
