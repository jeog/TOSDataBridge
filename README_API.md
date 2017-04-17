### C/C++ API
- - -

#### Types

name               | language | defined by       | description 
-------------------|----------|------------------|------------------------------------------
TOS_Topics::TOPICS | C++      | tos_databridge.h | enum of fields to be added (e.g. BID, ASK, LAST )
DateTimeStamp      | C / C++  | tos_databridge.h | struct that wraps the C library tm struct, and adds a micro-second field info on the buffer to the client
UpdateLatency      | C / C++  | tos_databridge.h | enum of milliseconds values the client library waits before re-checking buffers
ILSet<>            | C++      | containers.hpp   | wrapper around std::set<> type that provides additional means of construction / copy / move / assignment
generic_type       | C++      | generic.hpp      | custom generic type 

#### Typedefs

typedef                  | underlying type                                | language | defined by
-------------------------|------------------------------------------------|----------|--------------------------
LPCSTR                   | const char*                                    | C / C++  | WINAPI
LPS TR                   | char*                                          | C / C++  | WINAPI
BOOL                     | unsigned int                                   | C / C++  | WINAPI
size_type                | uint32_t                                       | C / C++  | tos_databridge.h
type_bits_type           | uint8_t                                        | C / C++  | tos_databridge.h
str_set_type             | ILSet<std::string>                             | C++      | tos_databridge.h 
topic_set_type           | ILSet<TOS_Topics::TOPICS>                      | C++      | tos_databridge.h 
def_price_type           | float                                          | C / C++  | tos_databridge.h
ext_price_type           | double                                         | C / C++  | tos_databridge.h
def_size_type            | long                                           | C / C++  | tos_databridge.h
ext_price_type           | long long                                      | C / C++  | tos_databridge.h
generic_dts_type         | std::pair<generic_type,DateTimeStamp>          | C++      | tos_databridge.h
generic_vector_type      | std::vector<generic_type>                      | C++      | tos_databridge.h
dts_vector_type          | std::vector<DateTimeStamp>                     | C++      | tos_databridge.h
generic_dts_vectors_type | std::pair<generic_vector_type,dts_vector_type> | C++      | tos_databridge.h
generic_map_type         | std::map<std::string, generic_type>            | C++      | tos_databridge.h
generic_dts_map_type     | std::map<std::string, generic_dts_type>        | C++      | tos_databridge.h
generic_matrix_type      | std::map<std::string, generic_map_type>        | C++      | tos_databridge.h
generic_dts_matrix_type  | std::map<std::string, generic_dts_map_type>    | C++      | tos_databridge.h

#### Error Codes

error                            | value
---------------------------------|-------
TOSDB_ERROR_BAD_INPUT            | -1
TOSDB_ERROR_BAD_INPUT_BUFFER     | -2
TOSDB_ERROR_NOT_CONNECTED        | -3
TOSDB_ERROR_TIMEOUT              | -4
TOSDB_ERROR_BLOCK_ALREADY_EXISTS | -5
TOSDB_ERROR_BLOCK_DOESNT_EXIST   | -6
TOSDB_ERROR_BLOCK_CREATION       | -7
TOSDB_ERROR_BLOCK_SIZE           | -8
TOSDB_ERROR_BAD_TOPIC            | -9
TOSDB_ERROR_BAD_ITEM             | -10
TOSDB_ERROR_BAD_SIG              | -11
TOSDB_ERROR_IPC                  | -12
TOSDB_ERROR_IPC_MSG              | -13
TOSDB_ERROR_CONCURRENCY          | -14
TOSDB_ERROR_ENGINE_NO_TOPIC      | -15
TOSDB_ERROR_ENGINE_NO_ITEM       | -16
TOSDB_ERROR_SERVICE              | -17
TOSDB_ERROR_GET_DATA             | -18
TOSDB_ERROR_GET_STATE            | -19
TOSDB_ERROR_SET_STATE            | -20
TOSDB_ERROR_DDE_POST             | -21
TOSDB_ERROR_DDE_NO_ACK           | -22
TOSDB_ERROR_SHEM_BUFFER          | -23
TOSDB_ERROR_UNKNOWN              | -24
TOSDB_ERROR_DECREMENT_BASE       | <= -25

#### C vs C++ Return Conventions

In most cases an 'int' return value indicates sucess(0) or failure(non-0 error code). Most C calls don't return values directly, they assign a (pointed to) value or populate a buffer/array. C++ versions of the same calls, generally, return a value directly and throw an exception on error. A function returning a pure boolean value is represented by an unsigned int(C) or bool(C++).

#### Connecting 

**`[C/C++] TOSDB_Connect() -> int`** 

