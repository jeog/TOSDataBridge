### Important Details & Provisos
- - -
- **Clean.. Exit.. Close.. Stop..:** Assuming everything is up and running you really want to avoid shutting down the TOS platform and/or the Service while your client code is running. As a general rule follow this order: 

    1. Clean up: close blocks, call clean_up etc. 
    2. Exit your program
    3. Stop the Service (optional)
    4. Close TOS platform

    Once you've done (i) the remaining order is less important. 

- **DDE Data:** It's important to realize that we (us, you, this code, and yours) are at the mercy of the TOS platform, the DDE technology, and TOS's implementation of it. You may notice the streams of data will not match the Time & Sales perfectly.  If you take a step back and aggregate the data in more usable forms this really shouldn't be an issue.  Another way we are at the mercy of the DDE server is that fields like last price and last size are two different topics. That means they are changing at slightly different times with slightly different time-stamps even if they are the correct pairing. To get around this we can write code like this to simulate a live-stream :
    ```
    >>> block = tosdb.TOSDB_DataBlock()
    >>> block.add_items('GOOG')
    >>> block.add_topics('LAST','VOLUME')
    >>> vol = 0
    >>>
    >>> print("price".ljust(10), "size") 
    >>> while(run_flag):
    ...     v = block.get('GOOG','VOLUME')
    ...     p = block.get('GOOG','LAST')
    ...     # if total volume has changed, output the difference
    ...     if(v != vol): 
    ...         print(str(p).ljust(10), str(v - vol))
    ...     vol = v
    ...     # the less we sleep the more accurate the stream
    ...     time.sleep(.1)
    ```

- **Numerical Values for 'Non-Decimal' Instruments:** The TOS DDE server doesn't handle numerical values for 'non-decimal' instruments well. For instance, trying to get a bid/ask/last etc. for a 10-yr note future (/zn) will not return the fractional part of the price. In these cases use the topic versions suffixed with an x (bidx/askx/lastx etc.) which will provide a string you'll need to parse.

- **Case-Sensitivity:** Case Sensitivity is a minor issues with how Item values are handled. The underlying C/C++ library are case-sensitive; it's up to the client to make sure they are passing case consistent item strings. The Python wrapper is case-insensitive; on receiving item and topic strings they are converted to upper-case by default.

- **Closing Large Blocks:** Currently Closing/Destroying large blocks(1,000,000+ combined data-stream elements) involves a large number of internal deallocations/destructions and becomes quite CPU intensive. The process is spun-off into its own thread but this may fail, returning to the main thread when the library is being freed, or block the python interpreter regardless of when or how it's called. Use caution when creating/closing large blocks.

- **Block Size and Memory:** As mentioned you need to use some sense when creating blocks. As a simple example: let's say you want LAST,BID,ASK for 100 symbols. If you were to create a block of size 1,000,000, WITHOUT DateTime, you would need to allocate over 2.4 GB of memory - not good. As a general rule keep data-streams of similar desired size in the same block, and use new blocks as necessary. In our example if you only need 1,000,000 LAST elems for 10 items and 100 for everything else create 2 blocks: 1) a block of size 1,000,000 with those 10 items and the topic LAST; 2) a block of size 100 with all 100 items and the three topics. Then you would only need a little over 80 MB. (Really you could create three blocks to avoid any overlap but in this case its only a minor performance and space improvement and not worth worth the inconvenience.)

- **Inter-Process Communication(IPC):** Uses a pair of master/slave objects that share two duplexed named pipes and a shared memory segment.  Internally the client library continually calls the masters ->connected() method which is built to fail easily, for any number of reasons.

- **Asymmetric Responsibilities & Leaks:** Connection 'probing' only works one way, from master to slave. The slave(which is owned by the Service) therefore may know that one of the clients isn't there and handle a disconnection, it just doesn't know what stream objects they are responsible for. Internally all it does is keep a ref-count to the streams it's been asked to create and obviously write the necessary data into the appropriate shared memory segments. To see the status of the service and if there are stranded or dangling streams open up the command shell and use **`TOSDB_DumpSharedBufferStatus`** to dump all the current stream information to /log . 

- **DateTimeStamp:** THESE ARE NOT OFFICIAL STAMPS FROM THE EXCHANGE, they are manually created once the TOS DDE server returns the data. They use the system clock to assure high_resolution( the micro-seconds field) and therefore there is no guarantee that the clock is accurate or won't change between stamps, as is made by the STL's std::steady_clock. 

- **Bad Items & Topics:** The implementation can easily handle bad topics passed to a call since it has a large mapping of the allowed topic strings mapped to enum values for TOS_Topics. If a passed string is mapped to a NULL_TOPIC then it can be rejected, or even if it is passed it won't get a positive ACK from the server and should be rejected. Bad item strings on the other hand are a bit of a problem. The DDE server is supposed to respond with a negative ACK if the item is invalid but TOS responds with a positive ACK and a 'N/A' string. **Currently it's up to the user to deal with passing bad Items.** 

