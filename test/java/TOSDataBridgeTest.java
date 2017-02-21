/*
Copyright (C) 2017 Jonathon Ogden   <jeog.dev@gmail.com>

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



import io.github.jeog.tosdatabridge.TOSDataBridge;
import io.github.jeog.tosdatabridge.TOSDataBridge.*;
import io.github.jeog.tosdatabridge.DataBlock;
import io.github.jeog.tosdatabridge.DataBlock.*;
import io.github.jeog.tosdatabridge.DataBlockWithDateTime;
import io.github.jeog.tosdatabridge.Topic;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.*;

/**
 * TOSDataBirdgeTest.java
 *
 * Simple test for TOSDataBridge Java Wrapper/API (tosdatabridge.jar)
 *
 * 1) load native library and try to connect
 * 2) check connection state
 * 3) test admin calls
 * 4) create a DataBlock
 * 5) add/remove items/topics
 * 6) use different versions of 'get' methods
 * 7) use 'frame methods'
 *
 * Attempt to check what it can statically and print/throw on error but
 * most of the methods that return data must be run dynamically and the
 * output reviewed manually, in real-time.
 *
 * @author Jonathon Ogden
 * @version 0.7
 */
public class TOSDataBridgeTest {
    private static final int SLEEP_PERIOD = 1000;

    public static void
    main(String[] args)
    {
        if(args.length == 0 || args[0] == null){
            System.err.println("TOSDataBridge library path must be passed as arg0");
            return;
        }

        if( !(new File(args[0])).exists() ){
            System.err.println("Arg0 (" + args[0] + ") is not a valid file.");
            throw new IllegalArgumentException("Arg0 is not a valid file");
        }

        try{
            if( !TOSDataBridge.init(args[0]) ){
                throw new RuntimeException("library failed to connect to Engine/TOS");
            }
            testConnection();
            testAdminCalls();
            testBlock1();
            testBlock2();
            System.out.println();
            System.out.println("*** DONE (SUCCESS) ***");
            return;
        } catch (LibraryNotLoaded e) {
            System.out.println("EXCEPTION: LibraryNotLoaded");
            System.out.println(e.toString());
            e.printStackTrace();
        } catch (CLibException e) {
            System.out.println("EXCEPTION: CLibException");
            System.out.println(e.toString());
            e.printStackTrace();
        } catch (InterruptedException e) {
            System.out.println("EXCEPTION: InterruptedException");
            System.out.println(e.toString());
            e.printStackTrace();
        } catch (Throwable t) {
            System.out.println("EXCEPTION: ***");
            System.out.println(t.toString());
            t.printStackTrace();
            // catch anything else so we can print fail message
        }

        System.out.println();
        System.err.println("*** DONE (FAILURE) ***");
    }