- Connect to the service/engine.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_Disconnect() -> int`** 

- Close connection. (Called automatically when the library is unloaded.)
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_IsConnected() -> unsigned int` [DEPRECATED]** 

- Returns 1 if connected to the service/engine, 0 if not.

**`[C/C++] TOSDB_IsConnectedToEngine() -> unsigned int`** 

- Returns 1 if connected to the service/engine, 0 if not.

**`[C/C++] TOSDB_IsConnectedToEngineAndTOS() -> unsigned int`** 

- Returns 1 if connected to the service/engine AND can communicate with TOS platform, 0 if not.

**`[C/C++] TOSDB_ConnectionState() -> unsigned int`** 

Returns a constant indicating state of connection:
- TOSDB_CONN_NONE: not connected to engine/service or TOS
- TOSDB_CONN_ENGINE: connected to engine/service but not TOS (admin calls only)
- TOSDB_CONN_EGINE_TOS: connected to engine/service and TOS (admin calls AND data from platform)


#### Data Blocks

TOSDB's main organizational unit is the (data) block (struct TOSDBlock in client.hpp). A block contains streams, of a certain size, that hold historical data.

**`[C/C++] TOSDB_CreateBlock(LPCSTR id, size_type sz, BOOL is_datetime, size_type timeout) -> int`** 

- Create a block. 
- 'id' is a unique ID string(<= TOSDB_BLOCK_ID_SZ) that will be used to access it throughout its lifetime.
- 'sz indicates how much historical data is saved in the block's data-streams.
- 'is_datetime' is a flag indicating whether date-time (see DateTimeStamp) should be saved alongside primary data in the streams.
- 'timeout' is milliseconds used for internal waiting/synchronization(see TOSDB_DEF_TIMEOUT, TOSDB_MIN_TIMEOUT). 
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_CloseBlock(LPCSTR id) -> int`** 

- Deallocate block's internal resources and remove it.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_CloseBlocks() -> int`** 

- Close all blocks that currently exist in the dll instance. 
- Returns 0 on success, error code on failure. 

Within each block is a pointer to a RawDataBlock object created by an internal factory. The factory has a limit (default is 10) which can be adjusted. (The number of blocks  should always be the same as the number RawDataBlocks.)

**`[C/C++] TOSDB_GetBlockLimit() -> size_type`**

- Returns maximum number of blocks that can exist in the dll instance. 

**`[C/C++] TOSDB_SetBlockLimit(size_type sz) -> size_type`**

- Set the maximum number of blocks that can exist in the dll instance.
- Returns new limit.

**`[C/C++] TOSDB_GetBlockCount() -> size_type `** 

- Returns the number of blocks allocated in the dll instance.

**`[C/C++] TOSDB_GetBlockIDs(LPSTR* dest, size_type array_len, size_type str_len) -> int`** 

- Populates '*dest' with IDs of all blocks that currently exist in the dll instance. 
- 'array_len' is the size of the array to be populated and should == TOSDB_GetBlockCount().
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_MAX_STR_SZ.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetBlockIDs() -> str_set_type`**

- Returns 'str_set_type' with IDs of all blocks that currently exist in the dll instance.
- Throws on failure.

**`[C/C++] TOSDB_GetBlockSize(LPCSTR id, size_type* pSize) -> int`**

- Sets '*pSize' to how much historical data can be saved in the block's data-streams.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetBlockSize(std::string id) -> size_type`** 

- Returns how much historical data can be saved in the block's data-streams.

**`[C/C++] TOSDB_SetBlockSize(LPCSTR id, size_type sz) -> int`**

- Changes how much historical data can be saved in the block's data-streams. 
- 'sz' must be > 1 and <= TOSDB_MAX_BLOCK_SZ.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_IsUsingDateTime(LPCSTR id, unsigned int* is_datetime) -> int`** 

- Set '*is_datetime' to 1 if block is storing DateTimeStamp objects alongside primary data, 0 otherwise.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_IsUsingDateTime(std::string id) -> bool`**

- Returns if block is storing DateTimeStamp objects alongside primary data.

The client library extracts data from the engine (tos-databridge-engine[].exe) through a shared memory segment. The library loops through its blocks/streams looking to see what buffers have been updated, reading the buffers if necessary. The 'latency' is the wait time between loops and is represented by the 'UpdateLatency' Enum. The default(Moderate, 300) should be fine for most users.

**`[C/C++] TOSDB_GetLatency() -> unsigned long`** 

- Returns wait time (in milliseconds) between reads of the shared memory buffer by the client library.

**`[C/C++] TOSDB_SetLatency(UpdateLatency latency) -> unsigned long`** 

