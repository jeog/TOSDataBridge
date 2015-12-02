## TOSDataBridge v0.1 
- - -
TOSDataBridge (TOSDB) is an open-source collection of resources for 'scraping' real-time streaming data off of TDAmeritrade's ThinkOrSwim(TOS) platform, providing C, C++, and Python interfaces. Users of the TOS platform - with some basic programming/scripting knowledge - can use these tools to populate their own databases with market data; analyze large data-sets in real-time; test and debug other financial apps; or even build extensions on top of it.

TOSDB uses TOS's antiquated, yet still useful, DDE feature, directly through the Windows API. The C / C++ interfaces are implemented as a shared library(DLL) that communicates with a back-end Windows Service. The Python interface wraps this library in a more object-oriented, user-friendly format, providing an interactive environment for easy access to the low(er)-level calls.

Obviously the core implementation is not portable, but the python interface does provides a thin virtualization layer over TCP. A user running Windows in a Virtual Machine, for instance, can expose the exact same python interface to a different host system running python3. 

### Requirements
- - -
- Windows for the core implementation(tested on Windows 7 SP1, Vista SP2,and Server 2008 R2.) The python interface is available to any system running python3(also tested on Debian/Linux-3.2.)
- TDAmeritrade's ThinkOrSwim(TOS) platform that exposes DDE functionality (the Window's verion)
- VC++ 2012 Redistributable (included)
- Python3 (optional, for the python wrapper)
- Some basic Windows knowledge; some basic C, C++, or Python programming knowledge

### Quick Setup
- - -
- tosdb-setup.bat will attempt to install the necessary modules/dependencies for you but you should refer to **Installation Details** below for a more detailed explanation
- Be sure to know what build you need(x86 vs x64); it should match your system, all the modules you'll need, and your version of Python(if you plan on using the python wrapper)

    ##### Core C/C++ Libraries
    ```
    (Admin) C:\[...TOSDataBridge]\tosdb-setup.bat   [x86|x64]   [admin]   [session]
    ```

    - [x86|x64] : the version to build (required)
    - [admin] : does your TOS platform require elevation? (optional) 
    - [session] : override the service's attempt to determine the session id when exiting from session-0 isolation. MOST USERS SHOULDN'T WORRY ABOUT THIS unless they plan to run in a non-standard environment (e.g an EC2 instance). The tos-databridge-engine.exe[] binary needs to run in the same session as the ThinkOrSwim platform. (optional)

    ##### Python Wrapper (optional)
    ```
    C:\[...TOSDataBridge]\python\python setup.py install
    ```
    - Core C/C++ libs (above) must be installed first to use the (non-virtual) interface
    - C++ python extensions have been converted to pure python to avoid portability/build issues. (tosdb/_tosdb.py is now generated automatically by setup.py)

### Quick Start
- - -
1. You may need to white-list some of these files (specifically tos-databridge-engine-\[x86|x64].exe) in your Anti-Virus software before proceeding.
2. Start the service:

    ```
    (Admin) C:\> SC start TOSDataBridge
    ```
3. Log on to your TOS platform

##### For C/C++:
- Include tos_databridge.h header in your code (if C++ make sure containers.hpp and generic.hpp can be found by the compiler)
- Use the library calls detailed in the **C/C++ Interface...** sections below
- Link with *tos-databridge-0.1-[x86|x64].dll*
- Build
- Run

##### For Python:
- Open a python shell/interpreter or create a script
- import tosdb
- tosdb.init(...)


### Contents
- - -
- */VisualStudioBuild* 

    The complete Visual Studio solution with properties, pre-processor options, relative links etc. set, ready to be built.

- */include* 

    C/C++ header files; your C/C++ code must include the tos-databridge.h header; C++ code needs to make sure that containers.hpp and generic.hpp are in the include path.

- */src* 

    C/C++ source files; if compiling from source simply open the .sln file inside /VisualStudioBuild, select the configuration/platform, and build.

