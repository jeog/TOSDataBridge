package com.tosdatabridge;

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
    public static final int ERROR_CONCURRENCY = -13;
    public static final int ERROR_ENGINE = -14;
    public static final int ERROR_SERVICE = -15;
    public static final int ERROR_GET_DATA = -16;
    public static final int ERROR_GET_STATE = -17;
    public static final int ERROR_SET_STATE = -18;
    public static final int ERROR_UNKNOWN = -19;
    public static final int ERROR_DECREMENT_BASE = -20;

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
        LOOKUP.put(-13, "ERROR_CONCURRENCY");
        LOOKUP.put(-14, "ERROR_ENGINE");
        LOOKUP.put(-15, "ERROR_SERVICE");
        LOOKUP.put(-16, "ERROR_GET_DATA");
        LOOKUP.put(-17, "ERROR_GET_STATE");
        LOOKUP.put(-18, "ERROR_SET_STATE");
        LOOKUP.put(-19, "ERROR_UNKNOWN");
        LOOKUP.put(-20, "ERROR_DECREMENT_BASE");
    }

    public static String
    errorLookup(int errorCode)
    {
        String errorStr = LOOKUP.get(errorCode);

        if(errorStr != null)
            return errorStr;

        int baseError = Collections.min(LOOKUP.keySet());
        if(errorCode < baseError)
            return "ERROR_DECREMENT_BASE(" + String.valueOf(errorCode-baseError) + ")";
        else
            return "***unrecognized error code***";

    }
}
