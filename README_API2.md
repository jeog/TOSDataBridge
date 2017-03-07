### C/C++ API
- - -

#### Connecting

> **IMPORTANT:** The TOSDataBridge service/engine and the TOS platform need to be running.

**`TOSDB_Connect()`** - connect to the service/engine; returns 0 on success, error code on failure. 

**`TOSDB_IsConnected()`** - **DEPRECATED** - returns 1 if connected to the service/engine, 0 if not.

**`TOSDB_IsConnectedToEngine()`** - returns 1 if connected to the service/engine, 0 if not.

**`TOSDB_IsConnectedToEngineAndTOS()`** - returns 1 if connected to the service/engine AND can communicate with TOS platform, 0 if not.

**`TOSDB_ConnectionState()`** - return a constant indicating state of connection:
- TOSDB_CONN_NONE: not connected to engine/service or TOS
- TOSDB_CONN_ENGINE: connected to engine/service but not TOS
- TOSDB_CONN_EGINE_TOS: connected to engine/service and TOS

**`TOSDB_Disconnect()`** - close connection. (Unnecessary, as it's called automatically when the library is unloaded.)

> **NOTABLE CONVENTIONS:** The C calls, except in a few cases, don't return values but populate variables, arrays/buffers, and arrays of pointers to char buffers. The 'dest' argument is for primary data; its a pointer to a scalar variable, or an array/buffer when followed by argument 'arr_len' for the number of array/buffer elements. 

> The String versions of the calls take a char\*\* argument, followed by arr_len for the number of char\*s, and str_len for the size of the buffer each char\* points to (obviously they should all be >= to this value). 

> If the call requires more than one array/buffer besides 'dest' (the Get...Frame... calls for instance) it assumes an array length equal to that of 'arr_len'. If it is of type char\*\* you need to specify a char buffer length just as you do for the initial char\*\*.

#### Blocks

TOSDB's main organizational unit is the 'block'(struct TOSDBlock in client.hpp): it's important functionally and conceptually.

**`TOSDB_CreateBlock()`** - pass a unique ID(<= TOSDB_BLOCK_ID_SZ) that will be used to access it throughout its lifetime, a size(how much historical data is saved in the block's data-streams), a flag indicating whether it saves DateTime in the stream, and a timeout value in milliseconds used for its internal waiting/synchronization(see TOSDB_DEF_TIMEOUT, TOSDB_MIN_TIMEOUT). 

**`TOSDB_CloseBlock()`** - deallocate block's internal resources and remove it.

**`TOSDB_CloseBlocks()`** - close all block's that currently exist in that dll instance. 

**`TOSDB_GetBlockIDs()`** - get IDs of all block's that currently exist in that instance. 

**`TOSDB_GetBlockCount()`** returns the number of RawDataBlocks allocated. (Should always be the same as the number of TOSDBlocks.)

Within each 'block' is a pointer to a RawDataBlock object created by an internal factory. The factory has a limit (default is 10) which can be adjusted:

 **`TOSDB_GetBlockLimit()`** 

**`TOSDB_SetBlockLimit()`**


#### Items/Topics/Streams

Once a block is created, items and topics are added. Topics are the TOS fields (e.g. LAST, VOLUME, BID ) and items are the individual symbols (e.g. IBM, GE, SPY).  The somewhat unintuitive terms 'item' and 'topic' come from DDE terminology and just stuck. Data are stored in 'streams', created internally as a result of topics and items being added. 

If we add two topics and three items we have six streams:

&nbsp;     | SPY | QQQ | GOOG
-----------|-----|-----|-----
**LAST**   |&nbsp;&nbsp;X  |  &nbsp;&nbsp;&nbsp;X  |  &nbsp;&nbsp;&nbsp;&nbsp;X
**VOLUME** |  &nbsp;&nbsp;X  |  &nbsp;&nbsp;&nbsp;X  |  &nbsp;&nbsp;&nbsp;&nbsp;X

View the prototypes in tos_databridge.h for all the different versions of the following 'Add' calls:

**`TOSDB_Add()`** 

**`TOSDB_AddTopic()`** 

**`TOSDB_AddItem()`** 

**`TOSDB_AddTopics()`** 

**`TOSDB_AddItems()`** 


**`TOSDB_RemoveItem()`**

**`TOSDB_RemoveTopic()`**

**`TOSDB_GetItemNames()`** - item strings currently in the block 

**`TOSDB_GetTopicNames()`** - topic strings currently in the block

**`TOSDB_GetTopicEnums()`** - topic enums currently in the block

**`TOSDB_GetItemCount()`** - number of items in the block. (Use for size of buffers to pass into the previous C calls.) 

**`TOSDB_GetTopicCount()`** - number of topics in block. (Use for size of buffers to pass into the previous C calls.) 

Items\[Topics\] added before any topics\[items\] exist in the block will be pre-cached, i.e they will be visible to the back-end but not to the interface until a topic\[item\] is added; likewise if all the items\[topics\] are removed(thereby leaving only topics\[items\]). See [Important Details and Provisos](README_DETAILS.md). To view the pre-cache use the following calls:

**`TOSDB_GetPreCachedItemNames()`** - item strings currently in the block's pre-cache

**`TOSDB_GetPreCachedTopicNames()`** - topic strings currently in the block's pre-cache

**`TOSDB_GetPreCachedTopicEnums()`** - topic enums currently in the block's pre-cache

**`TOSDB_GetPreCachedTopicCount()`** - number of items in the block's pre-cache. (Use for size of buffers to pass into the previous C calls.)

**`TOSDB_GetPreCachedItemCount()`** - number of topics in the block's pre-cache. (Use for size of buffers to pass into the previous C calls.)

#### Data Storage

**`TOSDB_GetBlockSize()`** - how many data-points can be stored in the individual streams

**`TOSDB_SetBlockSize()`** - change to a different size (> 1, <= TOSDB_MAX_BLOCK_SZ)

**`TOSDB_GetStreamOccupancy()`** - how many data-points are currently in a particular stream

**`TOSDB_IsUsingDateTime()`** - is block storing DateTimeStamp objects alongside primary data-points

Different streams store data as different types. Unpack the type info from inside the topic enum:

**`TOS_Topics::Type< ...topic enum... >::type`** underlying type at compile-time

**`TOSDB_GetTypeBits()`** - type bits at run-time

Check the bits with the appropriately named TOSDB_\[...\]_BIT constants in tos_databridge.h. 

    if( TOSDB_TypeBits("BID") == TOSDB_INTGR_BIT ) 
       \\ just the integer bit
       \\ data are longs (def_size_type).
    else if( TOSDB_TypeBits("BID") == TOSDB_INTGR_BIT | TOSDB_QUAD_BIT ) 
       \\ integer bit and the quad bit
       \\ data are long longs (ext_size_type)
    else if ( TOSDB_TypeBits("BID") == TOSDB_QUAD_BIT )
       \\ quad bit, no integer bit
       \\ data are doubles (ext_price_type)
    else if ( TOSDB_TypeBits("BID") == 0 )
       \\ no quad bit, no integer bit
       \\ data are floats (def_price_type)
    else if ( TOSDB_TypeBits("BID") == TOSDB_STRING_BIT )
       \\ no quad bit, no integer bit, just the string bit
       \\ data are strings
    else 
       \\ ERROR 
    
Make sure you don't simply check a bit with logical AND when what you really want is to check the entire type_bits_type value. In this example checking for the INTGR_BIT will return true for def_size_type AND ext_size_type(long and long long). 

**`TOSDB_GetTypeString()`** - (platform dependent) string of the type, for convenience.

The client library extracts data from the engine (tos-databridge-engine[].exe) through a shared memory segment. The library loops through its blocks/streams looking to see what buffers have been updated, reading the buffers if necessary. 

**`TOSDB_GetLatency()`** - wait time (in milliseconds) between reads of the shared memory buffer by the client library.

**`TOSDB_SetLatency()`** - change this value by passing a new UpdateLatency enum value. The default(Moderate, 300) should be fine for most users. 


#### Get Historical Data

The data-streams have been implemented in an effort to:

1. provide convenience by allowing both a generic type and strings to be returned.
2. not make the client pay for features that they don't want to use.(i.e. genericism or DateTimeStamps) 
3. take advantage of a type-correcting feature of the underlying data-stream.(see the comments and macros in data_stream.hpp) 
4. throw derived exceptions or return error codes depending on the context and language.
5. provide at least one very efficient, 'stream-lined' call for large data requests.

> **IMPLEMENTATION NOTE:** In order to comply with #5 we violate the abstraction of the data block for all the **`Get..`** calls and the interface of the data-stream for the non-generic versions of those same calls. For the former we allow the return of a const pointer to the stream, allowing the client-side back-end to operate directly on the stream. For the latter we provide 'copy(...)' virtual methods of the data-stream that take raw pointer(s) to be populated directly, bypassing generic_type construction and STL overhead.

Each stream is indexed thusly: 0 or (-block size) is the most recent value, -1 or (block size - 1) is the least recent. When indexing a sub-range it is important to keep in mind both values are INCLUSIVE \[ , \]. Therefore a sub-range of a data-stream of size 10 where index begin = 3(or -7) and index end = 5(or -5) will look like: \[0\]\[1\]\[2\]**\[3\]\[4\]\[5\]**\[6\]\[7\]\[8\]\[9\]. C++ calls will throw std::invalid_argument().

Generally the client has three different options:

1. figure out the type of the data-stream (see above) and use that specific call
2. use the string version (data will be cast to char* or std::string, SLOW)
3. (C++ only) use the generic version to return a custom generic type(generic.hpp)

##### Individual Data-Points

**`TOSDB_GetLong(...)`**  

**`TOSDB_GetLongLong(...)`** 

**`TOSDB_GetFloat(...)`**  

**`TOSDB_GetDouble(...)`**  

**`TOSDB_GetString(...)`** 

Templatized C++ versions (with DateTimeStamp): 

**`TOSDB_Get<long,true>(...)`**

**`TOSDB_Get<long long,true>(...)`**

**`TOSDB_Get<float,true>(...)`**

**`TOSDB_Get<double,true>(...)`**

**`TOSDB_Get<std::string,true >(...)`**

Templatized C++ versions (without DateTimeStamp): 

**`TOSDB_Get<long,false>(...)`**

**`TOSDB_Get<long long,false>(...)`**

**`TOSDB_Get<float,false>(...)`**

**`TOSDB_Get<double,false>(...)`**

**`TOSDB_Get<std::string,false>(...)`**

Or avoid determining type and get a custom generic type:

**`TOSDB_Get<generic_type,true>(...)`**

**`TOSDB_Get<generic_type,false>(...)`**

The C++ version's second template argument is a boolean indicating whether it should return DateTimeStamp values as well(assuming the block is set for that) while the C version accepts a pointer to a DateTimeStamp struct(pDateTimeStamp) that will be populated with the value (pass a NULL value otherwise).

##### Multiple Contiguous Data-Points

**`TOSDB_GetStreamSnapshotLongs(...)`**  

**`TOSDB_GetStreamSnapshotLongLongs(...)`** 

**`TOSDB_GetStreamSnapshotFloats(...)`**  

**`TOSDB_GetStreamSnapshotDoubles(...)`**  

**`TOSDB_GetStreamSnapshotStrings(...)`** 

Templatized C++ versions (with DateTimeStamp): 

**`TOSDB_GetStreamSnapshot<long,true>(...)`**

**`TOSDB_GetStreamSnapshot<long long,true>(...)`**

**`TOSDB_GetStreamSnapshot<float,true>(...)`**

**`TOSDB_GetStreamSnapshot<double,true>(...)`**

**`TOSDB_GetStreamSnapshot<std::string,true >(...)`**

Templatized C++ versions (without DateTimeStamp): 

**`TOSDB_GetStreamSnapshot<long,false>(...)`**

**`TOSDB_GetStreamSnapshot<long long,false>(...)`**

**`TOSDB_GetStreamSnapshot<float,false>(...)`**

**`TOSDB_GetStreamSnapshot<double,false>(...)`**

**`TOSDB_GetStreamSnapshot<std::string,false>(...)`**

Or avoid determining type and get a custom generic type:

**`TOSDB_GetStreamSnapshot<generic_type,true>(...)`**

**`TOSDB_GetStreamSnapshot<generic_type,false>(...)`**

Similar to the single data-point 'Get' calls except they return containers(C++) or populate arrays(C) and require a beginning and ending index value. The C calls require you to state the explicit dimensions of the arrays(the string version requires length of the string buffers as well; internally, data moved to string buffers is of maximum size TOSDB_STR_DATA_SZ so no need to allocate larger than that). 

DateTimeStamp is dealt with in the same way as above. If NULL is not passed it's array length is presumed to be the same as the other array so make sure you pay attention to what you allocate and pass. The C++ calls are implemented to return either a vector of different types or a pair of vectors(the second a vector of DateTimeStamp), depending on the boolean template argument. 

**Please review the function prototypes in tos_databridge.h, and the [Glossary section](README_DETAILS.md#source-glossary), for a better understanding of the options available.**

Internally the data-stream tries to limit what is copied by keeping track of the streams occupancy and only returning valid data. C++ calls
will therefore return a dynamically sized containter that may be smaller than what you expected. C calls may leave 'tail' elements of the passed array undefined if there is not valid data to fill them.

Not suprisingly, when n is very large the the non-string **`TOSDB_GetStreamSnapshot[Type]s`** C calls are the fastest, with the non-generic, non-string **`TOSDB_GetStreamSnapshot<Type,false>`** C++ calls just behind. 


##### Multiple Contiguous Data-Points From an 'Atomic' Marker

**`TOSDB_GetStreamSnapshotLongsFromMarker(...)`**  

**`TOSDB_GetStreamSnapshotLongLongsFromMarker(...)`** 

**`TOSDB_GetStreamSnapshotFloatsFromMarker(...)`**  

**`TOSDB_GetStreamSnapshotDoublesFromMarker(...)`**  

**`TOSDB_GetStreamSnapshotStringsFromMarker(...)`** 

It's likely the stream will grow between consecutive calls. These calls (C only) guarantee to pick up where the last **`TOSDB_Get...`**, **`TOSDB_GetStreamSnanpshot...`**, or **`TOSDB_GetStreamSnapshotFromMarker...`** call ended (under a few assumptions).  Internally the stream maintains a 'marker' that tracks the position of the last value pulled; the act of retreiving data and moving the marker can be thought of as a single, 'atomic' operation. The \*get_size arg will return the size of the data copied, it's up to the caller to supply a large enough buffer. A negative value indicates the buffer was to small to fit all the data, or the stream is 'dirty' . 

> **'Dirty Stream':** indicates the marker has hit the back of the stream and data between the beginning of the last call and the end of the next will be dropped. To avoid this be sure you use a big enough stream and/or keep the marker moving foward (by using calls mentioned above).

**`TOSDB_IsMarkerDirty()`** - is the stream dirty; there is no guarantee that a 'clean' stream will not become dirty before the next call.

**`TOSDB_GetMarkerPosition()`** - returns the current position (index) of the marker in the stream.


#### Get Most Recent Data as a Frame

A frame is the most-recent data for ALL topics, items, or both. 

All the most-recent item values for a particular topic (i.e index=0):

**`TOSDB_GetItemFrameLongs(...)`**

**`TOSDB_GetItemFrameLongLongs(...)`**

**`TOSDB_GetItemFrameFloats(...)`**

**`TOSDB_GetItemFrameDoubles(...)`**

**`TOSDB_GetItemFrameStrings(...)`**

C++ version returning mapping of item names to pairs of data and DateTimeStamps:
**`TOSDB_GetItemFrame<true>()`**

C++ version returning mapping of item names to data (no DateTimeStamps):
**`TOSDB_GetItemFrame<false>()`**

The C calls require pointers to arrays of appropriate type, with the dimensions of the arrays. The value array is required; the second array of strings that is populated with the corresponding labels(item strings in this case) and the third array of DateTimeStamp object are both optional. Remember, where specified the arrays require dimensions to be passed; if not they are presumed be the same as the primary value array('dest').

All the most-recent topic values for a particular item (i.e index=0):

**`TOSDB_GetTopicFrameStrings()`**

C++ version returning mapping of topic enums to pairs of data and DateTimeStamps:
**`TOSDB_GetTopicFrame<true>()`**

C++ version returning mapping of topic enums to data (no DateTimeStamps):
**`TOSDB_GetTopicFrame<false>()`**

Note there is only the one C call, populating an array of c-strings. Since each item can have multiple topic values, and each topic value may be of a different type, it's necessary to return strings(C) or generic_types(C++).

All the most-recent topic AND item values for the block (i.e index=0):

C++ version returning mapping of item names to mappings of topic enums to pairs of data and DateTimeStamps:

**`TOSDB_GetTotalFrame<>(...)`** 

C++ version returning mapping of item names to mappings of topic enums to data (no DateTimeStamps):

**`TOSDB_GetTotalFrame<>(...)`** 

Because of the complexity of the the matrix, with mapped strings, and possible DateTimeStamp structs included there is only a C++ version. C code will have to iterate through the items or topics and call the appropriate 'Frame' call to get the total frame.


#### Logging, Exceptions & Stream Overloads

The library exports some logging functions that dovetail with its use of custom exception classes. Most of the modules use these logging functions internally. The files are sent to appropriately named .log files in /log. Client code is sent to /log/client-log.log by using the following calls:  

**`TOSDB_LogH()`** - log high priority message.

**`TOSDB_Log()`** - log low priority message.

**`TOSDB_LogEx()`** - pass additional argument for an error code like one returned from *GetLastError()*. 

The library also defines an exception hierarchy (exceptions.hpp) derived from TOSDB_Error and std::exception. C++ code can catch them and respond accordingly; the base class provides threadID(), processID(), tag(), and info() methods as well as the inherited what() from std::exception. 

There are operator\<\< overloads (client_out.cpp) for most of the custom objects and containers returned by the **`TOSDB_Get...`** calls.