    private static void 
    testBlock1() throws LibraryNotLoaded, CLibException, NoSuchMethodException, IllegalAccessException,
            InvocationTargetException, InterruptedException, DataIndexException {
        int sz = 100000;
        System.out.println();
        System.out.println("CREATE BLOCK 1...");
        DataBlock block = new DataBlock(sz);

        if( !testBlockState(block, sz, false, TOSDataBridge.DEF_TIMEOUT) ) {
            // just break out of the try-block
            throw new IllegalStateException("testBlockState(...) failed");
        }else {
            System.out.println("Successfully created block: " + block.getName());
        }

        Set<String> items = new HashSet<>(Arrays.asList("XLF","XLE"));
        Set<Topic> topics = new HashSet<>(Arrays.asList(Topic.DESCRIPTION, Topic.BID, Topic.LAST_SIZE));

        System.out.println("Add items: " + items.toString());
        for( String s : items ) {
            block.addItem(s);
        }
        printBlockItemsTopics(block);

        // check pre-cache
        if(!block.getItemsPreCached().contains("XLF")
                || !block.getItemsPreCached().contains("XLE")){
            System.err.println("Item pre-cache test failed");
            throw new RuntimeException("Item pre-cache test failed");
        }
        System.out.println("Item pre-cache looks good: " + block.getItemsPreCached().toString());

        // add bad topic
        try{
            block.addTopic(Topic.NULL_TOPIC);
            throw new RuntimeException("Add NULL_TOPIC/CLIbException test failed");
        }catch(TOSDataBridge.CLibException e){
            System.out.println("Successfully caught exception from adding NULL_TOPIC: " + e.toString() );
        }

        //add good topics
        System.out.println("Add Topics: " + topics.toString());
        for( Topic t : topics ) {
            block.addTopic(t);
        }
        printBlockItemsTopics(block);

        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        //NEED TO TEST DIRTY MARKER BEFORE ANY OTHER GET CALLS
        System.out.println("TEST DIRTY MARKER EXCEPTION: " + block.getName());
        testDirtyMarkerExceptions(block, false);
        System.out.println();

        System.out.println("TEST DATA INDEX EXCEPTION: " + block.getName());
        testDataIndexExceptions(block, false);
        System.out.println();

        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        System.out.println("TEST GET CALLS, BLOCK: " + block.getName());
        testGetCalls(block,false);
        System.out.println();

        System.out.println("TEST GET CALLS NULL/EXCEPTION BEHAVIOR, BLOCK: " + block.getName());
        testGetCallsExcNullBehavior(block,false);
        System.out.println();

        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        System.out.println("TEST STREAM SNAPSHOT CALLS, BLOCK: " + block.getName());
        testStreamSnapshotCalls(block,5,false);
        System.out.println();

        System.out.println();
        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        System.out.println("TEST STREAM SNAPSHOT FROM MARKER CALLS, BLOCK: " + block.getName());
        testStreamSnapshotFromMarkerCalls(block,3,SLEEP_PERIOD,false,false);
        System.out.println();

        System.out.println("TEST TOTAL FRAME CALLS: " + block.getName());
        testTotalFrameCalls(block, false);
        System.out.println();

        System.out.println("TEST ITEM FRAME CALLS: " + block.getName());
        testItemFrameCalls(block, false);
        System.out.println();

        System.out.println("TEST TOPIC FRAME CALLS: " + block.getName());
        testTopicFrameCalls(block, false);
        System.out.println();

    }

