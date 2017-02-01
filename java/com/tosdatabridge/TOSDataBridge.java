package com.tosdatabridge;

import com.sun.jna.Native;
import com.sun.jna.NativeLong;
import sun.org.mozilla.javascript.internal.ast.Block;

import java.io.File;
import java.util.*;

public class TOSDataBridge{

    /* hardcode for time being */
    public static final int CONN_NONE = 0;
    public static final int CONN_ENGINE = 1;
    public static final int CONN_ENGINE_TOS = 2;

    public static final int INTGR_BIT = 0x80;
    public static final int QUAD_BIT = 0x40;
    public static final int STRING_BIT = 0x20;

    public static final int TOPIC_IS_LONG = 1;
    public static final int TOPIC_IS_DOUBLE = 2;
    public static final int TOPIC_IS_STRING = 3;

    public static final int MAX_STR_SZ = 0xff;
    public static final int STR_DATA_SZ = 40;

    public static final int MARKER_MARGIN_OF_SAFETY = 100;

    private static CLib library = null;

    public static class LibraryNotLoaded extends Exception{
        public LibraryNotLoaded(){
            super("library not loaded");
        }
    }

    public static class CLibException extends Exception{
        public CLibException(String callStr, int errorCode){
            super("CLib call [" + callStr + "] failed; " + CError.errorLookup(errorCode));
        }
    }

    public static class BlockDoesntSupportDateTime extends Exception{
        public BlockDoesntSupportDateTime(){
            super("block doesn't support DateTime");
        }
    }

    public static class Pair<T1,T2>{
        final T1 first;
        final T2 second;

        public
        Pair(T1 first, T2 second)
        {
            this.first = first;
            this.second = second;
        }

        public
        Pair()
        {
            this.first = null;
            this.second = null;
        }

        public String
        toString()
        {
            return "<" + String.valueOf(first) + "," + String.valueOf(second) + ">";

        }
    }

    public static boolean 
    init(String path)
    {
        if(library != null)
            return true;

        try{
            File f = new File(path);
            String n = f.getName();
            String d = f.getParentFile().getPath();
        
            int pos = n.lastIndexOf(".");
            if(pos > 0)
                n = n.substring(0,pos);

            System.out.println("directory: " + d + ", name: " + n);
            System.setProperty("jna.library.path", d);
            library = Native.loadLibrary(n, CLib.class);
        }catch(Throwable t){
            System.out.println("TOSDataBridge init() failed: " + t.toString());
            t.printStackTrace();
            return false;
        }

        return true;
    }


    public static boolean
    connect() throws LibraryNotLoaded
    {
        if(library == null)
            throw new LibraryNotLoaded();

        return library.TOSDB_Connect() == 0;
    }

    public static boolean
    connected() throws LibraryNotLoaded
    {
        if(library == null)
            throw new LibraryNotLoaded();

        return library.TOSDB_IsConnectedToEngineAndTOS() != 0;
    }

    public static int
    connectionState() throws LibraryNotLoaded
    {
        if(library == null)
            throw new LibraryNotLoaded();

        return library.TOSDB_ConnectionState();
    }

    public static int 
    getBlockLimit() throws LibraryNotLoaded
    {
        if(library == null)
            throw new LibraryNotLoaded();

        return library.TOSDB_GetBlockLimit();
    }

    public static int
    setBlockLimit(int limit) throws LibraryNotLoaded
    {
        if(library == null)
            throw new LibraryNotLoaded();

        return library.TOSDB_SetBlockLimit(limit);
    }

    public static int
    getBlockCount() throws LibraryNotLoaded
    {
        if(library == null)
            throw new LibraryNotLoaded();

        return library.TOSDB_GetBlockCount();
    }

    public static int
    getTypeBits(String topic) throws LibraryNotLoaded, CLibException
    {
        if(library == null)
            throw new LibraryNotLoaded();

        byte[] bits = {0};
        int err = library.TOSDB_GetTypeBits(topic, bits);
        if(err != 0)
            throw new CLibException("TOSDB_GetTypeBits", err);

        return bits[0] & 0xFF;
    }

    public static int
    getTopicType(String topic) throws CLibException, LibraryNotLoaded
    {
        switch(getTypeBits(topic)){
            case INTGR_BIT | QUAD_BIT:
            case INTGR_BIT:
                return TOPIC_IS_LONG;
            case QUAD_BIT:
            case 0:
                return TOPIC_IS_DOUBLE;
            default:
                return TOPIC_IS_STRING;
        }
    }

    public static class DataBlock {
        private String _name;
        private int _size;
        private boolean _dateTime;
        private int _timeout;
        private Set<String> _items;
        private Set<String> _topics;

        public DataBlock(int size, boolean dateTime, int timeout)
                throws LibraryNotLoaded, CLibException
        {
            _size = size;
            _dateTime = dateTime;
            _timeout = timeout;
            _name = UUID.randomUUID().toString();
            _items = new HashSet<>();
            _topics = new HashSet<>();

            if (library == null)
                throw new LibraryNotLoaded();

            int err = library.TOSDB_CreateBlock(_name, size, dateTime, timeout);
            if (err != 0)
                throw new CLibException("TOSDB_CreateBlock", err);

        }

