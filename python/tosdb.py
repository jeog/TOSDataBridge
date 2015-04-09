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

"""tosdb.py :  A Front-End / Wrapper for the TOS-DataBridge Library

Please refer to README.html for an explanation of the underlying library.
Please refer to PythonTutorial.html in /python/docs for a step-by-step 
walk-through. (currently this may be out-dated as the python layer is updated)

Class TOS_DataBlock: very similar to the 'block' approach of the underlying
library(again, see the README.html) except the interface is explicitly
object-oriented. A unique block id is handled by the object and the unique C
calls are handled accordingly. It also abstracts away most of the type 
complexity of the underlying calls, raising TOSDB_Error exceptions ifinternal 
errors occur.

Class VTOS_DataBlock: same interface as TOS_DataBlock except it utilizes a thin
virtualization layer over UDP. Method calls are serialized and sent over a
phsyical/virtual network to a windows machine running the core implemenataion,
passing returned values back.

Those who want to access the TOS_DataBlock on linux while running Windows in
a virtual machine can abstract away nearly all the (unfortunately necessary)
non-portable parts of the core implementation.

In order to create a virtual block the 'local' windows implementation must
do everything it would normally do to instantiate a TOS_DataBlock (i.e connect
to a running TOSDataBridge service via init/connect) and then call
enable_virtualization() with an address tuple (addr,port) that the virtual
server ( _VTOS_DataServer ) will used to listen for 'remote' virtual blocks.

tosdb.py will attempt to load the non-portable _tosdb_win.py if on windows.
The following are some of the important calls imported from _tosdb_win.py that
control the underlying DLL:

  init() initializes the underlying library
  connect() connects to the library (init attemps this for you)
  connected() returns connection status (boolean)
  get_block_limit() returns block limit of the library's RawDataBlock factory
  set_block_limit() changes block limit of the library's RawDataBlock factory
  get_block_count() returns the current number of blocks in the library

  ********************************* IMPORTANT *********************************
  clean_up() de-allocates shared resources of the underlying library and
    Service. We attempt to clean up resources automatically for you on exit
    but in certain cases we can't, so its not guaranteed to happen. Therefore
    it's HIGHLY RECOMMENDED YOU CALL THIS FUNCTION before you exit. 
"""


# _tosdb is how we deal with C++ header defined consts, those exported from the
# back-end libs, and '#define' compile-time consts necessary for C compatibility
from _tosdb import *  # also allows us to migrate away from ctypes when necessary
from _tosdb_errors import *
from tosdb_datetime import TOS_DateTime, _DateTimeStamp

# types and type 'modifiers' use _[ name ]_ format
from ctypes import cast as _cast, pointer as _pointer, \
     create_string_buffer as _BUF_, POINTER as _PTR_, c_double as _double_, \
     c_float as _float_, c_ulong as _ulong_, c_long as _long_, \
     c_longlong as _longlong_, c_char_p as _string_, c_char as _char_, \
     c_ubyte as _uchar_, c_int as _int_, c_void_p as _pvoid_
_pchar_ = _PTR_( _char_ )
_ppchar_ = _PTR_( _pchar_ )

from collections import namedtuple as _namedtuple
from io import StringIO as _StringIO
from uuid import uuid4 as _uuid4
from threading import Thread as _Thread
from argparse import ArgumentParser as _ArgumentParser
from platform import system as _system
import socket as _socket
import pickle as _pickle

if _system() in ["Windows","windows"]:
    from  _tosdb_win import *
    if __name__ == "__main__":
        _handle_args()        
else:
    def _lib_call( f, *args, ret_type = _int_, arg_list = None ):
        raise TOSDB_Error("_lib_call can only be called from Windows. " +
                          "Make sure you are working with VTOS_DataBlock." )

_virtual_blocks = dict() # add remote address #
_virtual_data_server = None
_virtual_CREATE = '1'
_virtual_CALL = '2'
_virtual_DESTROY = '3'
_virtual_FAIL = '4'
_virtual_SUCESS = '5'
_virtual_SUCCESS_NT = '6'
_virtual_MAX_REQ_SZ = 1000 # arbitrary for now
_virtual_VAR_TYPES = {'i':int,'s':str,'b':bool}