    private static void
    testBlock2() throws LibraryNotLoaded, CLibException, InterruptedException, DataIndexException,
            NoSuchMethodException, IllegalAccessException, InvocationTargetException {
        int sz = 1000;
        int timeout = 3000;
        System.out.println();
        System.out.println("CREATE BLOCK 2...");
        DataBlockWithDateTime block = new DataBlockWithDateTime(sz, timeout);

        if( !testBlockState(block, sz, true, timeout) ) {
            // just break out of the try-block
            throw new IllegalStateException("testBlockState(...) failed");
        }else {
            System.out.println("Successfully created block: " + block.getName());
        }
        System.out.println();
        System.out.println("Double block size...");
        block.setBlockSize(block.getBlockSize() * 2);
        if(block.getBlockSize() != 2 * sz){
            System.err.println("failed to double block size");
            throw new IllegalStateException();
        }

        String item1 = "SPY";
        String item2 = "QQQ";
        Topic topic1 = Topic.LAST; // double
        Topic topic2 = Topic.VOLUME; // long
        Topic topic3 = Topic.LASTX; // string

        System.out.println("Add item: " + item1);
        block.addItem(item1);
        printBlockItemsTopics(block);

        System.out.println("Add item: " + item2);
        block.addItem(item2);
        printBlockItemsTopics(block);

        System.out.println("Add topic: " + topic1);
        block.addTopic(topic1);
        printBlockItemsTopics(block);

        System.out.println("Remove item: " + item1);
        block.removeItem(item1);
        printBlockItemsTopics(block);

        System.out.println("Remove Topic: " + topic1);
        block.removeTopic(topic1);
        printBlockItemsTopics(block);

        System.out.println("Add ALL items");
        block.addItem(item1);
        block.addItem(item2);
        printBlockItemsTopics(block);

        System.out.println("Add ALL topics");
        block.addTopic(topic1);
        block.addTopic(topic2);
        block.addTopic(topic3);
        printBlockItemsTopics(block);

        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        //NEED TO TEST DIRTY MARKER BEFORE ANY OTHER GET CALLS
        System.out.println("TEST DIRTY MARKER EXCEPTION (WITH DATETIME): " + block.getName());
        testDirtyMarkerExceptions(block, true);
        System.out.println();

        System.out.println("TEST DATA INDEX EXCEPTION (WITH DATETIME): " + block.getName());
        testDataIndexExceptions(block, true);
        System.out.println();

        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        System.out.println("TEST GET CALLS, BLOCK: " + block.getName());
        testGetCalls(block,false);
        System.out.println();

        System.out.println("TEST GET CALLS (WITH DATETIME), BLOCK: " + block.getName());
        testGetCalls(block,true);
        System.out.println();

        System.out.println("TEST GET CALLS NULL/EXCEPTION BEHAVIOR, BLOCK: " + block.getName());
        testGetCallsExcNullBehavior(block,false);
        System.out.println();

        System.out.println("TEST GET CALLS NULL/EXCEPTION BEHAVIOR (WITH DATETIME), BLOCK: " + block.getName());
        testGetCallsExcNullBehavior(block,true);
        System.out.println();

        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        System.out.println("TEST STREAM SNAPSHOT CALLS, BLOCK: " + block.getName());
        testStreamSnapshotCalls(block,5,false);
        System.out.println();

        System.out.println("TEST STREAM SNAPSHOT CALLS (WITH DATETIME), BLOCK: " + block.getName());
        testStreamSnapshotCalls(block,3,true);
        System.out.println();

        System.out.println();
        System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
        Thread.sleep(SLEEP_PERIOD);
        System.out.println();

        System.out.println("TEST STREAM SNAPSHOT FROM MARKER CALLS, BLOCK: " + block.getName());
        testStreamSnapshotFromMarkerCalls(block,3,SLEEP_PERIOD,false,false);
        System.out.println();

        System.out.println("TEST STREAM SNAPSHOT FROM MARKER (WITH DATETIME) CALLS, BLOCK: " + block.getName());
        testStreamSnapshotFromMarkerCalls(block,3,SLEEP_PERIOD,true,true);
        System.out.println();

        System.out.println("TEST TOTAL FRAME CALLS: " + block.getName());
        testTotalFrameCalls(block, false);
        System.out.println();

        System.out.println("TEST TOTAL FRAME (WITH DATETIME) CALLS, BLOCK: " + block.getName());
        testTotalFrameCalls(block, true);
        System.out.println();

        System.out.println("TEST ITEM FRAME CALLS: " + block.getName());
        testItemFrameCalls(block, false);
        System.out.println();

        System.out.println("TEST ITEM FRAME (WITH DATETIME) CALLS, BLOCK: " + block.getName());
        testItemFrameCalls(block, true);
        System.out.println();

        System.out.println("TEST TOPIC FRAME CALLS: " + block.getName());
        testTopicFrameCalls(block, false);
        System.out.println();

        System.out.println("TEST TOPIC FRAME (WITH DATETIME) CALLS, BLOCK: " + block.getName());
        testTopicFrameCalls(block, true);
        System.out.println();

    }


    private static void
    testConnection() throws LibraryNotLoaded {
        // connect() (should already be connected)
        TOSDataBridge.connect();
        // connected()
        if(!TOSDataBridge.connected()){
            System.out.println("NOT CONNECTED");
            return;
        }
        // connection_state()
        int connState = TOSDataBridge.connectionState();
        System.out.println("CONNECTION STATE: " + String.valueOf(connState));
        if(connState != TOSDataBridge.CONN_ENGINE_TOS){
            System.out.println("INVALID CONNECTION STATE");
        }
    }

