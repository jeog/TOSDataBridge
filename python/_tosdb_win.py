# Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#   See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License,
#   'LICENSE.txt', along with this program.  If not, see 
#   <http://www.gnu.org/licenses/>.

"""_tosdb_win.py :  the non-portable bits of the window's tosdb implementation.

These calls represent the closest layer above the underlying C/C++ library,
providing important admin funtions like init() and a means of directly
accessing the C/C++ calls ( _lib_call() )
"""

from _tosdb_errors import *
from io import StringIO as _StringIO
from functools import partial as _partial
from math import log as _log
from sys import maxsize as _maxsize
from time import asctime as _asctime, localtime as _localtime
from os import walk as _walk, stat as _stat, curdir as _curdir, \
     listdir as _listdir, sep as _sep
from re import compile as _compile, search as _search, match as _match, \
     split as _split
# types and type 'modifiers' use _[ name ]_ format
from ctypes import WinDLL as _WinDLL, pointer as _pointer, \
     create_string_buffer as _BUF_, c_double as _double_, c_float as _float_, \
     c_ulong as _ulong_, c_long as _long_, c_longlong as _longlong_, \
     c_char_p as _string_, c_ubyte as _uchar_, c_int as _int_   

DLL_BASE_NAME = "tos-databridge"
SYS_ARCH_TYPE = "x64" if ( _log( _maxsize * 2, 2) > 33 ) else "x86"

_REGEX_NON_ALNUM = _compile("[\W+]")
_REGEX_DLL_NAME = _compile('^('+DLL_BASE_NAME 
                          + '-)[\d]{1,2}.[\d]{1,2}-'
                          + SYS_ARCH_TYPE +'(.dll)$')
_dll = None
        
def init(dllpath = None, root = "C:\\"):
    """ Initialize the underlying tos-databridge DLL

    dllpath: string of the exact path of the DLL
    root: string of the directory to start walking/searching to find the DLL
    """
    global _dll
    rel = set()
    try:
        if dllpath is None:
            matcher = _partial( _match, _REGEX_DLL_NAME)  # regex match function
            for nfile in map( matcher, _listdir( _curdir )):
                if nfile: # try the current dir first             
                    rel.add( _curdir+ _sep + nfile.string )                    
            if not rel:                
                for root,dirs, files in _walk(root): # no luck, walk the dir tree 
                    for file in map( matcher, files):  
                        if file:                           
                            rel.add( root + _sep + file.string )                           
                if not rel: # if still nothing throw
                    raise TOSDB_Error(" could not locate DLL")    
            if len(rel) > 1:  # only use the most recent version(s)
                verVal = _compile('-[\d]{1,2}.[\d]{1,2}-')
                vers = tuple( zip( map( 
                    lambda x: _search(verVal,x).group().strip('-'),rel), rel ) )
                versMax = max(vers)[0].split('.')[0]
                minTup= tuple( (x[0].split('.')[1],x[1]) 
                               for x in vers if x[0].split('.')[0] == versMax )         
                minMax = max(minTup)[0]
                rel = set( x[1] for x in minTup if x[0] == minMax )                      
            # find the most recently updated
            d = dict( zip(map( lambda x : _stat(x).st_mtime, rel), rel ) )
            rec = max(d)
            dllpath = d[ rec ]
        _dll = _WinDLL( dllpath )
        print( "+ Using Module ", dllpath )
        print( "+ Last Update ", _asctime(_localtime(_stat(dllpath).st_mtime)))
        if connect():
            print("+ Succesfully Connected to Service \ Engine")
        else:
            print("- Failed to Connect to Service \ Engine")
    except Exception as e:
        raise TOSDB_Error(" unable to initialize library", e )

def connect():
    """ Attempts to connect to the library """
    return not bool( _lib_call( "TOSDB_Connect") )

def connected():
    """ Returns true if there is an active connection to the library """
    return bool( _lib_call("TOSDB_IsConnected", ret_type = _ulong_ ) )
                
def clean_up():
    """ Clean up shared resources. ! CALL BEFORE EXITING INTERPRETER ! """
    global _dll
    if _dll is not None:       
        if _lib_call( "TOSDB_CloseBlocks" ):
            print("- Error Closing Blocks")
        else:
            print("+ Closing Blocks")
        _lib_call( "TOSDB_Disconnect" )
        print("+ Disconnecting From Service \ Engine")        
        print( "+ Closing Module ", _dll._name )
        del _dll
        _dll = None

###
### all this stuff can be wrapped with 'virtual' calls
###

def get_block_limit():
    """ Returns the block limit of C/C++ RawDataBlock factory """
    return _lib_call( "TOSDB_GetBlockLimit", ret_type =_ulong_ )

def set_block_limit( new_limit ):
    """ Changes the block limit of C/C++ RawDataBlock factory """
    _lib_call( "TOSDB_SetBlockLimit", new_limit, ret_type = _ulong_ )

def get_block_count():
    """ Returns the count of current instantiated blocks """
    return _lib_call( "TOSDB_GetBlockCount", ret_type = _ulong_ )

def type_bits( topic ):
    """ Returns the type bits for a particular 'topic'

    topic: string representing a TOS data field('LAST','ASK', etc)
    returns -> value that can be logical &'d with type bit contstants 
    ( ex. QUAD_BIT )
    """
    tyBits = _uchar_()
    err = _lib_call( "TOSDB_GetTypeBits", topic.encode("ascii"), 
                     _pointer(tyBits) )
    if(err):
       raise TOSDB_Error( "error value [ " + str(err) +
                          " ] returned from library call", "TOSDB_GetTypeBits")
    return tyBits.value

def type_string( topic ):
    """ Returns a platform-dependent string of the type of a particular 'topic'

    topic: string representing a TOS data field('LAST','ASK', etc)
    """
    tyStr = _BUF_( MAX_STR_SZ + 1 )
    err = _lib_call( "TOSDB_GetTypeString", topic.encode("ascii"), tyStr, 
                     (MAX_STR_SZ + 1) )
    if(err):
        raise TOSDB_Error( "error value [ " + str(err) +
                          " ] returned from library call","TOSDB_GetTypeString")
    return tyStr.value.decode()

# how we access the underlying calls
def _lib_call( f, *args, ret_type = _int_, arg_list = None ):        
    if not _dll:
        raise TOSDB_Error( "tos-databridge DLL is currently not loaded ")
        return # in case exc gets ignored on clean_up
    try:        
        attr = getattr( _dll, str(f) )
        if ret_type:
            attr.restype = ret_type
        if isinstance( arg_list, list):
            attr.argtypes = arg_list
        return attr(*args)              
    except Exception as e:
        cStat = " Not Connected"
        if f != "TOSDB_IsConnected": # avoid infinite recursion
           if connected():
               cStat = " Connected"               
        raise TOSDB_Error( "Unable to execute library function ", f ,cStat, e)

def _type_switch( typeB ):
    if ( typeB == INTGR_BIT + QUAD_BIT ):
        return ( "LongLong", _longlong_ )
    elif ( typeB == INTGR_BIT ):
        return ( "Long", _long_ )
    elif ( typeB == QUAD_BIT ):
        return ( "Double", _double_ )
    elif ( typeB == 0 ):
        return ( "Float", _float_ )
    else: # default to string
        return( "String", _string_ )

def _str_clean( *strings ):    
    fin = []
    for s in strings:               
        tmpS = ''
        for sub in _split( _REGEX_NON_ALNUM, s):
            tmpS = tmpS + sub
        fin.append(tmpS)
    return fin
