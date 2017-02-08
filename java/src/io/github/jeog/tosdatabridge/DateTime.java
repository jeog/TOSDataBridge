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

    //TODO: implement add/subtract methods
}
