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

import java.util.*;

/**
 * DataBlockWithDateTime is used to get real-time financial data from the TOS
 * platform. It extends DataBlock, attaching a DateTime object to each data-point.
 * <p>
 * <strong>IMPORTANT:</strong> Before creating/using a DataBlockWithDateTime the
 * underlying library must be loaded AND connected to the backend Service/Engine
 * and the TOS platform.
 * <ol>
 * <li> be sure you've installed the C mods (tosdb-setup.bat)
 * <li> start the Service/Engine (SC Start TOSDataBridge)
 * <li> start the TOS Platform
 * <li> call TOSDataBridge.init(...)
 * <li> create DataBlockWithDateTime(s)
 * </ol>
 * <p>
 * DataBlockWithDateTime works much the same as its python cousin
 * (tosdb._win.TOSDB_DataBlock). It's instantiated with a size parameter to indicate
 * how much historical data is saved. Once created 'topics' (data fields found in the
 * Topic enum) and 'items' (symbols) are added to create a matrix of 'streams', each
 * the size of the block. The block can be thought of as a 3D object with the following
 * dimensions:
 * <p>
 * &nbsp&nbsp&nbsp&nbsp (# of topics) &nbsp x &nbsp (# of items) &nbsp x &nbsp (block size)
 * <p>
 * As data flows into these streams the various 'get' methods are then used to
 * 1) examine the underlying state of the block/streams, and 2) pull data from
 * the block/streams in myriad ways.
 *
 * @author Jonathon Ogden
 * @version 0.7
 * @throws CLibException  error code returned by C lib
 * @throws LibraryNotLoaded  C lib has not been loaded
 * @see DataBlock
 * @see TOSDataBridge
 * @see Topic
 * @see DateTime
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md"> README </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_JAVA.md"> README - JAVA API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_API.md"> README - C API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_PYTHON.md"> README - PYTHON API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_SERVICE.md"> README - SERVICE </a>
 */
public class DataBlockWithDateTime extends DataBlock {

    /**
     * DataBlockWithDateTime Constructor.
     *
     * @param size maximum amount of historical data block can hold
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     */
    public DataBlockWithDateTime(int size) throws CLibException, LibraryNotLoaded {
        super(size, DEF_TIMEOUT); // use the public base constructor
    }

    /**
     * DataBlockWithDateTime Constructor.
     *
     * @param size maximum amount of historical data block can hold
     * @param timeout timeout of internal IPC and DDE mechanisms (milliseconds)
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DataBlockWithDateTime(int size, int timeout) throws LibraryNotLoaded, CLibException {
        super(size, true, timeout); // use the protected base constructor
    }

    /**
     * @return if block is storing DateTime alongside primary data
     */
    @Override
    public boolean
    isUsingDateTime() {
        return true;
    }

    /**
     * Returns most recent data-point and DateTime of a stream, as DateTimePair&lt;Long&gt;
     * (or null if no data).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return most recent data-point and DateTime of stream (or null)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @see Topic
     */
    public DateTimePair<Long>
    getLongWithDateTime(String item, Topic topic) throws CLibException, LibraryNotLoaded {
        return getHelper().getMostRecent(item, topic, true, Long.class);
    }

    /**
     * Returns data-point and DateTime of a stream, as DateTimePair&lt;Long&gt;
     * (or null if no data at that position/index).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param indx  index/position of data-point
     * @return data-point and DateTime of stream (or null)
     * @throws CLibException      error code returned by C lib
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public DateTimePair<Long>
    getLongWithDateTime(String item, Topic topic,int indx)
            throws CLibException, LibraryNotLoaded, DataIndexException {
        return getHelper().get(item,topic, indx, true, Long.class);
    }

    /**
     * Returns most recent data-point and DateTime of a stream, as DateTimePair&lt;Double&gt;
     * (or null if no data).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return most recent data-point and DateTime of stream (or null)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @see Topic
     */
    public DateTimePair<Double>
    getDoubleWithDateTime(String item, Topic topic) throws CLibException, LibraryNotLoaded {
        return getHelper().getMostRecent(item, topic, true, Double.class);
    }

    /**
     * Returns data-point and DateTime of a stream, as DateTimePair&lt;Double&gt;
     * (or null if no data at that position/index).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param indx  index/position of data-point
     * @return data-point and DateTime of stream (or null)
     * @throws CLibException      error code returned by C lib
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public DateTimePair<Double>
    getDoubleWithDateTime(String item, Topic topic,int indx)
            throws CLibException, LibraryNotLoaded, DataIndexException {
        return getHelper().get(item,topic, indx, true, Double.class);
    }

    /**
     * Returns most recent data-point and DateTime of a stream, as DateTimePair&lt;String&gt;
     * (or null if no data).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return most recent data-point and DateTime of stream (or null)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @see Topic
     */
    public DateTimePair<String>
    getStringWithDateTime(String item, Topic topic) throws CLibException, LibraryNotLoaded {
        return getHelper().getMostRecent(item, topic, true, String.class);
    }

