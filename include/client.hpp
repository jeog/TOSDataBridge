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

#ifndef JO_TOSDB_CLIENT
#define JO_TOSDB_CLIENT

#include "tos_databridge.h"
#include <mutex>
#include <chrono>

typedef std::lock_guard<std::recursive_mutex> our_rlock_guard_type;
extern std::recursive_mutex global_rmutex;

#define GLOBAL_RLOCK_GUARD our_rlock_guard_type our_rlock_guard(global_rmutex);

template<typename T,typename T2> class RawDataBlock; 
typedef RawDataBlock< generic_type, DateTimeStamp> TOSDB_RawDataBlock;

typedef std::chrono::duration<long, std::milli>  milli_sec_type;

typedef struct {
  TOSDB_RawDataBlock* block;
  str_set_type        item_precache;
  topic_set_type      topic_precache;  
  unsigned long       timeout;
} TOSDBlock; /* no ptr or const typedefs; force code to state explicitly */

bool CheckIDLength(LPCSTR id);
bool CheckStringLength(LPCSTR str);
bool CheckStringLength(LPCSTR str, LPCSTR str2);
bool CheckStringLengths(LPCSTR* str, size_type items_len);

TOS_Topics::TOPICS GetTopicEnum(std::string sTopic); 
const TOSDBlock*   GetBlockPtr(std::string id);
const TOSDBlock*   GetBlockOrThrow(std::string id);

#endif