        public void
        close() throws CLibException, LibraryNotLoaded
        {
            if (library == null)
                throw new LibraryNotLoaded();

            int err = library.TOSDB_CloseBlock(_name);
            if (err != 0)
                throw new CLibException("TOSDB_CloseBlock", err);
        }

        protected void
        finalize() throws Throwable
        {
            try {
                close();
            } finally {
                super.finalize();
            }
        }

        public String
        getName()
        {
            return _name;
        }

        public boolean
        isUsingDateTime()
        {
            return _dateTime;
        }

        public int
        getTimeout()
        {
            return _timeout;
        }

        public int
        getBlockSize() throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            int[] size = {0};
            int err = library.TOSDB_GetBlockSize(_name, size);
            if(err != 0)
                throw new CLibException("TOSDB_GetBlockSize", err);

            if(size[0] != _size)
                throw new IllegalStateException("size != _size");

            return size[0];
        }

        public void
        setBlockSize(int size) throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            int err = library.TOSDB_SetBlockSize(_name, size);
            if(err != 0)
                throw new CLibException("TOSDB_SetBlockSize", err);

            _size = size;
        }

        public int
        getStreamOccupancy(String item, String topic) throws CLibException, LibraryNotLoaded
        {
            if(library == null)
                throw new LibraryNotLoaded();

            item = item.toUpperCase();
            _isValidItem(item);

            topic = topic.toUpperCase();
            _isValidTopic(topic);

            int occ[] = {0};
            int err = library.TOSDB_GetStreamOccupancy(_name,item,topic,occ);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamOccupancy", err);

            return occ[0];
        }

        public Set<String>
        getItems() throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            int nItems = _getItemCount();

            String[] arrayVals = new String[nItems];
            for(int i = 0; i < nItems; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }


            int err = library.TOSDB_GetItemNames(_name, arrayVals, nItems, MAX_STR_SZ + 1);
            if(err != 0)
                throw new CLibException("TOSDB_GetItemNames", err);

            return new HashSet<>(Arrays.asList(arrayVals));
        }

        public Set<String>
        getTopics() throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            int nTopics = _getTopicCount();

            String[] arrayVals = new String[nTopics];
            for(int i = 0; i < nTopics; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }


            int err = library.TOSDB_GetTopicNames(_name, arrayVals, nTopics, MAX_STR_SZ + 1);
            if(err != 0)
                throw new CLibException("TOSDB_GetTopicNames", err);

            return new HashSet<>(Arrays.asList(arrayVals));
        }

        public String[]
        getItemsPreCached() throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            int nItems = _getItemPreCachedCount();

            String[] arrayVals = new String[nItems];
            for(int i = 0; i < nItems; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }


            int err = library.TOSDB_GetPreCachedItemNames(_name, arrayVals, nItems, MAX_STR_SZ + 1);
            if(err != 0)
                throw new CLibException("TOSDB_GetPreCachedItemNames", err);

            return arrayVals;
        }

        public String[]
        getTopicsPreCached() throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            int nTopics = _getTopicPreCachedCount();

            String[] arrayVals = new String[nTopics];
            for(int i = 0; i < nTopics; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }


            int err = library.TOSDB_GetPreCachedTopicNames(_name, arrayVals, nTopics, MAX_STR_SZ + 1);
            if(err != 0)
                throw new CLibException("TOSDB_GetPreCachedTopicNames", err);

            return arrayVals;
        }

        public void
        addItem(String item) throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            /* check we are consistent with C lib */
            if(!_items.equals(getItems()))
                throw new IllegalStateException("_items != getItems()");

            int err = library.TOSDB_AddItem(_name, item);
            if(err != 0)
                throw new CLibException("TOSDB_AddItem", err);

            /* in case topics came out of pre-cache */
            if(_items.isEmpty() && _topics.isEmpty())
                _topics = getTopics();

            _items = getItems();
        }

        public void
        addTopic(String topic) throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            /* check we are consistent with C lib */
            if(!_topics.equals(getTopics()))
                throw new IllegalStateException("_topics != getTopics()");

            int err = library.TOSDB_AddTopic(_name, topic);
            if(err != 0)
                throw new CLibException("TOSDB_AddTopic", err);

            /* in case items came out of pre-cache */
            if(_topics.isEmpty() && _items.isEmpty())
                _items = getItems();

            _topics = getTopics();
        }

        public void
        removeItem(String item) throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            /* check we are consistent with C lib */
            if(!_items.equals(getItems()))
                throw new IllegalStateException("_items != getItems()");

            int err = library.TOSDB_RemoveItem(_name, item);
            if(err != 0)
                throw new CLibException("TOSDB_RemoveItem", err);

            _items = getItems();

            /* in case topics get sent to pre-cache */
            if(_items.isEmpty())
                _topics = getTopics();
        }

        public void
        removeTopic(String topic) throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            /* check we are consistent with C lib */
            if(!_topics.equals(getTopics()))
                throw new IllegalStateException("_topics != getTopics()");

            int err = library.TOSDB_RemoveTopic(_name, topic);
            if(err != 0)
                throw new CLibException("TOSDB_RemoveTopic", err);

            _topics = getTopics();

            /* in case items get sent to pre-cache */
            if(_topics.isEmpty())
                _items = getItems();
        }

        public Long
        getLong(String item, String topic, boolean checkIndx)
                throws CLibException, LibraryNotLoaded
        {
            return this.<Long>_getLong(item,topic,0, checkIndx, false);
        }

        public Long
        getLong(String item, String topic,int indx, boolean checkIndx)
                throws CLibException, LibraryNotLoaded
        {
            return this.<Long>_getLong(item,topic, indx, checkIndx, false);
        }

        public Pair<Long, CLib.DateTimeStamp>
        getLongWithDateTime(String item, String topic, boolean checkIndx)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getLong(item,topic,0, checkIndx, true);
        }

        public Pair<Long, CLib.DateTimeStamp>
        getLongWithDateTime(String item, String topic,int indx, boolean checkIndx)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getLong(item,topic, indx, checkIndx, true);
        }

        private <T> T
        _getLong(String item, String topic,int indx, boolean checkIndx, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _GetPre pre = new _GetPre(item,topic,indx,checkIndx);

            long ptrVal[] = {0};
            CLib.DateTimeStamp[] ptrDTS =  withDateTime ? new CLib.DateTimeStamp[1] : null;

            int err = library.TOSDB_GetLongLong(
                            _name, pre.item, pre.topic, pre.indx, ptrVal, ptrDTS);
            if(err != 0)
                throw new CLibException("TOSDB_GetLongLong", err);

            return (T)(withDateTime ? new Pair<>(ptrVal[0],ptrDTS) : ptrVal[0]);
        }

        public Double
        getDouble(String item, String topic, boolean checkIndx)
                throws CLibException, LibraryNotLoaded
        {
            return this.<Double>_getDouble(item,topic,0, checkIndx, false);
        }

        public Double
        getDouble(String item, String topic,int indx, boolean checkIndx)
                throws CLibException, LibraryNotLoaded
        {
            return this.<Double>_getDouble(item,topic, indx, checkIndx, false);
        }

        public Pair<Double, CLib.DateTimeStamp>
        getDoubleWithDateTime(String item, String topic, boolean checkIndx)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getDouble(item,topic,0, checkIndx, true);
        }

        public Pair<Double, CLib.DateTimeStamp>
        getDoubleWithDateTime(String item, String topic,int indx, boolean checkIndx)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getDouble(item,topic, indx, checkIndx, true);
        }

        private <T> T
        _getDouble(String item, String topic,int indx, boolean checkIndx, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _GetPre pre = new _GetPre(item,topic,indx,checkIndx);

            double ptrVal[] = {0};
            CLib.DateTimeStamp[] ptrDTS =  withDateTime ? new CLib.DateTimeStamp[1] : null;

            int err = library.TOSDB_GetDouble(
                            _name, pre.item, pre.topic, pre.indx, ptrVal, ptrDTS);
            if(err != 0)
                throw new CLibException("TOSDB_GetDouble", err);

            return (T)(withDateTime ? new Pair<>(ptrVal[0],ptrDTS) : ptrVal[0]);
        }

        public String
        getString(String item, String topic, boolean checkIndx)
                throws CLibException, LibraryNotLoaded
        {
            return this.<String>_getString(item,topic,0, checkIndx, false);
        }

        public String
        getString(String item, String topic,int indx, boolean checkIndx)
                throws CLibException, LibraryNotLoaded
        {
            return this.<String>_getString(item,topic, indx, checkIndx, false);
        }

        public Pair<String, CLib.DateTimeStamp>
        getStringWithDateTime(String item, String topic, boolean checkIndx)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getString(item,topic,0, checkIndx, true);
        }

        public Pair<String, CLib.DateTimeStamp>
        getStringWithDateTime(String item, String topic,int indx, boolean checkIndx)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getString(item,topic, indx, checkIndx, true);
        }

        private <T> T
        _getString(String item, String topic,int indx, boolean checkIndx, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _GetPre pre = new _GetPre(item,topic,indx,checkIndx);

            String val = new String(new StringBuffer(MAX_STR_SZ+1));
            CLib.DateTimeStamp[] ptrDTS =  withDateTime ? new CLib.DateTimeStamp[1] : null;

            int err = library.TOSDB_GetString(
                            _name, pre.item, pre.topic, pre.indx,
                            val, STR_DATA_SZ + 1, ptrDTS);
            if(err != 0)
                throw new CLibException("TOSDB_GetString", err);

            return (T)(withDateTime ? new Pair<>(val,ptrDTS) : val);
        }

        public Long[]
        getStreamSnapshotLongs(String item, String topic)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Long>_getStreamSnapshotLongs(
                                item,topic,-1,0,true, false);
        }

        public Long[]
        getStreamSnapshotLongs(String item, String topic, int end)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Long>_getStreamSnapshotLongs(
                                item,topic,end,0,true, false);
        }

        public Long[]
        getStreamSnapshotLongs(String item, String topic, int end, int beg)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Long>_getStreamSnapshotLongs(
                                item,topic,end,beg,true,false);
        }

        public Pair<Long, CLib.DateTimeStamp>[]
        getStreamSnapshotLongsWithDateTime(String item, String topic)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotLongs(
                                item,topic,-1,0,true, true);
        }

        public Pair<Long, CLib.DateTimeStamp>[]
        getStreamSnapshotLongsWithDateTime(String item, String topic, int end)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotLongs(
                                item,topic,end,0,true, true);
        }

        public Pair<Long, CLib.DateTimeStamp>[]
        getStreamSnapshotLongsWithDateTime(String item, String topic, int end, int beg)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotLongs(
                                item,topic,end,beg,true,true);
        }

        private <T> T[]
        _getStreamSnapshotLongs(String item, String topic, int end, int beg,
                                  boolean smartSize, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _StreamSnapshotPre pre;
            try{
                pre = new _StreamSnapshotPre(item,topic,end,beg,smartSize);
            }catch(_StreamSnapshotPre.ReturnEmpty e){
                return (T[])(withDateTime ? new Pair[]{} : new Long[]{});
            }

            long[] arrayVals = new long[pre.size];
            CLib.DateTimeStamp[] arrayDTS = (withDateTime ? new CLib.DateTimeStamp[pre.size] : null);

            int err = library.TOSDB_GetStreamSnapshotLongLongs(
                    _name, pre.item, pre.topic, arrayVals,
                    pre.size, arrayDTS, pre.end, pre.beg);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotLongs", err);

            if(withDateTime) {
                /* 'zip' the two arrays */
                Pair<Long, CLib.DateTimeStamp> data[] = new Pair[pre.size];
                for(int i = 0; i < pre.size; ++i) {
                    data[i] = new Pair<>(arrayVals[i], arrayDTS[i]);
                }
                return (T[])data;
            }else{
                Long data[] = new Long[pre.size];
                for(int i = 0; i < pre.size; ++i){
                    data[i] = arrayVals[i];
                }
                return (T[])data;
            }
        }

        public Double[]
        getStreamSnapshotDoubles(String item, String topic)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Double>_getStreamSnapshotDoubles(
                                    item,topic,-1,0,true, false);
        }

        public Double[]
        getStreamSnapshotDoubles(String item, String topic, int end)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Double>_getStreamSnapshotDoubles(
                                    item,topic,end,0,true, false);
        }

        public Double[]
        getStreamSnapshotDoubles(String item, String topic, int end, int beg)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Double>_getStreamSnapshotDoubles(
                                    item,topic,end,beg,true,false);
        }

        public Pair<Double, CLib.DateTimeStamp>[]
        getStreamSnapshotDoublesWithDateTime(String item, String topic)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotDoubles(
                                item,topic,-1,0,true, true);
        }

        public Pair<Double, CLib.DateTimeStamp>[]
        getStreamSnapshotDoublesWithDateTime(String item, String topic, int end)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotDoubles(
                                item,topic,end,0,true, true);
        }

        public Pair<Double, CLib.DateTimeStamp>[]
        getStreamSnapshotDoublesWithDateTime(String item, String topic, int end, int beg)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotDoubles(
                                item,topic,end,beg,true,true);
        }

        private <T> T[]
        _getStreamSnapshotDoubles(String item, String topic, int end, int beg,
                                boolean smartSize, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _StreamSnapshotPre pre;
            try{
                pre = new _StreamSnapshotPre(item,topic,end,beg,smartSize);
            }catch(_StreamSnapshotPre.ReturnEmpty e){
                return (T[])(withDateTime ? new Pair[]{} : new Double[]{});
            }

            double[] arrayVals = new double[pre.size];
            CLib.DateTimeStamp[] arrayDTS = (withDateTime ? new CLib.DateTimeStamp[pre.size] : null);

            int err = library.TOSDB_GetStreamSnapshotDoubles(
                    _name, pre.item, pre.topic, arrayVals,
                    pre.size, arrayDTS, pre.end, pre.beg);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotDoubles", err);

            if(withDateTime) {
                /* 'zip' the two arrays */
                Pair<Double, CLib.DateTimeStamp> data[] = new Pair[pre.size];
                for(int i = 0; i < pre.size; ++i) {
                    data[i] = new Pair<>(arrayVals[i], arrayDTS[i]);
                }
                return (T[])data;
            }else{
                Double data[] = new Double[pre.size];
                for(int i = 0; i < pre.size; ++i){
                    data[i] = arrayVals[i];
                }
                return (T[])data;
            }
        }

        public String[]
        getStreamSnapshotStrings(String item, String topic)
                throws LibraryNotLoaded, CLibException
        {
            return this.<String>_getStreamSnapshotStrings(
                                    item,topic,-1,0,true, false);
        }

        public String[]
        getStreamSnapshotStrings(String item, String topic, int end)
                throws LibraryNotLoaded, CLibException
        {
            return this.<String>_getStreamSnapshotStrings(
                                    item,topic,end,0,true, false);
        }

        public String[]
        getStreamSnapshotStrings(String item, String topic, int end, int beg)
                throws LibraryNotLoaded, CLibException
        {
            return this.<String>_getStreamSnapshotStrings(
                                    item,topic,end,beg,true,false);
        }

        public Pair<String, CLib.DateTimeStamp>[]
        getStreamSnapshotStringsWithDateTime(String item, String topic)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotStrings(
                                item,topic,-1,0,true, true);
        }

        public Pair<String, CLib.DateTimeStamp>[]
        getStreamSnapshotStringsWithDateTime(String item, String topic, int end)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotStrings(
                                item,topic,end,0,true, true);
        }

        public Pair<String, CLib.DateTimeStamp>[]
        getStreamSnapshotStringsWithDateTime(String item, String topic, int end, int beg)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotStrings(
                            item,topic,end,beg,true,true);
        }

        private <T> T[]
        _getStreamSnapshotStrings(String item, String topic, int end, int beg,
                                boolean smartSize, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _StreamSnapshotPre pre;
            try{
                pre = new _StreamSnapshotPre(item,topic,end,beg,smartSize);
            }catch(_StreamSnapshotPre.ReturnEmpty e){
                return (T[])(withDateTime ? new Pair[]{} : new String[]{});
            }

            String[] arrayVals = new String[pre.size];
            for(int i = 0; i < pre.size; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }

            CLib.DateTimeStamp[] arrayDTS =
                    (withDateTime ? new CLib.DateTimeStamp[pre.size] : null);

            int err = library.TOSDB_GetStreamSnapshotStrings(
                            _name, pre.item, pre.topic, arrayVals,
                            pre.size,  STR_DATA_SZ + 1, arrayDTS, pre.end, pre.beg);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotStrings", err);

            if(withDateTime) {
                /* 'zip' the two arrays */
                Pair<String, CLib.DateTimeStamp> data[] = new Pair[pre.size];
                for(int i = 0; i < pre.size; ++i) {
                    data[i] = new Pair<>(arrayVals[i], arrayDTS[i]);
                }
                return (T[])data;
            }else{
                String data[] = new String[pre.size];
                for(int i = 0; i < pre.size; ++i){
                    data[i] = arrayVals[i];
                }
                return (T[])data;
            }
        }

        public Long[]
        getStreamSnapshotLongsFromMarker(String item, String topic)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Long>_getStreamSnapshotLongsFromMarker(
                                item,topic,0,true,false);
        }

        public Long[]
        getStreamSnapshotLongsFromMarker(String item, String topic, int beg)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Long>_getStreamSnapshotLongsFromMarker(
                                item,topic,beg,true,false);
        }

        public Pair<Long,CLib.DateTimeStamp>[]
        getStreamSnapshotLongsFromMarkerWithDateTime(String item, String topic)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotLongsFromMarker(
                                item,topic,0,true,true);
        }

        public Pair<Long,CLib.DateTimeStamp>[]
        getStreamSnapshotLongsFromMarkerWithDateTime(String item, String topic, int beg)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotLongsFromMarker(
                                item,topic,beg,true,true);
        }

        private <T> T[]
        _getStreamSnapshotLongsFromMarker(String item, String topic, int beg,
                                          boolean throwIfDataLost, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _StreamSnapshotMarkerPre pre;
            try {
                pre = new _StreamSnapshotMarkerPre(item, topic, beg, throwIfDataLost);
            }catch(_StreamSnapshotMarkerPre.ReturnEmpty e){
                return (T[])(withDateTime ? new Pair[]{} : new Long[]{});
            }

            long[] arrayVals = new long[pre.safeSz];
            CLib.DateTimeStamp[] arrayDTS =
                    (withDateTime ? new CLib.DateTimeStamp[pre.safeSz] : null);
            NativeLong[] ptrGetSz = {new NativeLong(0)};

            int err = library.TOSDB_GetStreamSnapshotLongLongsFromMarker(
                            _name, pre.item, pre.topic, arrayVals,
                            pre.safeSz, arrayDTS, pre.beg, ptrGetSz);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotLongLongsFromMarker", err);

            long szGot = ptrGetSz[0].longValue();
            if(szGot == 0)
                return (T[])(withDateTime ? new Pair[]{} : new Long[]{});
            else if(szGot < 0){
                if(throwIfDataLost)
                    throw new IllegalStateException("data lost behind marker");
                else
                    szGot *= -1;
            }

            if(withDateTime) {
                /* 'zip' the two arrays */
                Pair<Long, CLib.DateTimeStamp> data[] = new Pair[(int)szGot];
                for(int i = 0; i < (int)szGot; ++i) {
                    data[i] = new Pair<>(arrayVals[i], arrayDTS[i]);
                }
                return (T[])data;
            }else{
                Long data[] = new Long[(int)szGot];
                for(int i = 0; i < (int)szGot; ++i){
                    data[i] = arrayVals[i];
                }
                return (T[])data;
            }
        }

        public Double[]
        getStreamSnapshotDoublesFromMarker(String item, String topic)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Double>_getStreamSnapshotDoublesFromMarker(
                                    item,topic,0,true,false);
        }

        public Double[]
        getStreamSnapshotDoublesFromMarker(String item, String topic, int beg)
                throws LibraryNotLoaded, CLibException
        {
            return this.<Double>_getStreamSnapshotDoublesFromMarker(
                                    item,topic,beg,true,false);
        }

        public Pair<Double,CLib.DateTimeStamp>[]
        getStreamSnapshotDoublesFromMarkerWithDateTime(String item, String topic)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotDoublesFromMarker(
                                item,topic,0,true,true);
        }

        public Pair<Double,CLib.DateTimeStamp>[]
        getStreamSnapshotDoublesFromMarkerWithDateTime(String item, String topic, int beg)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotDoublesFromMarker(
                                item,topic,beg,true,true);
        }

        private <T> T[]
        _getStreamSnapshotDoublesFromMarker(String item, String topic, int beg,
                                          boolean throwIfDataLost, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _StreamSnapshotMarkerPre pre;
            try {
                pre = new _StreamSnapshotMarkerPre(item, topic, beg, throwIfDataLost);
            }catch(_StreamSnapshotMarkerPre.ReturnEmpty e){
                return (T[])(withDateTime ? new Pair[]{} : new Double[]{});
            }

            double[] arrayVals = new double[pre.safeSz];
            CLib.DateTimeStamp[] arrayDTS =
                    (withDateTime ? new CLib.DateTimeStamp[pre.safeSz] : null);
            NativeLong[] ptrGetSz = {new NativeLong(0)};

            int err = library.TOSDB_GetStreamSnapshotDoublesFromMarker(
                            _name, pre.item, pre.topic, arrayVals,
                            pre.safeSz, arrayDTS, pre.beg, ptrGetSz);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotDoublesFromMarker", err);

            long szGot = ptrGetSz[0].longValue();
            if(szGot == 0)
                return (T[])(withDateTime ? new Pair[]{} : new Double[]{});
            else if(szGot < 0){
                if(throwIfDataLost)
                    throw new IllegalStateException("data lost behind marker");
                else
                    szGot *= -1;
            }

            if(withDateTime) {
                /* 'zip' the two arrays */
                Pair<Double, CLib.DateTimeStamp> data[] = new Pair[(int)szGot];
                for(int i = 0; i < (int)szGot; ++i) {
                    data[i] = new Pair<>(arrayVals[i], arrayDTS[i]);
                }
                return (T[])data;
            }else{
                Double data[] = new Double[(int)szGot];
                for(int i = 0; i < (int)szGot; ++i){
                    data[i] = arrayVals[i];
                }
                return (T[])data;
            }
        }


        public String[]
        getStreamSnapshotStringsFromMarker(String item, String topic)
                throws LibraryNotLoaded, CLibException
        {
            return this.<String>_getStreamSnapshotStringsFromMarker(
                    item,topic,0,true,false);
        }

        public String[]
        getStreamSnapshotStringsFromMarker(String item, String topic, int beg)
                throws LibraryNotLoaded, CLibException
        {
            return this.<String>_getStreamSnapshotStringsFromMarker(
                    item,topic,beg,true,false);
        }

        public Pair<String,CLib.DateTimeStamp>[]
        getStreamSnapshotStringsFromMarkerWithDateTime(String item, String topic)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotStringsFromMarker(
                    item,topic,0,true,true);
        }

        public Pair<String,CLib.DateTimeStamp>[]
        getStreamSnapshotStringsFromMarkerWithDateTime(String item, String topic, int beg)
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Pair>_getStreamSnapshotStringsFromMarker(
                    item,topic,beg,true,true);
        }

        private <T> T[]
        _getStreamSnapshotStringsFromMarker(String item, String topic, int beg,
                                            boolean throwIfDataLost, boolean withDateTime)
                throws LibraryNotLoaded, CLibException
        {
            _StreamSnapshotMarkerPre pre;
            try {
                pre = new _StreamSnapshotMarkerPre(item, topic, beg, throwIfDataLost);
            }catch(_StreamSnapshotMarkerPre.ReturnEmpty e){
                return (T[])(withDateTime ? new Pair[]{} : new String[]{});
            }

            String[] arrayVals = new String[pre.safeSz];
            for(int i = 0; i < pre.safeSz; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }

            CLib.DateTimeStamp[] arrayDTS =
                    (withDateTime ? new CLib.DateTimeStamp[pre.safeSz] : null);
            NativeLong[] ptrGetSz = {new NativeLong(0)};

            int err = library.TOSDB_GetStreamSnapshotStringsFromMarker(
                            _name, pre.item, pre.topic, arrayVals, pre.safeSz,
                             STR_DATA_SZ + 1, arrayDTS, pre.beg, ptrGetSz);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotStringsFromMarker", err);

            long szGot = ptrGetSz[0].longValue();
            if(szGot == 0)
                return (T[])(withDateTime ? new Pair[]{} : new String[]{});
            else if(szGot < 0){
                if(throwIfDataLost)
                    throw new IllegalStateException("data lost behind marker");
                else
                    szGot *= -1;
            }

            if(withDateTime) {
                /* 'zip' the two arrays */
                Pair<String, CLib.DateTimeStamp> data[] = new Pair[(int)szGot];
                for(int i = 0; i < (int)szGot; ++i) {
                    data[i] = new Pair<>(arrayVals[i], arrayDTS[i]);
                }
                return (T[])data;
            }else{
                String data[] = new String[(int)szGot];
                for(int i = 0; i < (int)szGot; ++i){
                    data[i] = arrayVals[i];
                }
                return (T[])data;
            }
        }

        public Map<String,String>
        getItemFrame(String topic) throws CLibException, LibraryNotLoaded
        {
            return this.<Map>_getFrame(topic, true,false);
        }

        public Map<String,Pair<String,CLib.DateTimeStamp>>
        getItemFrameWithDateTime(String topic)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Map>_getFrame(topic, true,true);
        }

        public Map<String,String>
        getTopicFrame(String item) throws CLibException, LibraryNotLoaded
        {
            return this.<Map>_getFrame(item, false, false);
        }

        public Map<String,Pair<String,CLib.DateTimeStamp>>
        getTopicFrameWithDateTime(String item)
                throws CLibException, LibraryNotLoaded, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            return this.<Map>_getFrame(item, false,true);
        }

        private <T> T
        _getFrame(String s, boolean itemFrame, boolean withDateTime)
                throws CLibException, LibraryNotLoaded
        {
            if(library == null)
                throw new LibraryNotLoaded();

            s = s.toUpperCase();

            int n;
            if(itemFrame) {
                _isValidTopic(s);
                n = _getItemCount();
            }else{
                _isValidItem(s);
                n = _getTopicCount();
            }

            String arrayVals[] = new String[n];
            for(int i = 0; i < n; ++i){
                arrayVals[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }

            String arrayLabels[] = new String[n];
            for(int i = 0; i < n; ++i){
                arrayLabels[i] = new String(new StringBuffer(MAX_STR_SZ+1));
            }

            CLib.DateTimeStamp[] arrayDTS = (withDateTime ? new CLib.DateTimeStamp[n] : null);

            if(itemFrame){
                int err = library.TOSDB_GetItemFrameStrings(
                                    _name, s, arrayVals, n, STR_DATA_SZ + 1,
                                    arrayLabels, MAX_STR_SZ + 1, arrayDTS);
                if(err != 0)
                    throw new CLibException("TOSDB_GetItemFrameStrings", err);
            }else {
                 int err = library.TOSDB_GetTopicFrameStrings(
                                    _name, s, arrayVals, n, STR_DATA_SZ + 1,
                                    arrayLabels, MAX_STR_SZ + 1, arrayDTS);
                if(err != 0)
                    throw new CLibException("TOSDB_GetTopicFrameStrings", err);
            }

            if(withDateTime) {
                Map<String, Pair<String, CLib.DateTimeStamp>> frame = new HashMap<>();
                for (int i = 0; i < n; ++i) {
                    frame.put(arrayLabels[i], new Pair(arrayVals[i], arrayDTS[i]));
                }
                return (T)frame;
            }else{
                Map<String,String> frame = new HashMap<>();
                for(int i = 0; i < n; ++i){
                    frame.put(arrayLabels[i],arrayVals[i]);
                }
                return (T)frame;
            }
        }

        public Map<String, Map<String,String>>
        getTotalFrame() throws LibraryNotLoaded, CLibException
        {
            if(library == null)
                throw new LibraryNotLoaded();

            Map<String, Map<String,String>> frame = new HashMap<>();
            for(String item : getItems()){
                Map<String,String> tf = getTopicFrame(item);
                frame.put(item, tf);
            }

            return frame;
        }

        public Map<String, Map<String,Pair<String,CLib.DateTimeStamp>>>
        getTotalFrameWithDateTime()
                throws LibraryNotLoaded, CLibException, BlockDoesntSupportDateTime
        {
            if(!_dateTime)
                throw new BlockDoesntSupportDateTime();

            if(library == null)
                throw new LibraryNotLoaded();

            Map<String, Map<String,Pair<String,CLib.DateTimeStamp>>> frame = new HashMap<>();
            for(String item : getItems()){
                Map<String,Pair<String,CLib.DateTimeStamp>> tf
                        = getTopicFrameWithDateTime(item);
                frame.put(item, tf);
            }

            return frame;
        }

        private void
        _isValidItem(String item) throws IllegalArgumentException, CLibException, LibraryNotLoaded
        {
            if(_items.isEmpty())
                _items = getItems();

            if(!_items.contains(item))
                throw new IllegalArgumentException("item " + item + " not found");
        }

        private void
        _isValidTopic(String topic) throws IllegalArgumentException, CLibException, LibraryNotLoaded
        {
            if(_topics.isEmpty())
                _topics = getTopics();

            if(!_topics.contains(topic))
                throw new IllegalArgumentException("topic " + topic + " not found");
        }

        private int
        _getItemCount() throws CLibException
        {
            int[] count = {0};
            int err = library.TOSDB_GetItemCount(_name, count);
            if(err != 0)
                throw new CLibException("TOSDB_GetItemCount", err);

            return count[0];
        }

        private int
        _getTopicCount() throws CLibException
        {
            int[] count = {0};
            int err = library.TOSDB_GetTopicCount(_name, count);
            if(err != 0)
                throw new CLibException("TOSDB_GetTopicCount", err);

            return count[0];
        }

        private int
        _getItemPreCachedCount() throws CLibException
        {
            int[] count = {0};
            int err = library.TOSDB_GetPreCachedItemCount(_name, count);
            if(err != 0)
                throw new CLibException("TOSDB_GetPreCachedItemCount", err);

            return count[0];
        }

        private int
        _getTopicPreCachedCount() throws CLibException
        {
            int[] count = {0};
            int err = library.TOSDB_GetPreCachedTopicCount(_name, count);
            if(err != 0)
                throw new CLibException("TOSDB_GetPreCachedTopicCount", err);

            return count[0];
        }

        private class _BasePre {
            String item;
            String topic;

            public class ReturnEmpty extends Exception{
            }

            public _BasePre(String item, String topic) throws LibraryNotLoaded, CLibException
            {
                if(library == null)
                    throw new LibraryNotLoaded();

                this.item = item.toUpperCase();
                this.topic = topic.toUpperCase();

                _isValidItem(this.item);
                _isValidTopic(this.topic);
            }
        }

        private class _GetPre extends _BasePre {
            int indx;

            public
            _GetPre(String item, String topic, int indx, boolean checkIndx)
                    throws LibraryNotLoaded, CLibException
            {
                super(item,topic);

                this.indx = indx;

                if(this.indx < 0)
                    this.indx += _size;

                if(this.indx >= _size)
                    throw new IndexOutOfBoundsException("invalid indx");

                if(checkIndx && this.indx > getStreamOccupancy(this.item,this.topic))
                    throw new IllegalStateException("data not available at this index yet"
                            + " (disable checkIndx to avoid this exception)");
            }
        }

        private class _StreamSnapshotPre extends _BasePre {
            int size;
            int beg;
            int end;

            public
            _StreamSnapshotPre(String item, String topic, int end, int beg, boolean smartSize)
                    throws LibraryNotLoaded, CLibException, ReturnEmpty
            {
                super(item,topic);

                this.end = end;
                this.beg = beg;

                if(this.end < 0)
                    this.end += _size;

                if(this.beg < 0)
                    this.beg += _size;

                this.size = this.end - this.beg + 1;

                if(this.beg < 0 || this. end < 0
                        || this.beg >= _size || this.end >= _size
                        || this.size <= 0)
                {
                    throw new IndexOutOfBoundsException("invalid 'beg' and/or 'end' index");
                }

                if(smartSize){
                    int occ = getStreamOccupancy(this.item, this.topic);
                    if(occ == 0 || occ <= this.beg)
                        throw new ReturnEmpty();
                    this.end = Math.min(this.end, occ-1);
                    this.beg = Math.min(this.beg, occ-1);
                    this.size = this.end - this.beg + 1;
                }
            }
        }

        private class _StreamSnapshotMarkerPre extends _BasePre {
            int safeSz;
            int beg;

            public
            _StreamSnapshotMarkerPre(String item, String topic, int beg, boolean throwIfDataLost)
                    throws LibraryNotLoaded, CLibException, ReturnEmpty
            {
                super(item,topic);

                this.beg = beg;

                if(this.beg < 0)
                    this.beg += _size;

                if(this.beg < 0 || this.beg >= _size)
                    throw new IndexOutOfBoundsException("invalid 'beg' index");

                int[] ptrIsDirty = {0};
                int err = library.TOSDB_IsMarkerDirty(_name,this.item,this.topic,ptrIsDirty);
                if(err != 0)
                    throw new CLibException("TOSDB_IsMarkerDirty", err);

                if(ptrIsDirty[0] != 0 && throwIfDataLost)
                    throw new IllegalStateException("marker is already dirty");

                long[] ptrMarkerPos = {0};
                err = library.TOSDB_GetMarkerPosition(_name,this.item,this.topic,ptrMarkerPos);
                if(err != 0)
                    throw new CLibException("TOSDB_GetMarkerPosition", err);

                long curSz = ptrMarkerPos[0] - this.beg + 1;

                if(curSz < 0)
                    throw new ReturnEmpty();

                // NEED TO CHECK WE CAN GO FROM LONG TO INT
                this.safeSz = (int)curSz + MARKER_MARGIN_OF_SAFETY;
            }
        }


    }
}