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

"""_tosdb_win.py:  The non-portable bits of the window's tosdb implementation

Please refer to the tosdb.py docstring for detailed information.
"""

from ._common import * # a 'library' consisting of c/cpp _tosdb consts,
# errors/exceptions, datetime objects, and helper/utility functions
from ._common import _DateTimeStamp, _TOSDB_DataBlock, _type_switch

from io import StringIO as _StringIO
from uuid import uuid4 as _uuid4
from functools import partial as _partial
from math import log as _log, ceil as _ceil
from sys import maxsize as _maxsize
from atexit import register as _on_exit
from collections import namedtuple as _namedtuple
from time import asctime as _asctime, localtime as _localtime
from os import walk as _walk, stat as _stat, curdir as _curdir, \
     listdir as _listdir, sep as _sep
from re import compile as _compile, search as _search, match as _match, \
     split as _split
from ctypes import WinDLL as _WinDLL, cast as _cast, pointer as _pointer, \
     create_string_buffer as _BUF_, POINTER as _PTR_, c_double as _double_, \
     c_float as _float_, c_ulong as _ulong_, c_long as _long_, \
     c_longlong as _longlong_, c_char_p as _str_, c_char as _char_, \
     c_ubyte as _uchar_, c_int as _int_, c_void_p as _pvoid_, c_uint as _uint_
_pchar_ = _PTR_( _char_ )
_ppchar_ = _PTR_( _pchar_ )   

DLL_BASE_NAME = "tos-databridge"
SYS_ARCH_TYPE = "x64" if ( _log( _maxsize * 2, 2) > 33 ) else "x86"
MIN_MARGIN_OF_SAFETY = 10

_REGEX_NON_ALNUM = _compile("[\W+]")
_REGEX_DLL_NAME = _compile('^('+DLL_BASE_NAME 
                          + '-)[\d]{1,2}.[\d]{1,2}-'
                          + SYS_ARCH_TYPE +'(.dll)$')
           
_dll = None
        
def init(dllpath = None, root = "C:\\", bypass_check=False):
    """ Initialize the underlying tos-databridge DLL

    dllpath: string of the exact path of the DLL
    root: string of the directory to start walking/searching to find the DLL
    """  
    global _dll
    rel = set()
    if not bypass_check and dllpath is None and root == "C:\\":
        if abort_init_after_warn():
            return
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
                ver = _compile('-[\d]{1,2}.[\d]{1,2}-')
                vers = tuple( zip( map( 
                    lambda x: _search(ver,x).group().strip('-'), rel), rel) )
                vers_max = max(vers)[0].split('.')[0]
                mtup = tuple( (x[0].split('.')[1],x[1]) 
                              for x in vers if x[0].split('.')[0] == vers_max)         
                mtup_max = max(mtup)[0]
                rel = set( x[1] for x in mtup if x[0] == mtup_max )                      
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
        return True # indicate the lib was loaded
    except Exception as e:
        raise TOSDB_CLibError( "unable to initialize library", e )        

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

_on_exit( clean_up )
    
###
### all this stuff can be wrapped with 'virtual' calls
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
    err = _lib_call( "TOSDB_GetTypeBits", topic.upper().encode("ascii"), 
                     _pointer(tyBits) )
    if err:
       raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                              "from library call", "TOSDB_GetTypeBits" )
    return tyBits.value

def type_string( topic ):
    """ Returns a platform-dependent string of the type of a particular 'topic'

    topic: string representing a TOS data field('LAST','ASK', etc)
    """
    tyStr = _BUF_( MAX_STR_SZ + 1 )
    err = _lib_call( "TOSDB_GetTypeString", topic.upper().encode("ascii"),
                     tyStr, (MAX_STR_SZ + 1) )
    if err:
        raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                               "from library call","TOSDB_GetTypeString" )
    return tyStr.value.decode()

###
###
###