- **Pre-Caching:** As mentioned the block requires at least one valid topic AND item, otherwise it can't hold a data-stream. Because of this, if only items(topics) are added they are held in a pre-cache until a valid topic(item) is added. Likewise, if all topics(items) are removed, the remaining items(topics) are moved into the pre-cache. This has two important consequences: 1) pre-cached entries are assumed to be valid until they come out and are sent to the engine and 2) when using the API calls or python methods to see the items/topics currently in the 'block' you WILL NOT see pre-cached items. Use the appropriate API calls or python methods to see what's in the pre-cache.

- **SendMessage vs. SendMessageTimeout:** To initiate a topic with the TOS server we should send out a broadcast message via the SendMessage() system call. This call is built to block to insure the client has had a chance to deal with the ACK message. For some reason, it's deadlocking, so we've been forced to use SendMessageTimeout() with an arbitrary 500 millisecond timeout. Therefore, until this gets fixed adding topics will introduce an amount of latency in milliseconds = 500 x # of topics.


### Source Glossary 
- - -
Variable                 | Description
------------------------ | -------------
topics                   | strings or TOS_Topics::TOPICS enums(recommended) of fields to be added (e.g. BID, ASK, LAST )
items                    | strings of symbols to be added (e.g. GE, SPY, GOOG )
data-stream              | the historical data for each item-topic entry (see data_stream.hpp)
frame                    | all the items, topics, or matrix of both, for a particular index( only index of 0 is currently implemented )
TOS_Topics               | a specialization that provides a scoped enum containing all the topics( TOS data fields), a mapping to the relevant strings, and some utilities. This is the recommended way for C++ calling code to pass topics. Inside the high-order bits of the enum value are the type bit constants TPC..[ ] which can be checked directly or via one of the aptly named public utilities
DateTimeStamp            | struct that wraps the C library tm struct, and adds a micro-second field info on the buffer to the client
ILSet<>                  | wrapper around std::set<> type that provides additional means of construction / copy / move / assignment
UpdateLatency            | Enum of milliseconds values the client library waits before re-checking buffers
BufferHead               | struct that sits in front of buffers, holding state info
generic_type             | custom generic type (generic.hpp/cpp)
generic_dts_type         | pair of generic and DateTimeStamp
generic_vector_type      | vector of generics
dts_vector_type          | vector of DateTimeStamps
generic_dts_vectors_type | pair of : ( vector of generics, vector of DateTimeStamps )
generic_map_type         | mapping of item/topic strings and generics
generic_matrix_type      | mapping of item/topic strings and generic_map_type
generic_dts_map_type     | mapping of item/topic strings and generic_dts_type
generic_dts_matrix_type  | mapping of item/topic strings and generic_dts_map_type
str_set_type             | instantiation of ILSet<> for std::string
topic_set_type           | instantiation of ILSet<> for TOS_Topics::TOPICS
def_size_type            | default size type for data (long)
ext_size_type            | extend size type for data (long long)
def_price_type           | default price type for data (float)
ext_price_type           | extended price type for data (double)
size_type                | explicit size type for Python Wrapper (uint32_t)
type_bits_type           | bits set in an unsigned char to indicate the underlying type of a TOS_Topics::TOPICS (uint8_t)
TOSDB_INTGR_BIT          | type bits for an integral data type
TOSDB_QUAD_BIT           | type bits for an 8-byte data type
TOSDB_STRING_BIT         | type bits for a string data type
TOSDB_TOPIC_BITMASK      | logical OR of all the type bits
TOSDB_MAX_STR_SZ         | maximum string size for client input (items, topics etc) among other strings
TOSDB_STR_DATA_SZ        | maximum size of char buffer pulled from DDE server and pushed into a data-stream of std::strings (everything larger is truncated)
TOSDB_BLOCK_ID_SZ        | maximum string size for the name/ID of a TOSDBlock
TOSDB_DEF_LATENCY        | the default UpdateLatency (Moderate= 300 milleseconds) 
TOSDB_DEF_TIMEOUT        | default timeout for internal wait/block operations
TOSDB_MIN_TIMEOUT        | minimum timeout user can pass for internal wait/block operations (reverts to TOSDB_DEF_TIMEOUT)
TOSDB_SHEM_BUF_SZ        | byte size of the shared memory circular buffers
LOCAL_LOG_PATH           | directory relative to (our) root where log/dump files *can* go
TOSDB_LOG_PATH           | the actual path of where log/dump files *will* go (depends on compile-time options)

