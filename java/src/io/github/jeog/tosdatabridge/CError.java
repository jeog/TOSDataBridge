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

/**
 * Error codes that can be returned by the C Lib.
 * 
 * @author Jonathon Ogden
 * @version 0.9
 */
public final class CError {
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

    private static final Map<Integer,String> ERROR_STRING_LOOKUP = new HashMap<>();
    static{
        ERROR_STRING_LOOKUP.put(ERROR_BAD_INPUT, "ERROR_BAD_INPUT");
        ERROR_STRING_LOOKUP.put(ERROR_BAD_INPUT_BUFFER, "ERROR_BAD_INPUT_BUFFER");
        ERROR_STRING_LOOKUP.put(ERROR_NOT_CONNECTED, "ERROR_NOT_CONNECTED");
        ERROR_STRING_LOOKUP.put(ERROR_TIMEOUT, "ERROR_TIMEOUT");
        ERROR_STRING_LOOKUP.put(ERROR_BLOCK_ALREADY_EXISTS, "ERROR_BLOCK_ALREADY_EXISTS");
        ERROR_STRING_LOOKUP.put(ERROR_BLOCK_DOESNT_EXIST, "ERROR_BLOCK_DOESNT_EXIST");
        ERROR_STRING_LOOKUP.put(ERROR_BLOCK_CREATION, "ERROR_BLOCK_CREATION");
        ERROR_STRING_LOOKUP.put(ERROR_BLOCK_SIZE, "ERROR_BLOCK_SIZE");
        ERROR_STRING_LOOKUP.put(ERROR_BAD_TOPIC, "ERROR_BAD_TOPIC");
        ERROR_STRING_LOOKUP.put(ERROR_BAD_ITEM, "ERROR_BAD_ITEM");
        ERROR_STRING_LOOKUP.put(ERROR_BAD_SIG, "ERROR_BAD_SIG");
        ERROR_STRING_LOOKUP.put(ERROR_IPC, "ERROR_IPC");
        ERROR_STRING_LOOKUP.put(ERROR_IPC_MSG, "ERROR_IPC_MSG");
        ERROR_STRING_LOOKUP.put(ERROR_CONCURRENCY, "ERROR_CONCURRENCY");
        ERROR_STRING_LOOKUP.put(ERROR_ENGINE_NO_TOPIC, "ERROR_ENGINE_NO_TOPIC");
        ERROR_STRING_LOOKUP.put(ERROR_ENGINE_NO_ITEM, "ERROR_ENGINE_NO_ITEM");
        ERROR_STRING_LOOKUP.put(ERROR_SERVICE, "ERROR_SERVICE");
        ERROR_STRING_LOOKUP.put(ERROR_GET_DATA, "ERROR_GET_DATA");
        ERROR_STRING_LOOKUP.put(ERROR_GET_STATE, "ERROR_GET_STATE");
        ERROR_STRING_LOOKUP.put(ERROR_SET_STATE, "ERROR_SET_STATE");
        ERROR_STRING_LOOKUP.put(ERROR_DDE_POST, "ERROR_DDE_POST");
        ERROR_STRING_LOOKUP.put(ERROR_DDE_NO_ACK, "ERROR_DDE_NO_ACK");
        ERROR_STRING_LOOKUP.put(ERROR_SHEM_BUFFER, "ERROR_SHEM_BUFFER");
        ERROR_STRING_LOOKUP.put(ERROR_UNKNOWN, "ERROR_UNKNOWN");
        ERROR_STRING_LOOKUP.put(ERROR_DECREMENT_BASE, "ERROR_DECREMENT_BASE");
    }

    /**
     * Return error string from error code.
     *
     * @param errorCode error code
     * @return error string
     */
    public static String
    errorLookup(int errorCode){
        String errorStr = ERROR_STRING_LOOKUP.get(errorCode);
        if(errorStr != null) {
            return errorStr;
        }
        int baseError = Collections.min(ERROR_STRING_LOOKUP.keySet());
        if(errorCode < baseError) {
            return "ERROR_DECREMENT_BASE(" + String.valueOf(errorCode - baseError) + ")";
        }else {
            return "***unrecognized error code***";
        }
    }
}