- Set wait time (in milliseconds) between reads of the shared memory buffer by the client library.
- Returns new latency.


#### Items / Topics / Streams

Once a block is created, items and topics are added. Topics are the TOS fields (e.g. LAST, VOLUME, BID ) and items are the individual symbols (e.g. IBM, GE, SPY). C++ uses the TOS_Topics::TOPICS enum. The somewhat unintuitive terms 'item' and 'topic' come from DDE terminology. Data are stored in 'streams', created internally as a result of topics and items being added. 

For example, if we add two topics and three items we have six streams:

&nbsp;     | SPY | QQQ | GOOG
-----------|-----|-----|-----
**LAST**   |&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;&nbsp;X
**VOLUME** |&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;&nbsp;X


**`[C/C++] TOSDB_Add(LPCSTR id, LPCSTR* items, size_type items_len, LPCSTR* topics_str , size_type topics_len) -> int`**  
**`[C++] TOSDB_Add(std::string id, str_set_type items, topic_set_type topics_t) -> int`** 

- Add items and topics to the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_AddTopic(LPCSTR id, LPCSTR topic_str) -> int`**   
**`[C++] TOSDB_AddTopic(std::string id, TOS_Topics::TOPICS topic_t) -> int`**

- Add a single topic to the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_AddItem(LPCSTR id, LPCSTR item) -> int`**   
**`[C++] TOSDB_AddItem(std::string id, std::string item) -> int`**

- Add a single item to the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_AddTopics(LPCSTR id, LPCSTR* topics_str, size_type topics_len) -> int`**   
**`[C++] TOSDB_AddTopics(std::string id, topic_set_type topics_t) -> int`**

- Add multiple topics to the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_AddItems(LPCSTR id, LPCSTR* items, size_type items_len) -> int`**   
**`[C++] TOSDB_AddItems(std::string id, str_set_type items) -> int`**

- Add multiple items to the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_RemoveTopic(LPCSTR id, LPCSTR topic_str) -> int`**  
**`[C++] TOSDB_RemoveTopic(std::string id, TOS_Topics::TOPICS topic_t) -> int`**

- Remove a single topic from the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_RemoveItem(LPCSTR id, LPCSTR item) -> int`**  
**`[C++] TOSDB_RemoveItem(std::string id, std::string item) -> int`**

- Remove a single item from the block.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_GetItemCount(LPCSTR id, size_type* count) -> int`**

- Sets '*count' to the number of items in the block. 
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetItemCount(std::string id) -> size_type`**

- Returns the number of items in the block. 
- Throws on failure.

**`[C/C++] TOSDB_GetTopicCount(LPCSTR id, size_type* count) -> int`**

- Sets '*count' to the number of topics in the block. 
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetTopicCount(std::string id) -> size_type`**

- Returns the number of topics in the block. 
- Throws on failure.

**`[C/C++] TOSDB_GetItemNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len) -> int`** 

- Populates 'dest' with item strings currently in the block. 
- 'array_len' is the size of the array and should == TOSDB_GetItemCount().
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_MAX_STR_SZ.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetItemNames(std::string id) -> str_set_type`**

- Returns 'str_set_type' containing all item strings in the block.
- Throws on failure.

**`[C/C++] TOSDB_GetTopicNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len) -> int`** 

- Populates 'dest' with topic strings currently in the block. 
- 'array_len' is the size of the array and should == TOSDB_GetTopicCount().
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_MAX_STR_SZ.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetTopicNames(std::string id) -> str_set_type`**

- Returns 'str_set_type' containing all topic strings in the block.
- Throws on failure.

**`[C++] TOSDB_GetTopicEnums() -> topic_set_type`** 

- Returns 'topic_set_type' containing all topic enums(TOS_Topics::TOPICS) in the block.
- Throws on failure.

##### Pre-Caching

Items\[Topics\] added before any topics\[items\] exist in the block will be pre-cached, i.e they will be visible to the back-end but not to the interface until a topic\[item\] is added; likewise if all the items\[topics\] are removed(thereby leaving only topics\[items\]). See [Important Details and Provisos](README_DETAILS.md). To view the pre-cache use the following calls:

**`[C/C++] TOSDB_GetPreCachedItemCount(LPCSTR id, size_type* count) -> int`**

- Sets '*count' to the number of items in the block's pre-cache. 
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetPreCachedItemCount(std::string id) -> size_type`**

- Returns the number of items in the block's pre-cache. 
- Throws on failure.

**`[C/C++] TOSDB_GetPreCachedTopicCount(LPCSTR id, size_type* count) -> int`**