- */bin* 

    Compiled binaries by build type. (We don't include debug builds.)

- */python* 

    Files relevant to the python wrapper.

- */sigs* 

    The detached signature for each binary; sha256 checksums for binaries, signatures, and the jeog.dev public key

- */docs* 

    Documentation, Tutorials, Licenses etc.


### Modules
- - -
- *tos-databridge-serv-[x86|x64].exe*

    The service process that spawns and controls the main engine described below. This program should be run as a typical windows service. In certain cases, debugging for instance, you can pass the --noservice arg to run as a pure executable. This module runs as SYSTEM, with such privileges; as such its intended role is very limited.

- *tos-databridge-engine-[x86|x64].exe*

    The main engine that interacts with the TOS platform. It is spawned from tos-databridge-serv.exe with a lowered integrity level (system-to-high or system-to-medium) and drastically reduced privileges to interact with the TOS platform (as standard user or admin). It is also able to communicate with our shared library(below) running in a normal user context at medium integrity level.

- *tos-databridge-0.1-[x86|x64].dll*

    The main library that client code will use to access TOSDB. It's part light-weight database(s) and part interface. Review tos-databridge.h, and the sections below, for all the necessary calls, types, and objects.

- *_tos-databridge-shared-[x86|x64].dll*

    A back-end library used by the previous two modules. It provides the concurrency and IPC objects used in both modules as well as the Topic-String mapping. It doesn't need to be linked to your code but it needs to be in the right path for other modules that are dependent on it. (see below)

- *_tos-databridge-static-[x86|x64].lib*

    The other back-end library that provides some basic logging and utility functions shared between the modules.

- *tosdb/*

    A python package that serves as a wrapper around tos-databridge-0.1-x86.dll / -x64.dll. It provides a more object oriented, simplified means of accessing the core functionality.

- *tosdb/cli_scripts/*

    Python scripts build on top of the python wrapper.

- *tos-databridge-shell-[x86|x64]*  

    A 'shell' used to interact directly with the library calls; for testing and debugging.

*(Going forward we'll exclude the build suffix (i.e. -x64 ) for syntactic convenience. We'll replace it with [] in an attempt to avoid confusion. Unless stated explicitly, if sensible, assume that both builds apply.)*


### Build Notes
- - -

- Object code and logs are placed into an intermediate sub-directory of /VisualStudioBuild of form /$(Configuration)/$(Platform). After the object code is linked the binaries are sent to a sub-directory of /bin of form /$(Configuration)/$(Platform) and the symbol files are sent to /symbols/$(Configuration).

- A debug build with VLD_ defined will use virtual leak detector (assuming you've followed the other instructions for using it.).

- Defining SPPRSS_INPT_CHCK will disable most of the internal C-string checks for overflows, NULL terminators etc. (not recommended).

- There are 32 and 64 bit (Win32 and x64) binaries included along with the relevant configurations in the VisualStudio solution. Debug versions have a "_d" suffix to avoid collisions. It's up to the user to choose and use the correct builds for ALL modules. The python library will search for the underlying DLL (-x64 vs. -x86) that matches the build of that version of python.


### Installation Details
- - -
The following sections will outline how to setup TOSDB's core C/C++ libraries. At the end of this section is a screen-shot of all the relevant commands for using the x64 binaries. **If you're only interested in using the python wrapper you still need to follow these steps. If you're looking to use the virtual python interface (and have already followed these steps - and those from the Python Wrapper section - on a windows system) you can skip to the Python Wrapper section.**

1. Move the unzipped tos-databridge folder to its permanent location(our's is in C:/ for convenience.) If you change it in the future you'll need to redo some of these steps because the Service module relies on an absolute path.

2. (Optional) Compile the appropriate binaries if building from source.

3. Determine the appropriate build you'll need: 32-bit(x86) or 64-bit(x64). ***Make sure you don't mix and match in future steps, python will automatically look for a version that matches its own build.***
    
4. Determine if your TOS platform runs under elevated privileges (does it ask for an admin password before starting?)
   
5. Determine if you need to run under a custom Session. **MOST USERS SHOULDN'T WORRY ABOUT THIS** unless they plan to run in a non-standard environment (e.g an EC2 instance). The tos-databridge-engine.exe[] binary needs to run in the same session as the ThinkOrSwim platform.
  
6. Open a command shell with Admin Privileges (right-click on cmd.exe and click 'run as administrator'). Navigate to the tos-databridge root directory and run the tosdb-setup.bat setup script with the info from steps #3, #4, and #5:
    
    ```
    Usage: C:\TOSDataBridge\> tosdb-setup.bat   [x86|x64]   [admin]   [session]
     
    Example 1: C:\TOSDataBridge\> tosdb-setup.bat x86
    Example 2: C:\TOSDataBridge\> tosdb-setup.bat x64 admin
    Example 3: C:\TOSDataBridge\> tosdb-setup.bat x64 admin 2
    ```

7. The setup script is going to do a few things: 
    - move _tos-databridge-shared[].dll(s) to the Windows Directory (%WINDIR%). This is generally not the best idea but we don't know the details of what and where you'll link from so we are doing this for convenience. If you create your own executables you can update your PATH environment variable or setup application specific paths in the registry and move this file there; 
    - make sure you have the appropriate run-time libraries installed; if not the appropriate redist executable will be run to install them. (If this fails you can do this manually by downloading the most recent VC++ Redistributable from Microsoft); 
    - create the Windows Service.
 
8. Before we continue it's a good idea, possibly necessary, to add the tos-databridge binaries, or the whole directory, to your anti-virus's 'white-list'. ***There's a good chance that if you don't do this tos-databridge-engine[].exe, at the very least, may be stopped and/or deleted for 'suspicious' behavior.***
  
9.  After the Windows Service has been successfully created run ```C:\> 'SC start TOSDataBridge'``` . Returned text and various system utilities should confirms this. (SEE THE SCREEN-SHOT BELOW) Some other commands you may need:
    - *SC stop TOSDataBridge* - this will stop the service. All the data collected so far will still exist but the engine will sever its connection to TOS and exit.  It should no longer be shown as a running process and its status should be Stopped.
    - *SC pause TOSDataBridge* - this will pause the service. All the data collected so far will still exist but the engine will stop recording new data in the buffers. It should still be shown as a running process but its status should be Paused. It's not recommended you pause the service.
    - *SC continue TOSDataBridge* - this should continue a paused service. All the data collected so far will still exist, the engine will start recording new data into the buffers, but you will have missed any streaming data while paused. The service should return to the Running state.
    
10. (***SKIP IF ONLY USING PYTHON***) Include the tos_databridge.h header in your code ( if its C++ make sure the compiler can find containers.hpp and generic.hpp ) and adjust the link settings to import the tos-databridge-0.1-[].lib stub. (If you run into trouble review the VisualStudio settings for tos-databridge-shell[].exe as they should resemble what you're trying to do.)
   
11. (***SKIP IF ONLY USING PYTHON***) Jump down to the next section for some of the basic library calls to add to your program.
   
12. Make sure the TOS Platform is running, execute your program or start the python wrapper(see below).

![](./docs/SCss1.png)

### Python Wrapper
- - -
The python wrapper is a simpler, yet still robust, way to get started with the underlying library. To use it you'll still need all the underlying modules mentioned above. 

> **IMPLEMENTATION NOTE:** tosdb was only written to be compatible with python3


> **IMPLEMENTATION NOTE:** tosdb uses ctypes.py to load the tos-databridge[].dll library, which depends on _tos-databridge-shared[].dll. That's why we manually copied the latter to your %WINDIR% directory in the 'Installation Details' section above.

Make sure the build of the modules you installed in the 'Installation Details' section matches your python build. Open a python shell and look to see if it says 32 bit or 64 bit on the top. 32 bit needs x86 modules; 64 bit needs x64. If they don't match redo the earlier steps. From a command prompt navigate to the tos-databridge/python directory and enter:
      
`C:\TOSDataBridge\python\> python setup.py install`

Remeber, if installing on a non-windows system to utilize the virtual interface you'll still need to install on a (physically or virtually) networked windows sytem.

> **IMPLEMENTATION NOTE:** We recently shifted from providing (low-level) constants via a C++ extension to having the setup.py script automatically pull constants and topic enum values from tos_databridge.h to generate _tosdb.py, all in pure python. This avoids a number of portability and build issues.

tosdb/ is structured as a package with the bulk of its code in \__init__.py and \_win.py , the latter holding the non-portable parts that \__init__.py will import if it determines it's being run on a windows sytem. This structure allows you to simply import the package(*import tosdb*) or, if needed, extensions like intervalize.py(*from tosdb import intervalize*). Once imported you'll have to initialize it, which requires the path or general location of the underlying library it's going to load (the tos-databridge[].dll) or the root directory it's going to search in for the latest version. Please see the (currently somewhat outdated) tutorial in /docs for a walk-through with screen-shots.

> **IMPORTANT:**  There is a minor issue with how python uses the underlying library to deallocate shared resources used by it and the Service. Generally calls like **`TOSDB_CloseBlock()`** and **`TOSDB_RemoveTopic()`** handle this on a case by case basis or the DLL's DllMain method attempts to close the necessary resources and signal the Service with the corresponding requests when the library is freed.

> tosdb attemps to handle this implicity in methods like **`remove_items()`** and **`remove_topics()`** and the object's **`__del__()`** method which calls the underlying **`TOSDB_CloseBlock()`** function. The problem with the **`__del__()`** method is two-fold: 

>    1. a del call only decrements the ref-count of the object, it doesn't necessarily call **`__del__()`** and the underlying **`TOSDB_CloseBlock()`**, and 

>    2. on exit() the underlying call is dependent on the order in which globals are deleted and may fail, for this or other reasons. In some cases there appears to be no attempt to call the objects **`__del__()`** methods or free the underlying library.

> Because of all this WE STRONGLY RECOMMEND you call clean_up() before exiting to be sure all the shared resources have been properly dealt with. If this is not possible - the program terminates abruptly for instance - there's a good chance you've got dangling BufferStreams (shared mem segments) and mutexes. You can check this by opening the tos-databridge-shell[].exe calling Connect and then DumpBufferStatus to create a file in the Systems appdata folder to see what resources are held by the Service. You'll probably want to restart the Service if your program had more than a small number of trivial TOSDB_DataBlocks/VTOSDB_DataBlocks. 

### Glossary
- - -
Variable                 | Description
------------------------ | -------------
topics                   | strings or TOS_Topics::TOPICS enums(recommended) of fields to be added (e.g. BID, ASK, LAST )
items                    | strings of symbols to be added (e.g. GE, SPY, GOOG )
data-stream              | the historical data for each item-topic entry (see data_stream.hpp)
frame                    | all the items, topics, or matrix of both, for a particular index( only index of 0 is currently implemented )
TOS_Topics               | a specialization that provides a scoped enum containing all the topics( TOS data fields), a mapping to the relevant strings, and some utilities. This is the recommended way for C++ calling code to pass topics. Inside the high-order bits of the enum value are the type bit constants TPC..[ ] which can be checked directly or via one of the aptly named public utilities
DateTimeStamp            | struct that wraps the C library tm struct, and adds a micro-second field
BufferHead               | struct that aligns with the front of the Stream Buffers in shared memory providing info on the buffer to the client
ILSet<>                  | wrapper around std::set<> type that provides additional means of construction / copy / move / assignment
TwoWayHashMap<>          |mapping that allows values to be indexed both ways, using custom hashes, with a thread-safe specialization
UpdateLatency            | Enum of milliseconds values the client library waits before re-checking buffers
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
def_size_type            | default size type for data ( long )
ext_size_type            | extend size type for data ( long long )
def_price_type           | default price type for data ( float )
ext_price_type           | extended price type for data ( double )
size_type                | explicit size type for Python Wrapper ( unsigned long )
type_bits_type           | bits set in an unsigned char to indicate the underlying type of a TOS_Topics::TOPICS
TOSDB_APP_NAME           | name of the DDE Application ( TOS )
TOSDB_COMM_CHANNEL       | name of the DynamicIPC communication mechanism
TOSDB_LOG_PATH           | path derived from users and system's %APPDATA%, for log files to be sent
TOSDB_INTGR_BIT          | type bits for an integral data type
TOSDB_QUAD_BIT           | type bits for an 8-byte data type
TOSDB_STRING_BIT         | type bits for a string data type
TOSDB_TOPIC_BITMASK      | logical OR of all the type bits
TOSDB_SIG_ADD            | message sent to the data engine to add a stream/connection to TOS
TOSDB_SIG_REMOVE         | message sent to the data engine to remove a stream/connection to TOS
TOSDB_SIG_PAUSE          | message sent to the data engine from the service to pause
TOSDB_SIG_CONTINUE       | message sent to the data engine from the service to continue
TOSDB_SIG_STOP           | message sent to the data engine from the service to stop
TOSDB_SIG_DUMP           | message sent to the data engine to dump the buffer data
TOSDB_SIG_GOOD           | signal sent between data engine and service
TOSDB_SIG_BAD            | signal sent between data engine and service
TOSDB_MIN_TIMEOUT        | minimum timeout period for the locks/latches/signals used to for internal communication / synchronization
TOSDB_DEF_TIMEOUT        | default timeout for above
TOSDB_SHEM_BUF_SZ        | size in bytes of the buffer that the data engine will write to
TOSDB_MAX_STR_SZ         | maximum string size for client input (items, topics etc) among other strings
TOSDB_STR_DATA_SZ        | maximum size of char buffer pulled from DDE server and pushed into a data-stream of std::strings (everything larger is truncated)
TOSDB_BLOCK_ID_SZ        | maximum string size for the name/ID of a TOSDBlock
TOSDB_DEF_LATENCY        | the default UpdateLatency (Moderate= 300 milleseconds) 

### C/C++ Interface ::: Administrative Calls
- - -
Once the Service is running start by calling **`TOSDB_Connect()`** which will return 0 if successful. Call the Library function **`TOSDB_IsConnected()`** which returns 1 if you are 'connected' to the TOSDataBridge service.

> **IMPLEMENTATION NOTE:** Be careful: **`TOSDB_IsConnected()`** returns an unsigned int that represents a boolean value; most of the other C admin calls return a signed int to indicate error(non-0) or success(0). Boolean values will be represented by unsigned int return values for C and bool values for C++. 

Generally **`TOSDB_Disconnect()`** is unnecessary as it's called automatically when the library is unloaded.

> **NOTABLE CONVENTIONS:** The C calls, except in a few cases, don't return values but populate variables, arrays/buffers, and arrays of pointers to char buffers. The 'dest' argument is for primary data, and is a pointer to a variable or an array/buffer when followed by argument 'arr_len' - which takes its size in array elements. 

> The String versions of the calls take a char\*\* argument, followed by arr_len for the number of char*s, and str_len for the size of the buffer each char\* points to (obviously they should all be >= to this value). NOTE: the poorly named str_len arg is the size of the total char buffer you are passing. If the call requires more than one array/buffer besides 'dest' (the Get...Frame... calls for instance) it assumes an array length equal to that of 'arr_len'. If it is of type char\*\* you need to specify a char buffer length just as you do for the initial char\*\*.

TOSDB's main organizational unit is the 'block' (struct TOSDBlock in client.hpp) - it's important functionally and conceptually. The first thing client code does is call **`TOSDB_CreateBlock()`** passing it a unique ID(<= TOSDB_BLOCK_ID_SZ) that will be used to access it throughout it's lifetime, a size(how much historical data is saved in the block's data-streams), a flag indicating whether it saves DateTime in the stream, and a timeout value used for its internal waiting/synchronization. 

When you no longer need the data in the block **`TOSDB_CloseBlock()`** should be called to deallocate its internal resources or **`TOSDB_CloseBlocks()`** to close all that currently exist. If you lose track of what's been created use the C or C++ version of **`TOSDB_GetBlockIDs()`** . Technically **`TOSDB_GetBlockCount()`** returns the number of RawDataBlocks allocated (see below), but this should always be the same as the number of TOSDBlocks.

Within each 'block' is a pointer to a RawDataBlock object which relies on an internal factory to return a constant pointer to a RawDataBlock object. Internally the factory has a limit ( the default is 10 ) which can be adjusted with the appropriately named admin calls **`TOSDB_GetBlockLimit()`** **`TOSDB_SetBlockLimit()`**

Once a block is created, items and topics are added. Topics are the TOS fields (e.g. LAST, VOLUME, BID ) and items are the individual symbols (e.g. IBM, GE, SPY). **`TOSDB_Add()`** **`TOSDB_AddTopic()`** **`TOSDB_AddItem()`** **`TOSDB_AddTopics()`** **`TOSDB_AddItems()`** There are a number of different versions for C and C++, taking C-Strings(const char\*), arrays of C-Strings(const char\*\*), string objects(std::string), TOS_Topics::TOPICS enums, and/or specialized sets (str_set_type, topic_set_type) of the latter two. Check the prototypes in tos_databridge.h for all the versions and arguments.

> **IMPLEMENTATION NOTE:** Items(Topics) added before any topics exist in the block will be pre-cached, i.e. they will be visible to the back-end but not to the interface until a respective Topic(Item) is added; likewise if all the items(topics) are removed(thereby leaving only topics(items)). See 'Important Details and Provisos' section below.) To view the pre-cache use the C++(only) calls **`TOSDB_GetPreCachedTopicNames()`** **`TOSDB_GetPreCachedItemNames()`** **`TOSDB_GetPreCachedTopicEnums()`**

Currently its only possible to remove individual items **`TOSDB_RemoveItem()`** and topics **`TOSDB_RemoveTopic()`** or close the whole block. **`TOSDB_CloseBlock()`**

As mentioned, the size of the block represents how large the data-streams are, i.e. how much historical data is saved for each item-topic (each entry in the block has the same size; if you prefer different sizes create a new block). Call **`TOSDB_GetBlockSize()`** to get the size and **`TOSDB_SetBlockSize()`** to change it.

> **IMPLEMENTATION NOTE:** The use of the term size may be misleading when getting into implementation details. This is the size from the block's perspective and the bound from the data-stream's perspective. For all intents and purposes the client can think of size as the maximum number of elements that can be in the block and the maximum range that can be indexed. To get the occupancy (how much valid data has come into the stream) call **`TOSDB_GetStreamOccupancy()`** .

To find out the the items / topics currently in the block call the C or C++ versions of **`TOSDB_GetItemNames()`** **`TOSDB_GetTopicNames()`** **`TOSDB_GetTopicEnums()`** or if you just need the number **`TOSDB_GetItemCount()`** **`TOSDB_GetTopicCount()`** . To find out if the block is saving DateTime call the C or C++ versions of **`TOSDB_IsUsingDateTime()`**.

Because the data-engine behind the blocks handles a number of types it's necessary to pack the type info inside the topic enum. Get the type bits at compile-time with **`TOS_Topics::Type< ...topic enum... >::type`** or at run-time with **`TOSDB_GetTypeBits()`** , checking the bits with the appropriately named TOSDB_[ ]_BIT constants in tos_databridge.h. 

    if( TOSDB_TypeBits( "BID" ) == TOSDB_INTGR_BIT) 
       \\ data is a long (def_size_type).
    else if( TOSDB_TypeBits( "BID" ) == TOSDB_INTGR_BIT | TOSDB_QUAD_BIT) 
       \\ data is a long long (ext_size_type)
    
Make sure you don't simply check a bit with logical AND when what you really want is to check the entire type_bits_type value. In this example checking for the INTGR_BIT will return true for def_size_type AND ext_size_type. **`TOSDB_GetTypeString()`** provides a string of the type for convenience.

> **IMPLEMENTATION NOTE:** The client-side library extracts data from the Service (tos-databridge-engine[].exe) through a series of protected kernel objects in the global namespace, namely a read-only shared memory segment and a mutex. The Service receives data messages from the TOS DDE server and immediately locks the mutex, writes them into the shared memory buffer, and unlocks the mutex. At the same time the library loops through its blocks and item-topic pairs looking to see what buffers have been updated, acquiring the mutex, and reading the buffers, if necessary. 

>The speed at which the looping occurs depends on the UpdateLatency enum value set in the library. The lower the value, the less it waits, the faster the updates. **`TOSDB_GetLatency()`** and **`TOSDB_SetLatency()`** are the relevant calls. A value of Fastest(0) allows for the quickest refreshes, but can chew up clock cycles - view the relevant CPU% in process explorer or task manager to see for yourself. The default(Fast, 30) or Moderate(300) should be fine for most users. 


### C/C++ Interface ::: Get Calls
- - -
Once you've created a block with valid items and topics, at some point you'll want to extract the data that is collected. This is done via the non-administrative **`TOSDB_Get...`** calls and is where things can get a little tricky.  The two basic techniques are pulling data as: 

1. *a segment of a data-stream:* some portion of the historical data of the block for a particular item-topic entry. A block with 3 topics and 4 items has 12 different data-streams each of the size passed to **`TOSDB_CreateBlock()`**. The data-stream is indexed thusly: 0 or -(block size) is the most recent value, -1 or (block size - 1) is the least recent. 

    > **IMPORTANT:** When indexing a sub-range it is important to keep in mind both values are INCLUSIVE \[ , \]. Therefore a sub-range of a data-stream of size 10 where index begin = 3(-7) and index end = 5(-5) will look like: \[0\]\[1\]\[2\]**\[3\]\[4\]\[5\]**\[6\]\[7\]\[8\]\[9\]. Be sure to keep this in mind when passing index values as the C++ versions will throw std::invalid_argument() if you pass an ending index of 100 to a stream of size 100.

2. *a frame:* spans ALL the topics, items, or both. Think of all the data-streams as the 3rd dimension of 2-dimensional frames. In theory there can be a frame for each index( for example a frame of all the most recent values or of all the oldest ) but in practice we've only implemented the retrieval of the most recent frame because the data is currently structured in a 'tick-dependent' rather than a 'time-dependent' chronological order, making an index of frames mostly useless.

Let's drill down a bit on some specific calls. First are the single value **`TOSDB_Get< , >(...)`** and **`TOSDB_Get[Type](...)`** calls; the former a templatized C++ version, the latter a C version with the required type stated explicitly in the call (e.g. **`TOSDB_GetDouble()`** ). The C++ version's first template arg is the value type to return. The client has three options: 

1. figure out the type of the data-stream (the type bits are packed in the topic enum, see above) and make the appropriate call, or

2. pass in generic_type to receive a custom generic type that knows its own type, can be cast to the relevant type, or can have its as_string() method called, or

3. use the specialized std::string version

The C versions require you to figure the type and make the appropriately named call; or call the string version, passing in a char\* to be populated with the data ( \< TOSDB_STR_DATA_SZ ). Obviously the generic and string versions come with a cost but since n=1 it's insignificant. 

The C++ version's second template argument is a boolean indicating whether it should return DateTimeStamp values as well(assuming the block is set for that) while the C version accepts a pointer to a DateTimeStamp struct(pDateTimeStamp) that will be populated with the value (pass a NULL value otherwise).

In most cases you'll want more than a single value: use the **`TOSDB_GetStreamSnapshot< , >(...)`** and **`TOSDB_GetStreamSnapshot[Type]s()`** calls. The concept is similar to the **`TOSDB_Get...`** calls from above except they return containers(C++) or populate arrays(C) and require a beginning and ending index value. The C calls require you to state the explicit dimensions of the arrays( the string version requires length of the string buffers as well; internally, data moved to string buffers is of maximum size TOSDB_STR_DATA_SZ so no need to allocate larger than that). 

DateTimeStamp is dealt with in the same way as above. If NULL is not passed it's array length is presumed to be the same as the other array so make sure you pay attention to what you allocate and pass. The C++ calls are implemented to return either a vector of different types or a pair of vectors(the second a vector of DateTimeStamp), depending on the boolean template argument. **Please review the function prototypes in tos_databridge.h, and the Glossary section, for a better understanding of the options available.**

> **IMPLEMENTATION NOTE:** Internally the data-stream tries to limit what is copied by keeping track of the streams occupancy and finding the MIN( occupancy count, difference between the end and begin indexes +1\[since they're inclusive\], size of parameter passed in). For C++ calls that return a container it's possible you may want the sub-stream from index 5 to 50 but if only 10 values have been pushed into the stream it will return a container with values from index 5 to 9. Therefore you should use the GetStreamOccupancy call mentioned in the above section, check the returned object's size, use its resize method(s), or pass an explicit array to one of the C calls if relying on an object of certain size(length). If you choose the latter keep in mind the data-stream will NOT copy/initialize the 'tail' elements of the array that do not correspond to valid indexes in the data-stream and the value of those elements should be assumed undefined.

It's likely the stream will grow between consecutive calls. The **`TOSDB_GetStreamSnapshot[Type]sFromMarker(...)`** calls (C only) guarantee to pick up where the last **`TOSDB_Get`**, **`TOSDB_GetStreamSnanpshot`**, or **`TOSDB_GetStreamSnapshotFromMarker`** call ended (under a few assumptions).  Internally the stream maintains a 'marker' that tracks the position of the last value pulled; the act of retreiving data and moving the marker can be thought of as a single, 'atomic' operation. The \*get_size arg will return the size of the data copied, it's up to the caller to supply a large enough buffer. A negative value indicates the buffer was to small to fit all the data, or the stream is 'dirty' . 

A 'dirty' stream indicates the marker has hit the back of the stream and data between the beginning of the last call and the end of the next will be dropped. To avoid this be sure you use a big enough stream and/or keep the marker moving foward (by using the get calls mentioned above). To determine if the stream is 'dirty' use the **`TOSDB_IsMarkerDirty()`** call. There is no guarantee that a 'clean' stream will not become dirty between the call to **`TOSDB_IsMarkerDirty`** and the retrieval of stream data, although you can look for a negative \*get_size value to indicate this rare state has occured.

The three main 'frame' calls are **`GetItemFrame`**, **`GetTopicFrame`**, **`GetTotalFrame`**( C++ only ). **`TOSDB_GetItemFrame<>()`** and **`TOSDB_GetItemFrame[Type]s(...)`** are used to retrieve the most recent values for all the items of a particular topic. Keep in mind, the frames are only dealing with the the 'front' of the block, i.e. index=0 of all the data-streams. Think of it as finding the specific topic on its axis, then pulling all the values along that row. Because the size of n is limited the C++ calls only return a generic type, whereas the C calls necessarily require the explicitly named call to be made( or the (C\-)String version). The C++ calls take one boolean template arg as above. 

One major difference between the frame calls and the data-stream calls is the former map their values to strings of relevant topic or item names. For instance, a call of **`GetItemFrame<true>(..., BID)`** for a block with topics: BID, ASK and items: IBM, GE, CAT will return ( "IBM", (val,dts); "GE", (val,dts); "CAT", (val,dts) ); that is, a mapping of item strings to either the value as generic_type, or to a pair of generic_type and DateTimeStamp (depending to the boolean passed to the template). 

The C calls require pointers to arrays of appropriate type, with the dimensions of the arrays. The value array is required; the second array of strings that is populated with the corresponding labels(item strings in this case) and the third array of DateTimeStamp object are both optional. Remember, where specified the arrays require dimensions to be passed; if not they presume them to be the same as the primary value array(arg 'dest').

**`TOSDB_GetTopicFrame<>()`** and **`TOSDB_GetTopicFrameStrings()`** are used to retrieve the most recent values for all the topics of a particular item. Think of it as finding the specific item on its axis, then pulling all the values along that row. They are similar to the **`GetItemFrame...`** calls from above except for the obvious fact they pull topic values and there is only the one C call, populating an array of c-strings. Since each item can have multiple topic values, and each topic value may be of a different type, it's necessary to return strings(C) or generic_types(C++).

**`TOSDB_GetTotalFrame<>(...)`** is the last type of frame call that returns the total frame(the recent values for ALL items AND topics) as a matrix, with the labels mapped to values and DateTimeStamps if true is passed as the template argument. Because of the complexity of the the matrix, with mapped strings, and possible DateTimeStamp structs included there is only a C++ version. C code will have to iterate through the items or topics and call **`GetTopicFrame(item)`** or **`GetItemFrame(topic)`**, respectively, like the Python Wrapper does.

> **IMPLEMENTATION NOTE:** The data-streams have been implemented in an effort to:

> 1. provide convenience by allowing both a generic type and strings to be returned; 
> 2. not make the client pay for features that they don't want to use.(i.e. genericism or DateTimeStamps); 
> 3. take advantage of a type-correcting feature of the underlying data-stream(see the comments and macros in data_stream.hpp); 
> 4. throw derived exceptions or return error codes depending on the context and language; and 
> 5. provide at least one very efficient, 'stream-lined' call for large data requests.

> In order to comply with #5 we violate the abstraction of the data block for all the **`Get..`** calls and the interface of the data-stream for the non-generic versions of those same calls. For the former we allow the return of a const pointer to the stream, allowing the client-side back-end to operate directly on the stream. For the latter we provide 'copy(...)' virtual methods of the data-stream that take raw pointer(s) to be populated directly, bypassing generic_type construction and STL overhead.

> Not suprisingly, when n is very large the the non-string **`TOSDB_GetStreamSnapshot[Type]s`** C calls are the fastest, with the non-generic, non-string **`TOSDB_GetStreamSnapshot<Type,false>`** C++ calls just behind. 


### C/C++ Interface ::: Logging, Exceptions & Stream Overloads
- - -
The library exports some logging functions that dovetail with its use of custom exception classes. Pass a file-name to **`TOSDB_StartLogging()`** to begin. **`TOSDB_LogH()`** and **`TOSDB_Log()`** will log high and low priority messages, respectively. Pass two strings: a tag that provides a short general category of what's being logged and a detailed description. **`TOSDB_LogEx()`** has an additional argument generally used for an error code like one returned from *GetLastError()*. If **`TOSDB_StartLogging()`** hasn't been called and a high-priority event(\_LogH, \_LogEx) is logged, the info will be sent to std::cerr. **`TOSDB_Log_Raw_()`** only takes a single string, doesn't format anything, doesn't provide additional information and, more importantly, doesn't block. **`TOSDB_ClearLog()`** and **`TOSDB_StopLogging()`** have the expected effects.

Most of the modules use these logging functions internally. The files are sent to appropriately named .log files that reside in a /tos-databridge folder in the 'appdata' path for the particular user(the path string returned from the APPDATA environment variable). The service and engine executables, being run by the system account, will write to different paths ("C:/Windows/System32/config/systemprofile/appdata...") than the modules run by the user.

The library also defines an exception hierarchy derived from TOSDB_Error and std::exception. C++ code can catch them and respond accordingly; the base class provides threadID(), processID(), tag(), and info() methods as well as the inherited what() from std::exception. Namespace data_stream (see data_stream.hpp for the implementation details) also provides some exceptions that, when possible, are caught internally and wrapped in TOSDB_Error.

There are operator\<\< overloads for most of the custom objects and containers returned by the **`TOSDB_Get...`** calls.


### Important Details & Provisos
- - -
- **Clean.. Exit.. Close.. Stop..:** Exiting gracefully can be tricky because of the myriad states the  TOSDB system can be in. Assuming everything is up and running you really want to avoid shutting down the TOS platform and/or the Service while your client code is running. As a general rule follow this order: 

    1. Clean up: close blocks, call clean_up etc. 
    2. Exit your program
    3. Stop the Service 
    4. Close TOS platform

    Once you've done (i) the remaining order is less important. (The Service is built to work with multiple instantiations of the client library, maintaining ref-counts to shared resources. Just because you cleaned up doesn't mean another instance has, or vice-versa.)

- **DDE Data:** It's important to realize that we (us, you, this code, and yours) are at the mercy of the TOS platform, the DDE technology, and TOS's implementation of it. DDE has been replaced by a number of better Windows technologies but it's the public interface that TOS exposes so it's what we're using. You may notice the streams of data - particularly on symbols with very high trading volume - will not match the Time & Sales perfectly.  If you take a step back and aggregate the data in more usable forms this really shouldn't be an issue.  Another way we are at the mercy of the DDE server is that fields like last price and last size are two different topics. That means they are changing at slightly different times with slightly different time-stamps even if they are the correct pairing. To get around this we can write (python) code like this to simulate a live-stream :
    ```
    >>> vol = 0
    >>> print("price"," : ","size") 
    >>> while(not closeTS):
    ...     v = db.get("GOOG","VOLUME")
    ...     p = db.get("GOOG","LAST")
    ...     if(v != vol):
    ...         print(p," : ",v - vol)
    ...     vol = v
    ...     time.sleep(.1)
    ```

- **Numerical Values for Certain Instrument Types:** The TOS DDE server doesn't handle numerical values for 'non-decimal' securities well. For instance, trying to get a bid/ask/last etc. for a 10-yr note future (/zn) will not return the fractional part of the price. In these cases use the topic versions suffixed with an x (bidx/askx/lastx etc.) which will provide a str that you'll need to parse.

- **Case-Sensitivity:** Case Sensitivity is a minor issues with how Item values are handled. The underlying C/C++ library are case-sensitive; it's up to the client to make sure they are passing case consistent item strings. The Python wrapper is case-insensitive; on receiving item and topic strings they are converted to upper-case by default.

- **Closing Large Blocks:** Currently Closing/Destroying large blocks(1,000,000+ combined data-stream elements) involves a large number of internal deallocations/destructions and becomes quite CPU intensive. The process is spun-off into its own thread but this may fail, returning to the main thread when the library is being freed, or block the python interpreter regardless of when or how it's called. One alternative is to utilize **`TOSDB_SetBlockSize() / set_block_size()`** to massively shrink the block/data-streams before closing the block. Internally the data-stream deque objects calls .resize() and .shrink_to_fit() but there are no guarantees as to if and when the actual memory will be deallocated so use caution when creating large blocks, especially those with many topics and items as the number of data-streams is a multiple of the two.

- **Block Size and Memory:** As mentioned you need to use some sense when creating blocks. As a simple example: let's say you want LAST,BID,ASK for 100 symbols. If you were to create a block of size 1,000,000, WITHOUT DateTime, you would need to allocate over 2.4 GB of memory - not good. As a general rule keep data-streams of similar desired size in the same block, and use new blocks as necessary. In our example if you only need 1,000,000 LAST elems for 10 items and 100 for everything else create 2 blocks: 1) a block of size 1,000,000 with those 10 items and the topic LAST; 2) a block of size 100 with all 100 items and the three topics. Then you would only need a little over 80 MB. Really you could create three blocks to avoid any overlap but in this case its only a minor performance and space improvement and not worth worth the inconvenience.

- **Inter-Process Communication(IPC):** The current means of IPC is a complicated mess. It uses a pair of master/slave objects that share two duplexed named pipes and a shared memory segment.  Because of all the moving parts and the waiting / blocking involved there will be connection issues while trying to debug as one thread hits a break-point causing part of the IPC mechanism's wait to expire. Internally the client library continually calls the masters ->connected() method which is built to fail easily, for any number of reasons. Debugging certain areas of code will make this connected call fail, shuting down communication between master and slave. In certain cases, where one side of the connection is waiting on a message via a pipe you'll get a deadlock.

- **Asymmetric Responsibilities & Leaks:** An issue related to the previous is the fact that the connection probing only works one way, from master to slave. The slave(which is owned by the Service) therefore may know that one of the clients isn't there and handle a disconnection, it just doesn't know what stream objects they are responsible for. Internally all it does st keep a ref-count to the streams it's been asked to create and obviously write the necessary data into the appropriate shared memory segments. To see the status of the service and if there are stranded or dangling streams open up the debug shell and use **`TOSDB_DumpSharedBufferStatus`** to dump all the current stream information to a file in the same directly as the internal logs(see above) for debugging. Until a number of these issues are worked out it's recommended to stop and re-start the Service whenever a client executable that has active blocks and streams fails in a way that may not allow it to send TOSDB_SIG_REMOVE messages to the Service.

- **DateTimeStamp:** There are some issues with the DateTimeStamps attached to data. The first and most important is THESE ARE NOT OFFICIAL STAMPS FROM THE EXCHANGE, they are manually created once the TOS DDE server returns the data. Secondly they use the system clock to assure high_resolution( the micro-seconds field) and therefore there is no guarantee that the clock is accurate or won't change between stamps, as is made by the STL's std::steady_clock. We've also had some issues converting high resolution time-points to C time when micro_seconds are close to 0 that we think is solved by casting the micro_seconds duration to seconds and adding that back to the epoch before converting to C time.

- **Bad Items & Topics:** The implementation can easily handle bad topics passed to a call since it has a large mapping of the allowed topic strings mapped to enum values for TOS_Topics. If a passed string is mapped to a NULL_TOPIC then it can be rejected, or even if it is passed it won't get a positive ACK from the server and should be rejected. Bad item strings on the other hand are a bit of a problem. The DDE server is supposed to respond with a negative ACK if the item is invalid but TOS responds with a positive ACK and a 'N/A' string. If you look in the internally generated engine.log file after passing a bad item you may see entries like "invalid stod argument" which is the engine trying to convert that N/A into the appropriate numerical type. Currently it's up to the user to deal with passing bad Items. Obviously this is not ideal but hey, that's life.

- **Pre-Caching:** As mentioned earlier the block requires at least one valid topic AND item, otherwise it can't hold a data-stream. Because of this if only items(topics) are added, or all topics(items) are removed, any of those, or the remaining, items(topics) are held in a pre-cache which is then emptied when a valid topic(item) is added. This has two important consequences: 1) pre-cached entries are assumed to be valid until they come out and are sent to the engine and 2) when using the set of TOSDB_Get...[Name] C/C++ calls or get_items/topics() python calls you will NOT see pre-cached items. This can be particularly confusing if you remove all the items from a block and then want to check what topics remain as they have all been removed until an item is re-entered. Currently there are C++ calls to check the pre-cached but not C or python versions.

- **SendMessage vs. SendMessageTimeout:** To initiate a topic with the TOS server we should send out a broadcast message via the SendMessage() system call. This call is built to block to insure the client has had a chance to deal with the ACK message. For some reason, it's deadlocking, so we've been forced to use SendMessageTimeout() with an arbitrary 500 millisecond timeout. Therefore until this gets fixed adding topics will introduce an amount of latency in milliseconds = 500 x # of topics.

- **Stream Nomenclature:** Somewhat stupidly we termed the IPC data-buffers where all the DDE data is written 'Streams', see AddStream in engine.cpp. These are not to be confused with the data-streams that are collected in the blocks which store the client specified amounts of data. At some point we will change the names to avoid confusion but we'll try use data-streams to refer to ones in the block and Streams or BufferStreams to refer to the others.

- **size_type:** Throughout you may see the use of a size_type in some ugly and seemingly unnecessary casts. Since we are creating 32 and 64 bit binaries which define size_t differently we wanted a guaranteed byte width for python code passing size and pointer-to-size arguments through ctypes.

- **Concurrency:** TOSDB attemptts to manage and synchronize multiple threads, processes, and shared resources while providing a set of substantive guarantees for calling the underlying library functions from multiple threads. Unfortunately the library has not undergone enough testing to make any such guarantees, despite having the framework in place.


### License & Terms
- - -
*TOSDB is released under the GNU General Public License(GPL); a copy (LICENSE.txt) should be included. If not, see http://www.gnu.org/licenses. The author reserves the right to issue current and/or future versions of TOSDB under other licensing agreements. Any party that wishes to use TOSDB, in whole or in part, in any way not explicitly stipulated by the GPL, is thereby required to obtain a separate license from the author. The author reserves all other rights.*

*This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.*

*By choosing to use the software - under the broadest interpretation of the term "use" \- you absolve the author of ANY and ALL responsibility, for ANY and ALL damages incurred; including, but not limited to, damages arising from the accuracy and/or timeliness of data the software does, or does not, provide.*

*Furthermore, TOSDB is in no way related to TDAmeritrade or affiliated parties; users of TOSDB must abide by their terms of service and are solely responsible for any violations therein.*

- - -

*Copyright (C) 2014 Jonathon Ogden*


