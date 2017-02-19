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
 * Extends DataBlock, adding DateTime objects to primary primary data.
 *
 * @author Jonathon Ogden
 * @version 0.7
 * @throws CLibException   If C Lib returned a 'native' error (CError.java)
 * @throws LibraryNotLoaded  If native C Lib has not been loaded via init(...)
 * @see DataBlock
 * @see TOSDataBridge
 * @see Topic
 * @see DateTime
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md"> README </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_JAVA.md"> README - JAVA API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_API.md"> README - C API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_SERVICE.md"> README - SERVICE </a>
 */
public class DataBlockWithDateTime extends DataBlock {

    /**
     * DataBlockWithDateTime Constructor.
     *
     * @param size maximum amount of historical data block can hold
     * @throws CLibException
     * @throws LibraryNotLoaded
     */
    public DataBlockWithDateTime(int size) throws CLibException, LibraryNotLoaded {
        super(size, DEF_TIMEOUT);
    }

    /**
     * DataBlockWithDateTime Constructor.
     *
     * @param size maximum amount of historical data block can hold
     * @param timeout timeout of internal IPC and DDE mechanisms (milliseconds)
     * @throws LibraryNotLoaded
     * @throws CLibException
     */
    public DataBlockWithDateTime(int size, int timeout) throws LibraryNotLoaded, CLibException {
        super(size, true, timeout); // use the protected base constructor
    }

    public DateTimePair<Long>
    getLongWithDateTime(String item, Topic topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic,0, checkIndx, true, Long.class);
    }

    public DateTimePair<Long>
    getLongWithDateTime(String item, Topic topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic, indx, checkIndx, true, Long.class);
    }

    public DateTimePair<Double>
    getDoubleWithDateTime(String item, Topic topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic,0, checkIndx, true, Double.class);
    }

    public DateTimePair<Double>
    getDoubleWithDateTime(String item, Topic topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic, indx, checkIndx, true, Double.class);
    }

    public DateTimePair<String>
    getStringWithDateTime(String item, Topic topic, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic,0, checkIndx, true, String.class);
    }

    public DateTimePair<String>
    getStringWithDateTime(String item, Topic topic,int indx, boolean checkIndx)
            throws CLibException, LibraryNotLoaded, DataIndexException
    {
        return get(item,topic, indx, checkIndx, true, String.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item, topic, -1, 0, true, true, Long.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,0,true, true,Long.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,beg,true,true,Long.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,-1,0,true, true,Double.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,0,true, true,Double.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,beg,true,true,Double.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,-1,0,true, true, String.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsWithDateTime(String item, Topic topic, int end)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,0,true,true,String.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsWithDateTime(String item, Topic topic, int end, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshot(item,topic,end,beg,true,true,String.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,0,true,true,Long.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,beg,true,true,Long.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,0,true,true,Double.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,beg,true,true,Double.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException,DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,0,true,true,String.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTime(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException, DirtyMarkerException
    {
        return getStreamSnapshotFromMarker(item,topic,beg,true,true,String.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,0,true,Long.class);
    }

    public DateTimePair<Long>[]
    getStreamSnapshotLongsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,beg,true,Long.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,0,true,Double.class);
    }

    public DateTimePair<Double>[]
    getStreamSnapshotDoublesFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,beg,true,Double.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,0,true,String.class);
    }

    public DateTimePair<String>[]
    getStreamSnapshotStringsFromMarkerWithDateTimeIgnoreDirty(String item, Topic topic, int beg)
            throws LibraryNotLoaded, CLibException, DataIndexException
    {
        return getStreamSnapshotFromMarkerIgnoreDirty(item,topic,beg,true,String.class);
    }

    public Map<String,DateTimePair<String>>
    getItemFrameWithDateTime(Topic topic)
            throws CLibException, LibraryNotLoaded
    {
        return _getFrame(topic, true,true);
    }

    public Map<Topic,DateTimePair<String>>
    getTopicFrameWithDateTime(String item)
            throws CLibException, LibraryNotLoaded
    {
        return _getFrame(item, false,true);
    }


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