- Sets '*count' to the number of topics in block's pre-cache. 
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetPreCachedTopicCount(std::string id) -> size_type ->`**

- Returns the number of topics in block's pre-cache. 
- Throws on failure.

**`[C/C++] TOSDB_GetPreCachedItemNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len) -> int`** 

- Populates 'dest' with item strings currently in the block's pre-cache. 
- 'array_len' is the size of the array and should == TOSDB_GetPreCachedItemCount().
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_MAX_STR_SZ.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetPreCachedItemNames(std::string id) -> str_set_type`**

- Returns 'str_set_type' containing all items in the block's pre-cache.
- Throws on failure.

**`[C/C++] TOSDB_GetPreCachedTopicNames(LPCSTR id, LPSTR* dest, size_type array_len, size_type str_len) -> int`** 

- Populates 'dest' with topic strings currently in the block's pre-cache. 
- 'array_len' is the size of the array and should == TOSDB_GetPreCachedTopicCount().
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_MAX_STR_SZ.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetPreCachedTopicNames(std::string id) -> str_set_type`**

- Returns 'str_set_type' containing all topics in the block's pre-cache.
- Throws on failure.

**`[C++] TOSDB_GetPreCachedTopicEnums() -> topic_set_type`** 

- Returns 'topic_set_type' containing all topic enums(TOS_Topics::TOPICS) in the block's pre-cache.
- Throws on failure.

##### Determining Type of a Topic

Different streams store data as different types. You need to unpack the type info from inside the topic enum in order to make the correct 'Get' call(below).

**`[C++] TOS_Topics::Type<E>::type`** 

- Static object used to get the underlying type of topic enum at compile-time.
- 'E' is a TOS_Topics::TOPICS enum.

**`[C/C++] TOSDB_GetTypeBits(LPCSTR topic_str, type_bits_type* type_bits) -> int`** 

- Sets '*type_bits' to the type bits of the topic string.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetTypeBits(TOS_Topics::TOPICS topic_t) -> type_bits_type`**

- Returns the type bits of the topic enum.
- Throws on failure.

Check the bits with the appropriate constants:

    type_bits_type tbits;
    TOSDB_GetTypeBits("BID", &tbits);

    if(tbits == TOSDB_INTGR_BIT) 
       \\ just the integer bit
       \\ data are longs (def_size_type).
    else if(tbits == TOSDB_INTGR_BIT | TOSDB_QUAD_BIT) 
       \\ integer bit and the quad bit
       \\ data are long longs (ext_size_type)
    else if (tbits == TOSDB_QUAD_BIT)
       \\ quad bit, no integer bit
       \\ data are doubles (ext_price_type)
    else if (tbits) == 0)
       \\ no quad bit, no integer bit
       \\ data are floats (def_price_type)
    else if (tbits == TOSDB_STRING_BIT)
       \\ no quad bit, no integer bit, just the string bit
       \\ data are strings
    else 
       \\ ERROR 
    
> **IMPORTANT** Make sure you don't simply check a bit with logical AND when what you really want is to check the entire type_bits_type value. In this example checking for the INTGR_BIT will return true for def_size_type(long) AND ext_size_type(long long). 

**`[C/C++] TOSDB_GetTypeString(LPCSTR topic_str, LPSTR dest, size_type str_len) -> int`** 

- Populates 'dest' with a (platform dependent) type string of the topic.
- 'str_len' is the size of the buffer and should be >= TOSDB_MAX_STR_SZ.
- Returns 0 on success, error code on failure. 

**`[C++] TOSDB_GetTypeString(TOS_Topics::TOPICS topic_t) -> std::string`**

- Returns a (platform dependent) type string of the topic.
- Throws on failure.


##### Accessing Engine Directly

**`[C/C++] TOSDB_DumpSharedBufferStatus() -> int`**

- Dump engine's buffer info to a file in /log.
- Returns 0 on success, error code on failure. 

**`[C/C++] TOSDB_RemoveOrphanedStream(LPCSTR item, LPCSTR topic_str) -> int`**

- Removes an independent stream from the engine.
- **WARNING** - this should only be used when certain a client lib has failed to close a stream during destruction of the containing block. If that's not the case **YOU CAN CORRUPT THE UNDERLYING BUFFERS FOR ANY OR ALL CLIENT INSTANCE(S)!**
- Returns 0 on success, error code on failure. 


#### Historical Data

In order to get data from the block/stream we use simple inclusive indexing. 

- positive indices go from most to least recent(new to old).
- negative indicies go from least to most recent(old to new).
- the beginning('beg') index represents the most recent value of a range.
- the ending('end') index represents the least recent(oldest) value of a range.
- 0 or -block size is the index of the most recent value.
- -1 or block size - 1 is the index of the least recent value. 
- C++ calls will throw std::invalid_argument() on index errors.

