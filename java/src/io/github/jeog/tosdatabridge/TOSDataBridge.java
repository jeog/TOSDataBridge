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

import com.sun.jna.Native;

import java.io.File;

public class TOSDataBridge{
    /* hardcode for time being */
    public static final int CONN_NONE = 0;
    public static final int CONN_ENGINE = 1;
    public static final int CONN_ENGINE_TOS = 2;

    public static final int INTGR_BIT = 0x80;
    public static final int QUAD_BIT = 0x40;
    public static final int STRING_BIT = 0x20;

    public static final int TOPIC_IS_LONG = 1;
    public static final int TOPIC_IS_DOUBLE = 2;
    public static final int TOPIC_IS_STRING = 3;

    public static final int MAX_STR_SZ = 0xff;
    public static final int STR_DATA_SZ = 40;

    public static final int DEF_TIMEOUT = 2000;
    public static final int MARKER_MARGIN_OF_SAFETY = 100;

    public static class LibraryNotLoaded extends Exception{
        public LibraryNotLoaded(){
            super("library not loaded");
        }
    }

    public static class CLibException extends Exception{
        public CLibException(String callStr, int errorCode){
            super("CLib call [" + callStr + "] failed; " + CError.errorLookup(errorCode));
        }
    }

    public static class DataBlockException extends Exception{
        public DataBlockException(String info){
            super(info);
        }
    }
    public static class DateTimeNotSupported extends DataBlockException{
        public DateTimeNotSupported() {
            super("DataBlock instance doesn't support DateTime");
        }
    }

    public static class DataIndexException extends DataBlockException{
        public DataIndexException(String info) {
            super(info);
        }
    }

    public static class DirtyMarkerException extends DataBlockException{
        public DirtyMarkerException() {
            super("data was lost behind the marker "
                    + "(set throwIfDataLost to false avoid this exception");
        }
    }

    private static CLib library = null;

    /* package-private */
    static final CLib
    getCLibrary() throws LibraryNotLoaded {
        if(library == null) {
            throw new LibraryNotLoaded();
        }
        return library;
    }

    public static boolean 
    init(String path) {
        if(library != null) {
            return true;
        }
        try{
            File f = new File(path);
            String n = f.getName();
            String d = f.getParentFile().getPath();
            int pos = n.lastIndexOf(".");
            if(pos > 0) {
                n = n.substring(0, pos);
            }
            System.out.println("directory: " + d + ", name: " + n);
            System.setProperty("jna.library.path", d);
            library = Native.loadLibrary(n, CLib.class);
            if( !connect() ){
                System.err.println("TOSDataBridge connect() failed");
                return false;
            }
        }catch(Throwable t){
            System.err.println("TOSDataBridge init() failed: " + t.toString());
            t.printStackTrace();
            return false;
        }
        return true;
    }

    public static boolean
    connect() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_Connect() == 0;
    }

    public static boolean
    connected() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_IsConnectedToEngineAndTOS() != 0;
    }

    public static int
    connectionState() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_ConnectionState();
    }

    public static int 
    getBlockLimit() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_GetBlockLimit();
    }

    public static int
    setBlockLimit(int limit) throws LibraryNotLoaded {
        return getCLibrary().TOSDB_SetBlockLimit(limit);
    }

    public static int
    getBlockCount() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_GetBlockCount();
    }

    public static int
    getTypeBits(Topic topic) throws LibraryNotLoaded, CLibException {
        byte[] bits = {0};
        int err = getCLibrary().TOSDB_GetTypeBits(topic.val, bits);
        if(err != 0) {
            throw new CLibException("TOSDB_GetTypeBits", err);
        }
        return bits[0] & 0xFF;
    }

    public static int
    getTopicType(Topic topic) throws CLibException, LibraryNotLoaded {
        switch(getTypeBits(topic)){
            case INTGR_BIT | QUAD_BIT:
            case INTGR_BIT:
                return TOPIC_IS_LONG;
            case QUAD_BIT:
            case 0:
                return TOPIC_IS_DOUBLE;
            default:
                return TOPIC_IS_STRING;
        }
    }


}