    private static boolean
    testAdminCalls() throws LibraryNotLoaded, CLibException {
        // get_block_limit()
        int l = TOSDataBridge.getBlockLimit();
        System.out.println("block limit: " + String.valueOf(l));

        // set_block_limit()
        System.out.println("double block limit");
        TOSDataBridge.setBlockLimit(l*2);

        // get_block_limit()
        int ll = TOSDataBridge.getBlockLimit();
        if( ll != l * 2 ){
            System.err.println("failed to double block limit");
            return false;
        }
        System.out.println("block limit: " + String.valueOf(l));

        // get_block_count()
        int c = TOSDataBridge.getBlockCount();
        if(c != 0){
            System.err.println("initial block count != 0");
            return false;
        }
        System.out.println("block count: " + String.valueOf(c));

        // type_bits()
        if(!testTypeBits(Topic.LAST, TOSDataBridge.QUAD_BIT, "QUAD_BIT",
                         TOSDataBridge.TOPIC_IS_DOUBLE, "TOPIC_IS_DOUBLE")) {
            return false;
        }
        if(!testTypeBits(Topic.EPS, 0, "", TOSDataBridge.TOPIC_IS_DOUBLE, "TOPIC_IS_DOUBLE")) {
            return false;
        }
        if(!testTypeBits(Topic.VOLUME, TOSDataBridge.QUAD_BIT | TOSDataBridge.INTGR_BIT, "QUAD_BIT | INTGR_BIT",
                         TOSDataBridge.TOPIC_IS_LONG, "TOPIC_IS_LONG")){
            return false;
        }
        if(!testTypeBits(Topic.LAST_SIZE, TOSDataBridge.INTGR_BIT, "INTGR_BIT",
                         TOSDataBridge.TOPIC_IS_LONG, "TOPIC_IS_LONG")){
            return false;
        }
        if(!testTypeBits(Topic.SYMBOL, TOSDataBridge.STRING_BIT, "STRING_BIT",
                         TOSDataBridge.TOPIC_IS_STRING, "TOPIC_IS_STRING")) {
            return false;
        }
        return true;
    }

    private static boolean
    testTypeBits(Topic topic, int cBits, String cName, int jTypeId, String jName)
            throws CLibException, LibraryNotLoaded {
        if( TOSDataBridge.getTypeBits(topic) != cBits) {
            System.err.println("Type Bits for '" + topic + "' != " + cName + "(" + String.valueOf(cBits) + ")");
            return false;
        }
        if( TOSDataBridge.getTopicType(topic) != jTypeId) {
            System.err.println("Topic type for '" + topic + "' != " + jName + "(" + String.valueOf(jTypeId) + ")");
            return false;
        }
        return true;
    }

    private static boolean
    testBlockState(DataBlock block, int blockSize, boolean withDateTime, int timeout)
            throws CLibException, LibraryNotLoaded {
        if(block.getBlockSize() != blockSize){
            System.out.println("invalid block size: "
                    + String.valueOf(block.getBlockSize()) + ", "
                    + String.valueOf(blockSize));
            return false;
        }
        if(block.isUsingDateTime() != withDateTime){
            System.out.println("invalid block DateTime: "
                    + String.valueOf(block.isUsingDateTime()) + ", "
                    + String.valueOf(withDateTime));
            return false;
        }
        if(block.getTimeout() != timeout){
            System.out.println("invalid block timeout: "
                    + String.valueOf(block.getTimeout()) + ", "
                    + String.valueOf(timeout));
            return false;
        }
        System.out.println("Block: " + block.getName());
        System.out.println("using datetime: " + String.valueOf(block.isUsingDateTime()));
        System.out.println("timeout: " + String.valueOf(block.getTimeout()));
        System.out.println("block size: " + String.valueOf(block.getBlockSize()));
        return true;
    }

