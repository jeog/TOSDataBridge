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

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

public class CError {
    public static final int ERROR_BAD_INPUT = -1;
    public static final int ERROR_BAD_INPUT_BUFFER = -2;
    public static final int ERROR_NOT_CONNECTED = -3;
    public static final int ERROR_TIMEOUT = -4;
    public static final int ERROR_BLOCK_ALREADY_EXISTS = -5;
    public static final int ERROR_BLOCK_DOESNT_EXIST = -6;
    public static final int ERROR_BLOCK_CREATION = -7;
    public static final int ERROR_BLOCK_SIZE = -8;
    public static final int ERROR_BAD_TOPIC = -9;
    public static final int ERROR_BAD_ITEM = -10;
    public static final int ERROR_BAD_SIG = -11;
    public static final int ERROR_IPC = -12;
    public static final int ERROR_IPC_MSG = -13;
    public static final int ERROR_CONCURRENCY = -14;
    public static final int ERROR_ENGINE_NO_TOPIC = -15;
    public static final int ERROR_ENGINE_NO_ITEM = -16;
    public static final int ERROR_SERVICE = -17;
    public static final int ERROR_GET_DATA = -18;
    public static final int ERROR_GET_STATE = -19;
    public static final int ERROR_SET_STATE = -20;
    public static final int ERROR_DDE_POST = -21;
    public static final int ERROR_DDE_NO_ACK = -22;
    public static final int ERROR_SHEM_BUFFER = -23;
    public static final int ERROR_UNKNOWN = -24;
    public static final int ERROR_DECREMENT_BASE = -25;

    public static final Map<Integer,String> LOOKUP = new HashMap<>();
    static{
        LOOKUP.put(-1, "ERROR_BAD_INPUT");
        LOOKUP.put(-2, "ERROR_BAD_INPUT_BUFFER");
        LOOKUP.put(-3, "ERROR_NOT_CONNECTED");
        LOOKUP.put(-4, "ERROR_TIMEOUT");
        LOOKUP.put(-5, "ERROR_BLOCK_ALREADY_EXISTS");
        LOOKUP.put(-6, "ERROR_BLOCK_DOESNT_EXIST");
        LOOKUP.put(-7, "ERROR_BLOCK_CREATION");
        LOOKUP.put(-8, "ERROR_BLOCK_SIZE");
        LOOKUP.put(-9, "ERROR_BAD_TOPIC");
        LOOKUP.put(-10, "ERROR_BAD_ITEM");
        LOOKUP.put(-11, "ERROR_BAD_SIG");
        LOOKUP.put(-12, "ERROR_IPC");
        LOOKUP.put(-13, "ERROR_IPC_MSG");
        LOOKUP.put(-14, "ERROR_CONCURRENCY");
        LOOKUP.put(-15, "ERROR_ENGINE_NO_TOPIC");
        LOOKUP.put(-16, "ERROR_ENGINE_NO_ITEM");
        LOOKUP.put(-17, "ERROR_SERVICE");
        LOOKUP.put(-18, "ERROR_GET_DATA");
        LOOKUP.put(-19, "ERROR_GET_STATE");
        LOOKUP.put(-20, "ERROR_SET_STATE");
        LOOKUP.put(-21, "ERROR_DDE_POST);
        LOOKUP.put(-22, "ERROR_DDE_NO_ACK");
        LOOKUP.put(-23, "ERROR_SHEM_BUFFER");
        LOOKUP.put(-24, "ERROR_UNKNOWN");
        LOOKUP.put(-25, "ERROR_DECREMENT_BASE");
    }

    public static String
    errorLookup(int errorCode){
        String errorStr = LOOKUP.get(errorCode);
        if(errorStr != null) {
            return errorStr;
        }
        int baseError = Collections.min(LOOKUP.keySet());
        if(errorCode < baseError) {
            return "ERROR_DECREMENT_BASE(" + String.valueOf(errorCode - baseError) + ")";
        }else {
            return "***unrecognized error code***";
        }
    }
}
