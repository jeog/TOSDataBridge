### Java Wrapper 
- - -

***WARNING: Still in early development:***

*1. interface is subject to modest change,*

*2. has undergone very little testing,*

*3. bugs and crashes should be expected,*

*4. implementation has not been optimized for speed, safety etc.*

*The Python, C, and C++ interfaces are considerably more stable at this time.*

- - -

The java wrapper, like the python wrapper, extends the underlying C/C++ interface. It uses [Java Native Access(JNA)](https://github.com/java-native-access/jna) to load and access the underlying DLL, wrapping these calls in a more user-friendly API. Currently(unlike the python wrapper) there is no virtual layer for use on non-windows systems.

> **IMPORTANT:** You still must [install the underlying C/C++ modules.](README_INSTALL.md) You also must be sure the build of these modules matches the build of the Java Runtime Environment(JRE) you plan to use. 

> **NOTE:** If the library build is different than your JRE, init() with throw, indicating it can't find the native library in the resource path.


#### Getting Started

1. Add *java/tosdatabridge.jar* to your project's classpath.

2. Add the requisite API calls and objects found in the *io.github.jeog.tosdatabridge* package(see below).

3. Compile and run.

An example using *test/java/TOSDataBridgeTest.java*:
```
C:\...\TOSDataBridge\test\java\> javac -classpath "../../java/tosdatabridge.jar" TOSDataBridgeTest.java
C:\...\TOSDataBridge\test\java\> java -classpath "../../java/tosdatabridge.jar;." TOSDataBridgeTest "../../bin/Release/x64/tos-databridge-0.7-x64.dll"
```


#### API Basics

As mentioned, the java wrapper mirrors the python wrapper in many important ways. It's recommended you skim the (more thorough) python docs and tutorial - even the C/C++ docs - for an explanation of the core concepts. The source is pretty straight forward:

- **TOSDataBridge.java**: constants, exceptions, and some of the (high-level) administrative calls.

- **DataBlock.java**: user-instantiated object used to pull data from the engine/platform (very similar to python's TOSDB_DataBlock)

- **DataBlockWithDateTime.java**: user-instantiated object used to pull data - that includes a DateTime object - from the engine/platform (very similar to python's TOSDB_DataBlock)

- **DateTime.java**: datetime object that wraps the C tm struct with an added millisecond field; returned in DateTimePair<Type> by the ...WithDateTime methods

- **Topic.java**: enum that holds all the topics(LAST, BID, VOLUME etc.) that can be added to the block


##### Connect to Native Library

    import io.github.jeog.tosdatabridge.TOSDataBridge;

    // path to the C Lib; tries to load lib and calls connect() 
    TOSDataBridge.init(".\bin\Release\x64\tos-databridge-0.7-x64.dll");

    switch( TOSDataBridge.connectionState() ){
    case TOSDataBridge.CONN_NONN:
        // Not connected to engine
        break;
    case TOSDataBridge.CONN_ENGINE:
        // Only connected to Engine (only access to admin calls)    
        break;
    case TOSDataBridge.CONN_ENGINE_TOS:
        // Connected to Engine AND TOS Platform (can get data from engine/platform)
        break;
    }


##### Create DataBlocks

There are two types of objects used to pull data from the Engine/TOS:

    import io.github.jeog.tosdatabridge.DataBlock;
    import io.github.jeog.tosdatabridge.DataBlockWithDateTime;

    int blockSz = 10000;

    // only primary data
    DataBlock block = new DataBlock(blockSz);

    /* include a DateTime object with each primary data-point
       extends DataBlock, adds 'WithDateTime' versions of the get methods */
    DataBlock blockDT = new DataBlockWithDateTime(blockSz);


##### Add Items/Topics

To start getting data we need 'streams' in the block. Streams are created internally as a result of topics and items being added. 

If we add two topics and three items we have six streams:

        SPY  QQQ  GOOG
LAST     1    2    3
VOLUME   4    5    6

Clearly we need items AND topics. Items(topics) added before any topics(items) exist in the block will be pre-cached, i.e they will be visible to the back-end but not to the interface until a topic(item) is added; likewise if all the items(topics) are removed, thereby leaving only topics(items). See [Important Details and Provisos](README_DETAILS.md). 

    import io.github.jeog.tosdatabridge.Topic

    blockDT.addItem("SPY"); // goes into pre-cache 
    blockDT.addItem("QQQ"); // goes into pre-cache 

    Set<String> myPrecachedItems = blockDT.getItemsPreCached() // {'SPY','QQQ'}

    blockDT.addTopic(Topic.LAST); // pulls items out of pre-cache 
     
    Set<String> myItems = blockDT.getItems(); // {'SPY','QQQ'}
    Set<Topic> myTopics = blockDT.getTopics(); // {Topic.LAST}
    myPrecachedItems = blockDT.getItemsPreCached() // { }

    blockDT.removeTopic(Topic.LAST); // items go back into pre-cache


#### Get (individual) Data-Points from a Stream

Like the C/C++ interfaces we have type-specific calls. If you call the wrong version the C lib will try to (safely) cast the value for you. If it can't it will return ERROR_GET_DATA and java will throw CLibException.

    import io.github.jeog.tosdatabridge.DataBlock.DateTimePair;

    try{
        switch( TOSDataBridge.getTopicType(Topic.LAST) ){

        case TOSDataBridge.TOPIC_IS_LONG:
            ...
            break;

        case TOSDataBridge.TOPIC_IS_DOUBLE: /* Topic.LAST stores doubles ... */
            
            // most recent 'LAST' for 'SPY'
            Double d = blockDT.getDouble("SPY", Topic.LAST); 
            if(d == null){
                // no data in block yet
            } 

            // 10 data-points ago, with DateTime
            DateTimePair<Double> dDT = blockDT.getDoubleWithDateTime("SPY", Topic.LAST, 9); 
            if(dDT == null){
                // no data at this index/position yet
            } 
            break;

        case TOSDataBridge.TOPIC_IS_STRING:
            ...
            break;
       }
    }catch(LibraryNotLoaded){
        // init(...) was not successfull or the library was freed (all methods can throw this)
    }catch(CLibException){
        // an error was thrown from the C Lib (see CError.java) (all methods can throw this)
    }catch(DataIndexException){
        // we tried to access data in a position that isn't possible for that block size
    }

'WithDateTime' versions return DateTimePair object(s):

    public static class DateTimePair<T> extends Pair<T,DateTime>{
        public final T first; // inherited
        public final DateTime second; //inherited
    }



#### Get (multiple) Data-Points from a Stream

Methods with a plural type in the name(e.g getStreamSnapshotLongs) return an array.

'StreamSnapshot' calls return an array of data between 'beg' and 'end', with a number of overloads.

'StreamSnapshotFromMarker' calls provide client code a guarantee that it won't miss data between calls, assuming the 'marker' doesn't hit the back of the stream, becoming 'dirty'. (See python and C/C++ docs for a complete explanation.)

    import io.github.jeog.tosdatabridge.DataBlock.DateTimePair;

    try{
        switch( TOSDataBridge.getTopicType(Topic.LAST) ){
        case TOSDataBridge.TOPIC_IS_LONG:
            ...
            break;

        case TOSDataBridge.TOPIC_IS_DOUBLE: /* Topic.LAST stores doubles ... */          

            // array of 10 most recent data-points (index 0 to 9, inclusive)
            Double[] dd = blockDT.getStreamSnapshotDoubles("SPY", Topic.LAST, 9, 0); 

            // array of ALL valid data-points, with DateTime
            DateTimePair<Double>[] ddDT = 
                blockDT.getStreamSnapshotDoublesWithDateTime("SPY", Topic.LAST) 

            /*
             *  use 'FromMarker' methods (below) to guarantee contiguous data BETWEEN calls 
             *  (see python and C/C++ docs for complete explanation)
             */

            // get 10 most recent values - THIS MOVES THE MARKER to 'beg' index (0 in this case)
            List<DateTimePair<Double>> oldLasts =  
                new ArrayList<>(Arrays.asList( 
                    blockDT.getStreamSnapshotDoublesWithDateTime("SPY", Topic.LAST, 9) )); 

            // add some time between the calls so data can come into stream            
            Thread.sleep(1000);            
            // as data comes into the stream the 'marker' is moving backwards

            // get all the data-points up to the marker - THIS ALSO MOVES THE MARKER (as above)
            List<DateTimePair<Double>> newLasts = 
                new ArrayList<>(Arrays.asList( 
                    blockDT.getStreamSnapshotDoublesFromMarkerWithDateTime("SPY", Topic.LAST) ));          

            // concatenating the two arrays will guarantee we got ALL the data
            newLasts.addAll(oldLasts); // most recent first
            for(DateTimePair<Double> p : newLasts){
                System.out.println(p);
            }

            /* 
             *  we can keep using the marker calls in this way; if the 'marker' hits 
             *  the back of the stream(becomes 'dirty') the next 'Marker' call will 
             *  throw DirtyMarkerException; use the 'IgnoreDirty' versions to avoid 
             *  this behavior, allowing it to return as much valid data as possible 
             */
            break;

        case TOSDataBridge.TOPIC_IS_STRING:
            ...
            break;
       }
    }catch(LibraryNotLoaded){
        // init(...) was not successfull or the library was freed (all methods can throw this)
    }catch(CLibException){
        // an error was thrown from the C Lib (see CError.java) (all methods can throw this)
    }catch(DataIndexException){
        // we tried to access data in a position that isn't possible for that block size
    }catch((DirtyMarkerException){
        // we are NOT using an 'IgnoreDirty' version and getStreamSnapshotFromMarker has a 'dirty' marker (see python/C/C++ docs)
    }


#### Get (multiple, most-recent) Data-Points from Block

    import java.util.Map;

    try{

        // get most-recent 'LAST' vals (as strings) for ALL items in the block
        Map<String,String> itemFrame = blockDT.getItemFrame(Topic.LAST)

        // get ALL most-recent topic vals (as strings) for 'SPY'
        Map<Topic, String> topicFrame = blockDT.getTopicFrame('SPY')

        // get 'matrix' of ALL most-recent topic and item vals (as strings)
        Map<String, Map<Topic,String>> totalFrame = blockDT.getTotalFrame();

    }catch(LibraryNotLoaded){
        // init(...) was not successfull or the library was freed (all methods can throw this)
    }catch(CLibException){
        // an error was thrown from the C Lib (see CError.java) (all methods can throw this)
    }


##### Close

    /* 
     *  DataBlock's finalize() method calls close() on destruction but we can't 
     *  guarantee when/if that will happend; IT'S RECOMMENDED you explicitly tell 
     *  the C Lib to close the underlying block when you're done with it. 
     */
    blockDT.close();
    





