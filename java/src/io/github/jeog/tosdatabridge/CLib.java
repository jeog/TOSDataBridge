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

import com.sun.jna.Library;
import com.sun.jna.NativeLong;
import com.sun.jna.Pointer;

/**
 * Clib.java
 *
 * JNA interface used to access the underlying C Lib.
 *
 * @author Jonathon Ogden
 * @version 0.7
 */
public interface CLib extends Library {
    int TOSDB_Connect();
    int TOSDB_IsConnectedToEngineAndTOS();
    int TOSDB_ConnectionState();
    int TOSDB_GetBlockLimit();
    int TOSDB_SetBlockLimit(int limit);
    int TOSDB_GetBlockCount();
    int TOSDB_GetTypeBits(String topic, byte[] ptrBits);

    int TOSDB_CreateBlock(String name, int size, boolean dateTime, int timeout);
    int TOSDB_CloseBlock(String name);
    int TOSDB_GetItemCount(String name, int[] ptrCount);
    int TOSDB_GetTopicCount(String name, int[] ptrCount);
    int TOSDB_GetPreCachedItemCount(String name, int[] ptrCount);
    int TOSDB_GetPreCachedTopicCount(String name, int[] ptrCount);
    int TOSDB_GetBlockSize(String name, int[] ptrSize);
    int TOSDB_SetBlockSize(String name, int size);
    int TOSDB_GetStreamOccupancy(String name, String item, String topic, int[] ptrOcc);

    int TOSDB_GetItemNames(String name, Pointer[] arrayStrings, int arraySz, int strSz);
    int TOSDB_GetTopicNames(String name, Pointer[] arrayStrings, int arraySz, int strSz);
    int TOSDB_GetPreCachedItemNames(String name, Pointer[] arrayStrings, int arraySz, int strSz);
    int TOSDB_GetPreCachedTopicNames(String name, Pointer[] arrayStrings, int arraySz, int strSz);

    int TOSDB_AddItem(String name, String item);
    int TOSDB_AddTopic(String name, String topic);
    int TOSDB_RemoveItem(String name, String item);
    int TOSDB_RemoveTopic(String name, String topic);

    int TOSDB_GetString(String name, String item, String topic, int indx, byte[] ptrVal, int strSz,
                        DateTime ptrDateTime);

    int TOSDB_GetDouble(String name, String item, String topic, int indx, double[] ptrVal,
                        DateTime ptrDateTime);

    int TOSDB_GetLongLong(String name, String item, String topic, int indx, long[] ptrVal,
                          DateTime ptrDateTime);

    int TOSDB_GetStreamSnapshotStrings(String name, String item, String topic, Pointer[] arrayVals,
                                       int arraySz, int strSz, DateTime[] arrayDateTime, int end,
                                       int beg);

    int TOSDB_GetStreamSnapshotDoubles(String name, String item, String topic, double[] arrayVals,
                                       int arraySz, DateTime[] arrayDateTime, int end, int beg);

    int TOSDB_GetStreamSnapshotLongLongs(String name, String item, String topic, long[] arrayVals,
                                         int arraySz, DateTime[] arrayDateTime, int end, int beg);

    int TOSDB_IsMarkerDirty(String name, String item, String topic, int[] ptrVal);
    int TOSDB_GetMarkerPosition(String name, String item, String topic, long[] ptrVal);

    int TOSDB_GetStreamSnapshotStringsFromMarker(String name, String item, String topic,
                                                 Pointer[] arrayVals, int arraySz, int strSz,
                                                 DateTime[] arrayDateTime, int beg,
                                                 NativeLong[] getSz);

    int TOSDB_GetStreamSnapshotDoublesFromMarker(String name, String item, String topic,
                                                 double[] arrayVals, int arraySz,
                                                 DateTime[] arrayDateTime, int beg,
                                                 NativeLong[] getSz);

    int TOSDB_GetStreamSnapshotLongLongsFromMarker(String name, String item, String topic,
                                                   long[] arrayVals, int arraySz,
                                                   DateTime[] arrayDateTime, int beg,
                                                   NativeLong[] getSz);

    int TOSDB_GetItemFrameStrings(String name, String topic, Pointer[] arrayVals, int arraySz,
                                  int strSz, Pointer[] arrayLabels, int strLabelSz,
                                  DateTime[] arrayDateTime);

    int TOSDB_GetTopicFrameStrings(String name, String item, Pointer[] arrayVals, int arraySz,
                                   int strSz, Pointer[] arrayLabels, int strLabelSz,
                                   DateTime[] arrayDateTime);
}
