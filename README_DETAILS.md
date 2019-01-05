
TOSDataBridge consists of three main components: 
- TOS Platform
- backend Service
- frontend Library  

![](./res/tosdb_diagram2.png)

Once the Service and TOS Platform are running, the user loads the library and uses the API to communicate with the Service. 
- - -

#### The Service

The Service is a background process designed to act as a bridge between the TOS platfrom and (multiple) instances of client code. Upon request the service will continually pull data from the Platform, putting it into circular buffers in shared memory. This modular approach allows: 

- different user-space programs to share raw data, avoiding redundancy 
- most of the code to run with lower privileges
- those interested the ability to write their own interfaces/bindings
- easier debugging
- the service to stay running and/or start automatically on system startup

##### Service Commands:

A Window's Service is managed differently than a typical program. It can be controlled via 'services.msc' or the command line:

- ```(Admin) C:\> SC start TOSDataBridge``` 
- ```(Admin) C:\> SC query TOSDataBridge``` - Display current status of the service.
- ```(Admin) C:\> SC stop TOSDataBridge``` - Stop the service. All the data collected so far will still exist but the engine will sever its connection to TOS and exit.
- ```(Admin) C:\> SC pause TOSDataBridge``` - Pause the service. All the data collected so far will still exist but the engine will stop recording new data in the buffers. **Pausing the service is not recommended.**
- ```(Admin) C:\> SC continue TOSDataBridge``` - Continue a paused service. All the data collected so far will still exist, the engine will start recording new data into the buffers, but it will have missed any new data while paused.
- ```(Admin) C:\> SC config TOSDataBridge ...``` - Adjust the service's configuration/properties.
- ```        C:\> SC /?``` - Display help for the SC command.


##### The Engine

Once started the service spawns a child process(tos-databridge-engine) with lower/restricted privileges that does all the leg work. Occassionally(debuging, for instance) it can be useful to run the engine directly by calling the service binary with the --noservice switch. The engine binary will then enter a 'detached' state, **requiring the user to manually kill the proces when done.**

    Example 1: (Admin) C:\>TOSDataBridge\bin\Debug\x64\> tos-databridge-serv-x64_d.exe --noservice
    Example 2: (Admin) C:\>TOSDataBridge\bin\Release\Win32\> tos-databridge-serv-x86.exe --noservice --admin   

The engine creates a number of kernel objects(mutexs, shared memory segments etc.) that require certain privileges. These privileges are set in SpawnRestrictedProcess() in service.cpp. If you attempt to run the engine binary directly, as a standard user, the creation of these objects will fail, resulting in a fatal uncaught exception. (See the comments near the top of tos_databridge.h, where NO_KGBLNS is defined, for more details.) **Running the engine directly is not recommended.** 
- - -

#### The Library

The [Library](README_API.md) is a DLL used to communicate with the service. It continually copies the data in shared memory into 'local' streams, which can be returned to the user through the appropriate API call(s).  If using the C/C++ API the library is linked and loaded directly. If using [Python](README_PYTHON.md) or [Java](README_JAVA.md) you manually load the library via the init(...) calls.
- - -

#### Exiting Gracefully 

You want to avoid shutting down the TOS platform and/or the Service while your code is running. If you close TOS without cleaning up from the client side you can corrupt the underlying objects the Service uses to communicate. If you close your program without cleaning up you can cause a resource leak in the Service.

As a general rule follow this order:
1. Clean up: close blocks, call clean_up etc. 
2. Exit your program (optional)
3. Close TOS platform (optional)
4. Stop the Service (optional)
- - -

#### Objects

TOSDataBridge uses a mostly object-oriented approach (in concept and in code) to store, manage, and return data to the user. The main object is the ***block***. A user adds ***items*** and ***topics*** to the block for the data they want. Each item-topic pairing represents a ***stream*** that is managed automatically by the block.

An ***item*** is a string representing the individual instrument(e.g 'SPY', '/ZC:XCBT', '.SPY180119C250', '$DJT', 'EUR/USD').