class TOS_DataBlock:
    """ The main object for storing TOS data.    

    size: how much historical data to save
    date_time: should block include date-time stamp with each data-point?
    timeout: how long to wait for responses from TOS-DDE server 

    Please review the attached README.html for details.
    """    
    def __init__( self, size = 1000, date_time = False, timeout = DEF_TIMEOUT ):        
        self._name = (_uuid4().hex).encode("ascii")
        self._valid = False
        err = _lib_call( "TOSDB_CreateBlock", self._name , size, date_time, 
                         timeout )        
        if( err ):
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_CreateBlock" )         
        self._valid= True
        self._block_size = size
        self._timeout = timeout
        self._date_time = date_time
        self._items = []   
        self._topics = []
        self._namedtuple_flag = False
  
    def __del__( self ): # for convenience, no guarantee
        if _lib_call is not None and self._valid:
            err = _lib_call( "TOSDB_CloseBlock", self._name )      
            if( err ):
                print(" - Error [ ",err," ] Closing Block (leak possible) " )

    def __str__( self ):
        _clear_return_status()
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
                       end=' ',
                       file=sio ) 
            print('',file=sio)
            count += 1              
        sio.seek(0)
        return sio.read()

    def _clear_return_status():
        self._namedtuple_flag = False
    
    # private _valid calls assume .upper() already called
    def _valid_item( self, item ):
        if ( not self._items): # in case items came out of pre-cache
            self._items = self.items()
        if ( item not in self._items ):
            raise TOSDB_Error( item + " not found")

    def _valid_topic( self, topic ):
        if( not self._topics ): # in case topics came out of pre-cache
            self._topics = self.topics()
        if ( topic not in self._topics ):
             raise TOSDB_Error( topic + " not found" )

    def _item_count( self ):
        _clear_return_status()
        i_count = _ulong_()
        err = _lib_call("TOSDB_GetItemCount", self._name, _pointer(i_count) )
        if(err):
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_GetItemCount" )
        return i_count.value

    def _topic_count( self ):
        _clear_return_status()
        t_count = _ulong_()
        err =  _lib_call("TOSDB_GetTopicCount", self._name, _pointer(t_count) )
        if(err):
            raise TOSDB_Error( "error value [ "+ str(err) +
                               " ] returned from library call", 
                               "TOSDB_GetTopicCount" )
        return t_count.value

    def info(self):        
        """ Returns a more readable dict of info about the underlying block """
        _clear_return_status()
        return { "Name": self._name.decode('ascii'), 
                 "Items": self.items(),
                 "Topics": self.topics(), 
                 "Size": self._block_size,
                 "DateTime": "Enabled" if self._date_time else "Disabled",
                 "Timeout": self._timeout }
    
    def get_block_size( self ):
        """ Returns the amount of historical data stored in the block """
        _clear_return_status()
        b_size = _ulong_()
        err = _lib_call( "TOSDB_GetBlockSize", self._name, _pointer(b_size))
        if(err):
            raise TOSDB_Error( "error value [ "  + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_GetBlockSize" )
        return b_size.value
    
    def set_block_size( self, sz ):
        """ Changes the amount of historical data stored in the block """
        err = _lib_call( "TOSDB_SetBlockSize", self._name, sz)
        if(err):
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_SetBlockSize" )
        else:
            self._block_size = sz
            
    def stream_occupancy( self, item, topic ):
        _clear_return_status()
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
                         arg_list = [ _string_, _string_, _string_, 
                                      _PTR_(_ulong_) ] )
        if(err):
             raise TOSDB_Error( "error value [ " + str(err) + 
                                " ] returned from library call", 
                                "TOSDB_GetStreamOccupancy" )
        return occ.value
    
    def items( self, str_max = MAX_STR_SZ ):
        """ Returns the items currently in the block (and not pre-cached).
        
        str_max: the maximum length of item strings returned
        returns -> list of strings 
        """
        _clear_return_status()
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
                         arg_list = [ _string_,  _ppchar_, _ulong_, _ulong_ ] )
        if ( err ):            
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_GetItemNames" )
        else:
            return [_cast(ptr, _string_).value.decode() for ptr in strs_array]            
              
    def topics( self,  str_max = MAX_STR_SZ ):
        """ Returns the topics currently in the block (and not pre-cached).
        
        str_max: the maximum length of topic strings returned  
        returns -> list of strings 
        """
        _clear_return_status()
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
                        arg_list = [ _string_,  _ppchar_, _ulong_, _ulong_ ] )           
        if ( err ):            
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_GetTopicNames" )            
        else:
            return [_cast(ptr, _string_).value.decode() for ptr in strs_array] 
      
    
    def add_items( self, *items ):
        """ Add items ( ex. 'IBM', 'SPY' ) to the block.

        NOTE: if there are no topics currently in the block, these items will 
        be pre-cached and appear not to exist, until a valid topic is added.

        *items: any numer of item strings
        """               
        mTup = tuple( s.encode("ascii").upper() for s in items )
        itemsTy = _string_ * len( mTup )
        cItems = itemsTy( *mTup )
        err = _lib_call( "TOSDB_AddItems", self._name, cItems, len( mTup ) )
        if ( err ):
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_AddItems" )           
        if( not self._items and not self._topics):
            self._topics = self.topics() # in case topics came out of pre-cache
        self._items = self.items()
       

    def add_topics( self, *topics ):
        """ Add topics ( ex. 'LAST', 'ASK' ) to the block.

        NOTE: if there are no items currently in the block, these topics will 
        be pre-cached and appear not to exist, until a valid item is added.

        *topics: any numer of topic strings
        """               
        mTup = tuple( s.encode("ascii").upper() for s in topics )
        topicsTy = _string_ * len( mTup )
        cTopics = topicsTy( *mTup )
        err = _lib_call( "TOSDB_AddTopics", self._name, cTopics, len( mTup ) )
        if ( err ):
            raise TOSDB_Error( "error value [ " + str(err) + 
                               " ] returned from library call", 
                               "TOSDB_AddTopics" )
        if( not self._items and not self._topics ):
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
            if ( err ):
                raise TOSDB_Error( "error value [ " + str(err) + 
                                   " ] returned from library call", 
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
            if ( err ):
                raise TOSDB_Error( "error value [ " + str(err) + 
                                   " ] returned from library call", 
                                   "TOSDB_RemoveTopic" )  
        self._topics = self.topics()
        
    def get( self, item, topic, date_time = False, indx = 0, 
             check_indx = True, data_str_max = STR_DATA_SZ ):
        """ Return a single data-point from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object   
        indx: index of data-points [0 to block_size), [-block_size to -1]
        check_indx: throw if datum doesn't exist at that particular index
        data_str_max: the maximum size of string data returned
        """
        _clear_return_status()
        item = item.upper()
        topic = topic.upper()
        if( date_time and not self._date_time ):
            raise TOSDB_Error( " date_time is not available for this block " )
        self._valid_item(item)
        self._valid_topic(topic)
        if ( indx < 0 ):
            indx += self._block_size
        if( indx >= self._block_size ):
            raise IndexError("Invalid index value passed to get(...)")   
        if( check_indx and indx >= self.stream_occupancy( item, topic ) ):
            raise TOSDB_Error("data not available at this index yet " +
                              "(disable check_indx to avoid this error)" )
        dts = _DateTimeStamp()      
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )        
        if (typeTup[0] == "String"): 
            ret_str = _BUF_( data_str_max + 1 )
            err =_lib_call( "TOSDB_GetString", 
                            self._name,
                            item.encode("ascii"),
                            topic.encode("ascii"),
                            indx,
                            ret_str,
                            data_str_max + 1,
                            ( _pointer(dts) if date_time 
                                           else _PTR_(_DateTimeStamp)() ),
                            arg_list = [ _string_,  _string_, _string_, 
                                         _long_, _pchar_, _ulong_, 
                                         _PTR_(_DateTimeStamp) ] )
            if ( err ):
                raise TOSDB_Error( "error value [ " + str(err) + 
                                   " ] returned from library call", 
                                   "TOSDB_GetString" ) 
            if ( date_time ):
                return (ret_str.value.decode(), TOS_DateTime( dts ))
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
                             ( _pointer(dts) if date_time 
                                            else _PTR_(_DateTimeStamp)() ),
                             arg_list = [ _string_,  _string_, _string_, 
                                          _long_, _PTR_(typeTup[1]), 
                                          _PTR_(_DateTimeStamp) ] )
            if ( err ):
                raise TOSDB_Error( "error value [ " + str(err) + 
                                   " ] returned from library call",
                                   "TOSDB_Get"+typeTup[0] )  
            if( date_time ):
                return (val.value, TOS_DateTime( dts ))
            else:
                return val.value

    def stream_snapshot( self, item, topic, date_time = False, 
                         end = -1, beg = 0, smart_size = True, 
                         data_str_max = STR_DATA_SZ ):
        """ Return multiple data-points(a snapshot) from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object              
        end: index of least recent data-point ( end of the snapshot )
        beg: index of most recent data-point ( beginning of the snapshot )        
        smart_size: limits amount of returned data by data-stream's occupancy
        data_str_max: the maximum length of string data returned

        if date_time is True: returns-> list of 2tuple
        else: returns -> list              
        """
        _clear_return_status()
        item = item.upper()
        topic = topic.upper()
        if( date_time and not self._date_time ):
            raise TOSDB_Error( " date_time is not available for this block " )
        self._valid_item(item)
        self._valid_topic(topic)
        if ( end < 0 ):
            end += self._block_size
        if ( beg < 0 ):
            beg += self._block_size
        if( smart_size ):            
            end = min(end, self.stream_occupancy( item, topic )-1 )                 
        size = (end - beg) + 1
        if (  beg < 0 or
              end < 0 or
              beg >= self._block_size or
              end >= self._block_size or
              size <= 0):
            raise IndexError("Invalid index value(s) passed to stream_snapshot")           
        dtss = (_DateTimeStamp * size)()
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )   
        if (typeTup[0] == "String"):
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
                             ( dtss if date_time 
                                    else _PTR_(_DateTimeStamp)() ),
                             end,
                             beg,
                             arg_list = [ _string_, _string_, _string_, 
                                          _ppchar_, _ulong_, _ulong_, 
                                          _PTR_(_DateTimeStamp), _long_, 
                                          _long_ ] )
            if ( err ):
                raise TOSDB_Error( "error value [ " + str(err) + 
                                   " ] returned from library call",
                                   "TOSDB_GetStreamSnapshotStrings" ) 
            if ( date_time ):
                adj_dts =  [TOS_DateTime( x ) for x in dtss]
                return [_ for _ in zip( 
                            map( lambda x : _cast(x, _string_).value.decode(), 
                                 strs_array ), 
                            adj_dts ) ]        
            else:
                return [_cast(ptr,_string_).value.decode() for ptr in strs_array]
        else:
            num_array =  (typeTup[1] * size)()   
            err = _lib_call( "TOSDB_GetStreamSnapshot"+typeTup[0]+"s", 
                             self._name,
                             item.encode("ascii"),
                             topic.encode("ascii"),
                             num_array,
                             size,
                             ( dtss if date_time 
                                    else _PTR_(_DateTimeStamp)() ),
                             end,
                             beg,
                             arg_list = [ _string_, _string_, _string_, 
                                          _PTR_(typeTup[1]), _ulong_, 
                                          _PTR_(_DateTimeStamp), _long_, 
                                          _long_ ] )
            if ( err ):
                raise TOSDB_Error( "error value of [ " + str(err) + 
                                   " ] returned from library call",
                                   "TOSDB_GetStreamSnapshot"+typeTup[0]+"s" ) 
            if( date_time ):
                adj_dts =  [TOS_DateTime( x ) for x in dtss]
                return [_ for _ in zip( num_array, adj_dts )]       
            else:
                return [_ for _ in num_array]

    def item_frame( self, topic, date_time = False, labels = True, 
                    data_str_max = STR_DATA_SZ, label_str_max = MAX_STR_SZ ):
        """ Return all the most recent item values for a particular topic.

        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object       
        labels: (True/False) pull the item labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of item label strings returned

        if labels and date_time are True: returns-> namedtuple of 2tuple
        if labels is True: returns -> namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """
        _clear_return_status()
        topic = topic.upper()
        if( date_time and not self._date_time ):
            raise TOSDB_Error( " date_time is not available for this block " )
        self._valid_topic(topic)
        size = self._item_count()
        dtss = (_DateTimeStamp * size)()
        # store char buffers
        labs = [ _BUF_(label_str_max+1) for _ in range(size)] 
        # cast char buffers int (char*)[ ]
        labs_array = ( _pchar_ * size)( *[ _cast(s, _pchar_) for s in labs] )  
        tBits = type_bits( topic )
        typeTup = _type_switch( tBits )       
        if (typeTup[0] is "String"):                     
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
                             ( labs_array if labels 
                                          else _ppchar_() ),
                             label_str_max + 1,
                             ( dtss if date_time 
                                    else _PTR_(_DateTimeStamp)() ),
                             arg_list = [ _string_, _string_, _ppchar_, 
                                          _ulong_, _ulong_, _ppchar_, _ulong_, 
                                          _PTR_(_DateTimeStamp) ] )
            if ( err ):
                raise TOSDB_Error( "error value [ " + str(err) + 
                                   " ] returned from library call",
                                   "TOSDB_GetItemFrameStrings" )
            s_map = map( lambda x : _cast(x, _string_).value.decode(), 
                         strs_array )           
            if( labels):
                l_map = map( lambda x : _cast(x, _string_).value.decode(), 
                             labs_array )
                _ntuple_ = _namedtuple( _str_clean(topic)[0], 
                                       _str_clean( *l_map ) )
                self._namedtuple_flag = True
                if( date_time ):
                    adj_dts = [TOS_DateTime( x ) for x in dtss]
                    return _ntuple_( *zip( s_map, adj_dts ) )                        
                else:
                    return _ntuple_( * s_map )             
            else:
                if( date_time ):
                    adj_dts = [TOS_DateTime( x ) for x in dtss]
                    return list( zip( s_map, adj_dts ) )
                else:
                    return list( s_map )                  
        else: 
            num_array =  (typeTup[1] * size)()   
            err = _lib_call( "TOSDB_GetItemFrame"+typeTup[0]+"s", 
                             self._name,
                             topic.encode("ascii"),
                             num_array,
                             size,
                             ( labs_array if labels 
                                          else _ppchar_() ),
                             label_str_max + 1,
                             ( dtss if date_time 
                                    else _PTR_(_DateTimeStamp)() ),                           
                             arg_list = [ _string_, _string_, 
                                          _PTR_(typeTup[1]), _ulong_, 
                                          _ppchar_, _ulong_, 
                                          _PTR_(_DateTimeStamp) ] )
            if ( err ):
                raise TOSDB_Error( "error value of [ " + str(err)  + 
                                   " ] returned from library call",
                                   "TOSDB_GetItemFrame"+typeTup[0]+"s" ) 
            if( labels):                                
                l_map = map( lambda x : _cast(x, _string_).value.decode(), 
                             labs_array )
                _ntuple_ = _namedtuple( _str_clean(topic)[0], 
                                       _str_clean( *l_map ) )
                self._namedtuple_flag = True
                if( date_time ):
                    adj_dts = [TOS_DateTime( x ) for x in dtss]
                    return _ntuple_( *zip( num_array, adj_dts ) )                        
                else:
                    return _ntuple_( *num_array )                            
            else:
                if( date_time ):
                    adj_dts = [TOS_DateTime( x ) for x in dtss]
                    return list( zip( num_array, adj_dts ) )
                else:
                    return [_ for _ in num_array]    

    def topic_frame( self, item, date_time = False, labels = True, 
                     data_str_max = STR_DATA_SZ, label_str_max = MAX_STR_SZ ):
        """ Return all the most recent topic values for a particular item:
  
        item: any item string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object       
        labels: (True/False) pull the topic labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of topic label strings returned

        if labels and date_time are True: returns-> namedtuple of 2tuple
        if labels is True: returns -> namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """
        _clear_return_status()
        item = item.upper()
        if( date_time and not self._date_time ):
            raise TOSDB_Error( " date_time is not available for this block " )
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
                         ( labs_array if labels 
                                      else _ppchar_() ),
                         label_str_max + 1,
                         ( dtss if date_time 
                                else _PTR_(_DateTimeStamp)() ),
                         arg_list = [ _string_, _string_, _ppchar_, _ulong_, 
                                      _ulong_, _ppchar_, _ulong_, 
                                      _PTR_(_DateTimeStamp) ] )
        if ( err ):
            raise TOSDB_Error( "error value of [ " + str(err)  + 
                               " ] returned from library call",
                               "TOSDB_GetTopicFrameStrings" ) 
        s_map = map( lambda x : _cast(x, _string_).value.decode(), strs_array)
        if( labels):
            l_map = map( lambda x : _cast(x, _string_).value.decode(), 
                         labs_array )
            _ntuple_ = _namedtuple( _str_clean(item)[0], 
                                   _str_clean( *l_map ) )
            self._namedtuple_flag = True
            if( date_time ):
                adj_dts = [TOS_DateTime( x ) for x in dtss]
                return _ntuple_( *zip( s_map, adj_dts ) )                        
            else:
                return _ntuple_( *s_map )                
        else:
            if( date_time ):
                adj_dts = [TOS_DateTime( x ) for x in dtss]
                return list( zip(s_map, adj_dts) )             
            else:
                return list( s_map )

    def total_frame( self, date_time = False, labels = True, 
                     data_str_max = STR_DATA_SZ, label_str_max = MAX_STR_SZ ):
        """ Return a matrix of the most recent values:  
        
        date_time: (True/False) attempt to retrieve a TOS_DateTime object        
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

###
###
###            

class VTOS_DataBlock:
    """ The main object for storing TOS data. (VIRTUAL)   

    address: the adddress of the actual implementation
    size: how much historical data to save
    date_time: should block include date-time stamp with each data-point?
    timeout: how long to wait for responses from TOS-DDE server 

    Please review the attached README.html for details.
    """

    # check that address is 2-tuple
    def __init__( self, address, size = 1000, date_time = False,
                  timeout = DEF_TIMEOUT ):
        self._my_addr = address
        self._my_sock = _socket.socket( _socket.AF_INET, _socket.SOCK_DGRAM )
        self._name = self._call( _virtual_CREATE, '__init__',
                                 ('i',size), ('b',date_time), ('i',timeout) )
        
    def __del__( self ):
        try:
            if self._call( _virtual_DESTROY ):
                self._my_sock.close()
        except TOSDB_Error as e:
            print( e.args[0] )          

    def __str__( self ):
        return self._call( _virtual_CALL, '__str__' )    
  
    def _valid_item( self, item ):
        try:
            self._call( _virtual_CALL, '_valid_item', ('s',item) )
        except TOSDB_Error as e:
            raise TOSDB_Error( item + " not found")

    def _valid_topic( self, topic ):
        try:
            self._call( _virtual_CALL, '_valid_topic', ('s',topic) )
        except TOSDB_Error as e:
            raise TOSDB_Error( topic + " not found")

    def _item_count( self ):
        return self._call( _virtual_CALL, '_item_count' )

    def _topic_count( self ):
        return self._call( _virtual_CALL, '_topic_count' )

    def info(self):
        """ Returns a more readable dict of info about the underlying block """
        return self._call( _virtual_CALL, 'info' )
    
    def get_block_size( self ):
        """ Returns the amount of historical data stored in the block """
        return self._call( _virtual_CALL, 'get_block_size' )
    
    def set_block_size( self, sz ):
        """ Changes the amount of historical data stored in the block """
        self._call( _virtual_CALL, 'set_block_size', ('i',sz) )
            
    def stream_occupancy( self, item, topic ):
        return self._call( _virtual_CALL, 'stream_occupancy', ('s',item),
                           ('s',topic) )
    
    def items( self, str_max = MAX_STR_SZ ):
        """ Returns the items currently in the block (and not pre-cached).
        
        str_max: the maximum length of item strings returned
        returns -> list of strings 
        """
        return self._call( _virtual_CALL, 'items', ('i',str_max) )          
              
    def topics( self,  str_max = MAX_STR_SZ ):
        """ Returns the topics currently in the block (and not pre-cached).
        
        str_max: the maximum length of topic strings returned  
        returns -> list of strings 
        """
        return self._call( _virtual_CALL, 'topics', ('i',str_max) ) 
      
    
    def add_items( self, *items ):
        """ Add items ( ex. 'IBM', 'SPY' ) to the block.

        NOTE: if there are no topics currently in the block, these items will 
        be pre-cached and appear not to exist, until a valid topic is added.

        *items: any numer of item strings
        """               
        self._call( _virtual_CALL, 'add_items', *zip('s'*len(items), items) )
       

    def add_topics( self, *topics ):
        """ Add topics ( ex. 'LAST', 'ASK' ) to the block.

        NOTE: if there are no items currently in the block, these topics will 
        be pre-cached and appear not to exist, until a valid item is added.

        *topics: any numer of topic strings
        """               
        self._call( _virtual_CALL, 'add_topics', *zip('s'*len(topics), topics) )

    def remove_items( self, *items ):
        """ Remove items ( ex. 'IBM', 'SPY' ) from the block.

        NOTE: if there this call removes all items from the block the 
        remaining topics will be pre-cached and appear not to exist, until 
        a valid item is re-added.

        *items: any numer of item strings
        """
        self._call( _virtual_CALL, 'remove_items', *zip('s'*len(items), items) )

    def remove_topics( self, *topics ):
        """ Remove topics ( ex. 'LAST', 'ASK' ) from the block.

        NOTE: if there this call removes all topics from the block the 
        remaining items will be pre-cached and appear not to exist, until 
        a valid topic is re-added.

        *topics: any numer of topic strings
        """
        self._call( _virtual_CALL, 'remove_topics', *zip('s'*len(topics), topics) )
        
    def get( self, item, topic, date_time = False, indx = 0, 
             check_indx = True, data_str_max = STR_DATA_SZ ):
        """ Return a single data-point from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object   
        indx: index of data-points [0 to block_size), [-block_size to -1]
        check_indx: throw if datum doesn't exist at that particular index
        data_str_max: the maximum size of string data returned
        """
        return self._call( _virtual_CALL, 'get', ('s',item), ('s',topic),
                           ('b',date_time), ('i',indx), ('b',check_indx),
                           ('i', data_str_max) )

    def stream_snapshot( self, item, topic, date_time = False, 
                         end = -1, beg = 0, smart_size = True, 
                         data_str_max = STR_DATA_SZ ):
        """ Return multiple data-points(a snapshot) from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object              
        end: index of least recent data-point ( end of the snapshot )
        beg: index of most recent data-point ( beginning of the snapshot )        
        smart_size: limits amount of returned data by data-stream's occupancy
        data_str_max: the maximum length of string data returned

        if date_time is True: returns-> list of 2tuple
        else: returns -> list              
        """
        return self._call( _virtual_CALL, 'stream_snapshot', ('s',item),
                           ('s',topic), ('b',date_time), ('i',end), ('i',beg),
                           ('b',smart_size), ('i', data_str_max) )

    def item_frame( self, topic, date_time = False, labels = True, 
                    data_str_max = STR_DATA_SZ,
                    label_str_max = MAX_STR_SZ ):
        """ Return all the most recent item values for a particular topic.

        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object       
        labels: (True/False) pull the item labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of item label strings returned

        if labels and date_time are True: returns-> namedtuple of 2tuple
        if labels is True: returns -> namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """
        return self._call( _virtual_CALL, 'item_frame', ('s',topic),
                           ('b',date_time), ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )   

    def topic_frame( self, item, date_time = False, labels = True, 
                     data_str_max = STR_DATA_SZ,
                     label_str_max = MAX_STR_SZ ):
        """ Return all the most recent topic values for a particular item:
  
        item: any item string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object       
        labels: (True/False) pull the topic labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of topic label strings returned

        if labels and date_time are True: returns-> namedtuple of 2tuple
        if labels is True: returns -> namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """
        return self._call( _virtual_CALL, 'topic_frame', ('s',item),
                           ('b',date_time), ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )

    def total_frame( self, date_time = False, labels = True, 
                     data_str_max = STR_DATA_SZ,
                     label_str_max = MAX_STR_SZ ):
        """ Return a matrix of the most recent values:  
        
        date_time: (True/False) attempt to retrieve a TOS_DateTime object        
        labels: (True/False) pull the item and topic labels with the values 
        data_str_max: the maximum length of string data returned
        label_str_max: the maximum length of label strings returned
        
        if labels and date_time are True: returns-> dict of namedtuple of 2tuple
        if labels is True: returns -> dict of namedtuple
        if date_time is True: returns -> list of 2tuple
        else returns-> list
        """
        return self._call( _virtual_CALL, 'total_frame', ('b',date_time),
                           ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )
    
    def _call( self, virt_type, method='', *arg_buffer ):
        req_b = b''
        if virt_type == _virtual_CREATE:
            req_b = _virtual_CREATE.encode() + (b' ' + _pickle.dumps(arg_buffer))
        elif virt_type == _virtual_CALL:
            req_b = (_virtual_CALL + ' ' + self._name + ' ' + method).encode()
            if arg_buffer:
                req_b += (b' ' + _pickle.dumps(arg_buffer))
        elif virt_type == _virtual_DESTROY:
            req_b = (_virtual_CREATE + ' ' + self._name).encode()
        else:
            raise TOSDB_Error( "Type Error: virt_type" )        
        c = self._my_sock.sendto( req_b, self._my_addr )
        if c == 0:
            raise TOSDB_Error("_call -> sendto() failed")
        else:
            ret_b = self._my_sock.recv( 1000 )
            print(ret_b)
            # hold off on decode til we un-pickle 
            args = ret_b.strip().split(b' ')
            status = args[0].decode()
            if status == _virtual_FAIL:
                raise TOSDB_Error( "underlying block returned failure status: " +
                                    args[1].decode(),
                                   "virt_type: " + str(virt_type),
                                   "method_or_name: " + str(method_or_name),
                                   "arg_buffer: " + str(arg_buffer) )
            else:
                if virt_type == _virtual_CREATE:
                    return args[1].decode()
                elif virt_type == _virtual_CALL and len(args) > 1:
                    if status == _virtual_SUCCESS_NT:
                        return _loadnamedtuple( args[1] )
                    else:
                        return _pickle.loads( args[1] )
                elif virt_type == _virtual_DESTROY:
                    return True

###
###
###            
    
def enable_virtualization( address ):
    global _virtual_data_server
    
    def _create_callback( *args ):
        global _virtual_blocks
        print("DEBUG", "in _create_callback")
        blk = None
        try:
            print( *args )
            blk = TOS_DataBlock( *args ) 
            _virtual_blocks[blk._name] = blk
            return blk._name                                       
        except:
            if blk:
                _virtual_blocks.pop(blk._name)
                del blk
            return False       

    def _destroy_callback( name ):
        global _virtual_blocks
        print("DEBUG", "in _destroy_callback")
        try:
            blk = _virtual_blocks.pop( name )
            del blk
            return True
        except:
            return False

    # clean up encoding/decoding ops
    # output exception somewhere
    def _call_callback( name, meth, *args):
        global _virtual_blocks
        print("DEBUG", "in _call_callback")
        try:
            name = name.encode('ascii')
            print( name, meth, *args)
            blk = _virtual_blocks[ name ]         
            meth = getattr(blk, meth )          
            ret = meth( *args )          
            return (ret,blk._namedtuple_flag) if ret else True
        except Exception as e:
            print( str(e) )
            return False
        finally:
            self._namedtuple_flag = False

    class _VTOS_DataServer( _Thread ):
        
        def __init__( self, address, create_callback, destroy_callback,
                      call_callback):
            super().__init__()
            self._my_addr = address
            self._create_callback = create_callback
            self._destroy_callback = destroy_callback
            self._call_callback = call_callback
            self._rflag = False
            self._my_sock = _socket.socket( _socket.AF_INET, _socket.SOCK_DGRAM )
            self._my_sock.bind( address )
            
        def stop(self):
            self._rflag = False
            self._my_sock.close()

        def run(self):
            self._rflag = True
            # need a more active way to break out, poll more often !
            while self._rflag:           
                dat, addr = self._my_sock.recvfrom( _virtual_MAX_REQ_SZ )            
                if not dat:
                    continue
                print(dat) #DEBUG#
                args = dat.strip().split(b' ')
                msg_t = args[0].decode()
                ret_b = _virtual_FAIL.encode()
                if msg_t == _virtual_CREATE:
                    upargs = _pickle.loads(args[1])                
                    cargs = [ _virtual_VAR_TYPES[t](v) for t,v in upargs ]
                    print('create upargs', upargs)
                    ret = self._create_callback( *cargs )
                    if ret:
                        ret_b = (_virtual_SUCESS + ' ').encode() + ret                                                                   
                elif msg_t == _virtual_CALL:
                    if len(args) > 3:              
                        upargs = _pickle.loads(args[3])
                        cargs = [ _virtual_VAR_TYPES[t](v) for t,v in upargs ]
                        print('call upargs', upargs)
                        ret = self._call_callback( args[1].decode(),
                                                   args[2].decode(), *cargs)
                    else:
                        ret = self._call_callback( args[1].decode(),
                                                   args[2].decode() )                        
                    if ret:
                        ret_b = _virtual_SUCESS.encode()
                        if type(ret) != bool:
                            if ret[1]: #namedtuple special case
                                ret_b = _virtual_SUCCESS_NT.encode() \
                                        + b' ' + _dumpnamedtuple(ret[0])
                            else:
                                ret_b += b' ' + _pickle.dumps(ret[0])                        
                elif msg_t == _virtual_DESTROY:
                    if self._virtual_destroy_callback( args[1].decode() ):
                        ret_b = _virtual_SUCESS.encode()                           
                
                self._my_sock.sendto( ret_b, addr )

    try:
        _virtual_data_server = _VTOS_DataServer( address,_create_callback,
                                                _destroy_callback,_call_callback)
        _virtual_data_server.start()      
    except Exception as e:
        raise TOSDB_Error("virtualization error", str(e) )

def disable_virtualization():
    try:
        if _virtual_data_server is not None:
           _virtual_data_server.stop()
           _virtual_data_server = None
           _virtual_blocks.clear()
    except Exception as e:
        raise TOSDB_Error("virtualization error", str(e) )    

def _dumpnamedtuple( nt ):
    n = type(nt).__name__
    od = nt.__dict__
    return _pickle.dumps( (n,tuple(od.keys()),tuple(od.values())) )

def _loadnamedtuple( nt):
    name,keys,vals = _pickle.loads( nt )
    ty = _namedtuple( name, keys )
    return ty( *vals )

def _handle_args():
    parser = _ArgumentParser()
    parser.add_argument( "--root", 
                         help = "root directory to search for the library" )
    parser.add_argument( "--path", help="the exact path of the library" )
    parser.add_argument( "-n", "--noinit", 
                         help="don't initialize the library automatically",
                         action="store_true" )
    args = parser.parse_args()   
    if(args.noinit):
        return
    if( args.path ):
        init( dllpath = args.path )
    elif( args.root ):
        init( root = args.root )
    else:
        print( "*WARNING* by not supplying --root, --path, or " +
               "--noinit( -n ) arguments you are opting for a default " +
               "search root of 'C:\\'. This will attempt to search " +
               "the ENTIRE disk/drive for the tos-databridge library. " +
               "It's recommended you restart the program with the " +
               "library path (after --path) or a narrower directory " +
               "root (after --root)." )                
        if( input( "Do you want to continue anyway? (y/n): ") == 'y' ):
            init()
        else:
            print("- init(root='C:\\') aborted")
        


                