class TOSDB_DataBlock:
    """ The main object for storing TOS data.    

    size: how much historical data to save
    date_time: should block include date-time stamp with each data-point?
    timeout: how long to wait for responses from TOS-DDE server 

    Please review the attached README.html for details.
    """    
    def __init__( self, size = 1000, date_time = False, timeout = DEF_TIMEOUT ):        
        self._name = (_uuid4().hex).encode("ascii")
        self._valid = False
        err = _lib_call("TOSDB_CreateBlock", self._name, size, date_time, timeout)        
        if( err ):
            raise TOSDB_CLibError( "error value [ " + str(err) + " ] returned "
                                   "from library call", "TOSDB_CreateBlock" )         
        self._valid= True
        self._block_size = size
        self._timeout = timeout
        self._date_time = date_time
        self._items = []   
        self._topics = []        
  
    def __del__( self ): # for convenience, no guarantee
        if _lib_call is not None and self._valid:
            err = _lib_call( "TOSDB_CloseBlock", self._name )      
            if( err ):
                print(" - Error [ ",err," ] Closing Block (leak possible) " )

    def __str__( self ):      
        sio = _StringIO() # ouput buffer
        tf = self.total_frame() # get the frame
        count = 0
        maxDict = { "col0":0}       
        for k in tf:  # first, find the min column sizes not to truncate
            val = tf[k]
            if count == 0:
                maxDict.update( { (k,len(k)) for k in val._fields } )      
            if len(k) > maxDict["col0"]:
                maxDict["col0"] = len(k)
            for f in val._fields:
                l = len( str( getattr(val,f) ) )           
                if l > maxDict[f]:
                    maxDict[f] = l
            count += 1  
        count = 0       
        for k in tf:  # next, 'print' the formatted frame data
            val = tf[k]
            if count == 0:
                print( " " * maxDict["col0"], end=' ' , file=sio )
                for f in val._fields:               
                    print( f.ljust( maxDict[f] ),end=' ', file=sio ) 
                print('',file=sio)
            print( k.ljust( maxDict["col0"] ),end=' ', file=sio )
            for f in val._fields:
                print( str( getattr(val,f) ).ljust( maxDict[f]),
                       end=' ', file=sio ) 
            print('',file=sio)
            count += 1              
        sio.seek(0)
        return sio.read()
    
    # private _valid calls assume .upper() already called
    def _valid_item( self, item ):
        if not self._items: # in case items came out of pre-cache
            self._items = self.items()
        if item not in self._items:
            raise TOSDB_ValueError( item + " not found" )

    def _valid_topic( self, topic ):
        if not self._topics: # in case topics came out of pre-cache
            self._topics = self.topics()
        if topic not in self._topics:
             raise TOSDB_ValueError( topic + " not found" )

    def _item_count( self ):       
        i_count = _ulong_()
        err = _lib_call( "TOSDB_GetItemCount", self._name, _pointer(i_count) )
        if err:
            raise TOSDB_CLibError( "error value [ " + str(err) + " ] returned" +
                                   " from library call", "TOSDB_GetItemCount" )
        return i_count.value

    def _topic_count( self ):        
        t_count = _ulong_()
        err =  _lib_call("TOSDB_GetTopicCount", self._name, _pointer(t_count) )
        if err:
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_GetTopicCount" )
        return t_count.value

    def info(self):      
        """ Returns a more readable dict of info about the underlying block """       
        return { "Name": self._name.decode('ascii'), 
                 "Items": self.items(),
                 "Topics": self.topics(), 
                 "Size": self._block_size,
                 "DateTime": "Enabled" if self._date_time else "Disabled",
                 "Timeout": self._timeout }
    
    def get_block_size( self ):
        """ Returns the amount of historical data stored in the block """       
        b_size = _ulong_()
        err = _lib_call( "TOSDB_GetBlockSize", self._name, _pointer(b_size))
        if err:
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_GetBlockSize" )
        return b_size.value
    
    def set_block_size( self, sz ):
        """ Changes the amount of historical data stored in the block """
        err = _lib_call( "TOSDB_SetBlockSize", self._name, sz)
        if err:
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_SetBlockSize" )
        else:
            self._block_size = sz
            
    def stream_occupancy( self, item, topic ):       
        item = item.upper()
        topic = topic.upper()
        self._valid_item(item)
        self._valid_topic(topic)
        occ = _ulong_()
        err = _lib_call( "TOSDB_GetStreamOccupancy",
                         self._name,
                         item.encode("ascii"),
                         topic.encode("ascii"),
                         _pointer(occ),
                         arg_list = [ _str_, _str_, _str_, _PTR_(_ulong_) ] )
        if err:
             raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                    "from library call",
                                    "TOSDB_GetStreamOccupancy" )
        return occ.value
    
    def items( self, str_max = MAX_STR_SZ ):
        """ Returns the items currently in the block (and not pre-cached).
        
        str_max: the maximum length of item strings returned
        returns -> list of strings 
        """        
        size = self._item_count()  
        # store char buffers    
        strs = [ _BUF_(str_max + 1) for _ in range(size)]  
        # cast char buffers into (char*)[ ]     
        strs_array = ( _pchar_* size)( *[ _cast(s, _pchar_) for s in strs] ) 
        err = _lib_call( "TOSDB_GetItemNames", 
                         self._name,
                         strs_array,
                         size,
                         str_max + 1,                                      
                         arg_list = [ _str_,  _ppchar_, _ulong_, _ulong_ ] )
        if err:            
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_GetItemNames" )
        else:
            return [ _cast(ptr, _str_).value.decode() for ptr in strs_array ]            
              
    def topics( self,  str_max = MAX_STR_SZ ):
        """ Returns the topics currently in the block (and not pre-cached).
        
        str_max: the maximum length of topic strings returned  
        returns -> list of strings 
        """   
        size = self._topic_count()
        # store char buffers
        strs = [ _BUF_(str_max + 1) for _ in range(size)]  
        # cast char buffers into (char*)[ ]
        strs_array = ( _pchar_* size)( *[ _cast(s, _pchar_) for s in strs] )         
        err =_lib_call( "TOSDB_GetTopicNames", 
                        self._name,
                        strs_array,
                        size,
                        str_max + 1,                    
                        arg_list = [ _str_,  _ppchar_, _ulong_, _ulong_ ] )           
        if err:            
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_GetTopicNames" )            
        else:
            return [ _cast(ptr, _str_).value.decode() for ptr in strs_array ] 
      
    
    def add_items( self, *items ):
        """ Add items ( ex. 'IBM', 'SPY' ) to the block.

        NOTE: if there are no topics currently in the block, these items will 
        be pre-cached and appear not to exist, until a valid topic is added.

        *items: any numer of item strings
        """               
        mTup = tuple( s.encode("ascii").upper() for s in items )
        itemsTy = _str_ * len( mTup )
        cItems = itemsTy( *mTup )
        err = _lib_call( "TOSDB_AddItems", self._name, cItems, len( mTup ) )
        if err:
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_AddItems" )           
        if not self._items and not self._topics:
            self._topics = self.topics() # in case topics came out of pre-cache
        self._items = self.items()
       

    def add_topics( self, *topics ):
        """ Add topics ( ex. 'LAST', 'ASK' ) to the block.

        NOTE: if there are no items currently in the block, these topics will 
        be pre-cached and appear not to exist, until a valid item is added.

        *topics: any numer of topic strings
        """               
        mTup = tuple( s.encode("ascii").upper() for s in topics )
        topicsTy = _str_ * len( mTup )
        cTopics = topicsTy( *mTup )
        err = _lib_call( "TOSDB_AddTopics", self._name, cTopics, len( mTup ) )
        if err:
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] returned" +
                                   "from library call", "TOSDB_AddTopics" )
        if not self._items and not self._topics:
            self._items = self.items() # in case items came out of pre-cache
        self._topics = self.topics()

    def remove_items( self, *items ):
        """ Remove items ( ex. 'IBM', 'SPY' ) from the block.

        NOTE: if there this call removes all items from the block the 
        remaining topics will be pre-cached and appear not to exist, until 
        a valid item is re-added.

        *items: any numer of item strings
        """
        for item in items:
            err = _lib_call( "TOSDB_RemoveItem", self._name, 
                             item.encode("ascii").upper() )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call",
                                       "TOSDB_RemoveItem" )
        self._items = self.items()

    def remove_topics( self, *topics ):
        """ Remove topics ( ex. 'LAST', 'ASK' ) from the block.

        NOTE: if there this call removes all topics from the block the 
        remaining items will be pre-cached and appear not to exist, until 
        a valid topic is re-added.

        *topics: any numer of topic strings
        """
        for topic in topics:
            err = _lib_call( "TOSDB_RemoveTopic", self._name, 
                             topic.encode("ascii").upper() )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call", 
                                       "TOSDB_RemoveTopic" )  
        self._topics = self.topics()
        
    def get( self, item, topic, date_time = False, indx = 0, 
             check_indx = True, data_str_max = STR_DATA_SZ ):
        """ Return a single data-point from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOSDB_DateTime object   
        indx: index of data-points [0 to block_size), [-block_size to -1]
        check_indx: throw if datum doesn't exist at that particular index
        data_str_max: the maximum size of string data returned
        """       
        item = item.upper()
        topic = topic.upper()
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        self._valid_item(item)
        self._valid_topic(topic)
        if indx < 0:
            indx += self._block_size
        if indx >= self._block_size:
            raise TOSDB_IndexError( "invalid index value passed to get()" )   
        if check_indx and indx >= self.stream_occupancy( item, topic ):
            raise TOSDB_DataError( "data not available at this index yet " +
                                   "(disable check_indx to avoid this error)" )
        dts = _DateTimeStamp()      
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )        
        if typeTup[0] == "String":
            ret_str = _BUF_( data_str_max + 1 )
            err =_lib_call( "TOSDB_GetString", 
                            self._name,
                            item.encode("ascii"),
                            topic.encode("ascii"),
                            indx,
                            ret_str,
                            data_str_max + 1,
                            ( _pointer(dts) if date_time \
                                            else _PTR_(_DateTimeStamp)() ),
                            arg_list = [ _str_,  _str_, _str_, _long_, _pchar_,
                                         _ulong_, _PTR_(_DateTimeStamp) ] )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call", 
                                       "TOSDB_GetString" ) 
            if date_time :
                return (ret_str.value.decode(), TOSDB_DateTime( dts ))
            else:
                return ret_str.value.decode()
        else:
            val = typeTup[1]()
            err = _lib_call( "TOSDB_Get"+typeTup[0], 
                             self._name,
                             item.encode("ascii"),
                             topic.encode("ascii"),
                             indx,
                             _pointer(val),
                             ( _pointer(dts) if date_time \
                                             else _PTR_(_DateTimeStamp)() ),
                             arg_list = [ _str_,  _str_, _str_, _long_,
                                          _PTR_(typeTup[1]),
                                          _PTR_(_DateTimeStamp) ] )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call",
                                       "TOSDB_Get"+typeTup[0] )  
            if date_time:
                return (val.value, TOSDB_DateTime( dts ))
            else:
                return val.value

    def stream_snapshot( self, item, topic, date_time = False, 
                         end = -1, beg = 0, smart_size = True, 
                         data_str_max = STR_DATA_SZ ):
        """ Return multiple data-points(a snapshot) from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOSDB_DateTime object              
        end: index of least recent data-point ( end of the snapshot )
        beg: index of most recent data-point ( beginning of the snapshot )        
        smart_size: limits amount of returned data by data-stream's occupancy
        data_str_max: the maximum length of string data returned

        if date_time is True: returns-> list of 2tuple
        else: returns -> list              
        """     
        item = item.upper()
        topic = topic.upper()
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        self._valid_item(item)
        self._valid_topic(topic)
        if end < 0:
            end += self._block_size
        if beg < 0:
            beg += self._block_size
        if smart_size:            
            end = min(end, self.stream_occupancy( item, topic ) - 1 )                 
        size = (end - beg) + 1
        if beg < 0 or end < 0 \
           or beg >= self._block_size or end >= self._block_size \
           or size <= 0:
            raise TOSDB_IndexError("invalid 'beg' and/or 'end' index value(s)")           
        dtss = (_DateTimeStamp * size)()
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )   
        if typeTup[0] == "String":
            # store char buffers
            strs = [ _BUF_(  data_str_max +1 ) for _ in range(size)]   
            # cast char buffers into (char*)[ ]          
            strs_array = ( _pchar_ * size)( *[ _cast(s, _pchar_) for s in strs] ) 
            err = _lib_call( "TOSDB_GetStreamSnapshotStrings", 
                             self._name,
                             item.encode("ascii"),
                             topic.encode("ascii"),
                             strs_array,
                             size,
                             data_str_max + 1,                        
                             ( dtss if date_time else _PTR_(_DateTimeStamp)() ),
                             end,
                             beg,
                             arg_list = [ _str_, _str_, _str_, _ppchar_, _ulong_,
                                          _ulong_, _PTR_(_DateTimeStamp), _long_, 
                                          _long_ ] )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call",                
                                       "TOSDB_GetStreamSnapshotStrings" ) 
            if date_time:
                adj_dts = [ TOSDB_DateTime(x) for x in dtss ]
                return [ _ for _ in \
                         zip( map(lambda x: _cast(x,_str_).value.decode(),
                              strs_array ), adj_dts ) ]        
            else:
                return [ _cast(ptr,_str_).value.decode() for ptr in strs_array ]
        else:
            num_array = (typeTup[1] * size)()   
            err = _lib_call( "TOSDB_GetStreamSnapshot"+typeTup[0]+"s", 
                             self._name,
                             item.encode("ascii"),
                             topic.encode("ascii"),
                             num_array,
                             size,
                             ( dtss if date_time else _PTR_(_DateTimeStamp)() ),
                             end,
                             beg,
                             arg_list = [ _str_, _str_, _str_, _PTR_(typeTup[1]),
                                          _ulong_, _PTR_(_DateTimeStamp), _long_, 
                                          _long_ ] )
            if err:
                raise TOSDB_CLibError("error value [ "+ str(err) + " ] " +
                                      "returned from library call",
                                      "TOSDB_GetStreamSnapshot"+typeTup[0]+"s") 
            if date_time:
                adj_dts = [ TOSDB_DateTime(x) for x in dtss ]
                return [ _ for _ in zip(num_array,adj_dts) ]       
            else:
                return [ _ for _ in num_array ]

    def stream_snapshot_from_marker( self, item, topic, date_time = False, 
                                     beg = 0, margin_of_safety = 100,
                                     throw_if_data_lost = True,
                                     data_str_max = STR_DATA_SZ ):
        """ Return multiple data-points(a snapshot) from the data-stream,
        ending where the last call began

        It's likely the stream will grow between consecutive calls. This call
        guarantees to pick up where the last get(), stream_snapshot(), or
        stream_snapshot_from_marker() call ended (under a few assumptions, see
        below). 

        Internally the stream maintains a 'marker' that tracks the position of
        the last value pulled; the act of retreiving data and moving the
        marker can be thought of as a single, 'atomic' operation.

        There are three states to be aware of:
          1) a 'beg' value that is greater than the marker (even if beg = 0)
          2) a marker that moves through the entire stream and hits the bound
          3) passing a buffer that is too small for the whole range

        State (1) can be caused by passing in a beginning index that is past
        the current marker, or by passing in 0 when the marker has yet to
        move. 'None' will be returned.

        State (2) occurs when the marker doesn't get reset before it hits the
        bound (block_size); as the oldest data is popped of the back of the
        stream it is lost (the marker can't grow past the end of the stream). 

        State (3) occurs when an inadequately small buffer is used. The call
        handles buffer sizing for you by calling down to get the marker index,
        adjusting by 'beg' and 'margin_of_safety'. The latter helps assure the
        marker doesn't outgrow the buffer by the time the low-level retrieval
        operation completes. The default value indicates that over 100 push
        operations would have to take place during this call(highly unlikely).

        In either case (state (2) or (3)) if throw_if_data_lost is True a
        TOSDB_DataError will be thrown, otherwise the available data will
        be returned as normal. 
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOSDB_DateTime object                      
        beg: index of most recent data-point ( beginning of the snapshot )        
        margin_of_safety: (True/False) error margin for async stream growth
        throw_if_data_loss: (True/False) how to handle error states (see above)
        data_str_max: the maximum length of string data returned

        if beg > internal marker value: returns -> None        
        if date_time is True: returns-> list of 2tuple
        else: returns -> list              
        """
        
        item = item.upper()
        topic = topic.upper()
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        self._valid_item(item)
        self._valid_topic(topic)      
        if beg < 0:
            beg += self._block_size   
        if beg < 0 or beg >= self._block_size:
            raise TOSDB_IndexError("invalid 'beg' index value")

        if margin_of_safety < MIN_MARGIN_OF_SAFETY:
            raise TOSDB_ValueError("margin_of_safety < MIN_MARGIN_OF_SAFETY")

        is_dirty = _uint_()
        err = _lib_call( "TOSDB_IsMarkerDirty",
                         self._name,
                         item.encode("ascii"),
                         topic.encode("ascii"),
                         _pointer(is_dirty),
                         arg_list = [ _str_, _str_, _str_, _PTR_(_uint_) ] )
        if err:
           raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                  "returned from library call",
                                  "TOSDB_IsMarkerDirty" )

        if is_dirty and throw_if_data_lost:
            raise TOSDB_DataError("marker is already dirty")
        
        mpos = _longlong_()
        err2 = _lib_call( "TOSDB_GetMarkerPosition",
                         self._name,
                         item.encode("ascii"),
                         topic.encode("ascii"),
                         _pointer(mpos),
                         arg_list = [ _str_, _str_, _str_, _PTR_(_longlong_) ])
        if err2:
           raise TOSDB_CLibError( "error value [ "+ str(err2) + " ] " +
                                  "returned from library call",
                                  "TOSDB_GetMarkerPosition" )
        cur_sz = mpos.value - beg + 1
        if cur_sz < 0:
            return None
        safe_sz = cur_sz + margin_of_safety
        dtss = (_DateTimeStamp * safe_sz)()
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )
        get_size = _long_()
        if typeTup[0] == "String":
            # store char buffers
            strs = [ _BUF_(  data_str_max +1 ) for _ in range(safe_sz) ]   
            # cast char buffers into (char*)[ ]          
            strs_array = (_pchar_ * safe_sz)(*[ cast(s,_pchar_) for s in strs]) 
            err3 = _lib_call( "TOSDB_GetStreamSnapshotStringsFromMarker", 
                              self._name,
                              item.encode("ascii"),
                              topic.encode("ascii"),
                              strs_array,
                              safe_sz,
                              data_str_max + 1,                        
                              dtss if date_time else _PTR_(_DateTimeStamp)(),                            
                              beg,
                              _pointer( get_size ),
                              arg_list = [ _str_, _str_, _str_, _ppchar_,
                                           _ulong_, _ulong_,
                                           _PTR_(_DateTimeStamp), _long_,
                                           _PTR_(_long_) ] )
            if err3:
                raise TOSDB_CLibError( "error value [ " + str(err3) + 
                                       " ] returned from library call",
                                       "TOSDB_GetStreamSnapshotStrings" \
                                           + "FromMarker")

            print("DEBUG, get_size: ", str(get_size) )
            get_size = get_size.value
            if get_size == 0:
                return None
            elif get_size < 0:
                if throw_if_data_lost:
                    raise TOSDB_DataError("data lost behind the 'marker'")
                else:
                    get_size *= -1               
            
            if date_time:
                adj_dts = [ TOSDB_DateTime( x ) for x in dtss[:get_size] ]
                return [ _ for _ in \
                         zip( map( lambda x : cast(x, _str_).value.decode(), 
                                   strs_array[:get_size] ), adj_dts ) ]        
            else:
                return [ cast(ptr,_str_).value.decode()
                         for ptr in strs_array[:get_size] ]
        else:
            num_array = (typeTup[1] * safe_sz)()   
            err3 = _lib_call( "TOSDB_GetStreamSnapshot" \
                                 + typeTup[0] + "sFromMarker" , 
                              self._name,
                              item.encode("ascii"),
                              topic.encode("ascii"),
                              num_array,
                              safe_sz,
                              dtss if date_time else _PTR_(_DateTimeStamp)(),                             
                              beg,
                              _pointer( get_size ),
                              arg_list = [ _str_, _str_, _str_,
                                           _PTR_(typeTup[1]), _ulong_, 
                                           _PTR_(_DateTimeStamp), _long_,
                                           _PTR_(_long_) ] )
            if err3:
                raise TOSDB_CLibError( "error value of [ " + str(err3) + 
                                       " ] returned from library call",
                                       "TOSDB_GetStreamSnapshot" \
                                           + typeTup[0] + "sFromMarker" )

            print("DEBUG, get_size: ", str(get_size) )
            get_size = get_size.value
            if get_size == 0:
                return None
            elif get_size < 0:
                if throw_if_data_lost:
                    raise TOSDB_DataError("data lost behind the 'marker'")
                else:
                    get_size *= -1
            
            if date_time:
                adj_dts = [ TOSDB_DateTime( x ) for x in dtss[:get_size] ]
                return [ _ for _ in zip( num_array[:get_size], adj_dts ) ]       
            else:
                return [ _ for _ in num_array[:get_size] ]    
    

    def item_frame( self, topic, date_time = False, labels = True, 
                    data_str_max = STR_DATA_SZ, label_str_max = MAX_STR_SZ ):
        """ Return all the most recent item values for a particular topic.

        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOSDB_DateTime object       
        labels: (True/False) pull the item labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of item label strings returned

        if labels and date_time are True: returns-> namedtuple of 2tuple
        if labels is True: returns -> namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """      
        topic = topic.upper()
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        self._valid_topic(topic)
        size = self._item_count()
        dtss = (_DateTimeStamp * size)()
        # store char buffers
        labs = [ _BUF_(label_str_max+1) for _ in range(size)] 
        # cast char buffers int (char*)[ ]
        labs_array = ( _pchar_ * size)( *[ _cast(s, _pchar_) for s in labs] )  
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )       
        if typeTup[0] is "String":                     
            # store char buffers 
            strs = [ _BUF_(  data_str_max + 1 ) for _ in range(size)]               
            strs_array = ( _pchar_ * size)( *[ _cast(s, _pchar_) for s in strs] ) 
            # cast char buffers int (char*)[ ]                
            err = _lib_call( "TOSDB_GetItemFrameStrings", 
                             self._name,
                             topic.encode("ascii"),
                             strs_array,
                             size,
                             data_str_max + 1,                       
                             ( labs_array if labels else _ppchar_() ),
                             label_str_max + 1,
                             ( dtss if date_time else _PTR_(_DateTimeStamp)() ),
                             arg_list = [ _str_, _str_, _ppchar_, _ulong_,
                                          _ulong_, _ppchar_, _ulong_, 
                                          _PTR_(_DateTimeStamp) ] )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call",
                                       "TOSDB_GetItemFrameStrings" )
            s_map = map( lambda x: _cast(x, _str_).value.decode(), strs_array )           
            if labels:
                l_map = map(lambda x: _cast(x, _str_).value.decode(),labs_array)
                _nt_ = _gen_namedtuple(_str_clean(topic)[0], _str_clean(*l_map))             
                if date_time:
                    adj_dts = [ TOSDB_DateTime(x) for x in dtss ]
                    return _nt_( *zip(s_map,adj_dts) )                        
                else:
                    return _nt_( *s_map )             
            else:
                if date_time:
                    adj_dts = [ TOSDB_DateTime(x) for x in dtss ]
                    return list( zip(s_map,adj_dts) )
                else:
                    return list( s_map )                  
        else: 
            num_array =  (typeTup[1] * size)()   
            err = _lib_call( "TOSDB_GetItemFrame"+typeTup[0]+"s", 
                             self._name,
                             topic.encode("ascii"),
                             num_array,
                             size,
                             ( labs_array if labels else _ppchar_() ),
                             label_str_max + 1,
                             ( dtss if date_time else _PTR_(_DateTimeStamp)() ),                           
                             arg_list = [ _str_, _str_, _PTR_(typeTup[1]),
                                          _ulong_, _ppchar_, _ulong_, 
                                          _PTR_(_DateTimeStamp) ] )
            if err:
                raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                       "returned from library call",
                                       "TOSDB_GetItemFrame" + typeTup[0] + "s" ) 
            if labels:                                
                l_map = map(lambda x: _cast(x,_str_).value.decode(), labs_array)
                _nt_ = _gen_namedtuple(_str_clean(topic)[0], _str_clean(*l_map))              
                if date_time:
                    adj_dts = [ TOSDB_DateTime(x) for x in dtss ]
                    return _nt_( *zip(num_array,adj_dts) )                        
                else:
                    return _nt_( *num_array )                            
            else:
                if date_time:
                    adj_dts = [ TOSDB_DateTime(x) for x in dtss ]
                    return list( zip(num_array,adj_dts) )
                else:
                    return [ _ for _ in num_array ]    

    def topic_frame( self, item, date_time = False, labels = True, 
                     data_str_max = STR_DATA_SZ, label_str_max = MAX_STR_SZ ):
        """ Return all the most recent topic values for a particular item:
  
        item: any item string in the block
        date_time: (True/False) attempt to retrieve a TOSDB_DateTime object       
        labels: (True/False) pull the topic labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of topic label strings returned

        if labels and date_time are True: returns-> namedtuple of 2tuple
        if labels is True: returns -> namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """      
        item = item.upper()
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        self._valid_item(  item )
        size = self._topic_count()
        dtss = (_DateTimeStamp * size)()
        # cast char buffers int (char*)[ ]                
        labs = [ _BUF_( label_str_max + 1 ) for _ in range(size)] 
        strs = [ _BUF_( data_str_max + 1 ) for _ in range(size)] 
        # cast char buffers int (char*)[ ]                      
        labs_array = ( _pchar_ * size)( *[ _cast(s, _pchar_) for s in labs] ) 
        strs_array = ( _pchar_ * size)( *[ _cast(s, _pchar_) for s in strs] )  
        err = _lib_call( "TOSDB_GetTopicFrameStrings", 
                         self._name,
                         item.encode("ascii"),
                         strs_array,
                         size,
                         data_str_max + 1,                                            
                         ( labs_array if labels else _ppchar_() ),
                         label_str_max + 1,
                         ( dtss if date_time else _PTR_(_DateTimeStamp)() ),
                         arg_list = [ _str_, _str_, _ppchar_, _ulong_, _ulong_,
                                      _ppchar_, _ulong_, _PTR_(_DateTimeStamp) ] )
        if err:
            raise TOSDB_CLibError( "error value [ "+ str(err) + " ] " +
                                   "returned from library call",
                                   "TOSDB_GetTopicFrameStrings" ) 
        s_map = map( lambda x: _cast(x, _str_).value.decode(), strs_array )
        if labels:
            l_map = map( lambda x: _cast(x, _str_).value.decode(), labs_array )
            _nt_ = _gen_namedtuple( _str_clean(item)[0], _str_clean( *l_map ) )            
            if date_time:
                adj_dts = [TOSDB_DateTime( x ) for x in dtss]
                return _nt_( *zip( s_map, adj_dts ) )                        
            else:
                return _nt_( *s_map )                
        else:
            if date_time:
                adj_dts = [TOSDB_DateTime( x ) for x in dtss]
                return list( zip(s_map, adj_dts) )             
            else:
                return list( s_map )

    def total_frame( self, date_time = False, labels = True, 
                     data_str_max = STR_DATA_SZ, label_str_max = MAX_STR_SZ ):
        """ Return a matrix of the most recent values:  
        
        date_time: (True/False) attempt to retrieve a TOSDB_DateTime object        
        labels: (True/False) pull the item and topic labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of label strings returned
        
        if labels and date_time are True: returns-> dict of namedtuple of 2tuple
        if labels is True: returns -> dict of namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """
        if labels:
            return { item : self.topic_frame( item, 
                                              date_time,
                                              labels, 
                                              data_str_max, 
                                              label_str_max ) for item 
                                                              in self.items() }    
        else:                       
             return [ self.topic_frame( item, 
                                        date_time, 
                                        labels, 
                                        data_str_max, 
                                        label_str_max) for item 
                                                       in self.items() ]
            
# how we access the underlying calls
def _lib_call( f, *args, ret_type = _int_, arg_list = None ):        
    if not _dll:
        raise TOSDB_CLibError( "tos-databridge DLL is currently not loaded ")
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
        raise TOSDB_CLibError("Unable to execute library function", f, cStat, e)

# create a custom namedtuple with an i.d tag for special pickling
def _gen_namedtuple( name, attrs ):
    nt = _namedtuple( name, attrs )
    setattr(nt, NTUP_TAG_ATTR, True)
    return nt

# clean strings for namedtuple fields
def _str_clean( *strings ):    
    fin = []
    for s in strings:               
        tmpS = ''
        for sub in _split( _REGEX_NON_ALNUM, s):
            tmpS = tmpS + sub
        fin.append(tmpS)
    return fin
