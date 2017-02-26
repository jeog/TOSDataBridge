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

package io.github.jeog.tosdatabridge;

import io.github.jeog.tosdatabridge.TOSDataBridge.*;
import io.github.jeog.tosdatabridge.DateTime.DateTimePair;

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.*;

/**
 * DataBlock is used to get real-time financial data from the TOS platform.
 * It uses the JNA interface(CLib.java) to access the C API, wrapping the
 * underlying (conceptual) block of the C lib with an object-oriented interface.
 * <p>
 * <strong>IMPORTANT:</strong> Before creating/using a DataBlock the underlying library
 * must be loaded AND connected to the backend Service/Engine and the TOS platform.
 * <ol>
 * <li> be sure you've installed the C mods (tosdb-setup.bat)
 * <li> start the Service/Engine (SC Start TOSDataBridge)
 * <li> start the TOS Platform
 * <li> call TOSDataBridge.init(...)
 * <li> create DataBlock(s)
 * </ol>
 * <p>
 * DataBlock works much the same as its python cousin (tosdb._win.TOSDB_DataBlock).
 * It's instantiated with a size parameter to indicate how much historical data is saved.
 * Once created 'topics' (data fields found in the Topic enum) and 'items' (symbols)
 * are added to create a matrix of 'streams', each the size of the block. The block can
 * be thought of as a 3D object with the following dimensions:
 * <p>
 * &nbsp&nbsp&nbsp&nbsp (# of topics) &nbsp x &nbsp (# of items) &nbsp x &nbsp (block size)
 * <p>
 * As data flows into these streams the various 'get' methods are then used to
 * 1) examine the underlying state of the block/streams, and 2) pull data from
 * the block/streams in myriad ways.
 *
 * @author Jonathon Ogden
 * @version 0.7
 * @throws CLibException error code returned by C lib   
 * @throws LibraryNotLoaded C lib has not been loaded
 * @see TOSDataBridge
 * @see Topic
 * @see DateTime
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md"> README </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_JAVA.md"> README - JAVA API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_API.md"> README - C API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_PYTHON.md"> README - PYTHON API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_SERVICE.md"> README - SERVICE </a>
 */
public class DataBlock {
    /* max size of non-data (e.g topics, labels) strings (excluding \0)*/
    public static final int MAX_STR_SZ = TOSDataBridge.MAX_STR_SZ;

    /* max size of data strings (getString, getStreamSnapshotStrings etc.) */
    public static final int STR_DATA_SZ = TOSDataBridge.STR_DATA_SZ;

    /* how much buffer 'padding' for the marker calls */
    public static final int MARKER_MARGIN_OF_SAFETY = TOSDataBridge.MARKER_MARGIN_OF_SAFETY;

    /* default timeout for communicating with the engine/platform */
    public static final int DEF_TIMEOUT = TOSDataBridge.DEF_TIMEOUT;

    /* an implementation layer (below API, above JNA) we want to share with classes that
       may extend DataBlock within the package, but not with client code from outside it */
    private final DataBlockSharedHelper _helper = new DataBlockSharedHelper();

    /*package-private*/
    final DataBlockSharedHelper
    getHelper(){
        return _helper;
    }

    private final String _name;
    private final int _timeout;
    private int _size;
    private Set<String> _items;
    private Set<Topic> _topics;

    /**
     * DataBlock Constructor.
     *
     * @param size maximum amount of historical data block can hold
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     */
    public DataBlock(int size) throws CLibException, LibraryNotLoaded {
        this(size, DEF_TIMEOUT);
    }

    /**
     * DataBlock Constructor.
     *
     * @param size    maximum amount of historical data block can hold
     * @param timeout timeout of internal IPC and DDE mechanisms (milliseconds)
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public DataBlock(int size, int timeout) throws LibraryNotLoaded, CLibException {
        this(size, false, timeout);
    }

    /**
     * INTERNAL CONSTRUCTOR
     */
    protected DataBlock(int size, boolean dateTime, int timeout)
            throws LibraryNotLoaded, CLibException
    {
        _size = size;
        _timeout = timeout;
        _name = UUID.randomUUID().toString();
        _items = new HashSet<>();
        _topics = new HashSet<>();

        int err = TOSDataBridge.getCLibrary()
                .TOSDB_CreateBlock(_name, size, dateTime, timeout);
        if (err != 0) {
            throw new CLibException("TOSDB_CreateBlock", err);
        }
    }

    /**
     * Close the underlying C lib block.
     *
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     */
    public void
    close() throws CLibException, LibraryNotLoaded {
        int err = TOSDataBridge.getCLibrary().TOSDB_CloseBlock(_name);
        if (err != 0) {
            throw new CLibException("TOSDB_CloseBlock", err);
        }
    }

    protected void
    finalize() throws Throwable {
        try {
            close();
        } finally {
            super.finalize();
        }
    }

    /**
     * @return name of block (implementation detail).
     */
    public String
    getName() {
        return _name;
    }

    /**
     * @return if block is storing DateTime alongside primary data
     */
    public boolean
    isUsingDateTime() {
        return false;
    }

    /**
     * @return timeout the underlying block uses for IPC/DDE (milliseconds)
     */
    public int
    getTimeout() {
        return _timeout;
    }