Ex.  
block size = 10; begin = 3(-7), end = 5(-5),  
\[0\] \[1\] \[2\] **\[3\] \[4\] \[5\]** \[6\] \[7\] \[8\] \[9\] 


##### Occupancy

The size of a block (and all its streams) is simply the maximum amount of data it can hold. The occupancy tells how much data is actually in each stream.

**`[C/C++] TOSDB_GetStreamOccupancy(LPCSTR id,LPCSTR item, LPCSTR topic_str, size_type* sz) -> int`** 

- Sets '*sz' to how many data-points are currently in the particular stream.
- Returns 0 on success, error code on failure.

**`[C++] TOSDB_GetStreamOccupancy(std::string id, std::string item, TOS_Topics::TOPICS topic_t) -> size_type`**

- Returns how many data-points are currently in the particular stream.


##### Different Versions

Generally the client has three different versions of calls for getting historical data:

1. *type-named* (e.g TOSDB_GetDouble):  
figure out the type of the topic for that data-stream (see [Determining Type of a Topic](#determining-type-of-a-topic)) and use the appropriately named call.
2. *string* (e.g TOSDB_GetString):  
data will be represent as char* or std::string. **(SLOW)**
3. *generic* (e.g, TOSDB_Get<generic_type,false>):  
returns generic_type object(s) that know their native type(C++ only). **(SLOW)**

The following functions all have DateTimeStamp versions. In order to use them the block must have been constructed with this option set(is_datetime == TRUE). The C versions need a non-NULL pointer to a DateTimeStamp struct or an array of structs(pass NULL to avoid getting DateTimeStamp(s)). The C++ versions take a boolean template arg instead. 

> **IMPORTANT** DateTimeStamp arrays in C should be the same length as the primary data array.

> **IMPORTANT** Label arrays (see Frames, below) in C should be the same length as the primary data array and DateTimeStamp array (if used).

(Unfortunately) we use the explict type-names and their typedefs inconsistently. Keep in mind:

typedef        | type 
---------------|-----------
def_price_type | float
ext_price_type | double
def_size_type  | long
ext_price_type | long long

> **FUTURE DEPRECATION WARNING** These bad/confusing typedefs will probably be deprecated at some point. 


##### Individual Data-Points

**`[C/C++] TOSDB_GetDouble(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, ext_price_type* dest, pDateTimeStamp datetime) -> int`**  
**`[C/C++] TOSDB_GetFloat(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, def_price_type* dest, pDateTimeStamp datetime) -> int`**  
**`[C/C++] TOSDB_GetLongLong(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, ext_size_type* dest, pDateTimeStamp datetime) -> int`**  
**`[C/C++] TOSDB_GetLong(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, def_size_type* dest, pDateTimeStamp datetime) -> int`**  

- Sets '*dest' to the historical value at position 'indx'.
- Sets '*datetime' to DateTime struct if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- Returns 0 on success, error code on failure.

**`[C/C++] TOSDB_GetString(LPCSTR id, LPCSTR item, LPCSTR topic_str, long indx, LPSTR dest, size_type str_len, pDateTimeStamp datetime) -> int`** 

- Populates '*dest' with the historical value at position 'indx, as string.
- Populates '*datetime' struct if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- 'str_len' is the size of the string buffer and should be >= TOSDB_STR_DATA_SZ.
- Returns 0 on success, error code on failure.

**`[C++] TOSDB_Get<ext_price_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> ext_price_type`**  
**`[C++] TOSDB_Get<def_price_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> def_price_type`**  
**`[C++] TOSDB_Get<ext_size_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> ext_size_type`**  
**`[C++] TOSDB_Get<def_size_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> def_size_type`**  

- Returns the historical value at position 'indx'.
- Throws on failure.

**`[C++] TOSDB_Get<std::string,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> std::string`**

- Returns the historical value at position 'indx', as string.
- Throws on failure.

**`[C++] TOSDB_Get<generic_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> generic_type`**

- Returns the historical value at position 'indx', as generic_type.
- Throws on failure.

**`[C++] TOSDB_Get<ext_price_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> std::pair<ext_price_type, DateTimeStamp>`**  
**`[C++] TOSDB_Get<def_price_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> std::pair<def_price_type, DateTimeStamp>`**  
**`[C++] TOSDB_Get<ext_size_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> std::pair<ext_size_type, DateTimeStamp>`**  
**`[C++] TOSDB_Get<def_size_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> std::pair<def_size_type, DateTimeStamp>`**  

- Returns an std::pair of the historical value at position 'indx' AND DateTimeStamp if block supports it.
- Throws on failure.

**`[C++] TOSDB_Get<std::string,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> std::pair<std::string, DateTimeStamp>`**

- Returns an std::pair of the historical value at position 'indx', as string, AND DateTimeStamp if block supports it.
- Throws on failure.

**`[C++] TOSDB_Get<generic_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long indx) -> generic_dts_type`**

- Returns an std::pair of the historical value at position 'indx', as generic_type, AND DateTimeStamp if block supports it.
- Throws on failure.


##### Multiple Contiguous Data-Points

> **IMPLENTATION NOTE** Internally the data-stream tries to limit what is copied by keeping track of the streams occupancy and only returning valid data. C++ calls will therefore return a dynamically sized containter that may be smaller than what you expected. C calls may leave 'tail' elements of the passed array undefined if there is not valid data to fill them.

> **PERFORMANCE NOTE** Not suprisingly, when n is very large the the non-string C calls are the fastest, with the non-generic, non-string  C++ calls just behind. 

**`[C/C++] TOSDB_GetStreamSnapshotDoubles(LPCSTR id,LPCSTR item, LPCSTR topic_str, ext_price_type* dest, size_type array_len, pDateTimeStamp datetime, long end, long beg) -> int`**  
**`[C/C++] TOSDB_GetStreamSnapshotFloats(LPCSTR id,LPCSTR item, LPCSTR topic_str, def_price_type* dest, size_type array_len, pDateTimeStamp datetime, long end, long beg) -> int`**  
**`[C/C++] TOSDB_GetStreamSnapshotLongLongs(LPCSTR id,LPCSTR item, LPCSTR topic_str, ext_size_type* dest, size_type array_len, pDateTimeStamp datetime, long end, long beg) -> int`**  
**`[C/C++] TOSDB_GetStreamSnapshotLongs(LPCSTR id,LPCSTR item, LPCSTR topic_str, def_size_type* dest, size_type array_len, pDateTimeStamp datetime, long end, long beg) -> int`**  

- Populates '*dest' with historical data between position 'beg' and 'end', inclusively.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- Returns 0 on success, error code on failure.

**`[C/C++] TOSDB_GetStreamSnapshotStrings(LPCSTR id, LPCSTR item, LPCSTR topic_str, LPSTR* dest, size_type array_len, size_type str_len, pDateTimeStamp datetime, long end, long beg) -> int`**  

- Populates '*dest' with historical data, as strings, between position 'beg' and 'end', inclusively.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_STR_DATA_SZ.
- Returns 0 on success, error code on failure.

**`[C++] TOSDB_GetStreamSnapshot<ext_price_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::vector<ext_price_type>`**  
**`[C++] TOSDB_GetStreamSnapshot<def_price_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::vector<def_price_type>`**  
**`[C++] TOSDB_GetStreamSnapshot<ext_size_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::vector<ext_size_type>`**  
**`[C++] TOSDB_GetStreamSnapshot<def_size_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::vector<def_size_type>`**  

- Returns vector of historical data between position 'beg' and 'end', inclusively.
- Throws on failure.

**`[C++] TOSDB_GetStreamSnapshot<std::string,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::vector<std::string>`**  

- Returns vector of historical data, as strings, between position 'beg' and 'end', inclusively.
- Throws on failure.

**`[C++] TOSDB_GetStreamSnapshot<generic_type,false>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> generic_vector_type`**  

- Returns vector of historical data, as generic_types, between position 'beg' and 'end', inclusively.
- Throws on failure.

**`[C++] TOSDB_GetStreamSnapshot<ext_price_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::pair<std::vector<ext_price_type>,dts_vector_type>`**  
**`[C++] TOSDB_GetStreamSnapshot<def_price_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::pair<std::vector<def_price_type>,dts_vector_type>`**  
**`[C++] TOSDB_GetStreamSnapshot<ext_size_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::pair<std::vector<ext_size_type>,dts_vector_type>`**  
**`[C++] TOSDB_GetStreamSnapshot<def_size_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::pair<std::vector<def_size_type>,dts_vector_type>`**  

- Returns pair of vectors of historical data and matching DateTimeStamps between position 'beg' and 'end', inclusively.
- Throws on failure.

**`[C++] TOSDB_GetStreamSnapshot<std::string,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> std::pair<std::vector<std::string>,dts_vector_type>`**  

- Returns pair of vectors of historical data, as strings, and matching DateTimeStamps between position 'beg' and 'end', inclusively.
- Throws on failure.

**`[C++] TOSDB_GetStreamSnapshot<generic_type,true>(std::string id, std::string item, TOS_Topics::TOPICS topic_t, long end, long beg) -> generic_dts_vectors_type`**  

- Returns pair of vectors of historical data, as generic_types, and matching DateTimeStamps between position 'beg' and 'end', inclusively.
- Throws on failure.



##### Multiple Contiguous Data-Points From an 'Atomic' Marker

It's likely the stream will grow between consecutive calls. These calls guarantee to pick up where the last 'Get', 'GetStreamSnapshot', or 'GetStreamSnapshotFromMarker' call ended (under a few assumptions).  Internally the stream maintains a 'marker' that tracks the position of the last value pulled; the act of retreiving data and moving the marker can be thought of as a single, 'atomic' operation.

**`[C/C++] TOSDB_GetStreamSnapshotDoublesFromMarker(LPCSTR id,LPCSTR item,LPCSTR topic_str,ext_price_type* dest,size_type array_len, pDateTimeStamp datetime, long beg, long *get_size) -> int`**  
**`[C/C++] TOSDB_GetStreamSnapshotFloatsFromMarker(LPCSTR id,LPCSTR item,LPCSTR topic_str,def_price_type* dest,size_type array_len, pDateTimeStamp datetime, long beg, long *get_size) -> int`**  
**`[C/C++] TOSDB_GetStreamSnapshotLongLongsFromMarker(LPCSTR id,LPCSTR item,LPCSTR topic_str,ext_size_type* dest,size_type array_len, pDateTimeStamp datetime, long beg, long *get_size) -> int`**   
**`[C/C++] TOSDB_GetStreamSnapshotLongsFromMarker(LPCSTR id,LPCSTR item,LPCSTR topic_str,def_size_type* dest,size_type array_len, pDateTimeStamp datetime, long beg, long *get_size) -> int`**  

- Populates '*dest' with historical data between position 'beg' and the marker, inclusively.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- Sets '*get_size' to number of data-points copied. A negative value inidicates buffer is too small or stream is 'dirty'(see below).
- Returns 0 on success, error code on failure.

**`[C/C++] TOSDB_GetStreamSnapshotStringsFromMarker(LPCSTR id, LPCSTR item, LPCSTR topic_str, LPSTR* dest, size_type array_len, size_type str_len, pDateTimeStamp datetime, long beg, long *get_size) -> int`** 

- Populates '*dest' with historical data, as strings, between position 'beg' and the marker, inclusively.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_STR_DATA_SZ.
- Sets '*get_size' to number of data-points copied. A negative value inidicates buffer is too small or stream is 'dirty'(see below).
- Returns 0 on success, error code on failure.

A 'Dirty Stream' results from the marker hiting the back of the stream, indicating all data older than the marker is lost. To avoid this be sure you use a big enough block and/or keep the marker moving foward (by using calls mentioned above).

**`[C/C++] TOSDB_IsMarkerDirty(LPCSTR id, LPCSTR item, LPCSTR topic_str, unsigned int* is_dirty) -> int`**

- Sets '*is_dirty' to 1 if the stream is dirty, 0 otherwise.
- There is no guarantee that a 'clean' stream will not become dirty before the next call.
- Returns 0 on success, error code on failure.

**`[C++] TOSDB_IsMarkerDirty(std::string id, std::string item, TOS_Topics::TOPICS topic_t) -> bool`** 

- Returns if the stream is dirty.
- There is no guarantee that a 'clean' stream will not become dirty before the next call.
- Throws on failure.

**`[C/C++] TOSDB_GetMarkerPosition(LPCSTR id,LPCSTR item, LPCSTR topic_str, long long* pos) -> int`**

- Sets '*pos' with the current position (index) of the marker in the stream.
- Returns 0 on success, error code on failure.

**`[C++] TOSDB_GetMarkerPosition(std::string id, std::string item, TOS_Topics::TOPICS topic_t) -> long long`** 

- Returns the current position (index) of the marker in the stream.
- Throws on failure.


#### Frames

A frame is a collection of the most recent data (position/indx = 0) for ALL topics, ALL items, or both. 

##### Item Frames

Item Frames are the most recent values of ALL the items and a single topic. (e.g {'LAST','SPY'} {'LAST','QQQ'} {'LAST','IWM'})

**`[C/C++] TOSDB_GetItemFrameDoubles(LPCSTR id, LPCSTR topic_str, ext_price_type* dest, size_type array_len, LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime) -> int`**  
**`[C/C++] TOSDB_GetItemFrameFloats(LPCSTR id, LPCSTR topic_str, def_price_type* dest, size_type array_len, LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime) -> int`**  
**`[C/C++] TOSDB_GetItemFrameLongLongs(LPCSTR id, LPCSTR topic_str, ext_size_type* dest, size_type array_len, LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime) -> int`**  
**`[C/C++] TOSDB_GetItemFrameLongs(LPCSTR id, LPCSTR topic_str, def_size_type* dest, size_type array_len, LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime) -> int`**  

- Populates '*dest' with the current 'topic_str' values for each item in the block.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- Populates '*label_dest' with the matching labels(the matching item string) for each topic if NOT NULL.
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- Returns 0 on success, error code on failure.

**`[C/C++] TOSDB_GetItemFrameStrings(LPCSTR id, LPCSTR topic_str, LPSTR* dest, size_type array_len, size_type str_len, LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime) -> int`**

- Populates '*dest' with the current 'topic_str' values, as strings, for each item in the block.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- Populates '*label_dest' with the matching labels(the matching item string) for each item if NOT NULL.
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_STR_DATA_SZ.
- Returns 0 on success, error code on failure.


**`[C++] TOSDB_GetItemFrame<false>(std::string id, TOS_Topics::TOPICS topic_t) -> generic_map_type`**

- Returns std::map of item strings to the maching 'topic_t' value, as generic_type, for each item in the block. 
- Throws on error.

**`[C++] TOSDB_GetItemFrame<true>(std::string id, TOS_Topics::TOPICS topic_t) -> generic_dts_map_type`**

- Returns std::map of item strings to the std::pairs of the maching 'topic_t' value, as generic_type, and DateTimeStamps, for each item in the block. 
- Throws on error.


##### Topic Frames

Topic Frames are the most recent values of ALL the topics and a single item. (e.g {'LAST','SPY'} {'BID','SPY'} {'ASK','SPY'})

**`[C/C++] TOSDB_GetTopicFrameStrings(LPCSTR id, LPCSTR item, LPSTR* dest, size_type array_len, size_type str_len, LPSTR* label_dest, size_type label_str_len, pDateTimeStamp datetime) -> int`**

- Populates '*dest' with the current 'item' values, as strings, for each topic in the block.
- Populates '*datetime' with the matching DateTime structs if NOT NULL and block supports date-time(see TOSDB_IsUsingDateTime).
- Populates '*label_dest' with the matching labels(the matching topic string) for each topic if NOT NULL.
- 'array_len' is the size of the array(s) and must be big enough to fit the data or it will be truncated.
- 'str_len' is the size of each string buffer in the array and should be >= TOSDB_STR_DATA_SZ.
- Returns 0 on success, error code on failure.

**`[C++] TOSDB_GetTopicFrame<false>(std::string id, std::string item) -> generic_map_type`**

- Returns std::map of topic strings to the maching 'item' value, as generic_type, for each topic in the block. 
- Throws on error.
 
**`[C++] TOSDB_GetTopicFrame<true>(std::string id, std::string item) -> generic_dts_map_type`**

- Returns std::map of topic strings to the std::pairs of the maching 'item' value, as generic_type, and DateTimeStamps, for each topic in the block. 
- Throws on error.


##### Total Frames

Total Frames are the most recent values of ALL the topics and ALL the items. 

**`[C++] TOSDB_GetTotalFrame<false>(std::string id) -> generic_matrix_type`** 

- Returns std::map of item strings to the std::map of topic strings and the maching 'item' value, as generic_type, for each topic in the block. 
- Throws on error.

**`[C++] TOSDB_GetTotalFrame<true>(std::string id) -> generic_dts_matrix_type`** 

- Returns std::map of topic strings to std::map of the std::pairs of the maching 'item' value, as generic_type, and DateTimeStamps, for each topic in the block. 
- Throws on error.


#### Logging, Exceptions & Stream Overloads

The library exports some logging functions that dovetail with its use of custom exception classes. Most of the modules use these logging functions internally. The files are sent to appropriately named .log files in /log. Client code is sent to /log/client-log.log by using the following calls:  

**`[C/C++] TOSDB_LogH(LPCSTR tag, LPCSTR desc)` [MACRO]** 

- log high priority message.

**`[C/C++] TOSDB_Log(LPCSTR tag, LPCSTR desc)` [MACRO]** 

- log low priority message.

**`[C/C++] TOSDB_LogEx(LPCSTR tag,LPCSTR desc,int error)` [MACRO]** 

- pass additional argument for an error code like one returned from *GetLastError()*. 

The library also defines an exception hierarchy (exceptions.hpp) derived from TOSDB_Error and std::exception. C++ code can catch them and respond accordingly; the base class provides threadID(), processID(), tag(), and info() methods as well as the inherited what() from std::exception. 

There are operator\<\< overloads (client_out.cpp) for most of the custom objects and containers returned by the myriad 'Get...' calls.

