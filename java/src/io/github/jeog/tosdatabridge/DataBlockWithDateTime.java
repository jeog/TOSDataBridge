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

import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;
import io.github.jeog.tosdatabridge.TOSDataBridge.*;

import java.lang.reflect.Array;
import java.util.*;

/**
 * DataBlockWithDateTime(s) are used to get real-time financial data from the TOS
 * platform. They extend DataBlock, attaching a DateTime object to each data-point.
 * They use the JNA interface(CLib.java) to access the C API, wrapping the
 * underlying (conceptual) block of the C lib with an object-oriented java interface.
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
 * the size of the block. The block can be though of as a 3D object with the following
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
        super(size, DEF_TIMEOUT);
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
     * Get most recent data-point as DateTimePair&lt;Long&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param checkIndx throw if data doesn't exist at this index/position yet
     * @return data-point and DateTime at position 'indx'
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Long>
    getLongWithDateTime(String item, Topic topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic,0, checkIndx, true, Long.class);
    }

    /**
     * Get single data-point as DateTimePair&lt;Long&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param indx index/position of data-point
     * @param checkIndx throw if data doesn't exist at this index/position yet
     * @return data-point and DateTime at position 'indx'
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Long>
    getLongWithDateTime(String item, Topic topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic, indx, checkIndx, true, Long.class);
    }

    /**
     * Get most recent data-point as DateTimePair&lt;Double&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param checkIndx throw if data doesn't exist at this index/position yet
     * @return data-point and DateTime at position 'indx'
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Double>
    getDoubleWithDateTime(String item, Topic topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic,0, checkIndx, true, Double.class);
    }

    /**
     * Get single data-point as DateTimePair&lt;Double&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param indx index/position of data-point
     * @param checkIndx throw if data doesn't exist at this index/position yet
     * @return data-point and DateTime at position 'indx'
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Double>
    getDoubleWithDateTime(String item, Topic topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic, indx, checkIndx, true, Double.class);
    }

    /**
     * Get most recent data-point as DateTimePair&lt;String&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param checkIndx throw if data doesn't exist at this index/position yet
     * @return data-point and DateTime at position 'indx'
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<String>
    getStringWithDateTime(String item, Topic topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic,0, checkIndx, true, String.class);
    }

    /**
     * Get single data-point as DateTimePair&lt;String&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param indx index/position of data-point
     * @param checkIndx throw if data doesn't exist at this index/position yet
     * @return data-point and DateTime at position 'indx'
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<String>
    getStringWithDateTime(String item, Topic topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic, indx, checkIndx, true, String.class);
    }

    /**
     * Get all data-points currently in stream, as array of DateTimePair&lt;Long&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes in the stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException
    {
        return getStreamSnapshotAll(item, topic, true, Long.class);
        //return getStreamSnapshot(item, topic, -1, 0, true, true, Long.class);
    }

    /**
     * Get multiple data-points, between most recent and 'end', as array of
     * DateTimePair&lt;Long&gt;.
     *
     * @param item item string of the stream
     * @param topic Topic enum of the stream
     * @param end least recent index/position from which data is pulled
     * @return data-points and DateTimes between most recent and 'end'
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,0,true, true,Long.class);
    }

    /**
     *
     * Get multiple data-points, between 'beg' and 'end', as array of
     * DateTimePair&lt;Long&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param end least recent index/position from which data is pulled
     * @param beg most recent index/position from which data is pulled
     * @return data-points and DateTimes between 'beg' and 'end'
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,beg,true,true,Long.class);
    }

    /**
     * Get all data-points currently in stream, as array of
     * DateTimePair&lt;Double&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes in the stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException
    {
        return getStreamSnapshotAll(item, topic, true, Double.class);
        //return getStreamSnapshot(item,topic,-1,0,true, true,Double.class);
    }

    /**
     * Get multiple data-points, between most recent and 'end', as array of
     * DateTimePair&lt;Double&gt;.
     *
     * @param item item string of the stream
     * @param topic Topic enum of the stream
     * @param end least recent index/position from which data is pulled
     * @return data-points and DateTimes between most recent and 'end'
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,0,true, true,Double.class);
    }

    /**
     *
     * Get multiple data-points, between 'beg' and 'end', as array of
     * DateTimePair&lt;Double&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param end least recent index/position from which data is pulled
     * @param beg most recent index/position from which data is pulled
     * @return data-points and DateTimes between 'beg' and 'end'
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,beg,true,true,Double.class);
    }

    /**
     * Get all data-points currently in stream, as array of DateTimePair&lt;String&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes in the stream
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException
    {
        return getStreamSnapshotAll(item, topic, true, String.class);
        //return getStreamSnapshot(item,topic,-1,0,true, true, String.class);
    }

    /**
     * Get multiple data-points, between most recent and 'end', as array of
     * DateTimePair&lt;String&gt;.
     *
     * @param item item string of the stream
     * @param topic Topic enum of the stream
     * @param end least recent index/position from which data is pulled
     * @return data-points and DateTimes between most recent and 'end'
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,0,true,true,String.class);
    }

    /**
     *
     * Get multiple data-points, between 'beg' and 'end', as array of
     * DateTimePair&lt;String&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param end least recent index/position from which data is pulled
     * @param beg most recent index/position from which data is pulled
     * @return data-points and DateTimes between 'beg' and 'end'
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,beg,true,true,String.class);
    }

    /**
     * Get all data-points up to 'atomic' marker, as array of DateTimePair&lt;Long&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes up to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarkerToMostRecent(item,topic,true,Long.class);
        //return getStreamSnapshotFromMarker(item,topic,0,true,true,Long.class);
    }

    /**
     * Get all data-points from 'beg' to 'atomic' marker, as array of
     * DateTimePair&lt;Long&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param beg most recent index/position from which data is pulled
     * @return all data-points and DateTimes from 'beg' to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws DirtyMarkerException  marker is 'dirty' (data lost behind it)
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,beg,true,true,Long.class);
    }

    /**
     * Get all data-points up to 'atomic' marker, as array of
     * DateTimePair&lt;Double&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes up to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DirtyMarkerException  marker is 'dirty' (data lost behind it)
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarkerToMostRecent(item,topic,true,Double.class);
        //return getStreamSnapshotFromMarker(item,topic,0,true,true,Double.class);
    }

    /**
     * Get all data-points from 'beg' to 'atomic' marker, as array of
     * DateTimePair&lt;Double&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param beg most recent index/position from which data is pulled
     * @return all data-points and DateTimes from 'beg' to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws DirtyMarkerException  marker is 'dirty' (data lost behind it)
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,beg,true,true,Double.class);
    }

    /**
     * Get all data-points up to 'atomic' marker, as array of DateTimePair&lt;String&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes up to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DirtyMarkerException  marker is 'dirty' (data lost behind it)
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarkerToMostRecent(item,topic,true,String.class);
        //return getStreamSnapshotFromMarker(item,topic,0,true,true,String.class);
    }

    /**
     * Get all data-points from 'beg' to 'atomic' marker, as array of
     * DateTimePair&lt;String&gt;.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param beg most recent index/position from which data is pulled
     * @return all data-points and DateTimes from 'beg' to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     * @throws DirtyMarkerException  marker is 'dirty' (data lost behind it)
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,beg,true,true,String.class);
    }

    /**
     * Get all data-points up to 'atomic' marker, as array of DateTimePair&lt;Long&gt;
     * - IGNORE DIRTY MARKER.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes up to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException
    {
        return getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item,topic,true,Long.class);
        //return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,0,true,Long.class);
    }

    /**
     * Get all data-points from 'beg' to 'atomic' marker, as array of DateTimePair&lt;Long&gt;
     * - IGNORE DIRTY MARKER.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param beg most recent index/position from which data is pulled
     * @return all data-points and DateTimes from 'beg' to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,beg,true,Long.class);
    }

    /**
     * Get all data-points up to 'atomic' marker, as array of DateTimePair&lt;Double&gt;
     * - IGNORE DIRTY MARKER.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes up to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException
    {
        return getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item,topic,true,Double.class);
        //return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,0,true,Double.class);
    }

    /**
     * Get all data-points from 'beg' to 'atomic' marker, as array of DateTimePair&lt;Double&gt;
     * - INGORE DIRTY MARKER.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param beg most recent index/position from which data is pulled
     * @return all data-points and DateTimes from 'beg' to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,beg,true,Double.class);
    }

    /**
     * Get all data-points up to 'atomic' marker, as array of DateTimePair&lt;String&gt;
     * - IGNORE DIRTY MARKER.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @return all data-points and DateTimes up to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException
    {
        return getStreamSnapshotFromMarkerToMostRecentIgnoreDirty(item,topic,true,String.class);
        //return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,0,true,String.class);
    }

    /**
     * Get all data-points from 'beg' to 'atomic' marker, as array of
     * DateTimePair&lt;String&gt; - IGNORE DIRTY MARKER.
     *
     * @param item item string of stream
     * @param topic Topic enum of stream
     * @param beg most recent index/position from which data is pulled
     * @return all data-points and DateTimes from 'beg' to 'atomic' marker
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     * @throws DataIndexException invalid index/position value
     */
    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,beg,true,String.class);
    }

    /**
     * Get all item values, as strings, for a particular topic.
     *
     * @param topic Topic enum of streams
     * @return Mapping of item names to values
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     */
    public Map<String,DateTimePair<String>>
    getItemFrameWithDateTime(Topic topic)
            throws CLibException, LibraryNotLoaded
    {
        return _getFrame(topic, true,true);
    }

    /**
     * Get all topic values, as strings, for a particular item.
     *
     * @param item item string of streams
     * @return Mapping of topic names to values
     * @throws CLibException error code returned by C lib
     * @throws LibraryNotLoaded C lib has not been loaded
     */
    public Map<Topic,DateTimePair<String>>
    getTopicFrameWithDateTime(String item)
            throws CLibException, LibraryNotLoaded
    {
        return _getFrame(item, false,true);
    }

    /**
     * Get all topic AND item values, as strings, of the block.
     *
     * @return Mapping of item names to Mapping of Topic enums to values
     * @throws LibraryNotLoaded C lib has not been loaded
     * @throws CLibException error code returned by C lib
     */
    public Map<String, Map<Topic,DateTimePair<String>>>
    getTotalFrameWithDateTime() throws LibraryNotLoaded, CLibException
    {
        Map<String, Map<Topic,DateTimePair<String>>> frame = new HashMap<>();
        for(String item : getItems()){
            Map<Topic,DateTimePair<String>> tf = getTopicFrameWithDateTime(item);
            frame.put(item, tf);
        }
        return frame;
    }
}