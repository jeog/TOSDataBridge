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
from platform import system as _system
from abc import ABCMeta as _ABCMeta, abstractmethod as _abstractmethod
import socket as _socket
import pickle as _pickle

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
    def item_frame(): pass
    @_abstractmethod
    def topic_frame(): pass
    @_abstractmethod
    def total_frame(): pass
    
_isWinSys = _system() in ["Windows","windows","WINDOWS"]

if _isWinSys: 
    from ._win import * # import the core implementation
    _TOS_DataBlock.register( TOS_DataBlock ) # and register as virtual subclass

_virtual_blocks = dict() 
_virtual_data_server = None
_virtual_CREATE = '1'
_virtual_CALL = '2'
_virtual_DESTROY = '3'
_virtual_FAIL = '4'
_virtual_SUCESS = '5'
_virtual_SUCCESS_NT = '6'
_virtual_MAX_REQ_SZ = 512 # arbitrary for now
                           # need to handle large return values i.e a million vals
_virtual_VAR_TYPES = {'i':int,'s':str,'b':bool}   

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
            raise TOSDB_VirtError( "invalid virt_type" )        
        #c = self._my_sock.sendto( req_b, self._my_addr )
        c = _send_udp( self._my_sock, self._my_addr, req_b, _virtual_MAX_REQ_SZ )
        if c == 0:
            raise TOSDB_VirtCommError("_call -> sendto() failed")
        else:
            #ret_b = self._my_sock.recv( 1000 )
            ret_b = _recv_udp( self._my_sock, _virtual_MAX_REQ_SZ )
            print(ret_b)
            # hold off on decode til we un-pickle 
            args = ret_b.strip().split(b' ')
            status = args[0].decode()
            if status == _virtual_FAIL:
                # need to make the error/failure return more robust
                # more info on what happened
                raise TOSDB_VirtError( "failure status returned: ",
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

_TOS_DataBlock.register( VTOS_DataBlock )

###
###
###            
    
def enable_virtualization( address ):
    global _virtual_data_server
    
    def _create_callback( addr, *args ):
        global _virtual_blocks
        print("DEBUG", "in _create_callback")
        blk = None
        try:
            print( *args )
            blk = TOS_DataBlock( *args ) 
            _virtual_blocks[blk._name] = (blk, addr)
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
            blk = _virtual_blocks[name][0]         
            meth = getattr(blk, meth )          
            ret = meth( *args )          
            return ret if ret else True
        except Exception as e:
            print( str(e) )
            return False
       
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
            # use settimeout and poll on return
            while self._rflag:           
                #dat, addr = self._my_sock.recvfrom( _virtual_MAX_REQ_SZ )            
                dat, addr = _recv_udp( self._my_sock, _virtual_MAX_REQ_SZ )            
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
                    ret = self._create_callback( addr, *cargs )
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
                            if hasattr(ret,_NTUP_TAG_ATTR):
                                ret_b = _virtual_SUCCESS_NT.encode() \
                                        + b' ' + _dumpnamedtuple(ret)
                            else:
                                ret_b += b' ' + _pickle.dumps(ret)                        
                elif msg_t == _virtual_DESTROY:
                    if self._virtual_destroy_callback( args[1].decode() ):
                        ret_b = _virtual_SUCESS.encode()                           
                
                #self._my_sock.sendto( ret_b, addr )
                _send_udp( self._my_sock, addr, ret_b, _virtual_MAX_REQ_SZ )

    try:
        _virtual_data_server = _VTOS_DataServer( address, _create_callback,
                                                 _destroy_callback,
                                                 _call_callback )
        _virtual_data_server.start()      
    except Exception as e:
        raise TOSDB_VirtError( "(enable) virtualization error", e )

def _recv_udp( sock, dgram_sz ):
    tot = b''
    r, addr = sock.recvfrom( dgram_sz )
    while len(r) == dgram_sz:
        tot += r
        r, addr = sock.recvfrom( dgram_sz )
    tot += r
    return (tot,addr)

def _send_udp( sock, addr, dgram_sz, data ):
    dl = len(data)
    snt = 0
    for i in range( 0, dl, dgram_sz ):
        snt += sock.sendto( data[i:i+dgram_sz], addr )          
    if dl % dgram_sz == 0:
        sock.sendto( b'', addr)
    return snt

def disable_virtualization():
    try:
        if _virtual_data_server is not None:
           _virtual_data_server.stop()
           _virtual_data_server = None
           _virtual_blocks.clear()
    except Exception as e:
        raise TOSDB_VirtError( "(disable) virtualization error", e )    
    
def _dumpnamedtuple( nt ):
    n = type(nt).__name__
    od = nt.__dict__
    return _pickle.dumps( (n,tuple(od.keys()),tuple(od.values())) )

def _loadnamedtuple( nt):
    name,keys,vals = _pickle.loads( nt )
    ty = _namedtuple( name, keys )
    return ty( *vals )


if __name__ == "__main__" and _isWinSys:
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
        


                
