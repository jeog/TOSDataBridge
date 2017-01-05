### C/C++ API
- - -

#### Administrative Calls


Once the Service is running start by calling **`TOSDB_Connect()`** which will return 0 if successful. Call the Library function **`TOSDB_IsConnected()`** which returns 1 if you are 'connected' to the TOSDataBridge service.

> **IMPORTANT:** 'Connected' only means there is a connection between the client/library and the engine/service, NOT that the engine/service can communicate with the TOS platform (or TOS is retrieving data from its server). If, for instance, TOS is not running or it's running with elevated privileges(and you didn't pass 'admin' to the setup script) you may be 'connected' but not able to communicate with the TOS platform. 

> **IMPLEMENTATION NOTE:** Be careful: **`TOSDB_IsConnected()`** returns an unsigned int that represents a boolean value; most of the other C admin calls return a signed int to indicate error(non-0) or success(0). Boolean values will be represented by unsigned int return values for C and bool values for C++. 


Generally **`TOSDB_Disconnect()`** is unnecessary as it's called automatically when the library is unloaded.

> **NOTABLE CONVENTIONS:** The C calls, except in a few cases, don't return values but populate variables, arrays/buffers, and arrays of pointers to char buffers. The 'dest' argument is for primary data; its a pointer to a scalar variable, or an array/buffer when followed by argument 'arr_len' for the number of array/buffer elements. 

> The String versions of the calls take a char\*\* argument, followed by arr_len for the number of char\*s, and str_len for the size of the buffer each char\* points to (obviously they should all be >= to this value). 

> If the call requires more than one array/buffer besides 'dest' (the Get...Frame... calls for instance) it assumes an array length equal to that of 'arr_len'. If it is of type char\*\* you need to specify a char buffer length just as you do for the initial char\*\*.


