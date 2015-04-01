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

"""tosdb_virtual.py :  Provides a thin virtualization layer over UDP so users
on other (particularly non-Windows) systems can access the same 'block'
interface.

Method calls are serialized and sent over a phsyical/virtual network to a
windows machine running the core implemenataion, passing returned values back.
From the users perspective VTOS_DataBlock is the same as TOS_DataBlock.

Those running Windows in a virtual machine can abstract away nearly
all the (unfortunately necessary) non-portable parts of the core
implementation with limited bandwith issues.*

* obviously the added virtualization layer will affect performance

Please refer to README.html for an explanation of the underlying library.
Please refer to PythonTutorial.html in /python/docs for a step-by-step 
walk-through.

"""

### deal with global consts better, so we can import directly to here

from tosdb_datetime import TOS_DateTime
from threading import Thread
import socket
import pickle

virtual_CREATE = '1'
virtual_CALL = '2'
virtual_DESTROY = '3'
virtual_SUCCESS = '4'
virtual_FAIL = '5'

virtual_MAX_REQ_SIZE = 1000 # arbitrary for now
virtual_TYPE_ATTR = {'i':int,'s':str,'b':bool}

DUMMY_DEF_TIMEOUT = 2000
DUMMY_MAX_STR_SZ = 0xFF
DUMMY_STR_DATA_SZ = 0xFF


class TOSDB_Error(Exception):
    """ Base exception for tosdb.py """    
    def __init__(self,  *messages ):        
        Exception( *messages )

class VTOS_DataServer( Thread ):

    def __init__(self, address, create_callback, destroy_callback, call_callback):
        super().__init__()
        self._my_addr = address
        self._create_callback = create_callback
        self._destroy_callback = destroy_callback
        self._call_callback = call_callback
        self._rflag = False
        self._my_sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        self._my_sock.bind( address )

    def stop(self):
        self._rflag = False
        self._my_sock.close()

    def run(self):
        self._rflag = True
        # need a more active way to break out, poll more often !
        while self._rflag:           
            dat, addr = self._my_sock.recvfrom( virtual_MAX_REQ_SIZE )        
            if dat:            
                args = dat.decode().strip().split(' ')
                ret_b = virtual_FAIL.encode()
                if args[0] == virtual_CREATE:
                    upargs = pickle.loads(args[1])
                    cargs = [ virtual_TYPE_ATTR[t](v) for t,v in upargs ]
                    ret = self._create_callback( *cargs )
                    if ret:
                        ret_b = (virtual_SUCCESS + ' ' + ret).encode()                                                                      
                elif args[0] == virtual_CALL:
                    has_args = len(args) > 3
                    if has_args:
                        upargs = pickle.loads(args[3])
                        cargs = [ virtual_TYPE_ATTR[t](v) for t,v in upargs ]                    
                        ret = self._call_callback( args[1], args[2], *cargs)
                    else:
                        ret = self._call_callback( args[1], args[2] )                        
                    if ret:
                        ret_b = virtual_SUCCESS.encode()
                        if type(ret) != bool:                        
                            ret_b += b' ' + pickle.dumps(ret)                        
                elif args[0] == virtual_DESTROY:
                    if self._virtual_destroy_callback( args[1] ):
                        ret_b = virtual_SUCCESS.encode()                     
              
                if not ret_b:
                    raise TOSDB_Error("invalid return value to virtual block")
                
                virtual_sock.sendto( ret_b, addr ) 
            
                 
       
