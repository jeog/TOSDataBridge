
import com.tosdatabridge.CLib;
import com.tosdatabridge.DateTime;
import com.tosdatabridge.TOSDataBridge;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;


public class Test{    

    private static final String TOSDB_LIB_PATH =
        "C:/users/j/documents/github/tosdatabridge/bin/Release/x64/tos-databridge-0.7-x64.dll";

    private static final int SLEEP_PERIOD = 1000;

    public static void
    main(String[] args)
    {
        TOSDataBridge.init(TOSDB_LIB_PATH);

        try{
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
                return;
            }

            // get_block_limit()
            int l = TOSDataBridge.getBlockLimit();
            System.out.println("block limit: " + String.valueOf(l));

            // set_block_limit()
            System.out.println("set block limit to: " +String.valueOf(l*2));
            TOSDataBridge.setBlockLimit(l*2);

            // get_block_limit()
            l = TOSDataBridge.getBlockLimit();
            System.out.println("block limit: " + String.valueOf(l));

            // get_block_count()
            int c = TOSDataBridge.getBlockCount();
            System.out.println("block count: " + String.valueOf(c));

            // type_bits()
            int b = TOSDataBridge.getTypeBits("LAST");
            System.out.println("Type Bits for 'LAST': " + String.valueOf(b));

            b = TOSDataBridge.getTopicType("LAST");
            System.out.println("Type value for 'LAST': " + String.valueOf(b));

            int block1Sz = 10000;
            boolean block1DateTime = true;
            int block1Timeout = 3000;

            TOSDataBridge.DataBlock block1 =
                new TOSDataBridge.DataBlock(block1Sz, block1DateTime, block1Timeout);

            System.out.println("Create block: " + block1.getName());

            if(block1.getBlockSize() != block1Sz){
                System.out.println("invalid block size: "
                                    + String.valueOf(block1.getBlockSize()) + ", "
                                    + String.valueOf(block1Sz));
                return;
            }

            if(block1.isUsingDateTime() != block1DateTime){
                System.out.println("invalid block DateTime: "
                        + String.valueOf(block1.isUsingDateTime()) + ", "
                        + String.valueOf(block1DateTime));
                return;
            }

            if(block1.getTimeout() != block1Timeout){
                System.out.println("invalid block timeout: "
                        + String.valueOf(block1.getTimeout()) + ", "
                        + String.valueOf(block1Timeout));
                return;
            }

            System.out.println("Block: " + block1.getName());
            System.out.println("using datetime: " + String.valueOf(block1.isUsingDateTime()));
            System.out.println("timeout: " + String.valueOf(block1.getTimeout()));
            System.out.println("block size: " + String.valueOf(block1.getBlockSize()));

            System.out.println("Double block size...");
            block1.setBlockSize(block1.getBlockSize() * 2);
            System.out.println("block size: " + String.valueOf(block1.getBlockSize()));

            String item1 = "SPY";
            String item2 = "QQQ";
            String item3 = "IWM";

            String topic1 = "LAST"; // double
            String topic2 = "VOLUME"; // long
            String topic3 = "LASTX"; // string

            System.out.println("Add item: " + item1);
            block1.addItem(item1);
            printBlockItemsTopics(block1);

            System.out.println("Add item: " + item2);
            block1.addItem(item2);
            printBlockItemsTopics(block1);

            System.out.println("Add topic: " + topic1);
            block1.addTopic(topic1);
            printBlockItemsTopics(block1);

            System.out.println("Remove item: " + item1);
            block1.removeItem(item1);
            printBlockItemsTopics(block1);

            System.out.println("Remove Topic: " + topic1);
            block1.removeTopic(topic1);
            printBlockItemsTopics(block1);

            System.out.println("Add ALL items");
            block1.addItem(item1);
            block1.addItem(item2);
            block1.addItem(item3);
            printBlockItemsTopics(block1);

            System.out.println("Add ALL topics");
            block1.addTopic(topic1);
            block1.addTopic(topic2);
            block1.addTopic(topic3);
            printBlockItemsTopics(block1);

            Thread.sleep(SLEEP_PERIOD*3);
            System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD*3) + " MILLISECONDS***");

            System.out.println();
            System.out.println("TEST GET CALLS:");
            Test.<Long>printGet(block1,item1,topic2,1,"getLong");
            Test.<Long>printGet(block1,item1,topic2,0,"getLong");
            Test.<TOSDataBridge.Pair<Long,DateTime>>printGet(block1,item1,topic2,1,"getLongWithDateTime");
            Test.<TOSDataBridge.Pair<Long,DateTime>>printGet(block1,item1,topic2,0,"getLongWithDateTime");
            Test.<Double>printGet(block1,item2,topic1,1,"getDouble");
            Test.<Double>printGet(block1,item2,topic1,0,"getDouble");
            Test.<TOSDataBridge.Pair<Double,DateTime>>printGet(block1,item2,topic1,1,"getDoubleWithDateTime");
            Test.<TOSDataBridge.Pair<Double,DateTime>>printGet(block1,item2,topic1,0,"getDoubleWithDateTime");
            Test.<String>printGet(block1,item3,topic3,1,"getString");
            Test.<String>printGet(block1,item3,topic3,0,"getString");
            Test.<TOSDataBridge.Pair<String,DateTime>>printGet(block1,item3,topic3,1,"getStringWithDateTime");
            Test.<TOSDataBridge.Pair<String,DateTime>>printGet(block1,item3,topic3,0,"getStringWithDateTime");

            System.out.println();
            System.out.println("TEST STREAM SNAPSHOT CALLS:");
            Test.<Long>printGetStreamSnapshot(block1,item1,topic2,5,0,"getStreamSnapshotLongs");
            Test.<TOSDataBridge.Pair<Long,DateTime>>printGetStreamSnapshot(
                    block1,item1,topic2,5,0,"getStreamSnapshotLongsWithDateTime");
            Test.<Double>printGetStreamSnapshot(block1,item2,topic1,5,0,"getStreamSnapshotDoubles");
            Test.<TOSDataBridge.Pair<Double,DateTime>>printGetStreamSnapshot(
                    block1,item2,topic1,5,0,"getStreamSnapshotDoublesWithDateTime");
            Test.<String>printGetStreamSnapshot(block1,item3,topic3,5,0,"getStreamSnapshotStrings");
            Test.<TOSDataBridge.Pair<String,DateTime>>printGetStreamSnapshot(
                    block1,item3,topic3,5,0,"getStreamSnapshotStringsWithDateTime");

            System.out.println();
            System.out.println("TEST STREAM SNAPSHOT FROM MARKER CALLS:");
            Test.<Long>printGetStreamSnapshotFromMarker(block1,item1,topic2,0,"getStreamSnapshotLongsFromMarker");
            Thread.sleep(SLEEP_PERIOD);
            System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
            Test.<TOSDataBridge.Pair<Long,DateTime>>printGetStreamSnapshotFromMarker(
                    block1,item1,topic2,0,"getStreamSnapshotLongsFromMarkerWithDateTime");
            Thread.sleep(SLEEP_PERIOD);
            System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
            Test.<Double>printGetStreamSnapshotFromMarker(block1,item2,topic1,0,"getStreamSnapshotDoublesFromMarker");
            Thread.sleep(SLEEP_PERIOD);
            System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
            Test.<TOSDataBridge.Pair<Double,DateTime>>printGetStreamSnapshotFromMarker(
                    block1,item2,topic1,0,"getStreamSnapshotDoublesFromMarkerWithDateTime");
            Thread.sleep(SLEEP_PERIOD);
            System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
            Test.<String>printGetStreamSnapshotFromMarker(block1,item3,topic3,0,"getStreamSnapshotStringsFromMarker");
            Thread.sleep(SLEEP_PERIOD);
            System.out.println("***SLEEP FOR " + String.valueOf(SLEEP_PERIOD) + " MILLISECONDS***");
            Test.<TOSDataBridge.Pair<String,DateTime>>printGetStreamSnapshotFromMarker(
                    block1,item3,topic3,0,"getStreamSnapshotStringsFromMarkerWithDateTime");


        }catch(TOSDataBridge.LibraryNotLoaded e){
            System.out.println("EXCEPTION: LibraryNotLoaded");
            System.out.println(e.toString());
            e.printStackTrace();
        }catch(TOSDataBridge.CLibException e){
            System.out.println("EXCEPTION: CLibException");
            System.out.println(e.toString());
            e.printStackTrace();
        } catch (TOSDataBridge.BlockDoesntSupportDateTime e) {
            System.out.println("EXCEPTION: BlockDoesntSupportDateTime");
            System.out.println(e.toString());
            e.printStackTrace();
        } catch (InterruptedException e) {
            System.out.println("EXCEPTION: InterruptedException");
            System.out.println(e.toString());
            e.printStackTrace();
        }
    }


    public static <T> void
    printGet(TOSDataBridge.DataBlock block, String item, String topic, int indx, String mname)
            throws TOSDataBridge.CLibException, TOSDataBridge.LibraryNotLoaded,
                   TOSDataBridge.BlockDoesntSupportDateTime
    {
        Method m;
        try {
            m = block.getClass().getMethod(mname,String.class,String.class,int.class,boolean.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }

        try {
            T r1 = (T)m.invoke(block,item,topic,indx,true);
            System.out.println(mname + "(" + item + "," + topic + "," + String.valueOf(indx)
                                     + "): " + r1.toString());
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    public static <T> void
    printGetStreamSnapshot(TOSDataBridge.DataBlock block, String item,
                           String topic, int end, int beg, String mname)
            throws TOSDataBridge.CLibException, TOSDataBridge.LibraryNotLoaded,
            TOSDataBridge.BlockDoesntSupportDateTime
    {
        Method m;
        try {
            m = block.getClass().getMethod(mname,String.class,String.class,int.class,int.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }

        try {
            T[] r1 = (T[])m.invoke(block,item,topic,end,beg);
            System.out.println(mname + "(" + item + "," + topic + "," + String.valueOf(beg)
                               + "-" + String.valueOf(end) + "): ");
            for(int i = 0; i < r1.length; ++i){
                 System.out.println(String.valueOf(i) + ": " + r1[i].toString());
            }
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    public static <T> void
    printGetStreamSnapshotFromMarker(TOSDataBridge.DataBlock block, String item,
                                     String topic, int beg, String mname)
            throws TOSDataBridge.CLibException, TOSDataBridge.LibraryNotLoaded,
            TOSDataBridge.BlockDoesntSupportDateTime
    {
        Method m;
        try {
            m = block.getClass().getMethod(mname,String.class,String.class,int.class);
        } catch (NoSuchMethodException e) {
            e.printStackTrace();
            return;
        }

        try {
            T[] r1 = (T[])m.invoke(block,item,topic,beg);
            System.out.println(mname + "(" + item + "," + topic + "," + String.valueOf(beg) + "): ");
            for(int i = 0; i < r1.length; ++i){
                System.out.println(String.valueOf(i) + ": " + r1[i].toString());
            }
        } catch (IllegalAccessException e) {
            e.printStackTrace();
        } catch (InvocationTargetException e) {
            e.printStackTrace();
        }
    }

    public static void
    printBlockItemsTopics(TOSDataBridge.DataBlock block)
            throws TOSDataBridge.CLibException, TOSDataBridge.LibraryNotLoaded
    {
        System.out.println("Block: " + block.getName());
        for(String item : block.getItems())
            System.out.println("item: " + item);
        for(String topic : block.getTopics())
            System.out.println("topic: " + topic);
        for(String item : block.getItemsPreCached())
            System.out.println("item(pre-cache): " + item);
        for(String topic : block.getTopicsPreCached())
            System.out.println("topic(pre-cache): " + topic);
        System.out.println();
    }

}