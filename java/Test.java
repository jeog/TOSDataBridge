
import com.tosdatabridge.CLib;
import com.tosdatabridge.TOSDataBridge;


public class Test{    

    private static final String TOSDB_LIB_PATH =
        "C:/users/j/documents/github/tosdatabridge/bin/Release/x64/tos-databridge-0.7-x64.dll";

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

            System.out.println("TEST GET CALLS:");
            long g = block1.getLong(item1,topic2,true);
            System.out.println("GetLong (" + item1 + "," + topic2 + "): " + String.valueOf(g));
            g = block1.getLong(item1,topic2,1,true);
            System.out.println("GetLong (" + item1 + "," + topic2 + "1): " + String.valueOf(g));
            TOSDataBridge.Pair<Long,CLib.DateTimeStamp> p =
                    block1.getLongWithDateTime(item1,topic2,true);
            System.out.println("GetLong (" + item1 + "," + topic2 + "): " + p.toString());
            p = block1.getLongWithDateTime(item1,topic2,1,true);
            System.out.println("GetLong (" + item1 + "," + topic2 + "1): " + p.toString());


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