> The underlying C/C++ API is **case-sensitive**; it's up to the client to make sure they are passing connsistenly **upper-case** item strings. The Python and Java APIs are **case-insensitive**; on receiving item and topic strings they are converted to upper-case by default.

A ***topic*** is a string and/or enum (depending on the language) representing a data field(e.g 'LAST', 'VOLUME', 'BID_SIZE'). The complete list of topics can be found in src/topics.cpp.

> The implementation can easily handle bad topics passed to it, returning an error code(C) or throwing an exception(C++). Bad item strings on the other hand can be a problem. The DDE server is *supposed* to respond with a negative ACK if the item is invalid but TOS responds with a positive ACK, inserting 'N/A' strings into the stream. **Currently it's up to the user to deal with passing bad Items.** 

If we create one block, add two topics and three items we have six streams:

&nbsp;     | SPY | QQQ | GOOG
-----------|-----|-----|-----
**LAST**   |&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;&nbsp;X
**VOLUME** |&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;X | &nbsp;&nbsp;&nbsp;&nbsp;X

> **The block requires at least one valid topic AND item**, otherwise it can't hold a data-stream. Because of this, if only items(topics) are added they are held in a pre-cache until a valid topic(item) is added. Likewise, if all topics(items) are removed, the remaining items(topics) are moved into the pre-cache. Use the appropriate API calls to see what's in the pre-cache.

