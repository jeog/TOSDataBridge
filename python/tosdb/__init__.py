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

Class TOS_DataBlock (windows only): very similar to the 'block' approach of the
underlying library(again, see the README.html) except the interface is
explicitly object-oriented. A unique block id is handled by the object and the
unique C calls are handled accordingly. It also abstracts away most of the type
complexity of the underlying calls, raising TOSDB_CLibError exceptions if
internal errors occur.

Class VTOS_DataBlock: same interface as TOS_DataBlock except it utilizes a thin
virtualization layer over TCP. Method calls are serialized and sent over a
phsyical/virtual network to a windows machine running the core implemenataion,
passing returned values back.

Class _TOS_DataBlock: abstract base class of TOS_DataBlock and VTOS_DataBlock

                                  * * *   

Those who want to access the TOS_DataBlock on linux while running Windows in
a virtual machine can abstract away nearly all the (unfortunately necessary)
non-portable parts of the core implementation.

In order to create a virtual block the 'local' windows implementation must
do everything it would normally do to instantiate a TOS_DataBlock (i.e connect
to a running TOSDataBridge service via init/connect) and then call
enable_virtualization() with an address tuple (addr,port) that the virtual
hub/servers ( _VTOS_Hub / _VTOS_BlockServer ) will used to listen for 'remote'
virtual blocks. To use admin calls (those outside the block) call
admin_init.

tosdb.py will attempt to load the non-portable _win.py if on windows.
The following are some of the important calls imported from _win.py that
control the underlying DLL:

  init() initializes the underlying library
  connect() connects to the library (init attemps this for you)
  connected() returns connection status (boolean)
  get_block_limit() returns block limit of the library's RawDataBlock factory
  set_block_limit() changes block limit of the library's RawDataBlock factory
  get_block_count() returns the current number of blocks in the library

  ********************************* IMPORTANT ********************************
  clean_up() de-allocates shared resources of the underlying library and
    Service. We attempt to clean up resources automatically for you on exit
    but in certain cases we can't, so its not guaranteed to happen. Therefore
    it's HIGHLY RECOMMENDED YOU CALL THIS FUNCTION before you exit.
  ********************************* IMPORTANT ********************************