    /**
     * @return maximum amount of data that can be stored in the block/streams.
     *
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public int
    getBlockSize() throws LibraryNotLoaded, CLibException {
        int sz = _getBlockSize();
        if (sz != _size) {
            throw new IllegalStateException("_getBlockSize() != _size");
        }
        return sz;
    }

    /**
     * Set maximum amount of data that can be stored in the block/streams.
     *
     * @param size new size of the block
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public void
    setBlockSize(int size) throws LibraryNotLoaded, CLibException {
        int err = TOSDataBridge.getCLibrary().TOSDB_SetBlockSize(_name, size);
        /* need to update regardless */
        _size = _getBlockSize();
        if (err != 0) {
            throw new CLibException("TOSDB_SetBlockSize", err);
        }
        if (size != _size) {
            throw new IllegalStateException("size != _size");
        }
    }

    /**
     * Returns number of data-points currently in the stream.
     *
     * @param item item string of the stream
     * @param topic topic enum of the stream
     * @return number of data-points currently in the stream
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @see Topic
     */
    public int
    getStreamOccupancy(String item, Topic topic) throws CLibException, LibraryNotLoaded,
            InvalidItemOrTopic {
        item = _handleRawItemTopic(item,topic,true);
        int occ[] = {0};
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamOccupancy(_name, item, topic.val, occ);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamOccupancy", err);
        }
        return occ[0];
    }

    /**
     * @return items currently in the block
     *
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public Set<String>
    getItems() throws LibraryNotLoaded, CLibException {
        return _getItemTopicNames(true, false);
    }

    /**
     * @return topics currently in the block
     *
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @see Topic
     */
    public Set<Topic>
    getTopics() throws LibraryNotLoaded, CLibException {
        return _getItemTopicNames(false, false);
    }

    /**
     * @return items currently in the block's pre-cache
     *
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public Set<String>
    getItemsPreCached() throws LibraryNotLoaded, CLibException {
        return _getItemTopicNames(true, true);
    }

    /**
     * @return topics currently in the block's pre-cache
     *
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @see Topic
     */
    public Set<Topic>
    getTopicsPreCached() throws LibraryNotLoaded, CLibException {
        return _getItemTopicNames(false, true);
    }

    /**
     * Add item to the block.
     *
     * @param item item string to be added to the block
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     */
    public void
    addItem(String item) throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        _handleRawItem(item,false);
        /* check we are consistent with C lib */
        if (!_items.equals(getItems())) {
            throw new IllegalStateException("_items != getItems()");
        }
        int err = TOSDataBridge.getCLibrary().TOSDB_AddItem(_name, item);
        if (err != 0) {
            throw new CLibException("TOSDB_AddItem", err);
        }
        _topics = getTopics();
        _items = getItems();
    }

    /**
     * Add Topic to the block.
     *
     * @param topic topic enum to be added to the block
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public void
    addTopic(Topic topic) throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        _handleRawTopic(topic,false);
        /* check we are consistent with C lib */
        if (!_topics.equals(getTopics())) {
            throw new IllegalStateException("_topics != getTopics()");
        }
        int err = TOSDataBridge.getCLibrary().TOSDB_AddTopic(_name, topic.val);
        if (err != 0) {
            throw new CLibException("TOSDB_AddTopic", err);
        }
        _items = getItems();
        _topics = getTopics();
    }

    /**
     * Remove item from the block.
     *
     * @param item item string to be removed from the block
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     */
    public void
    removeItem(String item) throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        /* note: we don't throw if item isn't it block because it might be in pre-cache */
        _handleRawItem(item,false);
        /* check we are consistent with C lib */
        if (!_items.equals(getItems())) {
            throw new IllegalStateException("_items != getItems()");
        }
        int err = TOSDataBridge.getCLibrary().TOSDB_RemoveItem(_name, item);
        if (err != 0) {
            throw new CLibException("TOSDB_RemoveItem", err);
        }
        _items = getItems();
        _topics = getTopics();
    }

    /**
     * Remove Topic from the block.
     *
     * @param topic topic enum to be removed from the block.
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public void
    removeTopic(Topic topic) throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        /* note: we don't throw if topic isn't it block because it might be in pre-cache */
        _handleRawTopic(topic,false);
        /* check we are consistent with C lib */
        if (!_topics.equals(getTopics())) {
            throw new IllegalStateException("_topics != getTopics()");
        }
        int err = TOSDataBridge.getCLibrary().TOSDB_RemoveTopic(_name, topic.val);
        if (err != 0) {
            throw new CLibException("TOSDB_RemoveTopic", err);
        }
        _topics = getTopics();
        _items = getItems();
    }

    /**
     * Returns if item is in the block.(NOT THE PRE-CACHE)
     *
     * @param item item string to be removed from the block
     * @return if item is in the block
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     */
    public boolean
    containsItem(String item) throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        /* note: we don't throw if item isn't it block */
        _handleRawItem(item,false);
        return _items.contains(item);
    }

    /**
     * Returns if topic is in the block.(NOT THE PRE-CACHE)
     *
     * @param topic topic enum to be removed from the block
     * @return if topic is in the block
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     */
    public boolean
    containsTopic(Topic topic) throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        /* note: we don't throw if topic isn't it block */
        _handleRawTopic(topic,false);
        return _topics.contains(topic);
    }

    /**
     * Returns most recent data-point of a stream, as Long (or null if no data).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return most recent data-point of stream (or null)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public Long
    getLong(String item, Topic topic)
            throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        return _helper.getMostRecent(item, topic, false, Long.class);
    }

    /**
     * Returns data-point of a stream, as Long (or null if no data at that position/index).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param indx  index/position of data-point
     * @return data-point of stream (or null)
     * @throws CLibException      error code returned by C lib
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public Long
    getLong(String item, Topic topic, int indx)
            throws CLibException, LibraryNotLoaded, DataIndexException, InvalidItemOrTopic {
        return _helper.get(item, topic, indx, false, Long.class);
    }

    /**
     * Returns most recent data-point of a stream, as Double (or null if no data).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return most recent data-point of stream (or null)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public Double
    getDouble(String item, Topic topic)
            throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        return _helper.getMostRecent(item, topic, false, Double.class);
    }

    /**
     * Returns data-point of a stream, as Double (or null if no data at that position/index).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param indx  index/position of data-point
     * @return data-point of stream (or null)
     * @throws CLibException      error code returned by C lib
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public Double
    getDouble(String item, Topic topic, int indx)
            throws CLibException, LibraryNotLoaded, DataIndexException, InvalidItemOrTopic {
        return _helper.get(item, topic, indx, false, Double.class);
    }

    /**
     * Returns most recent data-point of a stream, as String (or null if no data).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return most recent data-point of stream (or null)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public String
    getString(String item, Topic topic)
            throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        return _helper.getMostRecent(item, topic, false, String.class);
    }

    /**
     * Returns data-point of a stream, as String (or null if no data at that position/index).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param indx  index/position of data-point 
     * @return data-point of stream (or null)
     * @throws CLibException      error code returned by C lib
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public String
    getString(String item, Topic topic, int indx)
            throws CLibException, LibraryNotLoaded, DataIndexException, InvalidItemOrTopic {
        return _helper.get(item, topic, indx, false, String.class);
    }

    /**
     * Returns all data-points of a stream, as List&lt;Long&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points of stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongs(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotAll(item, topic, false, Long.class);
    }

    /**
     * Returns multiple contiguous data-points of a stream, from most recent, 
     * as List&lt;Long&gt;. 
     *
     * @param item  item string of the stream
     * @param topic topic enum of the stream
     * @param end   least recent index/position from which data is pulled
     * @return multiple contiguous data-points of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongs(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshot(item, topic, end, 0, true, false, Long.class);
    }

    /**
     * Returns multiple contiguous data-points of a stream, as List&lt;Long&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param end   least recent index/position from which data is pulled
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongs(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshot(item, topic, end, beg, true, false, Long.class);
    }

    /**
     * Returns all data-points of a stream, as List&lt;Double&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points of stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoubles(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotAll(item, topic, false, Double.class);
    }

    /**
     * Returns multiple contiguous data-points of a stream, from most recent, 
     * as List&lt;Double&gt;. 
     *
     * @param item  item string of the stream
     * @param topic topic enum of the stream
     * @param end   least recent index/position from which data is pulled
     * @return multiple contiguous data-points of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoubles(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshot(item, topic, end, 0, true, false, Double.class);
    }

    /**
     * Returns multiple contiguous data-points of a stream, as List&lt;Double&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param end   least recent index/position from which data is pulled
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoubles(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshot(item, topic, end, beg, true, false, Double.class);
    }

    /**
     * Returns all data-points of a stream, as List&lt;String&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points of stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStrings(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotAll(item, topic, false, String.class);
    }

    /**
     * Returns multiple contiguous data-points of a stream, from most recent, 
     * as List&lt;String&gt;. 
     *
     * @param item  item string of the stream
     * @param topic topic enum of the stream
     * @param end   least recent index/position from which data is pulled
     * @return multiple contiguous data-points of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStrings(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshot(item, topic, end, 0, true, false, String.class);
    }

    /**
     * Returns multiple contiguous data-points of a stream, as List&lt;String&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param end   least recent index/position from which data is pulled
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStrings(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshot(item, topic, end, beg, true, false, String.class);
    }

    /**
     * Returns all data-points up to atomic marker of a stream, as List&lt;Long&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongsFromMarker(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException,
            InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerToMostRecent(item, topic, false, Long.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream, 
     * as List&lt;Long&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongsFromMarker(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException,
            InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarker(item, topic, beg, true, false, Long.class);
    }

    /**
     * Returns all data-points up to atomic marker of a stream, as List&lt;Double&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoublesFromMarker(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException,
            InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerToMostRecent(item, topic, false, Double.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream, 
     * as List&lt;Double&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoublesFromMarker(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException,
            InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarker(item, topic, beg, true, false, Double.class);
    }

    /**
     * Returns all data-points up to atomic marker of a stream, as List&lt;String&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStringsFromMarker(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException,
            InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerToMostRecent(item, topic, false, String.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream, 
     * as List&lt;String&gt;. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStringsFromMarker(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException,
            InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarker(item, topic, beg, true, false, String.class);
    }

    /**
     * Returns if marker is currently in a 'dirty' state. NOTE: there is no
     * guarantee it won't enter this state before another call is made.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return if stream/marker is dirty
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see "StreamSnapshotFromMarker calls"
     * @see Topic
     */
    public boolean
    isDirty(String item, Topic topic) throws LibraryNotLoaded, CLibException,
            InvalidItemOrTopic {
        item = _handleRawItemTopic(item,topic,true);
        int[] ptrIsDirty = {0};
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_IsMarkerDirty(_name, item, topic.val, ptrIsDirty);
        if (err != 0) {
            throw new CLibException("TOSDB_IsMarkerDirty", err);
        }
        return ptrIsDirty[0] != 0;
    }


    /**
     * Returns all data-points up to atomic marker of a stream, as List&lt;Long&gt;.
     * Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongsFromMarkerIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item, topic, false,
                Long.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream, 
     * as List&lt;Long&gt;. Ignores 'dirty' marker/stream. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Long>
    getStreamSnapshotLongsFromMarkerIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerIgnoreDirty(item, topic, beg, false, 
                Long.class);
    }

    /**
     * Returns all data-points up to atomic marker of a stream, as List&lt;Double&gt;.
     * Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoublesFromMarkerIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item, topic, false, 
                Double.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream, 
     * as List&lt;Double&gt;. Ignores 'dirty' marker/stream. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<Double>
    getStreamSnapshotDoublesFromMarkerIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerIgnoreDirty(item, topic, beg, false, 
                Double.class);
    }

    /**
     * Returns all data-points up to atomic marker of a stream, as List&lt;String&gt;.
     * Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStringsFromMarkerIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item, topic, false, 
                String.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream, 
     * as List&lt;String&gt;. Ignores 'dirty' marker/stream. 
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public List<String>
    getStreamSnapshotStringsFromMarkerIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic {
        return _helper.getStreamSnapshotFromMarkerIgnoreDirty(item, topic, beg, false, 
                String.class);
    }

    /**
     * Returns mapping of all item names to most recent item values for a particular topic.
     *
     * @param topic topic enum of streams
     * @return mapping of item names to most recent item values
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws InvalidItemOrTopic invalid item or topic argument
     * @see Topic
     */
    public Map<String, String>
    getItemFrame(Topic topic) throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        return _helper.getFrame(topic, true, false);
    }

    /**
     * Returns mapping of all topic enums to most recent topic values for a particular item.
     *
     * @param item item string of streams
     * @return mapping of topic enums to most recent topic values
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws InvalidItemOrTopic invalid item or topic argument
     */
    public Map<Topic, String>
    getTopicFrame(String item) throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        return _helper.getFrame(item, false, false);
    }

    /**
     * Returns mapping of all item strings to mappings of all topic enums to most recent topic
     * values. This returns ALL the most recent data in the block.
     *
     * @return mapping of item strings to mappings of topic enums to most recent topic values
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public Map<String, Map<Topic, String>>
    getTotalFrame() throws LibraryNotLoaded, CLibException {
        Map<String, Map<Topic, String>> frame = new HashMap<>();
        for (String item : getItems()) {
            Map<Topic, String> tf = null;
            try {
                tf = getTopicFrame(item);
            } catch (InvalidItemOrTopic e) {
                /* SHOULD NEVER GET HERE */
               throw new RuntimeException("InvalidItemOrTopic caught in getTotalFrame");
            }
            frame.put(item, tf);
        }
        return frame;
    }

    /**
     * (package-private) inner class that houses a number of 'unsafe' type
     * operations that we only want to expose to the implementation of the
     * DataBlock hierarchy within the package. It allows for code reuse below
     * the API calls but above many of the JNA calls because it still has
     * access to DataBlock's private methods/fields. *The* private instance
     * is accessed via DataBlock's getHelper() method.
     */
    class DataBlockSharedHelper {

        /* only allow DataBlock to instantiate */
        private DataBlockSharedHelper(){
        }

        /* suppress DataIndexException */
        public final <T> T
        getMostRecent(String item, Topic topic, boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
            try {
                return get(item, topic, 0, withDateTime, valType);
            } catch (DataIndexException e) {
                /* SHOULD NEVER GET HERE */
                throw new RuntimeException("getMostRecent failed to suppress DataIndexException");
            }
        }

        @SuppressWarnings("unchecked")
        public final <T> T
        get(String item, Topic topic, int indx, boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, DataIndexException, InvalidItemOrTopic
        {
            item = _handleRawItemTopic(item,topic,true);
            if (indx < 0) {
                indx += _size;
            }
            if (indx >= _size) {
                throw new DataIndexException("indx >= _size in DataBlock");
            }
            if (indx >= getStreamOccupancy(item, topic)) {
                return null;
            }
            return _nativeAccessByType(valType,"_get","", new Class<?>[]{String.class, Topic.class, 
                            int.class, boolean.class}, item, topic, indx, withDateTime);
        }

        /* suppress DataIndexException */
        public final <T> List<T>
        getStreamSnapshotAll(String item, Topic topic, boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
            try {
                return getStreamSnapshot(item, topic, -1, 0, true, withDateTime, valType);
            } catch (DataIndexException e) {
                    /* SHOULD NEVER GET HERE */
                throw new RuntimeException("getStreamSnapshotAll failed to suppress DataIndexException");
            }
        }

        @SuppressWarnings("unchecked")
        public final <T> List<T>
        getStreamSnapshot(String item, Topic topic, int end, int beg, boolean smartSize,
                          boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, DataIndexException,
                InvalidItemOrTopic
        {
            item = _handleRawItemTopic(item,topic,true);
            if (end < 0) {
                end += _size;
            }
            if (beg < 0) {
                beg += _size;
            }
            int size = end - beg + 1;
            if ((beg < 0) || (end < 0) || (beg >= _size) || (end >= _size) || (size <= 0)) {
                throw new DataIndexException("invalid 'beg' and/or 'end' index");
            }

            if (smartSize) {
                int occ = getStreamOccupancy(item, topic);
                if (occ == 0 || occ <= beg) { //redundant ?
                    return new ArrayList<>();
                }
                end = Math.min(end, occ - 1);
                beg = Math.min(beg, occ - 1);
                size = end - beg + 1;
            }

            return _nativeAccessByType(valType, "_getStreamSnapshot", "s",
                    new Class<?>[]{String.class, Topic.class, int.class, int.class,
                            boolean.class, int.class}, item, topic, end, beg, withDateTime, size);        
        }

        /* suppress DirtyMarkerException */
        public final <T> List<T>
        getStreamSnapshotFromMarkerIgnoreDirty(String item, Topic topic, int beg,
                                               boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, DataIndexException,
                InvalidItemOrTopic {
            try {
                return getStreamSnapshotFromMarker(item, topic, beg, false, withDateTime, valType);
            } catch (DirtyMarkerException e) {
                    /* SHOULD NEVER GET HERE */
                throw new RuntimeException("getStreamSnapshotFromMarkerIgnoreDirty " +
                        "failed to ignore dirty marker");
            }
        }

        /* suppress DataIndexException */
        public final <T> List<T>
        getStreamSnapshotFromMarkerToMostRecent(String item, Topic topic, boolean withDateTime,
                                                Class<?> valType)
                throws LibraryNotLoaded, CLibException, DirtyMarkerException,
                InvalidItemOrTopic {
            try {
                return getStreamSnapshotFromMarker(item, topic, 0, true, withDateTime, valType);
            } catch (DataIndexException e) {
                    /* SHOULD NEVER GET HERE */
                throw new RuntimeException("getStreamSnapshotFromMarkerToMostRecent" +
                        " failed to suppress DataIndexException");
            }
        }

        /* suppress DataIndexException and DirtyMarkerException */
        public final <T> List<T>
        getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(String item, Topic topic,
                                                           boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, InvalidItemOrTopic {
            try {
                return getStreamSnapshotFromMarker(item, topic, 0, false, withDateTime, valType);
            } catch (DataIndexException e) {
                    /* SHOULD NEVER GET HERE */
                throw new RuntimeException("getStreamSnapshotFromMarkerToMostRecentIgnoreDirty" +
                        " failed to suppress DataIndexException");
            } catch (DirtyMarkerException e) {
                    /* SHOULD NEVER GET HERE */
                throw new RuntimeException("getStreamSnapshotFromMarkerToMostRecentIgnoreDirty" +
                        " failed to ignore dirty marker");
            }
        }

        @SuppressWarnings("unchecked")
        public final <T> List<T>
        getStreamSnapshotFromMarker(String item, Topic topic, int beg, boolean throwIfDataLost,
                                    boolean withDateTime, Class<?> valType)
                throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException,
                InvalidItemOrTopic
        {
            item = _handleRawItemTopic(item,topic,true);
            if (beg < 0) {
                beg += _size;
            }
            if (beg < 0 || beg >= _size) {
                throw new DataIndexException("invalid 'beg' index argument");
            }
            if (isDirty(item, topic) && throwIfDataLost) {
                throw new DirtyMarkerException();
            }

            long[] markerPosition = {0};
            int err = TOSDataBridge.getCLibrary()
                    .TOSDB_GetMarkerPosition(_name, item, topic.val, markerPosition);
            if (err != 0) {
                throw new CLibException("TOSDB_GetMarkerPosition", err);
            }
            long szFromMarker = markerPosition[0] - beg + 1;
            if (szFromMarker < 0) {
                return new ArrayList<>();
            }

            return _nativeAccessByType(valType, "_getStreamSnapshot", "sFromMarker", 
                    new Class<?>[]{String.class, Topic.class, int.class, boolean.class, 
                            boolean.class, int.class}, item, topic, beg, withDateTime, 
                    throwIfDataLost, (int) szFromMarker + MARKER_MARGIN_OF_SAFETY);
        }

        public final <T, E, X> Map<X, T>
        getFrame(E e, boolean itemFrame, boolean withDateTime)
                throws CLibException, LibraryNotLoaded, InvalidItemOrTopic
        {
            if (itemFrame) {
                _handleRawTopic((Topic)e, true);
            } else {
                _handleRawItem((String)e, true);
            }

            int n = itemFrame ? _getItemCount() : _getTopicCount();
            Pointer[] vals = new Pointer[n];
            Pointer[] labels = new Pointer[n];
            for (int i = 0; i < n; ++i) {
                vals[i] = new Memory(STR_DATA_SZ + 1);
                labels[i] = new Memory(MAX_STR_SZ + 1);
            }
            DateTime[] dts = (withDateTime ? new DateTime[n] : null);

            int err;
            if (itemFrame) {
                err = TOSDataBridge.getCLibrary()
                        .TOSDB_GetItemFrameStrings(_name, ((Topic) e).toString(), vals, n,
                                STR_DATA_SZ + 1, labels, MAX_STR_SZ + 1, dts);
            } else {
                err = TOSDataBridge.getCLibrary()
                        .TOSDB_GetTopicFrameStrings(_name, ((String) e).toUpperCase(), vals, n,
                                STR_DATA_SZ + 1, labels, MAX_STR_SZ + 1, dts);
            }
            if (err != 0) {
                throw new CLibException("TOSDB_Get" + (itemFrame ? "Item" : "Topic") + "FrameStrings",
                        err);
            }

            Map<X, T> frame = new HashMap<>();
            for (int i = 0; i < n; ++i) {
                @SuppressWarnings("unchecked")
                T val = (T) (withDateTime ? new DateTimePair(vals[i].getString(0), dts[i])
                        : vals[i].getString(0));
                String label = labels[i].getString(0);
                @SuppressWarnings("unchecked")
                X lab = (X) (itemFrame ? label : Topic.toEnum(label));
                frame.put(lab, val);
            }
            return frame;
        }

        @SuppressWarnings("unchecked")
        private <R> R
        _nativeAccessByType(Class<?> valType, String callNamePre, String callNamePost,
                            Class<?>[] vArgTypes, Object... args) {
            String typeName = valType.equals(Long.class) ? "Long"
                    : (valType.equals(Double.class) ? "Double" : "String");
            String methodName = callNamePre + typeName + callNamePost;
            try {
                Method m = DataBlock.class.getDeclaredMethod(methodName, vArgTypes);
                m.setAccessible(true);
                return (R) m.invoke(DataBlock.this, args);
            } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException e) {
                throw new RuntimeException("_nativeAccessByType failed to call: " + methodName);
            }
        }
    } /* class DataBlockSharedHelper */


    private String
    _handleRawItem(String item, boolean throwIfNotInBlock)
            throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        if(item == null)
            throw new NullPointerException("null item string");
        item = item.toUpperCase();
        int slen = item.length();
        if(slen < 1 || slen > MAX_STR_SZ){
            throw new InvalidItemOrTopic("invalid string length: " + slen);
        }
        if(throwIfNotInBlock && !_items.contains(item)) {
            throw new InvalidItemOrTopic("item not in block: " + item);
        }
        return item;
    }

    private void
    _handleRawTopic(Topic topic, boolean throwIfNotInBlock)
            throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        if(topic == null){
            throw new NullPointerException("null topic enum");
        }
        if(topic == Topic.NULL_TOPIC){
            throw new InvalidItemOrTopic("'NULL_TOPIC' is not a valid Topic");
        }
        if(throwIfNotInBlock && !_topics.contains(topic)){
            throw new InvalidItemOrTopic("topic not in block: " + topic);
        }
    }

    private String
    _handleRawItemTopic(String item, Topic topic, boolean throwIfNotInBlock)
            throws CLibException, LibraryNotLoaded, InvalidItemOrTopic {
        _handleRawTopic(topic, throwIfNotInBlock);
        return _handleRawItem(item, throwIfNotInBlock);
    }

    private int
    _getBlockSize() throws LibraryNotLoaded, CLibException {
        int[] size = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetBlockSize(_name, size);
        if (err != 0) {
            throw new CLibException("TOSDB_GetBlockSize", err);
        }
        return size[0];
    }

    private int
    _getItemCount() throws CLibException, LibraryNotLoaded {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetItemCount(_name, count);
        if (err != 0) {
            throw new CLibException("TOSDB_GetItemCount", err);
        }
        return count[0];
    }

    private int
    _getTopicCount() throws CLibException, LibraryNotLoaded {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetTopicCount(_name, count);
        if (err != 0) {
            throw new CLibException("TOSDB_GetTopicCount", err);
        }
        return count[0];
    }

    private int
    _getItemPreCachedCount() throws CLibException, LibraryNotLoaded {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetPreCachedItemCount(_name, count);
        if (err != 0) {
            throw new CLibException("TOSDB_GetPreCachedItemCount", err);
        }
        return count[0];
    }

    private int
    _getTopicPreCachedCount() throws CLibException, LibraryNotLoaded {
        int[] count = {0};
        int err = TOSDataBridge.getCLibrary().TOSDB_GetPreCachedTopicCount(_name, count);
        if (err != 0) {
            throw new CLibException("TOSDB_GetPreCachedTopicCount", err);
        }
        return count[0];
    }

    private <T> Set<T>
    _getItemTopicNames(boolean useItemVersion, boolean usePreCachedVersion)
            throws LibraryNotLoaded, CLibException {
        int n;
        if (useItemVersion) {
            n = usePreCachedVersion ? _getItemPreCachedCount() : _getItemCount();
        } else {
            n = usePreCachedVersion ? _getTopicPreCachedCount() : _getTopicCount();
        }
        if (n < 1) {
            return new HashSet<>();
        }

        Pointer[] vals = new Pointer[n];
        for (int i = 0; i < n; ++i) {
            vals[i] = new Memory(MAX_STR_SZ + 1);
        }

        int err;
        if (useItemVersion) {
            err = usePreCachedVersion
                ? TOSDataBridge.getCLibrary().TOSDB_GetPreCachedItemNames(_name,vals,n,MAX_STR_SZ)
                : TOSDataBridge.getCLibrary().TOSDB_GetItemNames(_name,vals,n,MAX_STR_SZ);

        } else {
            err = usePreCachedVersion
                ? TOSDataBridge.getCLibrary().TOSDB_GetPreCachedTopicNames(_name,vals,n,MAX_STR_SZ)
                : TOSDataBridge.getCLibrary().TOSDB_GetTopicNames(_name,vals,n,MAX_STR_SZ);
        }
        if (err != 0) {
            throw new CLibException("TOSDB_Get" + (usePreCachedVersion ? "PreCached" : "")
                    + (useItemVersion ? "Item" : "Topic") + "Names", err);
        }

        Set<T> names = new HashSet<>();
        for (int i = 0; i < n; ++i) {
            String s = vals[i].getString(0);
            @SuppressWarnings("unchecked")
            T name = (T) (useItemVersion ? s : Topic.toEnum(s));
            names.add(name);
        }
        return names;
    }

    @SuppressWarnings("unchecked")
    private <T> T
    _getLong(String item, Topic topic, int indx, boolean withDateTime)
            throws CLibException, LibraryNotLoaded {
        DateTime dt = withDateTime ? new DateTime() : null;
        long[] val = {0};
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetLongLong(_name, item, topic.val, indx, val, dt);
        if (err != 0) {
            throw new CLibException("TOSDB_GetLongLong", err);
        }
        return (T) (withDateTime ? new DateTimePair<>(val[0], dt) : val[0]);
    }

    @SuppressWarnings("unchecked")
    private <T> T
    _getDouble(String item, Topic topic, int indx, boolean withDateTime)
            throws CLibException, LibraryNotLoaded {
        DateTime dt = withDateTime ? new DateTime() : null;
        double[] val = {0};
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetDouble(_name, item, topic.val, indx, val, dt);
        if (err != 0) {
            throw new CLibException("TOSDB_GetDouble", err);
        }
        return (T) (withDateTime ? new DateTimePair<>(val[0], dt) : val[0]);
    }

    @SuppressWarnings("unchecked")
    private <T> T
    _getString(String item, Topic topic, int indx, boolean withDateTime)
            throws CLibException, LibraryNotLoaded {
        DateTime dt = withDateTime ? new DateTime() : null;
        byte[] val = new byte[STR_DATA_SZ + 1];
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetString(_name, item, topic.val, indx, val, STR_DATA_SZ + 1, dt);
        if (err != 0) {
            throw new CLibException("TOSDB_GetString", err);
        }
        return (T) (withDateTime ? new DateTimePair<>(Native.toString(val), dt)
                                 : Native.toString(val));
    }

    @SuppressWarnings("unchecked")
    private <T> List<T>
    _getStreamSnapshotLongs(String item, Topic topic, int end, int beg, boolean withDateTime,
                            int size)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        DateTime[] dts = (withDateTime ? new DateTime[size] : null);
        long[] vals = new long[size];
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamSnapshotLongLongs(_name, item, topic.val, vals,
                        size, dts, end, beg);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamSnapshotLongs", err);
        }
        return (List<T>)(withDateTime ? rawArraysToList(vals, dts, size)
                                      : rawArraysToList(vals, size));
    }

    @SuppressWarnings("unchecked")
    private <T> List<T>
    _getStreamSnapshotDoubles(String item, Topic topic, int end, int beg, boolean withDateTime,
                              int size)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        DateTime[] dts = (withDateTime ? new DateTime[size] : null);
        double[] vals = new double[size];
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamSnapshotDoubles(_name, item, topic.val, vals,
                        size, dts, end, beg);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamSnapshotDoubles", err);
        }
        return (List<T>)(withDateTime ? rawArraysToList(vals, dts, size)
                                      : rawArraysToList(vals, size));
    }

    @SuppressWarnings("unchecked")
    private <T> List<T>
    _getStreamSnapshotStrings(String item, Topic topic, int end, int beg, boolean withDateTime,
                              int size)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        DateTime[] dts = (withDateTime ? new DateTime[size] : null);
        Pointer[] vals = new Pointer[size];
        for (int i = 0; i < size; ++i) {
            vals[i] = new Memory(STR_DATA_SZ + 1);
        }
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamSnapshotStrings(_name, item, topic.val, vals,
                        size, STR_DATA_SZ + 1, dts, end, beg);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamSnapshotStrings", err);
        }
        return (List<T>)(withDateTime ? rawArraysToList(vals, dts, size)
                                      : rawArraysToList(vals, size));
    }

    @SuppressWarnings("unchecked")
    private <T> List<T>
    _getStreamSnapshotLongsFromMarker(String item, Topic topic, int beg, boolean withDateTime,
                                      boolean throwIfDataLost, int safeSz)
            throws CLibException, LibraryNotLoaded, DirtyMarkerException {
        DateTime[] dts = (withDateTime ? new DateTime[safeSz] : null);
        NativeLong[] getSz = {new NativeLong(0)};
        long[] vals = new long[safeSz];
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamSnapshotLongLongsFromMarker(_name, item, topic.val,
                        vals, safeSz, dts, beg, getSz);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamSnapshotLongsFromMarker", err);
        }

        int szGot = (int) _handleSzGotFromMarker(getSz[0], throwIfDataLost);
        return (List<T>)(withDateTime ? rawArraysToList(vals, dts, szGot)
                                      : rawArraysToList(vals, szGot));
    }

    @SuppressWarnings("unchecked")
    private <T> List<T>
    _getStreamSnapshotDoublesFromMarker(String item, Topic topic, int beg, boolean withDateTime,
                                        boolean throwIfDataLost, int safeSz)
            throws CLibException, LibraryNotLoaded, DirtyMarkerException {
        DateTime[] dts = (withDateTime ? new DateTime[safeSz] : null);
        NativeLong[] getSz = {new NativeLong(0)};
        double[] vals = new double[safeSz];
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamSnapshotDoublesFromMarker(_name, item, topic.val,
                        vals, safeSz, dts, beg, getSz);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamSnapshotDoublesFromMarker", err);
        }

        int szGot = (int) _handleSzGotFromMarker(getSz[0], throwIfDataLost);
        return (List<T>)(withDateTime ? rawArraysToList(vals, dts, szGot)
                                      : rawArraysToList(vals, szGot));
    }

    @SuppressWarnings("unchecked")
    private <T> List<T>
    _getStreamSnapshotStringsFromMarker(String item, Topic topic, int beg, boolean withDateTime,
                                        boolean throwIfDataLost, int safeSz)
            throws CLibException, LibraryNotLoaded, DirtyMarkerException {
        Pointer[] vals = new Pointer[safeSz];
        DateTime[] dts = (withDateTime ? new DateTime[safeSz] : null);
        NativeLong[] getSz = {new NativeLong(0)};
        for (int i = 0; i < safeSz; ++i) {
            vals[i] = new Memory(STR_DATA_SZ + 1);
        }
        int err = TOSDataBridge.getCLibrary()
                .TOSDB_GetStreamSnapshotStringsFromMarker(_name, item, topic.val,
                        vals, safeSz, STR_DATA_SZ + 1, dts, beg, getSz);
        if (err != 0) {
            throw new CLibException("TOSDB_GetStreamSnapshotStringsFromMarker", err);
        }

        int szGot = (int) _handleSzGotFromMarker(getSz[0], throwIfDataLost);
        return (List<T>)(withDateTime ? rawArraysToList(vals, dts, szGot)
                                      : rawArraysToList(vals, szGot));
    }

    private long
    _handleSzGotFromMarker(NativeLong l, boolean throwIfDataLost) throws DirtyMarkerException {
        long szGot = l.longValue();
        if (szGot < 0) {
            if (throwIfDataLost) {
                throw new DirtyMarkerException();
            } else {
                szGot *= -1;
            }
        }
        return szGot;
    }

    static private List<Long>
    rawArraysToList(long[] p, int sz) {
        final List<Long> o = new ArrayList<>(sz);
        for (int i = 0; i < sz; ++i) {
            o.add(p[i]);
        }
        return o;
    }

    static private List<Double>
    rawArraysToList(double[] p, int sz) {
        final List<Double> o = new ArrayList<>(sz);
        for (int i = 0; i < sz; ++i) {
            o.add(p[i]);
        }
        return o;
    }

    static private List<String>
    rawArraysToList(Pointer[] p, int sz) {
        final List<String> o = new ArrayList<>(sz);
        for (int i = 0; i < sz; ++i) {
            o.add(p[i].getString(0));
        }
        return o;
    }

    static private List<DateTimePair<Long>>
    rawArraysToList(long[] p, DateTime[] dt, int sz) {
        final List<DateTimePair<Long>> o = new ArrayList<>(sz);
        for (int i = 0; i < sz; ++i) {
            o.add(new DateTimePair<>(p[i], dt[i]));
        }
        return o;
    }

    static private List<DateTimePair<Double>>
    rawArraysToList(double[] p, DateTime[] dt, int sz) {
        final List<DateTimePair<Double>> o = new ArrayList<>(sz);
        for (int i = 0; i < sz; ++i) {
            o.add(new DateTimePair<>(p[i], dt[i]));
        }
        return o;
    }

    static private List<DateTimePair<String>>
    rawArraysToList(Pointer[] p, DateTime[] dt, int sz) {
        final List<DateTimePair<String>> o = new ArrayList<>(sz);
        for (int i = 0; i < sz; ++i) {
            o.add(new DateTimePair<>(p[i].getString(0), dt[i]));
        }
        return o;
    }

}