TOSDB's main organizational unit is the 'block'(struct TOSDBlock in client.hpp): it's important functionally and conceptually. The first thing client code does is call **`TOSDB_CreateBlock()`** passing it a unique ID(<= TOSDB_BLOCK_ID_SZ) that will be used to access it throughout its lifetime, a size(how much historical data is saved in the block's data-streams), a flag indicating whether it saves DateTime in the stream, and a timeout value in milliseconds used for its internal waiting/synchronization(see TOSDB_DEF_TIMEOUT, TOSDB_MIN_TIMEOUT). 

When you no longer need the data in the block **`TOSDB_CloseBlock()`** should be called to deallocate its internal resources or **`TOSDB_CloseBlocks()`** to close all that currently exist. If you lose track of what's been created use the C or C++ version of **`TOSDB_GetBlockIDs()`** . Technically **`TOSDB_GetBlockCount()`** returns the number of RawDataBlocks allocated(see below), but this should always be the same as the number of TOSDBlocks.

Within each 'block' is a pointer to a RawDataBlock object which relies on an internal factory to return a constant pointer to a RawDataBlock object. Internally the factory has a limit ( the default is 10 ) which can be adjusted with the appropriately named admin calls **`TOSDB_GetBlockLimit()`** **`TOSDB_SetBlockLimit()`**

Once a block is created, items and topics are added. Topics are the TOS fields (e.g. LAST, VOLUME, BID ) and items are the individual symbols (e.g. IBM, GE, SPY). 

> **NOTABLE CONVENTIONS:** The somewhat unintuitive terms 'item' and 'topic' come from DDE terminology that just stuck - for a number of reasons.

**`TOSDB_Add()`** **`TOSDB_AddTopic()`** **`TOSDB_AddItem()`** **`TOSDB_AddTopics()`** **`TOSDB_AddItems()`** There are a number of different versions for C and C++, taking C-Strings(const char\*), arrays of C-Strings(const char\*\*), string objects(std::string), TOS_Topics::TOPICS enums, and/or specialized sets (str_set_type, topic_set_type) of the latter two. Check the prototypes in tos_databridge.h for all the versions and arguments.

To find out the the items / topics currently in the block call the C or C++ versions of **`TOSDB_GetItemNames()`** **`TOSDB_GetTopicNames()`** **`TOSDB_GetTopicEnums()`**; use **`TOSDB_GetItemCount()`** **`TOSDB_GetTopicCount()`** for their respective sizes. (Use these to determine the size of the buffers to pass into the C calls.) 

To remove individual items **`TOSDB_RemoveItem()`**, and topics **`TOSDB_RemoveTopic()`**.

> **IMPORTANT:** Items\[Topics\] added before any topics\[items\] exist in the block will be pre-cached, i.e they will be visible to the back-end but not to the interface until a topic\[item\] is added; likewise if all the items\[topics\] are removed(thereby leaving only topics\[items\]). See [Important Details and Provisos](README_DETAILS.md). To view the pre-cache use the C or C++ versions of **`TOSDB_GetPreCachedTopicNames()`** **`TOSDB_GetPreCachedItemNames()`** **`TOSDB_GetPreCachedTopicEnums()`**; use **`TOSDB_GetPreCachedTopicCount()`** **`TOSDB_GetPreCachedItemCount()`** for their respective sizes. (Use these to determine the size of the buffers to pass into the C calls.)

As mentioned, the size of the block represents how large the data-streams are, i.e. how much historical data is saved for each item-topic. Each entry in the block has the same size; if you prefer different sizes create a new block. Call **`TOSDB_GetBlockSize()`** to get the size and **`TOSDB_SetBlockSize()`** to change it.

> **IMPLEMENTATION NOTE:** The use of the term size may be misleading when getting into implementation details. This is the size from the block's perspective and the bound from the data-stream's perspective. For all intents and purposes the client can think of size as the maximum number of elements that can be in the block and the maximum range that can be indexed. To get the occupancy (how much valid data has come into the stream) call **`TOSDB_GetStreamOccupancy()`** .

To find out if the block is saving DateTime call the C or C++ versions of **`TOSDB_IsUsingDateTime()`**.

Because the data-engine behind the blocks handles a number of types it's necessary to pack the type info inside the topic enum. Get the type bits at compile-time with **`TOS_Topics::Type< ...topic enum... >::type`** or at run-time with **`TOSDB_GetTypeBits()`** , checking the bits with the appropriately named TOSDB_\[...\]_BIT constants in tos_databridge.h. 

    if( TOSDB_TypeBits("BID") == TOSDB_INTGR_BIT ) 
       \\ data is a long (def_size_type).
    else if( TOSDB_TypeBits("BID") == TOSDB_INTGR_BIT | TOSDB_QUAD_BIT ) 
       \\ data is a long long (ext_size_type)
    
Make sure you don't simply check a bit with logical AND when what you really want is to check the entire type_bits_type value. In this example checking for the INTGR_BIT will return true for def_size_type AND ext_size_type. **`TOSDB_GetTypeString()`** provides a string of the type for convenience.

> **IMPLEMENTATION NOTE:** The client-side library extracts data from the Service (tos-databridge-engine[].exe) through a series of protected kernel objects in the global namespace, namely a read-only shared memory segment and a mutex. The Service receives data messages from the TOS DDE server and immediately locks the mutex, writes them into the shared memory buffer, and unlocks the mutex. At the same time the library loops through its blocks and item-topic pairs looking to see what buffers have been updated, acquiring the mutex, and reading the buffers, if necessary. 

>The speed at which the looping occurs depends on the UpdateLatency enum value set in the library. The lower the value, the less it waits, the faster the updates. **`TOSDB_GetLatency()`** and **`TOSDB_SetLatency()`** are the relevant calls. A value of Fastest(0) allows for the quickest refreshes, but can chew up clock cycles - view the relevant CPU% in process explorer or task manager to see for yourself. The default(Fast, 30) or Moderate(300) should be fine for most users. 


#### Get Calls

Once you've created a block with valid items and topics you'll want to extract the data that are collected. This is done via the non-administrative **`TOSDB_Get...`** calls.

The two basic techniques are pulling data as: 

1. ***a segment:*** some portion of the historical data of the block for a particular item-topic entry. A block with 3 topics and 4 items has 12 different data-streams each of the size passed to **`TOSDB_CreateBlock(...)`**. The data-stream is indexed thusly: 0 or (-block size) is the most recent value, -1 or (block size - 1) is the least recent. 

    > **IMPORTANT:** When indexing a sub-range it is important to keep in mind both values are INCLUSIVE \[ , \]. Therefore a sub-range of a data-stream of size 10 where index begin = 3(or -7) and index end = 5(or -5) will look like: \[0\]\[1\]\[2\]**\[3\]\[4\]\[5\]**\[6\]\[7\]\[8\]\[9\]. Be sure to keep this in mind when passing index values as the C++ versions will throw std::invalid_argument() if you pass an ending index of 100 to a stream of size 100.

2. ***a frame:*** spans ALL the topics, items, or both. Think of all the data-streams as the 3rd dimension of 2-dimensional frames. In theory there can be a frame for each index - a frame of all the most recent values or of all the oldest, for instance - but in practice we've only implemented the retrieval of the most recent frame because of how the data are currently structured.


##### Segment Calls

**`TOSDB_Get< , >(...)`** and **`TOSDB_Get[Type](...)`** are simple ways to get a single data-point(think a segment of size 1); the former is a templatized C++ version, the latter a C version with the required type stated explicitly in the call (e.g. **`TOSDB_GetDouble(...)`** ). The C++ version's first template arg is the value type to return. 

> **IMPORTANT:** 

> Generally the client has three options for which specific C++ call to use:

> 1. figure out the type of the data-stream (the type bits are packed in the topic enum, see above) and pass it as the first template arg
> 2. pass in generic_type to receive a custom generic type that knows its own type, can be cast to the relevant type, or can have its as_string() method called
> 3. use the specialized std::string version

> Generally the client has two options for which specific C call to use:

> 1. figure out the type(as above) and make the appropriately named call
> 2. call the string version, passing in a char\* to be populated with the data ( \< TOSDB_STR_DATA_SZ ). 

> **Obviously the generic and string versions come with a cost/overhead. 


The C++ version's second template argument is a boolean indicating whether it should return DateTimeStamp values as well(assuming the block is set for that) while the C version accepts a pointer to a DateTimeStamp struct(pDateTimeStamp) that will be populated with the value (pass a NULL value otherwise).

In most cases you'll want more than a single value: use the **`TOSDB_GetStreamSnapshot< , >(...)`** and **`TOSDB_GetStreamSnapshot[Type]s(...)`** calls. The concept is similar to the **`TOSDB_Get...`** calls from above except they return containers(C++) or populate arrays(C) and require a beginning and ending index value. The C calls require you to state the explicit dimensions of the arrays(the string version requires length of the string buffers as well; internally, data moved to string buffers is of maximum size TOSDB_STR_DATA_SZ so no need to allocate larger than that). 

DateTimeStamp is dealt with in the same way as above. If NULL is not passed it's array length is presumed to be the same as the other array so make sure you pay attention to what you allocate and pass. The C++ calls are implemented to return either a vector of different types or a pair of vectors(the second a vector of DateTimeStamp), depending on the boolean template argument. **Please review the function prototypes in tos_databridge.h, and the [Glossary section](README_DETAILS.md#source-glossary), for a better understanding of the options available.**

> **IMPLEMENTATION NOTE:** Internally the data-stream tries to limit what is copied by keeping track of the streams occupancy and finding the *MIN(occupancy count, difference between the end and begin indexes +1\[since they're inclusive\], size of parameter passed in)*. 

> For C++ calls that return a container it's possible you may want the sub-stream from index 5 to 50, for instance, but if only 10 values have been pushed into the stream it will return a container with values only from index 5 to 9. 

> NOTE: If you pass an array to one of the C calls the data-stream will NOT copy/initialize the 'tail' elements of the array that do not correspond to valid indexes in the data-stream and the value of those elements should be assumed undefined.


It's likely the stream will grow between consecutive calls. The **`TOSDB_GetStreamSnapshot[Type]sFromMarker(...)`** calls (C only) guarantee to pick up where the last **`TOSDB_Get...`**, **`TOSDB_GetStreamSnanpshot...`**, or **`TOSDB_GetStreamSnapshotFromMarker...`** call ended (under a few assumptions).  Internally the stream maintains a 'marker' that tracks the position of the last value pulled; the act of retreiving data and moving the marker can be thought of as a single, 'atomic' operation. The \*get_size arg will return the size of the data copied, it's up to the caller to supply a large enough buffer. A negative value indicates the buffer was to small to fit all the data, or the stream is 'dirty' . 

> **'Dirty Stream':** indicates the marker has hit the back of the stream and data between the beginning of the last call and the end of the next will be dropped. To avoid this be sure you use a big enough stream and/or keep the marker moving foward (by using calls mentioned above). To determine if the stream is 'dirty' use the **`TOSDB_IsMarkerDirty()`** call. There is no guarantee that a 'clean' stream will not become dirty between the call to **`TOSDB_IsMarkerDirty`** and the retrieval of stream data, although you can look for a negative \*get_size value to indicate this rare state has occured.


##### Frame Calls

The three main 'frame' calls are **`GetItemFrame`**, **`GetTopicFrame`**, **`GetTotalFrame`**( C++ only ).

**`TOSDB_GetItemFrame<>()`** and **`TOSDB_GetItemFrame[Type]s(...)`** are used to retrieve the most recent values for all the items of a particular topic. Think of it as finding the specific topic on its axis, then pulling all the values along that row(for each item). Because the size of n is limited the C++ calls only return a generic type, whereas the C calls necessarily require the explicitly named call to be made(or the (C\-)String version). The C++ calls take one boolean template arg as above. 
> **IMPORTANT:** The frames are only dealing with the the 'front' of the block, i.e. index=0 of all the data-streams. 

One major difference between the frame calls and the data-stream calls is the former map their values to strings of relevant topic or item names. For instance, a call of **`GetItemFrame<true>(..., BID)`** for a block with topics: BID, ASK and items: IBM, GE, CAT will return `( "IBM", (val,dts); "GE", (val,dts); "CAT", (val,dts) )`; in this case a mapping of item strings to pairs of generic_types and DateTimeStamps. 

The C calls require pointers to arrays of appropriate type, with the dimensions of the arrays. The value array is required; the second array of strings that is populated with the corresponding labels(item strings in this case) and the third array of DateTimeStamp object are both optional. Remember, where specified the arrays require dimensions to be passed; if not they are presumed be the same as the primary value array('dest').

**`TOSDB_GetTopicFrame<>()`** and **`TOSDB_GetTopicFrameStrings()`** are used to retrieve the most recent values for all the topics of a particular item. Think of it as finding the specific item on its axis, then pulling all the values along that row. They are similar to the **`GetItemFrame...`** calls from above except for the obvious fact they pull topic values and there is only the one C call, populating an array of c-strings. Since each item can have multiple topic values, and each topic value may be of a different type, it's necessary to return strings(C) or generic_types(C++).

**`TOSDB_GetTotalFrame<>(...)`** is the last type of frame call that returns the total frame(the recent values for ALL items AND topics) as a matrix, with the labels mapped to values and DateTimeStamps if true is passed as the template argument. Because of the complexity of the the matrix, with mapped strings, and possible DateTimeStamp structs included there is only a C++ version. C code will have to iterate through the items or topics and call **`GetTopicFrame(item)`** or **`GetItemFrame(topic)`**, respectively, like the Python Wrapper does.

> **IMPLEMENTATION NOTE:** The data-streams have been implemented in an effort to:

> 1. provide convenience by allowing both a generic type and strings to be returned.
> 2. not make the client pay for features that they don't want to use.(i.e. genericism or DateTimeStamps) 
> 3. take advantage of a type-correcting feature of the underlying data-stream.(see the comments and macros in data_stream.hpp) 
> 4. throw derived exceptions or return error codes depending on the context and language.
> 5. provide at least one very efficient, 'stream-lined' call for large data requests.

> In order to comply with #5 we violate the abstraction of the data block for all the **`Get..`** calls and the interface of the data-stream for the non-generic versions of those same calls. For the former we allow the return of a const pointer to the stream, allowing the client-side back-end to operate directly on the stream. For the latter we provide 'copy(...)' virtual methods of the data-stream that take raw pointer(s) to be populated directly, bypassing generic_type construction and STL overhead.

> Not suprisingly, when n is very large the the non-string **`TOSDB_GetStreamSnapshot[Type]s`** C calls are the fastest, with the non-generic, non-string **`TOSDB_GetStreamSnapshot<Type,false>`** C++ calls just behind. 


#### Logging, Exceptions & Stream Overloads

The library exports some logging functions that dovetail with its use of custom exception classes. Most of the modules use these logging functions internally. The files are sent to appropriately named .log files in /log. Client code is sent to /log/client-log.log by using the following calls:  **`TOSDB_LogH()`** and **`TOSDB_Log()`** will log high and low priority messages, respectively. Pass two strings: a tag that provides a short general category of what's being logged and a detailed description. **`TOSDB_LogEx()`** has an additional argument generally used for an error code like one returned from *GetLastError()*. 

The library also defines an exception hierarchy (exceptions.hpp) derived from TOSDB_Error and std::exception. C++ code can catch them and respond accordingly; the base class provides threadID(), processID(), tag(), and info() methods as well as the inherited what() from std::exception. 

There are operator\<\< overloads (client_out.cpp) for most of the custom objects and containers returned by the **`TOSDB_Get...`** calls.