"""

# _tosdb is how we deal with C++ header defined consts, those exported from the
# back-end libs, and '#define' compile-time consts necessary for C compatibility
from _tosdb import *  # also allows us to migrate away from ctypes when necessary
from .errors import *
from collections import namedtuple as _namedtuple
from threading import Thread as _Thread
from argparse import ArgumentParser as _ArgumentParser
from functools import partial as _partial
from platform import system as _system
from abc import ABCMeta as _ABCMeta, abstractmethod as _abstractmethod
from sys import stderr as _stderr
from re import sub as _sub
from atexit import register as _on_exit
import struct as _struct
import socket as _socket
import pickle as _pickle

### TODO ###
# if a lib call is made immediately after init it's possible an err val
# of -1 will be returned because the lowlevel flags have not been set
# to true fast enough... FIX (block until we can comfirm)
###########

class _TOS_DataBlock(metaclass=_ABCMeta):
    """ The DataBlock interface """
    @_abstractmethod
    def __str__(): pass
    @_abstractmethod
    def info(): pass
    @_abstractmethod
    def get_block_size(): pass
    @_abstractmethod
    def set_block_size(): pass
    @_abstractmethod
    def stream_occupancy(): pass
    @_abstractmethod
    def items(): pass
    @_abstractmethod
    def topics(): pass
    @_abstractmethod
    def add_items(): pass
    @_abstractmethod
    def add_topics(): pass
    @_abstractmethod
    def remove_items(): pass
    @_abstractmethod
    def remove_topics(): pass
    @_abstractmethod
    def get(): pass
    @_abstractmethod
    def stream_snapshot(): pass
    @_abstractmethod
    def stream_snapshot_from_marker(): pass
    @_abstractmethod
    def item_frame(): pass
    @_abstractmethod
    def topic_frame(): pass
    #@_abstractmethod
    #def total_frame(): pass
    
_SYS_IS_WIN = _system() in ["Windows","windows","WINDOWS"]

if _SYS_IS_WIN: 
    from ._win import * # import the core implementation
    _TOS_DataBlock.register( TOS_DataBlock ) # register as virtual subclass

_virtual_hub = None
_virtual_hub_addr = None
_virtual_admin = False
_virtual_admin_sock = None

_vCREATE     = 'CREATE'
_vCALL       = 'CALL'
_vDESTROY    = 'DESTROY'
_vFAILURE    = 'FAILURE'
_vSUCCESS    = 'SUCCESS'
_vSUCCESS_NT = 'SUCCESS_NT'
_vCONN_BLOCK = 'CONN_BLOCK'
_vCONN_ADMIN = 'CONN_ADMIN'
_vALLOWED_ADMIN = ( 'init','connect','connected','clean_up',
                    'get_block_limit', 'set_block_limit',
                    'get_block_count', 'type_bits', 'type_string' )
_vTYPES = {'i':int,'s':str,'b':bool,'d':lambda d: d}
_vDELIM = b'\x7E' ## !! _vDELIM MUST NOT HAVE THE SAME VALUE AS _vEEXOR !! ##
_vESC = b'\x7D'
_vDEXOR = chr(ord(_vDELIM) ^ ord(_vESC)).encode()
_vEEXOR = chr(ord(_vESC) ^ ord(_vESC)).encode() # 0

# deal with bad vals coming into virtual layer ( see vinit )

# move to _tosdb
_NTUP_TAG_ATTR = "_dont_worry_about_why_this_attribute_has_a_weird_name_"

def vinit(dllpath = None, root = "C:\\"):
    """ Initialize the underlying tos-databridge Windows DLL

    dllpath: string of the exact path of the DLL
    root: string of the directory to start walking/searching to find the DLL
    """
    return _admin_call( 'init', ('s' if dllpath else 'd',dllpath), ('s',root) )     

def vconnect():
    """ Attempts to connect the underlying Windows Library/Service """
    return _admin_call( 'connect' )

def vconnected():
    """ True if an active connection to the Library/Service exists """
    return _admin_call( 'connected' )
                
def vclean_up():
    """ Clean up shared resources. ( !! ON THE WINDOWS SIDE !! ) """
    _admin_call( 'clean_up' )

def vget_block_limit():
    """ Returns the block limit of C/C++ RawDataBlock factory """
    return _admin_call( 'get_block_limit' )

def vset_block_limit( new_limit ):
    """ Changes the block limit of C/C++ RawDataBlock factory """
    _admin_call( 'set_block_limit', ('i',new_limit) )

def vget_block_count():
    """ Returns the count of current instantiated blocks """
    return _admin_call( 'get_block_count' )

def vtype_bits( topic ):
    """ Returns the type bits for a particular 'topic'

    topic: string representing a TOS data field('LAST','ASK', etc)
    returns -> value that can be logical &'d with type bit contstants 
    ( ex. QUAD_BIT )
    """    
    return _admin_call( 'type_bits', ('s',topic) )

def vtype_string( topic ):
    """ Returns a platform-dependent string of the type of a particular 'topic'

    topic: string representing a TOS data field('LAST','ASK', etc)
    """
    return _admin_call( 'type_string', ('s',topic) )


## what happens to these sockets when we exit the client side ??
def admin_init( address, poll_interval=DEF_TIMEOUT ):
    global _virtual_hub_addr, _virtual_admin_sock
    if _virtual_admin_sock:
        raise TOS_DataBlock("virtual admin socket already exists")
    _check_address( address )
    _virtual_hub_addr = address
    _virtual_admin_sock = _socket.socket()
    _virtual_admin_sock.settimeout( poll_interval / 1000 )
    _virtual_admin_sock.connect( address )
    _vcall( _pack_msg(_vCONN_ADMIN), _virtual_admin_sock, _virtual_hub_addr )

def _admin_call( method, *arg_buffer ):
    if not _virtual_admin_sock:
        raise TOSDB_VirtualizationError("no admin socket, call admin_init")
    a =  (method,) + ( (_pickle.dumps(arg_buffer),) if arg_buffer else () )    
    ret_b = _vcall( _pack_msg( *a ), _virtual_admin_sock, _virtual_hub_addr ) 
    args = _unpack_msg( ret_b )
    print("return", args)
    status = args[0].decode()
    if status == _vFAILURE:
        #
        # need to make the error/failure return more robust
        # more info on what happened
        #
        raise TOSDB_VirtualizationError( "failure status returned",                                       
                                         "method: " + str(method),
                                         "arg_buffer: " + str(arg_buffer) )    
  
    elif len(args) > 1:
        return _pickle.loads( args[1] )
  

class VTOS_DataBlock:
    """ The main object for storing TOS data. (VIRTUAL)   

    address: the adddress of the actual implementation ('XXX.XXX.XXX.XXX',port)
    size: how much historical data to save
    date_time: should block include date-time stamp with each data-point?
    timeout: how long to wait for responses from TOS-DDE server 

    Please review the attached README.html for details.
    """    
    def __init__( self, address, size = 1000, date_time = False,
                  timeout = DEF_TIMEOUT ):       
        _check_address( address )
        self._hub_addr = address
        self._my_sock = _socket.socket()
        self._my_sock.settimeout( timeout / 1000 )
        self._my_sock.connect( address )
        _vcall( _pack_msg( _vCONN_BLOCK ), self._my_sock, self._hub_addr)
        # in case __del__ is called during socket op
        if not self._call( _vCREATE, '__init__', ('i',size),
                           ('b',date_time), ('i',timeout) ):
            raise TOSDB_VirtualizationError("CREATE failed")
        
    def __del__( self ):
        try:
            self._call( _vDESTROY )
            self._my_sock.close()
        except TOSDB_Error as e:
            print( "Error closing virtual block:", repr(e) )          

    def __str__( self ):
        s = self._call( _vCALL, '__str__' )    
        return s if s else ''
    
    def info(self):
        """ Returns a more readable dict of info about the underlying block """
        return self._call( _vCALL, 'info' )
    
    def get_block_size( self ):
        """ Returns the amount of historical data stored in the block """
        return self._call( _vCALL, 'get_block_size' )
    
    def set_block_size( self, sz ):
        """ Changes the amount of historical data stored in the block """
        self._call( _vCALL, 'set_block_size', ('i',sz) )
            
    def stream_occupancy( self, item, topic ):
        return self._call( _vCALL, 'stream_occupancy', ('s',item), ('s',topic))
    
    def items( self, str_max = MAX_STR_SZ ):
        """ Returns the items currently in the block (and not pre-cached).
        
        str_max: the maximum length of item strings returned
        returns -> list of strings 
        """
        return self._call( _vCALL, 'items', ('i',str_max) )          
              
    def topics( self,  str_max = MAX_STR_SZ ):
        """ Returns the topics currently in the block (and not pre-cached).
        
        str_max: the maximum length of topic strings returned  
        returns -> list of strings 
        """
        return self._call( _vCALL, 'topics', ('i',str_max) ) 
      
    
    def add_items( self, *items ):
        """ Add items ( ex. 'IBM', 'SPY' ) to the block.

        NOTE: if there are no topics currently in the block, these items will 
        be pre-cached and appear not to exist, until a valid topic is added.

        *items: any numer of item strings
        """               
        self._call( _vCALL, 'add_items', *zip('s'*len(items), items) )
       

    def add_topics( self, *topics ):
        """ Add topics ( ex. 'LAST', 'ASK' ) to the block.

        NOTE: if there are no items currently in the block, these topics will 
        be pre-cached and appear not to exist, until a valid item is added.

        *topics: any numer of topic strings
        """               
        self._call( _vCALL, 'add_topics', *zip('s'*len(topics), topics) )

    def remove_items( self, *items ):
        """ Remove items ( ex. 'IBM', 'SPY' ) from the block.

        NOTE: if there this call removes all items from the block the 
        remaining topics will be pre-cached and appear not to exist, until 
        a valid item is re-added.

        *items: any numer of item strings
        """
        self._call( _vCALL, 'remove_items', *zip('s'*len(items), items) )

    def remove_topics( self, *topics ):
        """ Remove topics ( ex. 'LAST', 'ASK' ) from the block.

        NOTE: if there this call removes all topics from the block the 
        remaining items will be pre-cached and appear not to exist, until 
        a valid topic is re-added.

        *topics: any numer of topic strings
        """
        self._call( _vCALL, 'remove_topics', *zip('s'*len(topics), topics) )
        
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
        return self._call( _vCALL, 'get', ('s',item), ('s',topic),
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
        return self._call( _vCALL, 'stream_snapshot', ('s',item),
                           ('s',topic), ('b',date_time), ('i',end), ('i',beg),
                           ('b',smart_size), ('i', data_str_max) )

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
        date_time: (True/False) attempt to retrieve a TOS_DateTime object                      
        beg: index of most recent data-point ( beginning of the snapshot )        
        margin_of_safety: (True/False) error margin for async stream growth
        throw_if_data_loss: (True/False) how to handle error states (see above)
        data_str_max: the maximum length of string data returned

        if beg > internal marker value: returns -> None        
        if date_time is True: returns-> list of 2tuple
        else: returns -> list              
        """
        return self._call( _vCALL, 'stream_snapshot_from_marker', ('s',item),
                          ('s',topic), ('b',date_time), ('i',beg),
                          ('i',margin_of_safety), ('b',throw_if_data_lost),
                          ('i', data_str_max) )
    
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
        return self._call( _vCALL, 'item_frame', ('s',topic), ('b',date_time),
                           ('b',labels), ('i', data_str_max),
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
        return self._call( _vCALL, 'topic_frame', ('s',item), ('b',date_time),
                           ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )
##
##  !! need to find a way to pickle an iterable of namedtuples !!
##
##    def total_frame( self, date_time = False, labels = True, 
##                     data_str_max = STR_DATA_SZ,
##                     label_str_max = MAX_STR_SZ ):
##        """ Return a matrix of the most recent values:  
##        
##        date_time: (True/False) attempt to retrieve a TOS_DateTime object        
##        labels: (True/False) pull the item and topic labels with the values 
##        data_str_max: the maximum length of string data returned
##        label_str_max: the maximum length of label strings returned
##        
##        if labels and date_time are True: returns-> dict of namedtuple of 2tuple
##        if labels is True: returns -> dict of namedtuple
##        if date_time is True: returns -> list of 2tuple
##        else returns-> list
##        """
##        return self._call( _vCALL, 'total_frame', ('b',date_time),
##                           ('b',labels), ('i', data_str_max),
##                           ('i', label_str_max) )
   
    def _call( self, virt_type, method='', *arg_buffer ):
            
        if virt_type == _vCREATE:
            req_b = _pack_msg( _vCREATE, _pickle.dumps(arg_buffer) )
        elif virt_type == _vCALL:
            a = (_vCALL, method) \
                + ( (_pickle.dumps(arg_buffer),) if arg_buffer else () )
            req_b = _pack_msg( *a )           
        elif virt_type == _vDESTROY:
            req_b = _pack_msg( _vDESTROY )
        else:
            raise TOSDB_VirtualizationError( "invalid virt_type" )
        
        ret_b = _vcall( req_b, self._my_sock, self._hub_addr )
        print('ret_b',ret_b)
        args = _unpack_msg( ret_b )                 
        status = args[0].decode()
        print(args)
        if status == _vFAILURE:
            #
            # need to make the error/failure return more robust
            # more info on what happened
            #
            raise TOSDB_VirtualizationError( "failure status returned",
                                             "virt_type: " + str(virt_type),
                                             "method: " + str(method),
                                             "arg_buffer: " + str(arg_buffer) )
        
        if virt_type in [_vCREATE, _vDESTROY]:
            return True
        elif virt_type == _vCALL and len(args) > 1:
            if status == _vSUCCESS_NT:
                return _loadnamedtuple( args[1] )
            else:
                return _pickle.loads( args[1] )

      
_TOS_DataBlock.register( VTOS_DataBlock )

   
def enable_virtualization( address, poll_interval=DEF_TIMEOUT ):
    global _virtual_hub  
       
    class _VTOS_BlockServer( _Thread ):
        
        def __init__( self, conn, poll_interval, stop_callback):
            super().__init__(daemon=True)
            self._my_sock = conn[0]
            self._cli_addr = conn[1]
            self._poll_interval = poll_interval
            self._my_sock.settimeout( poll_interval / 1000 )
            self._rflag = False
            self._stop_callback = stop_callback
            
        def stop(self):
            self._rflag = False                      

        def run(self):
            self._rflag = True            
            while self._rflag:                                       
                try:                   
                    dat = _recv_tcp( self._my_sock )                  
                    if not dat:                        
                        break                
                    args = _unpack_msg( dat )
                    print('run',args)
                    msg_t = args[0].decode()
                    r = None
                    if msg_t == _vCREATE:
                        r = self._handle_create( args )                    
                    elif msg_t == _vCALL:
                        r = self._handle_call( args )                      
                    elif msg_t == _vDESTROY:
                        r = self._handle_destroy( args )                   
                    _send_tcp( self._my_sock, r if r else _pack_msg(_vFAILURE))                 
                except _socket.timeout as e:                
                    pass
                except Exception as e:                   
                    print( "Fatal Error in _VTOS_BlockServer (TERMINATING) : ",
                           repr(e), ":", msg_t, " from ", str(self._cli_addr),
                           file=_stderr )
                    self._rflag = False
            self._stop_callback( self )
        
        def _handle_create( self, args ):
            try:
                upargs = _pickle.loads(args[1])                
                cargs = [ _vTYPES[t](v) for t,v in upargs ]
                print( str(cargs) )
                self._blk = TOS_DataBlock( *cargs )
                print( str(self._blk) )
                return _pack_msg( _vSUCCESS )
            except:
                pass

        def _handle_destroy( self, args ):
            try:                
                del self._blk
                return _pack_msg( _vSUCCESS )
            except:
                pass                        
            
        def _handle_call( self, args ):
            print("DEBUG",args)
            try:
                meth = getattr(self._blk, args[1].decode())
                upargs = _pickle.loads( args[2]) if len(args) > 2 else ()
                cargs = [ _vTYPES[t](v) for t,v in upargs ]
                print( cargs )
                ret = meth(*cargs)                
                if ret is None: # None is still a success
                    return _pack_msg( _vSUCCESS )                
                elif hasattr(ret,_NTUP_TAG_ATTR): #our special namedtuple tag
                    return _pack_msg( _vSUCCESS_NT, _dumpnamedtuple(ret) )
                else:
                    return _pack_msg( _vSUCCESS, _pickle.dumps(ret) )   
            except:
                pass

    class _VTOS_AdminServer( _Thread ):
        
        def __init__( self, conn, poll_interval):
            super().__init__(daemon=True)
            self._my_sock = conn[0]
            self._cli_addr = conn[1]
            self._poll_interval = poll_interval
            self._my_sock.settimeout( poll_interval / 1000 )
            self._rflag = False
            self._globals = globals()
            
        def stop(self):
            self._rflag = False                      

        def run(self):
            self._rflag = True            
            while self._rflag:                                       
                try:
                    print('in run')
                    dat = _recv_tcp( self._my_sock )                  
                    if not dat:                        
                        break                    
                    args = _unpack_msg( dat )
                    print('run', args)
                    rmsg = _pack_msg(_vFAILURE)
                    try:
                        print('run', 'going into meth')
                        meth = self._globals[args[0].decode()]
                        print('run', 'coming out of meth')
                        upargs = _pickle.loads(args[1]) if len(args) > 1 else ()
                        cargs = [ _vTYPES[t](v) for t,v in upargs ]
                        print('cargs', str(cargs) )
                        r = meth(*cargs)
                        print( 'returned', str(r))
                        if r is None:
                            rmsg = _pack_msg( _vSUCCESS )
                        else:
                            rmsg = _pack_msg( _vSUCCESS, _pickle.dumps(r))                          
                    except BaseException as e:
                        print( "exc caught in run", repr(e) )
                        rmsg = _pack_msg(_vFAILURE)
                    print( 'run-out', args )
                    print( 'run-out', rmsg )
                    _send_tcp( self._my_sock, rmsg)                 
                except _socket.timeout as e:                
                    pass
                except Exception as e:                   
                    print( "Fatal Error in _VTOS_AdminServer (TERMINATING) : ",
                           repr(e), ":", msg_t, " from ", str(self._cli_addr),
                           file=_stderr )
                    self._rflag = False    

    class _VTOS_Hub( _Thread ):

        def __init__( self, address, poll_interval):
            super().__init__(daemon=True)
            _check_address( address )
            self._my_addr = address     
            self._rflag = False
            self._poll_interval = poll_interval
            self._my_sock = _socket.socket()
            self._my_sock.settimeout( poll_interval / 1000 )
            self._my_sock.bind( address )
            self._my_sock.listen(0)
            self._virtual_data_servers = set()
            self._virtual_admin_server = None
          
        def stop(self):            
            self._rflag = False         

        def run(self):
            self._rflag = True            
            while self._rflag:                
                try:                  
                    conn = self._my_sock.accept()                
                    dat = _recv_tcp( conn[0] )                   
                    dat = _unpack_msg( dat )[0].decode()                  
                    if dat == _vCONN_BLOCK:
                        print("IN BLOCK")
                        vserv = _VTOS_BlockServer( conn, self._poll_interval,
                                            self._virtual_data_servers.discard )
                        self._virtual_data_servers.add( vserv )
                        vserv.start()
                    elif dat == _vCONN_ADMIN:
                        print("IN ADMIN")
                        if self._virtual_admin_server:
                            self._virtual_admin_server.stop()                        
                        self._virtual_admin_server = _VTOS_AdminServer( conn,
                                                            self._poll_interval )
                        self._virtual_admin_server.start()
                    else:
                        raise TOSDB_VirtualizationError( "connection init msg "
                                                         "must be _vCONN_BLOCK "
                                                         "or _vCONN_ADMIN" )                                                        
                except _socket.timeout as e:                   
                    continue                
                except Exception as e: # anything else... shutdown the hub
                    print("_VTOS_Hub killed: ", repr(e), file=_stderr)
                    break                
            while self._virtual_data_servers:
                self._virtual_data_servers.pop().stop()
            if self._virtual_admin_server:
                self._virtual_admin_server.stop()
            
            
    try:
        if _virtual_hub is None:
            _virtual_hub = _VTOS_Hub( address, poll_interval )
            _virtual_hub.start()      
    except Exception as e:
        raise TOSDB_VirtualizationError( "(enable) virtualization error", e )

def disable_virtualization():
    global _virtual_hub
    try:
        if _virtual_hub is not None:
           _virtual_hub.stop()
           _virtual_hub = None        
    except Exception as e:
        raise TOSDB_VirtualizationError( "(disable) virtualization error", e )    

#currently uncessary as server is daemon thread, but may be useful later
_on_exit( disable_virtualization )

def _vcall( msg, my_sock, hub_addr ):
    try:
        print(msg)
        _send_tcp( my_sock, msg )       
        try:
            ret_b = _recv_tcp( my_sock )
        except _socket.timeout as e:
            raise TOSDB_VirtualizationError( "socket timed out",
                                             "VTOS_DataBlock._call" )
        print('v_call,ret_b',ret_b)
        return ret_b
    except ConnectionResetError:         
        try:
            self._my_sock.connect( hub_addr)
            _call( msg, my_sock, hub_addr)
        except:
            raise TOSDB_VirtualizationError("failed to reconnect to hub",
                                            "VTOS_DataBlock._call" )      
    except Exception as e:
        TOSDB_VirtualizationError( repr(e) )
                                              
def _dumpnamedtuple( nt ):
    n = type(nt).__name__
    od = nt.__dict__
    return _pickle.dumps( (n,tuple(od.keys()),tuple(od.values())) )

def _loadnamedtuple( nt):
    name,keys,vals = _pickle.loads( nt )
    ty = _namedtuple( name, keys )
    return ty( *vals )

def _recv_tcp( sock ):
    packedlen = _recvall_tcp( sock, 8 )
    if not packedlen:
        return None
    dlen = _struct.unpack( 'Q', packedlen)[0]
    return _recvall_tcp( sock, dlen )

def _recvall_tcp( sock, n ):
    data = b''
    while len(data) < n:
        p = sock.recv( n - len(data) )
        if not p:
            return None
        data += p
    return data

def _send_tcp( sock, data ):
    dl = len(data)
    msg = _struct.pack('Q',len(data)) + data
    return sock.sendall(msg)

def _pack_msg( *parts ):   
    def _escape_part( part ):
        enc = part.encode() if type(part) is not bytes else part         
        #escape the escape FIRST
        esc1 = _sub(_vESC, _vESC + _vEEXOR, enc )
        #escape the delim SECOND     
        return _sub(_vDELIM, _vESC + _vDEXOR, esc1)
    return _vDELIM.join( [ _escape_part(p) for p in parts ] )

##    tot = b''
##    def _escape_part( part ):
##        enc = part.encode() if type(part) is not bytes else part         
##        #escape the escape FIRST
##        esc1 = _sub(_vESC, _vESC + _vEEXOR, enc )
##        #escape the delim SECOND     
##        return _sub(_vDELIM, _vESC + _vDEXOR, esc1)
##    for p in parts:        
##        tot += ( _escape_part(p) + _vDELIM )
##    body = tot.rstrip(_vDELIM)
##    return _escape_part(_struct.pack('Q', len(body))) + _vDELIM + body

def _unpack_msg( msg ):    
    def _unescape_part( part ):
        #unescape the delim FIRST
        unesc1 = _sub( _vESC + _vDEXOR, _vDELIM, part )
        #unescape the escape SECOND
        return _sub( _vESC + _vEEXOR, _vESC, unesc1 )
    if not msg:
        return msg
    return [ _unescape_part(p) for p in msg.strip().split(_vDELIM) ]
    
##    def _unescape_part( part ):
##        #unescape the delim FIRST
##        unesc1 = _sub( _vESC + _vDEXOR, _vDELIM, part )
##        #unescape the escape SECOND
##        return _sub( _vESC + _vEEXOR, _vESC, unesc1 )
##    raw_head, raw_body = msg.split(_vDELIM,1)
##    head = _unescape_part(raw_head)
##    if _struct.unpack('Q',head)[0] != len(raw_body):
##        raise TOSDB_UDPMessageError("invalid msg length")
##    return [ _unescape_part(p) for p in raw_body.strip().split(_vDELIM) ]

def _check_address( addr ):
    # make this more thorough
    if not ( type(addr) is tuple and len(addr) == 2 and
             type(addr[0]) is str and type(addr[1]) is int ):
        raise TOSDB_TypeError("address must be of type (str,int)")

if __name__ == "__main__" and _SYS_IS_WIN:
    parser = _ArgumentParser()
    parser.add_argument( "--root", 
                         help = "root directory to search for the library" )
    parser.add_argument( "--path", help="the exact path of the library" )
    parser.add_argument( "-n", "--noinit", 
                         help="don't initialize the library automatically",
                         action="store_true" )
    args = parser.parse_args()   
    if not args.noinit:       
        if args.path:
            init( dllpath = args.path )
        elif args.root:
            init( root = args.root )
        else:
            print( "*WARNING* by not supplying --root, --path, or " +
                   "--noinit( -n ) arguments you are opting for a default " +
                   "search root of 'C:\\'. This will attempt to search " +
                   "the ENTIRE disk/drive for the tos-databridge library. " +
                   "It's recommended you restart the program with the " +
                   "library path (after --path) or a narrower directory " +
                   "root (after --root)." )                
            if input( "Do you want to continue anyway? (y/n): ") == 'y':
                init()
            else:
                print("- init(root='C:\\') aborted")
        


                
