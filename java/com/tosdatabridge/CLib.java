package com.tosdatabridge;

import com.sun.jna.Library;
import com.sun.jna.NativeLong;
import com.sun.jna.Structure;

import java.util.Arrays;
import java.util.List;

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
    int TOSDB_GetItemNames(String name, String[] arrayStrings, int arraySz, int strSz);
    int TOSDB_GetTopicNames(String name, String[] arrayStrings, int arraySz, int strSz);
    int TOSDB_GetPreCachedItemNames(String name, String[] arrayStrings, int arraySz, int strSz);
    int TOSDB_GetPreCachedTopicNames(String name, String[] arrayStrings, int arraySz, int strSz);
    int TOSDB_AddItem(String name, String item);
    int TOSDB_AddTopic(String name, String topic);
    int TOSDB_RemoveItem(String name, String item);
    int TOSDB_RemoveTopic(String name, String topic);

    int TOSDB_GetString(String name, String item, String topic, int indx, String strVal, int strSz,
                        DateTimeStamp[] ptrDateTime);

    int TOSDB_GetDouble(String name, String item, String topic, int indx, double[] ptrVal,
                        DateTimeStamp[] ptrDateTime);

    int TOSDB_GetLongLong(String name, String item, String topic, int indx, long[] ptrVal,
                          DateTimeStamp[] ptrDateTime);

    int TOSDB_GetStreamSnapshotStrings(String name, String item, String topic,
                                       String[] arrayStrings, int arraySz, int strSz,
                                       DateTimeStamp[] arrayDateTime, int end, int beg);

    int TOSDB_GetStreamSnapshotDoubles(String name, String item, String topic, double[] arrayVals,
                                       int arraySz, DateTimeStamp[] arrayDateTime, int end, int beg);

    int TOSDB_GetStreamSnapshotLongLongs(String name, String item, String topic, long[] arrayVals,
                                         int arraySz, DateTimeStamp[] arrayDateTime, int end, int beg);

    int TOSDB_IsMarkerDirty(String name, String item, String topic, int[] ptrVal);
    int TOSDB_GetMarkerPosition(String name, String item, String topic, long[] ptrVal);

    int TOSDB_GetStreamSnapshotStringsFromMarker(String name, String item, String topic,
                                                 String[] arrayStrings, int arraySz, int strSz,
                                                 DateTimeStamp[] arrayDateTime, int beg,
                                                 NativeLong[] getSz );

    int TOSDB_GetStreamSnapshotDoublesFromMarker(String name, String item, String topic,
                                                 double[] arrayVals, int arraySz,
                                                 DateTimeStamp[] arrayDateTime, int beg,
                                                 NativeLong[] getSz );

    int TOSDB_GetStreamSnapshotLongLongsFromMarker(String name, String item, String topic,
                                                   long[] arrayVals, int arraySz,
                                                   DateTimeStamp[] arrayDateTime, int beg,
                                                   NativeLong[] getSz );

    int TOSDB_GetItemFrameStrings(String name, String topic, String[] arrayStrings, int arraySz,
                                  int strSz, String[] arrayLabels, int strLabelSz,
                                  DateTimeStamp[] arrayDateTime);

    int TOSDB_GetTopicFrameStrings(String name, String item, String[] arrayStrings, int arraySz,
                                   int strSz, String[] arrayLabels, int strLabelSz,
                                   DateTimeStamp[] arrayDateTime);

    class DateTimeStamp extends Structure {

        public static class _CTime extends Structure {
            public int tmSec;
            public int tmMin;
            public int tmHour;
            public int tmMday;
            public int tmMon;
            public int tmYear;
            public int tmWday;
            public int tmYday;
            public int tmIsDst;

            @Override
            protected List<String>
            getFieldOrder() {
                return Arrays.asList("tmSec", "tmMin", "tmHour", "tmMday", "tmMon",
                        "tmYear", "tmWday", "tmYday", "tmIsDst");
            }
        }

        public _CTime cTime;
        public NativeLong microSecond;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("cTime", "microSecond");
        }
    }

    //errors
    //conn states
    //cleanUp();
}
