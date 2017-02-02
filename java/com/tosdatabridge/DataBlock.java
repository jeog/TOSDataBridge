package com.tosdatabridge;

import com.tosdatabridge.TOSDataBridge.*;

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;

import java.lang.reflect.Array;
import java.util.*;

public class DataBlock {
    /* max size of non-data (e.g topics, labels) strings (excluding \0)*/
    public static final int MAX_STR_SZ = TOSDataBridge.MAX_STR_SZ;
    /* max size of data strings (getString, getStreamSnapshotStrings etc.) */
    public static final int STR_DATA_SZ = TOSDataBridge.STR_DATA_SZ;

    /* how much buffer 'padding' for the marker calls (careful changing default) */
    public static int MARKER_MARGIN_OF_SAFETY = TOSDataBridge.MARKER_MARGIN_OF_SAFETY;

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

        int err = TOSDataBridge.getCLibrary().TOSDB_CreateBlock(_name, size, dateTime, timeout);
        if (err != 0)
            throw new CLibException("TOSDB_CreateBlock", err);

    }

    public void
    close() throws CLibException, LibraryNotLoaded
    {
        int err = TOSDataBridge.getCLibrary().TOSDB_CloseBlock(_name);
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
        int[] size = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetBlockSize(_name, size);
        if(err != 0)
            throw new CLibException("TOSDB_GetBlockSize", err);

        if(size[0] != _size)
            throw new IllegalStateException("size != _size");

        return size[0];
    }

    public void
    setBlockSize(int size) throws LibraryNotLoaded, CLibException
    {
        int err = TOSDataBridge.getCLibrary().TOSDB_SetBlockSize(_name, size);
        if(err != 0)
            throw new CLibException("TOSDB_SetBlockSize", err);

        _size = size;
    }

    public int
    getStreamOccupancy(String item, String topic) throws CLibException, LibraryNotLoaded
    {
        item = item.toUpperCase();
        _isValidItem(item);

        topic = topic.toUpperCase();
        _isValidTopic(topic);

        int occ[] = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetStreamOccupancy(_name,item,topic,occ);
        if(err != 0)
            throw new CLibException("TOSDB_GetStreamOccupancy", err);

        return occ[0];
    }


    private Set<String>
    _getItemTopicNames(boolean useItemVersion, boolean usePreCachedVersion)
            throws LibraryNotLoaded, CLibException
    {
        int n = 0;
        if (useItemVersion) {
            n = usePreCachedVersion ? _getItemPreCachedCount() : _getItemCount();
        }else{
            n = usePreCachedVersion ? _getTopicPreCachedCount() : _getTopicCount();
        }

        if(n < 1)
            return new HashSet<>();

        Pointer[] arrayVals = new Pointer[n];
        for(int i = 0; i < n; ++i){
            arrayVals[i] = new Memory(MAX_STR_SZ+1);
        }

        int err;
        final CLib lib = TOSDataBridge.getCLibrary();
        if (useItemVersion) {
            err = usePreCachedVersion
                    ? lib.TOSDB_GetPreCachedItemNames(_name, arrayVals, n, MAX_STR_SZ)
                    : lib.TOSDB_GetItemNames(_name, arrayVals, n, MAX_STR_SZ);

        }else{
            err = usePreCachedVersion
                    ? lib.TOSDB_GetPreCachedTopicNames(_name, arrayVals, n, MAX_STR_SZ)
                    : lib.TOSDB_GetTopicNames(_name, arrayVals, n, MAX_STR_SZ);
        }

        if(err != 0) {
            throw new CLibException("TOSDB_Get" + (usePreCachedVersion ? "PreCached" : "")
                    + (useItemVersion ? "Item" : "Topic") + "Names", err);
        }

        Set<String> strVals = new HashSet<>();
        for(int i = 0; i < n; ++i){
            strVals.add(arrayVals[i].getString(0));
        }
        return strVals;
    }

    public Set<String>
    getItems() throws LibraryNotLoaded, CLibException
    {
        return _getItemTopicNames(true,false);
    }

    public Set<String>
    getTopics() throws LibraryNotLoaded, CLibException
    {
        return _getItemTopicNames(false,false);
    }

    public Set<String>
    getItemsPreCached() throws LibraryNotLoaded, CLibException
    {
        return _getItemTopicNames(true,true);
    }

    public Set<String>
    getTopicsPreCached() throws LibraryNotLoaded, CLibException
    {
        return _getItemTopicNames(false,true);
    }

    public void
    addItem(String item) throws LibraryNotLoaded, CLibException
    {
        /* check we are consistent with C lib */
        if(!_items.equals(getItems()))
            throw new IllegalStateException("_items != getItems()");

        int err = TOSDataBridge.getCLibrary().TOSDB_AddItem(_name, item);
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
        /* check we are consistent with C lib */
        if(!_topics.equals(getTopics()))
            throw new IllegalStateException("_topics != getTopics()");

        int err = TOSDataBridge.getCLibrary().TOSDB_AddTopic(_name, topic);
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
        /* check we are consistent with C lib */
        if(!_items.equals(getItems()))
            throw new IllegalStateException("_items != getItems()");

        int err = TOSDataBridge.getCLibrary().TOSDB_RemoveItem(_name, item);
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
        /* check we are consistent with C lib */
        if(!_topics.equals(getTopics()))
            throw new IllegalStateException("_topics != getTopics()");

        int err = TOSDataBridge.getCLibrary().TOSDB_RemoveTopic(_name, topic);
        if(err != 0)
            throw new CLibException("TOSDB_RemoveTopic", err);

        _topics = getTopics();

            /* in case items get sent to pre-cache */
        if(_topics.isEmpty())
            _items = getItems();
    }

    public Long
    getLong(String item, String topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return this.<Long>_get(item,topic,0, checkIndx, false, Long.class);
    }

    public Long
    getLong(String item, String topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return this.<Long>_get(item,topic, indx, checkIndx, false, Long.class);
    }

    public Pair<Long, DateTime>
    getLongWithDateTime(String item, String topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_get(item,topic,0, checkIndx, true, Long.class);
    }

    public Pair<Long, DateTime>
    getLongWithDateTime(String item, String topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_get(item,topic, indx, checkIndx, true, Long.class);
    }

    public Double
    getDouble(String item, String topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return this.<Double>_get(item,topic,0, checkIndx, false, Double.class);
    }

    public Double
    getDouble(String item, String topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return this.<Double>_get(item,topic, indx, checkIndx, false, Double.class);
    }

    public Pair<Double, DateTime>
    getDoubleWithDateTime(String item, String topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_get(item,topic,0, checkIndx, true, Double.class);
    }

    public Pair<Double, DateTime>
    getDoubleWithDateTime(String item, String topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_get(item,topic, indx, checkIndx, true, Double.class);
    }

    public String
    getString(String item, String topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return this.<String>_get(item,topic,0, checkIndx, false, String.class);
    }

    public String
    getString(String item, String topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return this.<String>_get(item,topic, indx, checkIndx, false, String.class);
    }

    public Pair<String, DateTime>
    getStringWithDateTime(String item, String topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_get(item,topic,0, checkIndx, true, String.class);
    }

    public Pair<String, DateTime>
    getStringWithDateTime(String item, String topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_get(item,topic, indx, checkIndx, true, String.class);
    }

    public Long[]
    getStreamSnapshotLongs(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<Long>_getStreamSnapshot(item,topic,-1,0,true, false,Long.class);
    }

    public Long[]
    getStreamSnapshotLongs(String item, String topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<Long>_getStreamSnapshot(item,topic,end,0,true, false,Long.class);
    }

    public Long[]
    getStreamSnapshotLongs(String item, String topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<Long>_getStreamSnapshot(item,topic,end,beg,true,false,Long.class);
    }

    public Pair<Long, DateTime>[]
    getStreamSnapshotLongsWithDateTime(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,-1,0,true, true,Long.class);
    }

    public Pair<Long, DateTime>[]
    getStreamSnapshotLongsWithDateTime(String item, String topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,end,0,true, true,Long.class);
    }

    public Pair<Long, DateTime>[]
    getStreamSnapshotLongsWithDateTime(String item, String topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,end,beg,true,true,Long.class);
    }

    public Double[]
    getStreamSnapshotDoubles(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<Double>_getStreamSnapshot(item,topic,-1,0,true, false,Double.class);
    }

    public Double[]
    getStreamSnapshotDoubles(String item, String topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<Double>_getStreamSnapshot(item,topic,end,0,true, false,Double.class);
    }

    public Double[]
    getStreamSnapshotDoubles(String item, String topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<Double>_getStreamSnapshot(item,topic,end,beg,true,false,Double.class);
    }

    public Pair<Double, DateTime>[]
    getStreamSnapshotDoublesWithDateTime(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,-1,0,true, true,Double.class);
    }

    public Pair<Double, DateTime>[]
    getStreamSnapshotDoublesWithDateTime(String item, String topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,end,0,true, true,Double.class);
    }

    public Pair<Double, DateTime>[]
    getStreamSnapshotDoublesWithDateTime(String item, String topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,end,beg,true,true,Double.class);
    }

    public String[]
    getStreamSnapshotStrings(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<String>_getStreamSnapshot(item,topic,-1,0,true,false,String.class);
    }

    public String[]
    getStreamSnapshotStrings(String item, String topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<String>_getStreamSnapshot(item,topic,end,0,true,false,String.class);
    }

    public String[]
    getStreamSnapshotStrings(String item, String topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return this.<String>_getStreamSnapshot(item,topic,end,beg,true,false,String.class);
    }

    public Pair<String, DateTime>[]
    getStreamSnapshotStringsWithDateTime(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,-1,0,true, true, String.class);
    }

    public Pair<String, DateTime>[]
    getStreamSnapshotStringsWithDateTime(String item, String topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,end,0,true,true,String.class);
    }

    public Pair<String, DateTime>[]
    getStreamSnapshotStringsWithDateTime(String item, String topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshot(item,topic,end,beg,true,true,String.class);
    }

    public Long[]
    getStreamSnapshotLongsFromMarker(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return this.<Long>_getStreamSnapshotFromMarker(item,topic,0,true,false,Long.class);
    }

    public Long[]
    getStreamSnapshotLongsFromMarker(String item, String topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return this.<Long>_getStreamSnapshotFromMarker(item,topic,beg,true,false,Long.class);
    }

    public Pair<Long,DateTime>[]
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException,
                   DateTimeNotSupported, DirtyMarkerException
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshotFromMarker(item,topic,0,true,true,Long.class);
    }

    public Pair<Long,DateTime>[]
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, String topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException,
                   DateTimeNotSupported, DirtyMarkerException
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshotFromMarker(item,topic,beg,true,true,Long.class);
    }

    public Double[]
    getStreamSnapshotDoublesFromMarker(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return this.<Double>_getStreamSnapshotFromMarker(item,topic,0,true,false,Double.class);
    }

    public Double[]
    getStreamSnapshotDoublesFromMarker(String item, String topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return this.<Double>_getStreamSnapshotFromMarker(item,topic,beg,true,false,Double.class);
    }

    public Pair<Double,DateTime>[]
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException,
            DateTimeNotSupported, DirtyMarkerException
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshotFromMarker(item,topic,0,true,true,Double.class);
    }

    public Pair<Double,DateTime>[]
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, String topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException,
            DateTimeNotSupported, DirtyMarkerException
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshotFromMarker(item,topic,beg,true,true,Double.class);
    }

    public String[]
    getStreamSnapshotStringsFromMarker(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return this.<String>_getStreamSnapshotFromMarker(item,topic,0,true,false,String.class);
    }

    public String[]
    getStreamSnapshotStringsFromMarker(String item, String topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return this.<String>_getStreamSnapshotFromMarker(item,topic,beg,true,false,String.class);
    }

    public Pair<String,DateTime>[]
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, String topic)
            throws LibraryNotLoaded, CLibException, DataIndexException,
            DateTimeNotSupported, DirtyMarkerException
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshotFromMarker(item,topic,0,true,true,String.class);
    }

    public Pair<String,DateTime>[]
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, String topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException,
            DateTimeNotSupported, DirtyMarkerException
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Pair>_getStreamSnapshotFromMarker(item,topic,beg,true,true,String.class);
    }

    public Map<String,String>
    getItemFrame(String topic) throws CLibException, LibraryNotLoaded
    {
        return this.<Map>_getFrame(topic, true,false);
    }

    public Map<String,Pair<String,DateTime>>
    getItemFrameWithDateTime(String topic)
            throws CLibException, LibraryNotLoaded, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Map>_getFrame(topic, true,true);
    }

    public Map<String,String>
    getTopicFrame(String item) throws CLibException, LibraryNotLoaded
    {
        return this.<Map>_getFrame(item, false, false);
    }

    public Map<String,Pair<String,DateTime>>
    getTopicFrameWithDateTime(String item)
            throws CLibException, LibraryNotLoaded, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        return this.<Map>_getFrame(item, false,true);
    }

    public Map<String, Map<String,String>>
    getTotalFrame() throws LibraryNotLoaded, CLibException
    {
        Map<String, Map<String,String>> frame = new HashMap<>();
        for(String item : getItems()){
            Map<String,String> tf = getTopicFrame(item);
            frame.put(item, tf);
        }

        return frame;
    }

    public Map<String, Map<String,Pair<String,DateTime>>>
    getTotalFrameWithDateTime()
            throws LibraryNotLoaded, CLibException, DateTimeNotSupported
    {
        if(!_dateTime)
            throw new DateTimeNotSupported();

        Map<String, Map<String,Pair<String,DateTime>>> frame = new HashMap<>();
        for(String item : getItems()){
            Map<String,Pair<String,DateTime>> tf
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
    _getItemCount() throws CLibException, LibraryNotLoaded
    {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetItemCount(_name, count);
        if(err != 0)
            throw new CLibException("TOSDB_GetItemCount", err);

        return count[0];
    }

    private int
    _getTopicCount() throws CLibException, LibraryNotLoaded
    {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetTopicCount(_name, count);
        if(err != 0)
            throw new CLibException("TOSDB_GetTopicCount", err);

        return count[0];
    }

    private int
    _getItemPreCachedCount() throws CLibException, LibraryNotLoaded
    {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetPreCachedItemCount(_name, count);
        if(err != 0)
            throw new CLibException("TOSDB_GetPreCachedItemCount", err);

        return count[0];
    }

    private int
    _getTopicPreCachedCount() throws CLibException, LibraryNotLoaded
    {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetPreCachedTopicCount(_name, count);
        if(err != 0)
            throw new CLibException("TOSDB_GetPreCachedTopicCount", err);

        return count[0];
    }

    private class _PreGet {
        String item;
        String topic;

        public _PreGet(String item, String topic) throws LibraryNotLoaded, CLibException
        {
            this.item = item.toUpperCase();
            this.topic = topic.toUpperCase();

            _isValidItem(this.item);
            _isValidTopic(this.topic);
        }
    }

    private <T> T
    _get(String item, String topic,int indx, boolean checkIndx,
         boolean withDateTime, Class<?> valType)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        _PreGet pre = new _PreGet(item,topic);

        if(indx < 0)
            indx += _size;

        if(indx >= _size)
            throw new DataIndexException("indx >= _size in DataBlock");

        int occ = getStreamOccupancy(pre.item,pre.topic);
        if(checkIndx && indx > occ)
            throw new DataIndexException("Can not get data at indx: " + String.valueOf(indx)
                    + ", occupancy only: " + String.valueOf(occ));

        DateTime ptrDTS =  withDateTime ? new DateTime() : null;

        final CLib lib = TOSDataBridge.getCLibrary();
        if(valType.equals(Long.class)) {
            long[] ptrVal = {0};
            int err = lib.TOSDB_GetLongLong(_name, pre.item, pre.topic, indx, ptrVal, ptrDTS);
            if(err != 0)
                throw new CLibException("TOSDB_GetLongLongs", err);
            return (T)(withDateTime ? new Pair<>(ptrVal[0],ptrDTS) : ptrVal[0]);
        }else if(valType.equals(Double.class)) {
            double[] ptrVal = {0};
            int err = lib.TOSDB_GetDouble(_name, pre.item, pre.topic, indx, ptrVal, ptrDTS);
            if(err != 0)
                throw new CLibException("TOSDB_GetDoubles", err);
            return (T)(withDateTime ? new Pair<>(ptrVal[0],ptrDTS) : ptrVal[0]);
        }else {
            byte[] ptrVal = new byte[STR_DATA_SZ + 1];
            int err = lib.TOSDB_GetString(_name, pre.item, pre.topic, indx, ptrVal, STR_DATA_SZ + 1, ptrDTS);
            if(err != 0)
                throw new CLibException("TOSDB_GetStrings", err);
            return (T)(withDateTime ? new Pair<>(Native.toString(ptrVal),ptrDTS) : Native.toString(ptrVal));
        }
    }

    private <T> T[]
    _getStreamSnapshot(String item, String topic, int end, int beg,
                       boolean smartSize, boolean withDateTime, Class<?> valType)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        _PreGet pre = new _PreGet(item,topic);

        if(end < 0)
            end += _size;

        if(beg < 0)
            beg += _size;

        int size = end - beg + 1;

        if(beg < 0 || end < 0 || beg >= _size || end >= _size || size <= 0)
        {
            throw new DataIndexException("invalid 'beg' and/or 'end' index");
        }

        if(smartSize){
            int occ = getStreamOccupancy(item, topic);
            if(occ == 0 || occ <= beg)
                return (T[])(withDateTime ? new Pair[]{} : Array.newInstance(valType,0));
            end = Math.min(end, occ-1);
            beg = Math.min(beg, occ-1);
            size = end - beg + 1;
        }

        DateTime[] arrayDTS = (withDateTime ? new DateTime[size] : null);

        T[] data = (T[])(withDateTime ? new Pair[size] : Array.newInstance(valType,size));

        final CLib lib = TOSDataBridge.getCLibrary();
        if(valType.equals(Long.class)){
            long[] arrayVals = new long[size];
            int err = lib.TOSDB_GetStreamSnapshotLongLongs(
                    _name, pre.item, pre.topic, arrayVals, size, arrayDTS, end, beg);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotLongs", err);

            for(int i = 0; i < size; ++i) {
                data[i] = (T)(withDateTime ? new Pair<>(arrayVals[i], arrayDTS[i]) : arrayVals[i]);
            }
        }else if(valType.equals(Double.class)){
            double[] arrayVals = new double[size];
            int err = lib.TOSDB_GetStreamSnapshotDoubles(
                    _name, pre.item, pre.topic, arrayVals, size, arrayDTS, end, beg);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotDoubles", err);

            for(int i = 0; i < size; ++i) {
                data[i] = (T)(withDateTime ? new Pair<>(arrayVals[i], arrayDTS[i]) : arrayVals[i]);
            }
        }else{
            Pointer[] arrayVals = new Pointer[size];
            for(int i = 0; i < size; ++i){
                arrayVals[i] = new Memory(STR_DATA_SZ+1);
            }
            int err = lib.TOSDB_GetStreamSnapshotStrings(
                    _name, pre.item, pre.topic, arrayVals, size,
                    STR_DATA_SZ + 1, arrayDTS, end, beg);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotStrings", err);

            for(int i = 0; i < size; ++i) {
                data[i] = (T)(withDateTime ? new Pair<>(arrayVals[i].getString(0), arrayDTS[i])
                        : arrayVals[i].getString(0));
            }
        }
        return data;
    }

    private long
    _handleSzGotFromMarker(NativeLong l, boolean throwIfDataLost)
    {
        long szGot = l.longValue();
        if(szGot < 0){
            if(throwIfDataLost)
                throw new IllegalStateException("data lost behind marker");
            else
                szGot *= -1;
        }
        return szGot;
    }

    private <T> T[]
    _getStreamSnapshotFromMarker(String item, String topic, int beg, boolean throwIfDataLost,
                                 boolean withDateTime, Class<?> valType)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException {
        int err;

        _PreGet pre = new _PreGet(item,topic);

        if(beg < 0)
            beg += _size;

        if(beg < 0 || beg >= _size)
            throw new DataIndexException("invalid 'beg' index argument");

        final CLib lib = TOSDataBridge.getCLibrary();

        int[] ptrIsDirty = {0};
        err = lib.TOSDB_IsMarkerDirty(_name, item, topic, ptrIsDirty);
        if(err != 0)
            throw new CLibException("TOSDB_IsMarkerDirty", err);

        if(ptrIsDirty[0] != 0 && throwIfDataLost)
            throw new DirtyMarkerException();

        long[] ptrMarkerPos = {0};
        err = lib.TOSDB_GetMarkerPosition(_name, item, topic, ptrMarkerPos);
        if(err != 0)
            throw new CLibException("TOSDB_GetMarkerPosition", err);

        long curSz = ptrMarkerPos[0] - beg + 1;
        if(curSz < 0)
            return (T[])(withDateTime ? new Pair[]{} : Array.newInstance(valType,0));

        // NEED TO CHECK WE CAN GO FROM LONG TO INT
        int safeSz = (int)curSz + MARKER_MARGIN_OF_SAFETY;

        T[] data;
        DateTime[] arrayDTS = (withDateTime ? new DateTime[safeSz] : null);
        NativeLong[] ptrGetSz = {new NativeLong(0)};

        if(valType.equals(Long.class)){
            long[] arrayVals = new long[safeSz];
            err = lib.TOSDB_GetStreamSnapshotLongLongsFromMarker(
                    _name, pre.item, pre.topic, arrayVals, safeSz, arrayDTS, beg, ptrGetSz);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotLongsFromMarker", err);

            int szGot = (int)_handleSzGotFromMarker(ptrGetSz[0],throwIfDataLost);

            data = (T[])(withDateTime ? new Pair[szGot] : new Long[szGot]);
            for(int i = 0; i < szGot; ++i) {
                data[i] = (T)(withDateTime ? new Pair<>(arrayVals[i], arrayDTS[i]) : arrayVals[i]);
            }
        }else if(valType.equals(Double.class)){
            double[] arrayVals = new double[safeSz];
            err = lib.TOSDB_GetStreamSnapshotDoublesFromMarker(
                    _name, pre.item, pre.topic, arrayVals, safeSz, arrayDTS, beg, ptrGetSz);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotDoublesFromMarker", err);

            int szGot = (int)_handleSzGotFromMarker(ptrGetSz[0],throwIfDataLost);

            data = (T[])(withDateTime ? new Pair[szGot] : new Double[szGot]);
            for(int i = 0; i < szGot; ++i) {
                data[i] = (T)(withDateTime ? new Pair<>(arrayVals[i], arrayDTS[i]) : arrayVals[i]);
            }
        }else{
            Pointer[] arrayVals = new Pointer[safeSz];
            for(int i = 0; i < safeSz; ++i){
                arrayVals[i] = new Memory(STR_DATA_SZ+1);
            }
            err = lib.TOSDB_GetStreamSnapshotStringsFromMarker(
                    _name, pre.item, pre.topic, arrayVals, safeSz,
                    STR_DATA_SZ + 1, arrayDTS, beg, ptrGetSz);
            if(err != 0)
                throw new CLibException("TOSDB_GetStreamSnapshotStringsFromMarker", err);

            int szGot = (int)_handleSzGotFromMarker(ptrGetSz[0],throwIfDataLost);

            data = (T[])(withDateTime ? new Pair[szGot] : new String[szGot]);
            for(int i = 0; i < szGot; ++i) {
                data[i] = (T)(withDateTime
                        ? new Pair<>(arrayVals[i].getString(0), arrayDTS[i])
                        : arrayVals[i].getString(0));
            }
        }
        return data;
    }

    private <T> T
    _getFrame(String s, boolean itemFrame, boolean withDateTime)
            throws CLibException, LibraryNotLoaded
    {
        s = s.toUpperCase();

        if(itemFrame)
            _isValidTopic(s);
        else
            _isValidItem(s);

        int n = itemFrame ? _getItemCount() : _getTopicCount();

        Pointer[] arrayVals = new Pointer[n];
        for(int i = 0; i < n; ++i){
            arrayVals[i] = new Memory(STR_DATA_SZ+1);
        }

        Pointer[] arrayLabels = new Pointer[n];
        for(int i = 0; i < n; ++i){
            arrayLabels[i] = new Memory(MAX_STR_SZ+1);
        }

        DateTime[] arrayDTS = (withDateTime ? new DateTime[n] : null);

        final CLib lib = TOSDataBridge.getCLibrary();
        int err = itemFrame
                ? lib.TOSDB_GetItemFrameStrings(_name, s, arrayVals, n,
                        STR_DATA_SZ + 1, arrayLabels, MAX_STR_SZ + 1, arrayDTS)
                : lib.TOSDB_GetTopicFrameStrings(_name, s, arrayVals, n,
                        STR_DATA_SZ + 1, arrayLabels, MAX_STR_SZ + 1, arrayDTS);

        if(err != 0)
            throw new CLibException("TOSDB_Get" + (itemFrame ? "Item" : "Topic") + "FrameStrings", err);

        T frame = (T)new HashMap<>();

        for(int i = 0; i < n; ++i){
            ((Map)frame).put(arrayLabels[i].getString(0),
                             (withDateTime ? new Pair(arrayVals[i].getString(0), arrayDTS[i])
                                           : arrayVals[i].getString(0)));
        }
        return frame;

    }
}