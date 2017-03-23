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

import com.sun.jna.NativeLong;
import com.sun.jna.Structure;

import java.util.Arrays;
import java.util.List;

/**
 * A JNA Structure used to represent DateTime values.
 * It contains two fields:
 * <ul>
 * <li> cTime - a java version of C's tm struct, defined as a JNA Structure.
 * <li> microSeconds
 * </ul>
 * @author Jonathon Ogden
 * @version 0.8
 */
public class DateTime extends Structure {
    public _CTime cTime;
    public NativeLong microSeconds;
    {
        cTime = new _CTime();
        microSeconds = new NativeLong(0);
    }

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
        getFieldOrder(){
            return Arrays.asList("tmSec", "tmMin", "tmHour", "tmMday", "tmMon",
                    "tmYear", "tmWday", "tmYday", "tmIsDst");
        }
    }

    @Override
    protected List<String>
    getFieldOrder(){
        return Arrays.asList("cTime", "microSeconds");
    }

    @Override
    public String
    toString(){
        return String.valueOf(cTime.tmMon + 1) + "/" + String.valueOf(cTime.tmMday) + "/"
                + String.valueOf(1900 + cTime.tmYear) + " " + String.valueOf(cTime.tmHour) + ":"
                + String.valueOf(cTime.tmMin) + ":" + String.valueOf(cTime.tmSec) + ":"
                + String.valueOf(microSeconds);
    }

    /**
     * Pair of primary data and DateTime object.
     *
     * @param <T> type of primary data
     */
    public static class DateTimePair<T> extends Pair<T, DateTime> {
        public DateTimePair(T first, DateTime second) {
            super(first, second);
        }
    }

    /**
     * Triple(3-tuple) of primary data, DateTime object, and secondary data.
     *
     * @param <T3> type of secondary data
     */
    public static class DateTimeTriple<T,T3> extends DateTimePair<T> {
        public final T3 third;

        public DateTimeTriple(T first, DateTime second, T3 third) {
            super(first, second);
            this.third = third;
        }

        public DateTimeTriple(DateTimePair<T> dateTimePair, T3 third){
            super(dateTimePair.first, dateTimePair.second);
            this.third = third;
        }
    }
    //TODO: implement add/subtract methods
}