class VTOS_DataBlock:
    """ The main object for storing TOS data. (VIRTUAL)   

    address: the adddress of the actual implementation
    size: how much historical data to save
    date_time: should block include date-time stamp with each data-point?
    timeout: how long to wait for responses from TOS-DDE server 

    Please review the attached README.html for details.
    """

    def __init__( self, address, size = 1000, date_time = False,
                  timeout = DUMMY_DEF_TIMEOUT ):
        self._my_addr = address
        self._my_sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        self._call( virtual_CREATE, '__init__', ( ('i',size), ('b',date_time),
                                                  ('i',timeout) ) )
        
    def __del__( self ):
        try:
            if self._call( virtual_DESTROY ):
                self._my_sock.close()
        except TOSDB_Error as e:
            print( e.args[0] )          

    def __str__( self ):
        return self._call( virtual_CALL, '__str__' )    

    #
    # probably should hide these 'private' methods, but keep for convenience
    #
    def _valid_item( self, item ):
        try:
            self._call( virtual_CALL, '_valid_item', ('s',item) )
        except TOSDB_Error as e:
            raise TOSDB_Error( item + " not found")

    def _valid_topic( self, topic ):
        try:
            self._call( virtual_CALL, '_valid_topic', ('s',topic) )
        except TOSDB_Error as e:
            raise TOSDB_Error( topic + " not found")

    def _item_count( self ):
        return self._call( virtual_CALL, '_item_count' )

    def _topic_count( self ):
        return self._call( virtual_CALL, '_topic_count' )

    def info(self):
        """ Returns a more readable dict of info about the underlying block """
        return self._call( virtual_CALL, 'info' )
    
    def get_block_size( self ):
        """ Returns the amount of historical data stored in the block """
        return self._call( virtual_CALL, 'get_block_size' )
    
    def set_block_size( self, sz ):
        """ Changes the amount of historical data stored in the block """
        self._call( virtual_CALL, 'set_block_size', ('i',sz) )
            
    def stream_occupancy( self, item, topic ):
        return self._call( virtual_CALL, 'stream_occupancy', ('s',item),
                           ('s',topic) )
    
    def items( self, str_max = DUMMY_MAX_STR_SZ ):
        """ Returns the items currently in the block (and not pre-cached).
        
        str_max: the maximum length of item strings returned
        returns -> list of strings 
        """
        return self._call( virtual_CALL, 'items', ('i',str_max) )          
              
    def topics( self,  str_max = DUMMY_MAX_STR_SZ ):
        """ Returns the topics currently in the block (and not pre-cached).
        
        str_max: the maximum length of topic strings returned  
        returns -> list of strings 
        """
        return self._call( virtual_CALL, 'topics', ('i',str_max) ) 
      
    
    def add_items( self, *items ):
        """ Add items ( ex. 'IBM', 'SPY' ) to the block.

        NOTE: if there are no topics currently in the block, these items will 
        be pre-cached and appear not to exist, until a valid topic is added.

        *items: any numer of item strings
        """               
        self._call( virtual_CALL, 'add_items', *zip('s'*len(items), items) )
       

    def add_topics( self, *topics ):
        """ Add topics ( ex. 'LAST', 'ASK' ) to the block.

        NOTE: if there are no items currently in the block, these topics will 
        be pre-cached and appear not to exist, until a valid item is added.

        *topics: any numer of topic strings
        """               
        self._call( virtual_CALL, 'add_topics', *zip('s'*len(topics), topics) )

    def remove_items( self, *items ):
        """ Remove items ( ex. 'IBM', 'SPY' ) from the block.

        NOTE: if there this call removes all items from the block the 
        remaining topics will be pre-cached and appear not to exist, until 
        a valid item is re-added.

        *items: any numer of item strings
        """
        self._call( virtual_CALL, 'remove_items', *zip('s'*len(items), items) )

    def remove_topics( self, *topics ):
        """ Remove topics ( ex. 'LAST', 'ASK' ) from the block.

        NOTE: if there this call removes all topics from the block the 
        remaining items will be pre-cached and appear not to exist, until 
        a valid topic is re-added.

        *topics: any numer of topic strings
        """
        self._call( virtual_CALL, 'remove_topics', *zip('s'*len(topics), topics) )
        
    def get( self, item, topic, date_time = False, indx = 0, 
             check_indx = True, data_str_max = DUMMY_STR_DATA_SZ ):
        """ Return a single data-point from the data-stream
        
        item: any item string in the block
        topic: any topic string in the block
        date_time: (True/False) attempt to retrieve a TOS_DateTime object   
        indx: index of data-points [0 to block_size), [-block_size to -1]
        check_indx: throw if datum doesn't exist at that particular index
        data_str_max: the maximum size of string data returned
        """
        return self._call( virtual_CALL, 'get', ('s',item), ('s',topic),
                           ('b',date_time), ('i',indx), ('b',check_indx),
                           ('i', data_str_max) )

    def stream_snapshot( self, item, topic, date_time = False, 
                         end = -1, beg = 0, smart_size = True, 
                         data_str_max = DUMMY_STR_DATA_SZ ):
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
        return self._call( virtual_CALL, 'stream_snapshot', ('s',item),
                           ('s',topic), ('b',date_time), ('i',end), ('i',beg),
                           ('b',smart_size), ('i', data_str_max) )

    def item_frame( self, topic, date_time = False, labels = True, 
                    data_str_max = DUMMY_STR_DATA_SZ,
                    label_str_max = DUMMY_MAX_STR_SZ ):
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
        return self._call( virtual_CALL, 'item_frame', ('s',topic),
                           ('b',date_time), ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )   

    def topic_frame( self, item, date_time = False, labels = True, 
                     data_str_max = DUMMY_STR_DATA_SZ,
                     label_str_max = DUMMY_MAX_STR_SZ ):
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
        return self._call( virtual_CALL, 'topic_frame', ('s',item),
                           ('b',date_time), ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )

    def total_frame( self, date_time = False, labels = True, 
                     data_str_max = DUMMY_STR_DATA_SZ,
                     label_str_max = DUMMY_MAX_STR_SZ ):
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
        return self._call( virtual_CALL, 'total_frame', ('b',date_time),
                           ('b',labels), ('i', data_str_max),
                           ('i', label_str_max) )

    #
    ### 'client'-side virtualization layer implementation
    #
    def _call( self, virt_type, method='', *arg_buffer ):
        req_b = b''
        if virt_type == virtual_CREATE:
            req_b = virtual_CREATE.encode() + (b' ' + pickle.dumps(arg_buffer))
        elif virt_type == virtual_CALL:
            req_b = (virtual_CALL + ' ' + self._name + ' ' + method).encode()
            if arg_buffer:
                req_b += (b' ' + pickle.dumps(arg_buffer))
        elif virt_type == virtual_DESTROY:
            req_b = (virtual_CREATE + ' ' + self._name).encode()
        else:
            raise TOSDB_Error( "Type Error: virt_type" )        
        c = self._my_sock.sendto( req_b, self._my_addr )
        if c == 0:
            raise TOSDB_Error("_call -> sendto() failed")
        else:
            ret_b = self._my_sock.recv( 1000 )
            # hold off on decode til we un-pickle 
            args = ret_b.strip().split(' ')
            status = args[0].decode()
            if status == virtual_FAIL:
                raise TOSDB_Error( "underlying block returned failure status: " +
                                    args[1].decode(),
                                   "virt_type: " + str(virt_type),
                                   "method_or_name: " + str(method_or_name),
                                   "arg_buffer: " + str(arg_buffer) )
            else:
                if virt_type == virtual_CREATE:
                    return args[1].decode()
                elif virt_type == virtual_CALL and len(args) > 1:
                    return pickle.loads( args[1] )
                elif virt_type == virtual_DESTROY:
                    return True
                
                