Review the [Data Blocks](README_API.md#data-blocks) and [Items/Topics/Streams](README_API.md#items--topics--streams) sections of the C/C++ API docs for a thorough explanation.
- - -

#### Getting Data

There are numerous way to get current and historical data from a block/stream. 

- An (indexed) single historical data-point of a stream
- An (indexed) snapshot of multiple historical data-points of a stream
- An (indexed) snapshot of multiple historical data-points up to an internal, atomic marker
- A snapshot of 'n' historical data-points before an internal, atomic marker
- The most recent value of all streams
- The most recent value of all streams of a particular item
- The most recent value of all streams of a particular topic

Many of these calls offer type-specific, generic, and string versions. They can also return DateTime for each data-point, if applicable.
    
Review the [Historical Data](README_API.md#historical-data) and [Frames](README_API.md#frames) sections of the C/C++ API docs for a thorough explanation. Specific information about the Python and Java API calls can be found in the source documentation:

- ***python***: python/tosdb/_common.py
- ***java***: java/src/io/github/jeog/tosdatabridge/DataBlock.java
- - -

#### Custom Topics

TOS allows users to create custom quote fields that can be added as a column to various widgets/displays. These can be custom calculations, studies, strategy output etc. and can be created using the 'Condition Wizard' of the ThinkScript editor. Once created, and in use on the platform, the output of that field can then be exported via TOSDataBridge, like any other topic.

To create a CUSTOM quote in TOS:
1. Create a new watch-list or go to MarketWatch->Quotes
2. Right-click on the column header and click 'customize'
3. Select 'Custom Quotes' from the drop-down menu in the upper left.
4. Left-click the little scroll icon to the left of 'CUSTOM' field you want to use.
5. Select the 'Condition Wizard' tab or the ThinkScript editor tab to code your own.
6. When done add it to the list of columns, click Apply.

To export this CUSTOM quote via TOSDataBridge:
1. ***Add this custom quote to at least one widget/window in TOS.***
2. ***Add the symbols you want to export to THAT widget/window.***
3. Add the appropriate topic to your block, i.e topic 'CUSTOM1' for 'CUSTOM 1'.
4. Add the symbols/items you added in #2 to your block. (Symbols added to the block but not to TOS, will only return 'loading'.)

***Example 1***: Return a comma delimited string containing Fast, Slow, and Full Stochastic values.

(in the ThinkScript editor for 'CUSTOM 9')

    def sfast = StochasticFast();
    def sslow = StochasticSlow();
    def sfull = StochasticFull();
    AddLabel(Yes, Concat(sfast, Concat(",", Concat(sslow, Concat(",", sfull)))));

(in TOS create a watch-list that looks something like) 

&nbsp;|  LAST  | CUSTOM 9
------|--------|---------
SPY   | 260.10 | 14.37,18.23,17.99
QQQ   | 155.68 | 1.8,1.3,1.29

(in python)

    >> block = tosdb.TOSDB_DataBlock();
    >> block.add_items("SPY", "QQQ")
    >> block.add_topics("CUSTOM9")
    >> val = block.get("SPY", "CUSTOM9", date_time=False)
    >> stoch_fast, stoch_slow, stoch_full = map(float, val.split(",")) 


***Example 2***: Return a boolean value to indicate when the 10 min SMA crosses above the 30.

(in the ThinkScript editor for 'CUSTOM 9')

    plot data;
    if SimpleMovingAvg(close, 10) crosses above SimpleMovingAvg(close, 30) {
        data = 1;
    }else{
        data = 0;
    }

Follow the steps from Example 1 and check the stream for a change from '0' to '1'
- - -

#### DDE Data

It's important to realize that we are at the mercy of the TOS platform, the DDE technology, and TOS's implementation of it. You may notice the streams of data will not match the Time & Sales perfectly.  If you take a step back and aggregate the data in more usable forms this really shouldn't be an issue.  Another way we are at the mercy of the DDE server is that fields like last price and last size are two different topics. That means they are changing at slightly different times with slightly different time-stamps even if they are the correct pairing. To get around this we can write code like this to simulate a live-stream : 

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
- - -    

#### Numerical Values for 'Non-Decimal' Instruments

The TOS DDE server doesn't handle numerical values for 'non-decimal' instruments well. For instance, trying to get a bid/ask/last etc. for a 10-yr note future (/zn) will not return the fractional part of the price. In these cases use the topic versions suffixed with an x (bidx/askx/lastx etc.) which will provide a string you'll need to parse.
- - -

#### Closing Large Blocks

Currently Closing/Destroying large blocks(1,000,000+ combined data-stream elements) involves a large number of internal deallocations/destructions and becomes quite CPU intensive. The process is spun-off into its own thread but this may fail, returning to the main thread when the library is being freed, or block the python interpreter regardless of when or how it's called. Use caution when creating/closing large blocks.
- - -

#### Block Size and Memory

As mentioned you need to use some sense when creating blocks. As a simple example: let's say you want LAST,BID,ASK for 100 symbols. If you were to create a block of size 1,000,000, WITHOUT DateTime, you would need to allocate over 2.4 GB of memory - not good. As a general rule keep data-streams of similar desired size in the same block, and use new blocks as necessary. In our example if you only need 1,000,000 LAST elems for 10 items and 100 for everything else create 2 blocks: 1) a block of size 1,000,000 with those 10 items and the topic LAST; 2) a block of size 100 with all 100 items and the three topics. Then you would only need a little over 80 MB. (Really you could create three blocks to avoid any/all overlap.)
- - -

#### Asymmetric Responsibilities & Leaks

Connection 'probing' only works one way, from master(client/library) to slave(service). If the master disconnects abruptly, without sending the requisite remove signal(s), the slave will continue to maintain the master's resources. Internally, all it does is keep a ref-count to the streams it's been asked to create and write the necessary data into the appropriate shared memory segments. To see the status of the service, and whether there are leaked streams, open up the command shell, connect and use **`DumpBufferStatus`** to dump all the current stream information to /log . 
- - -

#### DateTimeStamp

***THESE ARE NOT OFFICIAL STAMPS FROM THE EXCHANGE,*** they are manually created once the TOS DDE server returns the data. They use the system clock to assure high_resolution( the micro-seconds field) and therefore there is no guarantee that the clock is accurate or won't change between stamps, as is made by the STL's std::steady_clock. 
- - -

#### SendMessage vs. SendMessageTimeout


To initiate a topic with the TOS server we should send out a broadcast message via the SendMessage() system call. This call is built to block to insure the client has had a chance to deal with the ACK message. For some reason, it's deadlocking, so we've been forced to use SendMessageTimeout() with an arbitrary 500 millisecond timeout. Therefore, until this gets fixed adding topics will introduce an amount of latency in milliseconds = 500 x # of topics.
- - -


