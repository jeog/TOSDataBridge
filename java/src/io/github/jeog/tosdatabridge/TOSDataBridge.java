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

/**
 * Core class containing constants, exceptions, a (package-private) getter for the
 * JNA C lib interface, and high-level admin calls for the API.
 * <p>
 * <strong>IMPORTANT:</strong> Before using the admin calls the underlying C library
 * must be loaded.
 * <ol>
 * <li> be sure you've installed the C mods (tosdb-setup.bat)
 * <li> call init(...)
 * <li> use admin calls
 * </ol>
 * @author Jonathon Ogden
 * @version 0.9
 * @see Topic
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md"> README </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_JAVA.md"> README - JAVA API </a>
 * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README_API.md"> README - C API </a>
 */
public final class TOSDataBridge{
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

    public static class TOSDataBridgeException extends Exception{
        TOSDataBridgeException(String info){
            super(info);
        }
    }

    public static class LibraryNotLoaded extends TOSDataBridgeException{
        LibraryNotLoaded(){
            super("library not loaded");
        }
    }

    public static class CLibException extends TOSDataBridgeException{
        CLibException(String callStr, int errorCode){
            super("CLib call [" + callStr + "] failed; " + CError.errorLookup(errorCode));
        }
    }

    public static class DataBlockException extends TOSDataBridgeException{
        DataBlockException(String info){
            super(info);
        }
    }

    public static class InvalidItemOrTopic extends DataBlockException{
        InvalidItemOrTopic(String info) {
            super(info);
        }
    }

    public static class DataIndexException extends DataBlockException{
        DataIndexException(String info) {
            super(info);
        }
    }

    public static class DirtyMarkerException extends DataBlockException{
        DirtyMarkerException() {
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

    /**
     * Initialize C lib via JNA.
     *
     * @param path path of the the tos-databridge DLL.
     * @return if library was loaded AND connected to engine
     * @throws LibraryNotLoaded if library was not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static boolean 
    init(String path) throws LibraryNotLoaded {
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
            System.err.println("TOSDataBridge.init(..) failed to load library: " + t.toString());
            t.printStackTrace();
            throw new LibraryNotLoaded();
        }
        return true;
    }

    /**
     * If the library is loaded try to connect to the Engine and TOS platform.
     * (TOSDataBridge.init(...) should do this automatically.)
     *
     * @return if library is connected to engine (not necessarily to TOS)
     * @throws LibraryNotLoaded if library is not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static boolean
    connect() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_Connect() == 0;
    }

    /**
     * @return  if library is connected to engine and TOS platform
     *
     * @throws LibraryNotLoaded if library is not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static boolean
    connected() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_IsConnectedToEngineAndTOS() != 0;
    }

    /**
     * @return state of the connection:
     * <ul>
     * <li> CONN_NONE: not connected to engine or TOS platform
     * <li> CONN_ENGINE: connected to engine but not TOS platform
     * <li> CONN_ENGINE_TOS: connected to engine and TOS platform
     * </ul>
     *
     * @throws LibraryNotLoaded if library is not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static int
    connectionState() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_ConnectionState();
    }

    /**
     * @return block limit imposed by the C library.
     *
     * @throws LibraryNotLoaded if library is not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static int 
    getBlockLimit() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_GetBlockLimit();
    }

    /**
     * Set block limit to be imposed by the C library.
     *
     * @param limit new block limit
     * @return new block limit
     * @throws LibraryNotLoaded if library is not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static int
    setBlockLimit(int limit) throws LibraryNotLoaded {
        return getCLibrary().TOSDB_SetBlockLimit(limit);
    }

    /**
     * @return current block count(according to C lib, not necessarily JRE).
     *
     * @throws LibraryNotLoaded if library is not loaded
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static int
    getBlockCount() throws LibraryNotLoaded {
        return getCLibrary().TOSDB_GetBlockCount();
    }

    /**
     * @return type bits (as int) of a particular topic
     *
     * @param topic topic enum to get type bits of
     * @throws LibraryNotLoaded if library is not loaded
     * @throws CLibException if the C lib call returns an error
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static int
    getTypeBits(Topic topic) throws LibraryNotLoaded, CLibException {
        byte[] bits = {0};
        int err = getCLibrary().TOSDB_GetTypeBits(topic.val, bits);
        if(err != 0) {
            throw new CLibException("TOSDB_GetTypeBits", err);
        }
        return bits[0] & 0xFF;
    }

    /**
     * @return type constant representing the type of a particular topic.
     * <ul>
     *     <li>TOPIC_IS_LONG</li>
     *     <li>TOPIC_IS_DOUBLE</li>
     *     <li>TOPIC_IS_STRING</li>
     * </ul>
     *
     * @param topic topic enum to get type constant of
     * @throws LibraryNotLoaded if library is not loaded
     * @throws CLibException if the C lib call returns an error
     * @see <a href="https://github.com/jeog/TOSDataBridge/blob/master/README.md">README</a>
     */
    public static int
    getTopicType(Topic topic) throws CLibException, LibraryNotLoaded {
        switch(getTypeBits(topic)){
            case INTGR_BIT | QUAD_BIT:
            case INTGR_BIT:
                return TOPIC_IS_LONG;
            case QUAD_BIT:
            case 0:
                return TOPIC_IS_DOUBLE;
            case STRING_BIT:
                return TOPIC_IS_STRING;
            default:
                throw new RuntimeException("invalid type bits");
        }
    }


}