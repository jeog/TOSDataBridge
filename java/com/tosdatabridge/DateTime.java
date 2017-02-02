package com.tosdatabridge;

import com.sun.jna.NativeLong;
import com.sun.jna.Structure;

import java.util.Arrays;
import java.util.List;

public class DateTime extends Structure {

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
        getFieldOrder()
        {
            return Arrays.asList("tmSec", "tmMin", "tmHour", "tmMday", "tmMon",
                    "tmYear", "tmWday", "tmYday", "tmIsDst");
        }
    }

    public _CTime cTime;
    public NativeLong microSecond;

    @Override
    protected List<String>
    getFieldOrder()
    {
        return Arrays.asList("cTime", "microSecond");
    }

    @Override
    public String
    toString()
    {
        return String.valueOf(cTime.tmMon + 1) + "/" + String.valueOf(cTime.tmMday) + "/"
                + String.valueOf(1900 + cTime.tmYear) + " " + String.valueOf(cTime.tmHour) + ":"
                + String.valueOf(cTime.tmMin) + ":" + String.valueOf(cTime.tmSec) + ":"
                + String.valueOf(microSecond);
    }
}