    /**
     * Returns data-point and DateTime of a stream, as DateTimePair&lt;String&gt;
     * (or null if no data at that position/index).
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param indx  index/position of data-point
     * @return data-point and DateTime of stream (or null)
     * @throws CLibException      error code returned by C lib
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public DateTimePair<String>
    getStringWithDateTime(String item, Topic topic,int indx)
            throws CLibException, LibraryNotLoaded, DataIndexException {
        return getHelper().get(item, topic, indx, true, String.class);
    }

    /**
     * Returns all data-points and DateTime(s) of a stream,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException {
        return getHelper().getStreamSnapshotAll(item, topic, true, Long.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) of a stream, from most recent,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;.
     *
     * @param item  item string of the stream
     * @param topic topic enum of the stream
     * @param end   least recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshot(item, topic, end, 0, true, true, Long.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) of a stream,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param end   least recent index/position from which data is pulled
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshot(item, topic, end, beg, true, true, Long.class);
    }

    /**
     * Returns all data-points and DateTime(s) of a stream,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException {
        return getHelper().getStreamSnapshotAll(item, topic, true, Double.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) of a stream, from most recent,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;.
     *
     * @param item  item string of the stream
     * @param topic topic enum of the stream
     * @param end   least recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshot(item, topic, end, 0, true, true, Double.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) of a stream,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param end   least recent index/position from which data is pulled
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshot(item, topic, end, beg, true, true, Double.class);
    }

    /**
     * Returns all data-points and DateTime(s) of a stream,
     * as List&lt;DateTimePair&lt;String&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsWithDateTime(String item, Topic topic) 
            throws LibraryNotLoaded, CLibException {
        return getHelper().getStreamSnapshotAll(item, topic, true, String.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) of a stream, from most recent,
     * as List&lt;DateTimePair&lt;String&gt;&gt;.
     *
     * @param item  item string of the stream
     * @param topic topic enum of the stream
     * @param end   least recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshot(item, topic, end, 0, true, true, String.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) of a stream,
     * as List&lt;DateTimePair&lt;String&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param end   least recent index/position from which data is pulled
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) of stream
     * @throws LibraryNotLoaded   C lib has not been loaded
     * @throws CLibException      error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshot(item, topic, end, beg, true, true, String.class);
    }

    /**
     * Returns all data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException {
        return getHelper().getStreamSnapshotFromMarkerToMostRecent(item, topic, true, Long.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException {
        return getHelper().getStreamSnapshotFromMarker(item, topic, beg, true, true, Long.class);
    }

    /**
     * Returns all data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException {
        return getHelper().getStreamSnapshotFromMarkerToMostRecent(item, topic, true, Double.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException {
        return getHelper().getStreamSnapshotFromMarker(item, topic, beg, true, true, Double.class);
    }

    /**
     * Returns all data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;String&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException {
        return getHelper().getStreamSnapshotFromMarkerToMostRecent(item, topic, true, String.class);
    }

    /**
     * Returns multiple contiguous data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;String&gt;&gt;.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @throws DirtyMarkerException marker is 'dirty' (data lost behind it)
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException {
        return getHelper().getStreamSnapshotFromMarker(item, topic, beg, true, true, String.class);
    }

    /**
     * Returns all data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;. Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException {
        return getHelper().getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item, topic,
                true, Long.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Long&gt;&gt;. Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<Long>>
    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshotFromMarkerIgnoreDirty(item, topic, beg, true,
                Long.class);
    }

    /**
     * Returns all data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;. Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException {
        return getHelper().getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item, topic,
                true, Double.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;Double&gt;&gt;. Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<Double>>
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshotFromMarkerIgnoreDirty(item, topic, beg, true,
                Double.class);
    }

    /**
     * Returns all data-points and DateTime(s) up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;String&gt;&gt;. Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @return all data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException {
        return getHelper().getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item, topic,
                true, String.class);
    }

    /**
     * Returns multiple contiguous data-points up to atomic marker of a stream,
     * as List&lt;DateTimePair&lt;String&gt;&gt;. Ignores 'dirty' marker/stream.
     *
     * @param item  item string of stream
     * @param topic topic enum of stream
     * @param beg   most recent index/position from which data is pulled
     * @return multiple contiguous data-points and DateTime(s) up to atomic marker of stream
     * @throws LibraryNotLoaded     C lib has not been loaded
     * @throws CLibException        error code returned by C lib
     * @throws DataIndexException   invalid index/position value
     * @see Topic
     */
    public List<DateTimePair<String>>
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException {
        return getHelper().getStreamSnapshotFromMarkerIgnoreDirty(item, topic, beg, true,
                String.class);
    }

    /**
     * Returns mapping of all item names to most recent item values and DateTime(s)
     * for a particular topic.
     *
     * @param topic topic enum of streams
     * @return mapping of item names to most recent item values and DateTime(s)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @see Topic
     */
    public Map<String,DateTimePair<String>>
    getItemFrameWithDateTime(Topic topic) throws CLibException, LibraryNotLoaded {
        return getHelper().getFrame(topic, true,true);
    }

    /**
     * Returns mapping of all topic enums to most recent topic values and DateTime(s)
     * for a particular item.
     *
     * @param item item string of streams
     * @return mapping of topic enums to most recent topic values and DateTime(s)
     * @throws CLibException    error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     */
    public Map<Topic,DateTimePair<String>>
    getTopicFrameWithDateTime(String item) throws CLibException, LibraryNotLoaded {
        return getHelper().getFrame(item, false,true);
    }

    /**
     * Returns mapping of all item strings to mappings of all topic enums to most recent
     * topic values and DateTime(s). This returns ALL the most recent data in the block.
     *
     * @return mapping of item strings to mappings of topic enums to most recent topic
     *         values and DateTime(s)
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException    error code returned by C lib
     */
    public Map<String, Map<Topic,DateTimePair<String>>>
    getTotalFrameWithDateTime() throws LibraryNotLoaded, CLibException {
        Map<String, Map<Topic,DateTimePair<String>>> frame = new HashMap<>();
        for(String item : getItems()){
            Map<Topic,DateTimePair<String>> tf = getTopicFrameWithDateTime(item);
            frame.put(item, tf);
        }
        return frame;
    }
}