    private static void
    testGetCalls(DataBlock block, boolean withDateTime) throws CLibException, LibraryNotLoaded, DataIndexException {
        Random rand = new Random(Double.doubleToLongBits(Math.random()));
        String dtSuffix = withDateTime ? "WithDateTime" : "";
        Set<String> items = block.getItems();
        Set<Topic> topics = block.getTopics();
        for(Topic topic : topics){
            int tType = TOSDataBridge.getTopicType(topic);
            for(String item : items) {
                int occ = block.getStreamOccupancy(item,topic);
                List<Integer> iParams = new ArrayList<>();
                switch(occ){
                case 0:
                    System.out.println("testGetCalls (" + item + "," + topic + ") occ == 0");
                    continue;
                case 1:
                    iParams.add(0); //first
                    break;
                case 2:
                    iParams.add(0); //first
                    iParams.add(occ-1); //last
                    break;
                default:
                    iParams.add(0); //first
                    iParams.add(occ-1); //last
                    iParams.add(1 + rand.nextInt(occ-1)); // something in between
                }
                for(int i : iParams) {
                    switch (tType) {
                    case TOSDataBridge.TOPIC_IS_LONG:
                        printGet(block, item, topic, i, "getLong" + dtSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_DOUBLE:
                        printGet(block, item, topic, i, "getDouble" + dtSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_STRING:
                        printGet(block, item, topic, i, "getString" + dtSuffix);
                        break;
                    }
                }
            }
        }

    }

    private static <T> void
    testGetExcNull(DataBlock block, String item, Topic topic, String fname)
            throws CLibException, LibraryNotLoaded, NoSuchMethodException,
            IllegalAccessException, InvocationTargetException {
        String outBndl = "(" + fname + "," + item + "," + topic + ")";
        try {
            //test null behavior of get calls
            T l = genericGet(block,item,topic,-1,fname);
            if(l != null){
                throw new RuntimeException("get from indx -1 failed to return null: " + outBndl );
            }else{
                System.out.println("get from indx -1 successfully returned null: " + outBndl);
            }
            //test exc behavior of get calls
            try {
                l = genericGet(block, item, topic, block.getBlockSize() + 1, fname);
            }catch(InvocationTargetException e){
                Throwable t = e.getCause();
                if (t.getClass().equals(DataIndexException.class)) {
                    throw (DataIndexException)t;
                }
            }
            throw new RuntimeException("DataIndexException test failed for get failed: " + outBndl);
        }catch( DataIndexException e){
            System.out.println("Successfully caught DataIndexException for get: " + outBndl );
        }
    }

    private static void
    testGetCallsExcNullBehavior(DataBlock block, boolean withDateTime)
            throws CLibException, LibraryNotLoaded, NoSuchMethodException, IllegalAccessException,
            InvocationTargetException
    {
        String dtSuffix = withDateTime ? "WithDateTime" : "";
        Set<String> items = block.getItems();
        Set<Topic> topics = block.getTopics();
        for(Topic topic : topics){
            int tType = TOSDataBridge.getTopicType(topic);
            for(String item : items) {
                switch (tType) {
                    case TOSDataBridge.TOPIC_IS_LONG:
                        testGetExcNull(block,item,topic,"getLong" + dtSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_DOUBLE:
                        testGetExcNull(block,item,topic,"getDouble" + dtSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_STRING:
                        testGetExcNull(block,item,topic,"getString" + dtSuffix);
                        break;
                }
            }
        }

    }

    private static void
    testStreamSnapshotCalls(DataBlock block, int sz, boolean withDateTime)
            throws CLibException, LibraryNotLoaded
    {
        if(sz < 1) {
            throw new IllegalArgumentException("sz < 1");
        }
        String dtSuffix = withDateTime ? "WithDateTime" : "";
        Set<String> items = block.getItems();
        Set<Topic> topics = block.getTopics();
        for(Topic topic : topics){
            int tType = TOSDataBridge.getTopicType(topic);
            for(String item : items) {
                int occ = block.getStreamOccupancy(item,topic);
                if(occ < sz){
                    System.out.println("testStreamSnapshotCalls (" + item + "," + topic
                            + ") occ < sz ");
                    continue;
                }
                for(int[] ii : new int[][]{{0,sz-1}, {occ-sz,-1}}) {
                    switch (tType) {
                    case TOSDataBridge.TOPIC_IS_LONG:
                        printGetStreamSnapshot(block,item,topic,ii[1],ii[0],
                                "getStreamSnapshotLongs" + dtSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_DOUBLE:
                        printGetStreamSnapshot(block,item,topic,ii[1],ii[0],
                                "getStreamSnapshotDoubles" + dtSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_STRING:
                        printGetStreamSnapshot(block,item,topic,ii[1],ii[0],
                                "getStreamSnapshotStrings" + dtSuffix);
                        break;
                    }
                }
            }
        }

    }

    private static void
    testStreamSnapshotFromMarkerCalls(DataBlock block, int passes, int wait, boolean withDateTime,
                                      boolean ignoreDirty) throws CLibException, LibraryNotLoaded
    {
        if(passes < 1) {
            throw new IllegalArgumentException("passes < 1");
        }
        String callSuffix = withDateTime ? "WithDateTime" : "";
        callSuffix = callSuffix + (ignoreDirty ? "IgnoreDirty" : "");
        Set<String> items = block.getItems();
        Set<Topic> topics = block.getTopics();
        for(Topic topic : topics){
            int tType = TOSDataBridge.getTopicType(topic);
            for(String item : items) {
                int occ = block.getStreamOccupancy(item,topic);
                for(int i = 0; i < passes; ++i) {
                    System.out.print("PASS #" + String.valueOf(i+1) + " :: ");
                    switch (tType) {
                    case TOSDataBridge.TOPIC_IS_LONG:
                        printGetStreamSnapshotFromMarker(block,item,topic,0,
                                "getStreamSnapshotLongsFromMarker" + callSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_DOUBLE:
                        printGetStreamSnapshotFromMarker(block,item,topic,0,
                                "getStreamSnapshotDoublesFromMarker" + callSuffix);
                        break;
                    case TOSDataBridge.TOPIC_IS_STRING:
                        printGetStreamSnapshotFromMarker(block,item,topic,0,
                                "getStreamSnapshotStringsFromMarker" + callSuffix);
                        break;
                    }
                    try {
                        Thread.sleep(wait);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
    }

    private static void
    testDirtyMarkerExceptions(DataBlock block, boolean withDateTime) throws CLibException, LibraryNotLoaded
    {
        Set<String> items = block.getItems();
        Set<Topic> topics = block.getTopics();
        int sz = block.getBlockSize();
        for(Topic topic : topics){
            int tType = TOSDataBridge.getTopicType(topic);
            for(String item : items) {
                int occ = block.getStreamOccupancy(item,topic);
                if(occ < 2) {
                    System.err.println("occ < 2, can't test DirtyMarkerException: " + item + "," + topic);
                    continue;
                }
                block.setBlockSize(occ); //force a dirty marker by shrinking block
                if( !block.isDirty(item,topic) ){
                    try {
                        Thread.sleep(SLEEP_PERIOD);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                    if( !block.isDirty(item,topic) ) {
                        System.err.println("stream did not become dirty after wait: " + item + "," + topic);
                        continue;
                    }
                }
                try {
                    switch (tType) {
                        case TOSDataBridge.TOPIC_IS_LONG:
                            if(withDateTime) {
                                ((DataBlockWithDateTime)block)
                                        .getStreamSnapshotLongsFromMarkerWithDateTime(item, topic);
                            }else {
                                block.getStreamSnapshotLongsFromMarker(item, topic);
                            }
                            break;
                        case TOSDataBridge.TOPIC_IS_DOUBLE:
                            if(withDateTime) {
                                ((DataBlockWithDateTime)block)
                                        .getStreamSnapshotDoublesFromMarkerWithDateTime(item, topic);
                            }else {
                                block.getStreamSnapshotDoublesFromMarker(item, topic);
                            }
                            break;
                        case TOSDataBridge.TOPIC_IS_STRING:
                            if(withDateTime) {
                                ((DataBlockWithDateTime)block)
                                        .getStreamSnapshotStringsFromMarkerWithDateTime(item, topic);
                            }else {
                                block.getStreamSnapshotStringsFromMarker(item, topic);
                            }
                            break;
                    }
                    System.out.println("Failed to throw/catch DirtyMarkerExeption: " + item + "," + topic);
                }catch(DirtyMarkerException e) {
                    System.out.println("Successfully caught DirtyMarkerException: " + item + "," + topic);
                }
            }
        }
        block.setBlockSize(sz);
    }

    private static void
    testDataIndexExceptions(DataBlock block, boolean withDateTime) throws CLibException, LibraryNotLoaded
    {
        Set<String> items = block.getItems();
        Set<Topic> topics = block.getTopics();
        int sz = block.getBlockSize();
        for(Topic topic : topics){
            int tType = TOSDataBridge.getTopicType(topic);
            for(String item : items) {
                try {
                    switch (tType) {
                        case TOSDataBridge.TOPIC_IS_LONG:
                            if(withDateTime) {
                                ((DataBlockWithDateTime)block)
                                        .getStreamSnapshotLongsWithDateTime(item, topic,sz);
                            }else {
                                block.getStreamSnapshotLongs(item, topic, sz-1, -sz -1);
                            }
                            break;
                        case TOSDataBridge.TOPIC_IS_DOUBLE:
                            if(withDateTime) {
                                ((DataBlockWithDateTime)block)
                                        .getStreamSnapshotDoublesWithDateTime(item, topic, sz-1, -sz -1);
                            }else {
                                block.getStreamSnapshotDoubles(item, topic,sz);
                            }
                            break;
                        case TOSDataBridge.TOPIC_IS_STRING:
                            if(withDateTime) {
                                ((DataBlockWithDateTime)block)
                                        .getStreamSnapshotStringsWithDateTime(item, topic,sz);
                            }else {
                                block.getStreamSnapshotStrings(item, topic, sz-1, -sz -1);
                            }
                            break;
                    }
                    System.out.println("Failed to throw/catch DataIndexException: " + item + "," + topic);
                }catch(DataIndexException e) {
                    System.out.println("Successfully caught DataIndexException: " + item + "," + topic);
                }
            }
        }
    }

    @SuppressWarnings("unchecked")
    private static <T> void
    testTotalFrameCalls(DataBlock block, boolean withDateTime)
            throws CLibException, LibraryNotLoaded
    {
        Method m;
        try {
            m = block.getClass().getMethod((withDateTime ? "getTotalFrameWithDateTime" : "getTotalFrame"));
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }
        Map<String,Map<Topic,T>> frame;
        try{
            frame = (Map<String,Map<Topic,T>>)m.invoke(block);
        } catch (IllegalAccessException | InvocationTargetException e) {
            e.printStackTrace();
            return;
        }
        for(String i : frame.keySet()){
            System.out.print(String.format("%-12s :::  ", i));
            Map<Topic,T> row = frame.get(i);
            for(Topic t : row.keySet()){
                if(withDateTime){
                    @SuppressWarnings("unchecked")
                    DateTimePair<String> p = (DateTimePair<String>)row.get(t);
                    System.out.print(String.format("%s %s %s  ", t, p.first, p.second.toString()));
                }else {
                    System.out.print(String.format("%s %s  ", t, row.get(t)));
                }
            }
            System.out.println();
        }
    }

    @SuppressWarnings("unchecked")
    private static <T> void
    testItemFrameCalls(DataBlock block, boolean withDateTime)
            throws CLibException, LibraryNotLoaded
    {
        Method m;
        try {
            m = block.getClass().getMethod((withDateTime ? "getItemFrameWithDateTime" : "getItemFrame"),
                    Topic.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }

        Set<Topic> topics = block.getTopics();
        for(Topic topic : topics) {
            Map<String, T> frame;
            try{
                frame = (Map<String, T>)m.invoke(block, topic);
            } catch (IllegalAccessException | InvocationTargetException e) {
                e.printStackTrace();
                return;
            }
            System.out.print(String.format("%-12s :::  ", topic));
            for(String item : frame.keySet()){
                if(withDateTime){
                    @SuppressWarnings("unchecked")
                    DateTimePair<String> p = (DateTimePair<String>)frame.get(item);
                    System.out.print(String.format("%s %s %s  ", item, p.first,
                            p.second.toString()));
                }else {
                    System.out.print(String.format("%s %s  ", item, frame.get(item)));
                }
            }
            System.out.println();
        }
    }

    @SuppressWarnings("unchecked")
    private static <T> void
    testTopicFrameCalls(DataBlock block,  boolean withDateTime)
            throws CLibException, LibraryNotLoaded
    {
        Method m;
        try {
            m = block.getClass().getMethod((withDateTime ? "getTopicFrameWithDateTime" : "getTopicFrame"),
                    String.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }

        Set<String> items = block.getItems();
        for(String item : items) {
            Map<Topic, T> frame;
            try{
                frame = (Map<Topic, T>)m.invoke(block, item);
            } catch (IllegalAccessException | InvocationTargetException e) {
                e.printStackTrace();
                return;
            }
            System.out.print(String.format("%-12s :::  ", item));
            for(Topic topic: frame.keySet()){
                if(withDateTime){
                    DateTimePair<String> p = (DateTimePair<String>)frame.get(topic);
                    System.out.print(String.format("%s %s %s  ", topic, p.first,
                            p.second.toString()));
                }else {
                    System.out.print(String.format("%s %s  ", topic, frame.get(topic)));
                }
            }
            System.out.println();
        }
    }

    @SuppressWarnings("unchecked")
    private static <T> T
    genericGet(DataBlock block, String item, Topic topic, int indx, String mname)
            throws CLibException, LibraryNotLoaded, NoSuchMethodException,
            InvocationTargetException, IllegalAccessException, DataIndexException
    {
        Method m = block.getClass().getMethod(mname,String.class,Topic.class,int.class);
        return (T)m.invoke(block,item,topic,indx);
    }

    private static <T> void
    printGet(DataBlock block, String item, Topic topic, int indx, String mname)
            throws CLibException, LibraryNotLoaded, DataIndexException {
        try {
            T r1 = genericGet(block,item,topic,indx,mname);
            System.out.println(mname + "(" + item + "," + topic + "," + String.valueOf(indx)
                                     + "): " + r1.toString());
        } catch (IllegalAccessException | InvocationTargetException e) {
            e.printStackTrace();
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
        }
    }

    private static <T> void
    printGetStreamSnapshot(DataBlock block, String item, Topic topic, int end, int beg, String mname)
            throws CLibException, LibraryNotLoaded
    {
        Method m;
        try {
            m = block.getClass().getMethod(mname,String.class,Topic.class,int.class,int.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }
        try {
            @SuppressWarnings("unchecked")
            T[] r1 = (T[])m.invoke(block,item,topic,end,beg);
            System.out.println(mname + "(" + item + "," + topic + "," + String.valueOf(beg)
                               + " to " + String.valueOf(end) + "): ");
            for(int i = r1.length-1; i >= 0; --i){
                 System.out.println(String.valueOf(i) + ": " + r1[i].toString());
            }
        } catch (IllegalAccessException | InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    private static <T> void
    printGetStreamSnapshotFromMarker(DataBlock block, String item, Topic topic, int beg, String mname)
            throws CLibException, LibraryNotLoaded
    {
        Method m;
        try {
            m = block.getClass().getMethod(mname,String.class,Topic.class,int.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }
        try {
            @SuppressWarnings("unchecked")
            T[] r1 = (T[])m.invoke(block,item,topic,beg);
            System.out.println(mname + "(" + item + "," + topic + "," + String.valueOf(beg) + "): ");
            for(int i = r1.length-1; i >= 0; --i){
                System.out.println(String.valueOf(i) + ": " + r1[i].toString());
            }
        } catch (IllegalAccessException | InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    private static void
    printBlockItemsTopics(DataBlock block) throws CLibException, LibraryNotLoaded {
        System.out.println("Block: " + block.getName());
        for(String item : block.getItems()) {
            System.out.println("item: " + item);
        }
        for(Topic topic : block.getTopics()) {
            System.out.println("topic: " + topic);
        }
        for(String item : block.getItemsPreCached()) {
            System.out.println("item(pre-cache): " + item);
        }
        for(Topic topic : block.getTopicsPreCached()) {
            System.out.println("topic(pre-cache): " + topic);
        }
        System.out.println();
    }

}