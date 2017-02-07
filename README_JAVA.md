### Java Wrapper *(Alpha version)*
- - -

***WARNING: Still in early development:***

*1. interface is subject to modest change,*

*2. has undergone very little testing,*

*3. bugs and crashes should be expected,*

*4. implementation has not been optimized for speed, safety etc.*

***The Python, C, and C++ interfaces are considerably more stable at this time.***

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

- **Topic.java**: enum that holds all the topics(LAST, BID, VOLUME etc.) that can be added to the block

- **DateTime.java**: datetime object that wraps the C tm struct with an added millisecond field; returned in DateTimePair<Type> by the ...WithDateTime methods


##### Important Details

The way java handles type-checking, default function args, and exceptions force us to be much more explicit in the methods provided. For instance, in python, within TOSDB_DataBlock, we have a single stream_snapshot_from_marker() method that takes a number of different arguments, throws a number of exceptions, and returns a number of different, contingent types. In the java wrapper we have the following methods (all overloaded 2x, 24 in total) for the same functionality:

    getStreamSnapshotLongsFromMarker(...);
    getStreamSnapshotDoublesFromMarker(...); 
    getStreamSnapshotStringsFromMarker(...); 

    getStreamSnapshotLongsFromMarkerWithDateTime(...);
    getStreamSnapshotDoublesFromMarkerWithDateTime(...); 
    getStreamSnapshotStringsFromMarkerWithDateTime(...); 

    getStreamSnapshotLongsFromMarkerIgnoreDirty(...); 
    getStreamSnapshotDoublesFromMarkerIgnoreDirty(...); 
    getStreamSnapshotStringsFromMarkerIgnoreDirty(...); 

    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(...);
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(...); 
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(...); 

Like the C/C++ interfaces we have type-specific calls. If you call the wrong version the C lib will try to (safely) cast the value for you. If it can't it will return ERROR_GET_DATA and java will throw CLibException: 

    switch( TOSDataBridge.getTopicType(topic) ){
    case TOPIC_IS_LONG:
        // use 'Long' version
    case TOPIC_IS_DOUBLE:
        // use 'Double' version
    case TOPIC_IS_STRING:
        //use 'String version
    } 

Methods with 'WithDateTime' appended return a Pair containing the underlying type AND a DateTime object: 

    public static class DateTimePair<T> extends Pair<T,DateTime>{
        public final T first; // inherited
        public final DateTime second; //inherited
    }

Methods with a plural type in the name(e.g getStreamSnapshotLongs(...)) return an array:

    DataBlock block = DataBlock(...);

    Long[] l =  block.getStreamSnapshotLongs(item,topic);
    DateTimePair<Long>[] ll = block.getStreamSnanshotLongsWithDateTime(item,topic);

StreamSnapshotFromMarker(...) calls provide client code a guarantee that it won't miss data between calls, assuming the 'marker' doesn't hit the back of the stream, becoming 'dirty'. The standard call will throw once the stream becomes 'dirty' to indicate old data has been dropped and the guarantee has been broken. Use the 'IgnoreDirty' versions, or keep the marker moving, to prevent the exception from being thrown.

##### Connect to Native Library

    import io.github.jeog.tosdatabridge.TOSDataBridge;

    // path to the C Lib
    TOSDataBridge.connect("C:\...\TOSDataBridge\bin\Release\x64\tos-databridge-0.7-x64.dll);

    switch( TOSDataBridge.connectionState() ){
    case TOSDataBridge.CONN_NONN:
        // Not connected to engine
    case TOSDataBridge.CONN_ENGINE:
        // Only connected to Engine (only access to admin calls)    
    case TOSDataBridge.CONN_ENGINE_TOS:
        // Connected to Engine AND TOS Platform (can get data from engine/platform)
    }
      
##### Create a Data Block

    import io.github.jeog.tosdatabridge.DataBlock;

    // can hold up to 1000 data-points with DateTime, default timeout 
    DataBlock block = DataBlock(1000, True); 

##### Add Items/Topics

    import io.github.jeog.tosdatabridge.Topic

    block.addItem("SPY"); // goes into 'pre-cache' (see python/C/C++ docs)
    block.addItem("QQQ"); // goes into 'pre-cache' (see python/C/C++ docs)
    block.addTopic(Topic.LAST); // pulls items out of pre-cache (see python/C/C++ docs)
    block.removeItem("QQQ");
 
    Set<String> myItems = block.getItems();
    Set<Topic> myTopics = block.getTopics();

##### Get Data

    import io.github.jeog.tosdatabridge.DataBlock.DateTimePair;

    try{
        switch( TOSDataBridge.getTopicType(Topic.LAST) ){
        case TOSDataBridge.TOPIC_IS_LONG:
            ...
            break;

        case TOSDataBridge.TOPIC_IS_DOUBLE: /* Topic.LAST stores doubles ... */
            
            // most recent data-point
            Double d = block.getDouble("SPY", Topic.LAST); 

            // 10 data-points ago, with datetime stamp
            DateTimePair<Double> dDT = block.getDoubleWithDateTime("SPY", Topic.LAST, 9); 

            // array of 10 most recent data-points
            Double[] dd = block.getStreamSnapshotDoubles("SPY", Topic.LAST, 9, 0); 

            // array of all valid data-points, with date time smap
            DateTimePair<Double>[] ddDT = 
                block.getStreamSnapshotDoublesWithDateTime("SPY", Topic.LAST) 

            // print most recent price and datetime string to stdout 
            System.out.println("SPY Last :: " + String.valueOf(ddDt[0].first) 
                               + " @ " + String.vlaueof(ddDt[0].second);

            //
            // use 'FromMarker' methods (below) to guarantee contiguous data BETWEEN calls 
            //   (see python and C/C++ docs for complete explanation)
            //

            // get 10 most recent values - THIS MOVES THE MARKER to 'beg' index (0 in this case)
            List<DateTimePair<Double>> dListOld =  
                new ArrayList<>(Arrays.asList( 
                    block.getStreamSnapshotDoublesWithDateTime("SPY", Topic.LAST,9) )); 

            // add some time between the calls so data can come into stream
            // as data comes into the stream the 'marker' is moving backwards
            Thread.sleep(1000);            

            // get all the data-points up to the marker - THIS ALSO MOVES THE MARKER (as above)
            List<DateTimePair<Double>> dlistNew = 
                new ArrayList<>(Arrays.asList( 
                    block.getStreamSnapshotDoublesFromMarkerWithDateTime("SPY", Topic.LAST) ));          

            // concatenating the two arrays will guarantee we got ALL the data
            dListNew.addAll(dListOld); // most recent first
            for(DateTimePair<Double> p : dListNew)
                System.out.println(p);

            /* we can keep using the marker calls in this way; if the 'marker' hits 
               the back of the stream(becomes 'dirty') the next 'Marker' call will 
               throw DirtyMarkerException; use the 'IgnoreDirty' versions to avoid 
               this behavior, allowing it to return as much valid data as possible */

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
        // we tried to access data that doesn't (or can't) exist (all methods can throw this)
    }catch(DateTimeNotSupported){
        // we are using a 'WithDateTime' call and our block doesn't support DateTime
    }catch((DirtyMarkerException){
        // we are NOT using an 'IgnoreDirty' call and getStreamSnapshotFromMarker has a 'dirty' marker (see python/C/C++ docs)
    }

##### Close

    /* DataBlock's finalize() method calls close() on destruction but we can't 
       guarantee when/if that will happend; IT'S RECOMMENDED you explicitly tell 
       the C Lib to close the underlying block when you're done with it. */
    block.close();
    





