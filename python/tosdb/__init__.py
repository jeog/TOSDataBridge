# Copyright (C) 2014 Jonathon Ogden   < jeog.dev@gmail.com >
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
Please refer to PythonTutorial.html in /python/docs for a (currently out-dated)
step-by-step walk-through.

class TOSDB_DataBlock(windows only): very similar to the 'block' approach of the
underlying C library except the interface is explicitly object-oriented. It 
abstracts away the underlying type complexity, raising TOSDB_CLibError on 
internal library errors.

class VTOSDB_DataBlock: same interface as TOSDB_DataBlock except it utilizes a 
thin virtualization layer over TCP, passing serialized method calls to a windows 
machine running the core implemenataion.

ABC _TOSDB_DataBlock: abstract base class of TOSDB_DataBlock and VTOSDB_DataBlock

                  * * *   

In order to create a virtual block the 'local' windows implementation must
do everything it would normally do to instantiate a TOSDB_DataBlock. It then 
calls enable_virtualization() with an address tuple (addr,port) that the virtual
hub / servers (_VTOS_Hub / _VTOS_BlockServer) will use to listen for 'remote'
virtual blocks. 

admin_init() must be called before virtual admin calls (e.g vinit(), vconnect())

tosdb.py will attempt to load the non-portable _win.py if on windows. The 
following are the most important calls that control the underlying DLL:

  init() / vinit() : initializes the underlying library

  connect() / vconnect() : connects to the library (init attemps this for you)

  clean_up() / vclean_up() : *** IMPORTANT *** de-allocates shared resources of 
  the underlying library and Service. We attempt to clean up resources 
  automatically on exit but in certain cases it's not guaranteed to happen. 
  It's HIGHLY RECOMMENDED YOU CALL THIS FUNCTION before exiting.
"""

from ._common import * 
from ._common import _DateTimeStamp, _TOSDB_DataBlock, _type_switch

from collections import namedtuple as _namedtuple
from threading import Thread as _Thread, Lock as _Lock
from functools import partial as _partial
from platform import system as _system
from sys import stderr as _stderr
from re import sub as _sub
from atexit import register as _on_exit
import struct as _struct
import socket as _socket
import pickle as _pickle
  
_SYS_IS_WIN = _system() in ["Windows","windows","WINDOWS"]

if _SYS_IS_WIN: 
  from ._win import * # import the core implementation
  
_virtual_hub = None
_virtual_hub_addr = None
_virtual_admin = False
_virtual_admin_sock = None # <- what happens when we exit the client side ?

_vCREATE     = 'CREATE'
_vCALL       = 'CALL'
_vDESTROY    = 'DESTROY'
_vFAILURE    = 'FAILURE'
_vFAIL_EXC   = '1'
_vSUCCESS    = 'SUCCESS'
_vSUCCESS_NT = 'SUCCESS_NT'
_vCONN_BLOCK = 'CONN_BLOCK'
_vCONN_ADMIN = 'CONN_ADMIN'

_vALLOWED_ADMIN = ('init','connect','connected','clean_up','get_block_limit', 
                   'set_block_limit','get_block_count','type_bits','type_string')

## !! _vDELIM MUST NOT HAVE THE SAME VALUE AS _vEEXOR !! ##
_vDELIM = b'\x7E' 
_vESC = b'\x7D'
_vDEXOR = chr(ord(_vDELIM) ^ ord(_vESC)).encode()
_vEEXOR = chr(ord(_vESC) ^ ord(_vESC)).encode() # 0


# NOTE: for time being preface virtual calls and objects with 'v' so
# we can load both on the windows side for easier debugging,

def vinit(dllpath=None, root="C:\\", bypass_check=False):
  """ Initialize the underlying tos-databridge Windows DLL

  dllpath: string of the exact path of the DLL
  root: string of the directory to start walking/searching to find the DLL
  """
  if not bypass_check and dllpath is None and root == "C:\\":
    if abort_init_after_warn():
      return
  return _admin_call('init', dllpath, root, True)

def vconnect():
  """ Attempts to connect the underlying Windows Library/Service """
  return _admin_call('connect')

def vconnected():
  """ True if an active connection to the Library/Service exists """
  return _admin_call('connected')
        
def vclean_up():
  """ Clean up shared resources. (!! ON THE WINDOWS SIDE !!) """
  _admin_call('clean_up')

def vget_block_limit():
  """ Returns the block limit of C/C++ RawDataBlock factory """
  return _admin_call('get_block_limit')

def vset_block_limit(new_limit):
  """ Changes the block limit of C/C++ RawDataBlock factory """
  _admin_call('set_block_limit', new_limit) 

def vget_block_count():
  """ Returns the count of current instantiated blocks """
  return _admin_call('get_block_count')

def vtype_bits(topic):
  """ Returns the type bits for a particular 'topic'

  topic: string representing a TOS data field('LAST','ASK', etc)
  returns -> value that can be logical &'d with type bit contstants 
  (ex. QUAD_BIT)
  """  
  return _admin_call('type_bits', topic)

def vtype_string(topic):
  """ Returns a platform-dependent string of the type of a particular 'topic'

  topic: string representing a TOS data field('LAST','ASK', etc)
  """
  return _admin_call('type_string', topic) 

def admin_init(address, poll_interval=DEF_TIMEOUT):
  """ Initialize virtual admin calls (e.g vinit(), vconnect()) 

  address:: tuple(str,int) :: (host/address of the windows implementation, port)
  """  
  global _virtual_hub_addr, _virtual_admin_sock
  if _virtual_admin_sock:
    raise TOSDB_VirtualizationError("virtual admin socket already exists")  
  _virtual_hub_addr = _check_and_resolve_address(address)
  _virtual_admin_sock = _socket.socket()
  _virtual_admin_sock.settimeout(poll_interval / 1000)
  try:
    _virtual_admin_sock.connect(address)
    _vcall(_pack_msg(_vCONN_ADMIN), _virtual_admin_sock, _virtual_hub_addr)
  except:
    _virtual_hub_addr = ''
    _virtual_admin_sock = None
    raise

def _admin_call(method, *arg_buffer):
  if not _virtual_admin_sock:
    raise TOSDB_VirtualizationError("no virtual admin socket, call admin_init")
  if method not in _vALLOWED_ADMIN:
    raise TOSDB_VirtualizationError("this virtual method call is not allowed")
  a = (method,)
  if arg_buffer:
    a += (_pickle.dumps(arg_buffer),) 
  ret_b = _vcall(_pack_msg(*a), _virtual_admin_sock, _virtual_hub_addr) 
  if ret_b[1]:
    return _pickle.loads(ret_b[1])
  

class VTOSDB_DataBlock:
  """ The main object for storing TOS data. (VIRTUAL)   

  address:: tuple(str,int) :: (host/address of the windows implementation, port)
  size: how much historical data to save
  date_time: should block include date-time stamp with each data-point?
  timeout: how long to wait for responses from TOS-DDE server 

  Please review the attached README.html for details.
  """  
  def __init__(self, address, size=1000, date_time=False, timeout=DEF_TIMEOUT):         
    self._hub_addr = _check_and_resolve_address(address)
    self._my_sock = _socket.socket()
    self._my_sock.settimeout(timeout / 1000)
    self._my_sock.connect(address)
    self._connected = False
    self._call_LOCK = _Lock()
    _vcall(_pack_msg(_vCONN_BLOCK), self._my_sock, self._hub_addr)      
    # in case __del__ is called during socket op
    self._call(_vCREATE, '__init__', size, date_time, timeout)
    self._connected = True    
    
  def __del__(self):
    try:
      if self._connected:
        self._call(_vDESTROY)
      if self._my_sock:
        self._my_sock.close()
    except:
      pass

  def __str__(self):
    s = self._call(_vCALL, '__str__')  
    return s if s else ''
  
  def info(self):
    """ Returns a more readable dict of info about the underlying block """
    return self._call(_vCALL, 'info')
  
  def get_block_size(self):
    """ Returns the amount of historical data stored in the block """
    return self._call(_vCALL, 'get_block_size')
  
  def set_block_size(self, sz):
    """ Changes the amount of historical data stored in the block """
    self._call(_vCALL, 'set_block_size', sz)
      
  def stream_occupancy(self, item, topic):
    """ Returns the current number of data-points pushed into the data-stream
        
    item: any item string in the block
    topic: any topic string in the block
    """           
    return self._call(_vCALL, 'stream_occupancy', item, topic)
  
  def items(self, str_max = MAX_STR_SZ):
    """ Returns the items currently in the block (and not pre-cached).
    
    str_max: the maximum length of item strings returned
    returns -> list of strings 
    """
    return self._call(_vCALL, 'items', str_max) 
        
  def topics(self,  str_max = MAX_STR_SZ):
    """ Returns the topics currently in the block (and not pre-cached).
    
    str_max: the maximum length of topic strings returned  
    returns -> list of strings 
    """
    return self._call(_vCALL, 'topics', str_max)    
  
  def add_items(self, *items):
    """ Add items (ex. 'IBM', 'SPY') to the block.

    NOTE: if there are no topics currently in the block, these items will 
    be pre-cached and appear not to exist, until a valid topic is added.

    *items: any numer of item strings
    """         
    self._call(_vCALL, 'add_items', *items)     

  def add_topics(self, *topics):
    """ Add topics (ex. 'LAST', 'ASK') to the block.

    NOTE: if there are no items currently in the block, these topics will 
    be pre-cached and appear not to exist, until a valid item is added.

    *topics: any numer of topic strings
    """         
    self._call(_vCALL, 'add_topics', *topics)

  def remove_items(self, *items):
    """ Remove items (ex. 'IBM', 'SPY') from the block.

    NOTE: if this call removes all items from the block the remaining topics 
    will be pre-cached and appear not to exist, until a valid item is re-added.

    *items: any numer of item strings
    """
    self._call(_vCALL, 'remove_items', *items)

  def remove_topics(self, *topics):
    """ Remove topics (ex. 'LAST', 'ASK') from the block.

    NOTE: if this call removes all topics from the block the remaining items 
    will be pre-cached and appear not to exist, until a valid topic is re-added.

    *topics: any numer of topic strings
    """
    self._call(_vCALL, 'remove_topics', *topics)
    
  def get(self, item, topic, date_time=False, indx = 0, check_indx=True, 
          data_str_max=STR_DATA_SZ):
    """ Return a single data-point from the data-stream
    
    item: any item string in the block
    topic: any topic string in the block
    date_time: (True/False) attempt to retrieve a TOSDB_DateTime object   
    indx: index of data-points [0 to block_size), [-block_size to -1]
    check_indx: throw if datum doesn't exist at that particular index
    data_str_max: the maximum size of string data returned
    """
    return self._call(_vCALL, 'get', item, topic, date_time, indx, check_indx, 
                      data_str_max) 

  def stream_snapshot(self, item, topic, date_time=False, end=-1, beg=0, 
                      smart_size=True, data_str_max=STR_DATA_SZ):
    """ Return multiple data-points(a snapshot) from the data-stream
    
    item: any item string in the block
    topic: any topic string in the block
    date_time: (True/False) attempt to retrieve a TOSDB_DateTime object        
    end: index of least recent data-point (end of the snapshot)
    beg: index of most recent data-point (beginning of the snapshot)    
    smart_size: limits amount of returned data by data-stream's occupancy
    data_str_max: the maximum length of string data returned

    if date_time is True: returns-> list of 2tuple
    else: returns -> list        
    """
    return self._call(_vCALL, 'stream_snapshot', item, topic, date_time,
                      end, beg, smart_size, data_str_max) 

  def stream_snapshot_from_marker(self, item, topic, date_time=False, beg=0, 
                                  margin_of_safety=100, throw_if_data_lost=True,
                                  data_str_max=STR_DATA_SZ):
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
    beg: index of most recent data-point (beginning of the snapshot)    
    margin_of_safety: (True/False) error margin for async stream growth
    throw_if_data_loss: (True/False) how to handle error states (see above)
    data_str_max: the maximum length of string data returned

    if beg > internal marker value: returns -> None    
    if date_time is True: returns-> list of 2tuple
    else: returns -> list        
    """
    return self._call(_vCALL, 'stream_snapshot_from_marker', item, topic,
                      date_time, beg, margin_of_safety, throw_if_data_lost,
                      data_str_max) 
  
  def item_frame(self, topic, date_time=False, labels=True, 
                 data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
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
    return self._call(_vCALL, 'item_frame', topic, date_time, labels,
                      data_str_max, label_str_max)   

  def topic_frame(self, item, date_time=False, labels=True, 
                  data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
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
    return self._call(_vCALL, 'topic_frame', item, date_time, labels,
                      data_str_max, label_str_max)

##
##  !! CREATE A WAY TO PICKLE AN ITERABLE OF DIFFERENT NAMEDTUPLES !!
##
##  def total_frame(self, date_time=False, labels=True, 
##                  data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
##    """ Return a matrix of the most recent values:  
##    
##    date_time: (True/False) attempt to retrieve a TOSDB_DateTime object    
##    labels: (True/False) pull the item and topic labels with the values 
##    data_str_max: the maximum length of string data returned
##    label_str_max: the maximum length of label strings returned
##    
##    if labels and date_time are True: returns-> dict of namedtuple of 2tuple
##    if labels is True: returns -> dict of namedtuple
##    if date_time is True: returns -> list of 2tuple
##    else returns-> list
##    """
##    return self._call(_vCALL, 'total_frame', date_time, labels,
##                      data_str_max, label_str_max)  

  def _call(self, virt_type, method='', *arg_buffer):      
    if virt_type == _vCREATE:
      req_b = _pack_msg(_vCREATE, _pickle.dumps(arg_buffer))
    elif virt_type == _vCALL:
      a = (_vCALL, method) + ((_pickle.dumps(arg_buffer),) if arg_buffer else ())
      req_b = _pack_msg(*a)       
    elif virt_type == _vDESTROY:
      req_b = _pack_msg(_vDESTROY)
    else:
      raise TOSDB_VirtualizationError("invalid virt_type")
    with self._call_LOCK:
      ret_b = _vcall(req_b, self._my_sock, self._hub_addr)
      if virt_type in [_vCREATE, _vDESTROY]:
        return True
      elif virt_type == _vCALL and ret_b[1]:
        if ret_b[0] == _vSUCCESS_NT:
          return _loadnamedtuple(ret_b[1])
        else:
          return _pickle.loads(ret_b[1])
      

def enable_virtualization(address, poll_interval=DEF_TIMEOUT):
  global _virtual_hub    
  ## --- NESTED CLASS _VTOS_BlockServer --- ##
  class _VTOS_BlockServer(_Thread):    
    def __init__(self, conn, poll_interval, stop_callback):
      super().__init__(daemon=True)
      self._my_sock = conn[0]
      self._cli_addr = conn[1]
      self._poll_interval = poll_interval
      self._my_sock.settimeout(poll_interval / 1000)
      self._blk = None
      self._rflag = False
      self._stop_callback = stop_callback
      
    def stop(self):
      self._rflag = False            

    def run(self):   
      def _handle_msg(dat): 
        def _handle_call(args):       
          try:
            meth = getattr(self._blk, args[1].decode())
            uargs = _pickle.loads(args[2]) if len(args) > 2 else ()            
            ret = meth(*uargs)        
            if ret is None: # None is still a success
              return _pack_msg(_vSUCCESS)        
            elif hasattr(ret,NTUP_TAG_ATTR): #special namedtuple tag
              return _pack_msg(_vSUCCESS_NT, _dumpnamedtuple(ret))
            else:
              return _pack_msg(_vSUCCESS, _pickle.dumps(ret))   
          except Exception as e:         
            return _pack_msg(_vFAILURE, _vFAIL_EXC, repr(e))   
        ### --- _handle_call --- ###   
        r = None
        kill = True
        args = _unpack_msg(dat)        
        msg_t = args[0].decode()        
        try:
          if msg_t == _vCREATE:
            uargs = _pickle.loads(args[1])         
            self._blk = TOSDB_DataBlock(*uargs)
            r = _pack_msg(_vSUCCESS)
            kill = False if r else True                     
          elif msg_t == _vDESTROY:
            if self._blk:
              del self._blk
            r = _pack_msg(_vSUCCESS)              
          elif msg_t == _vCALL:
            kill = False
            r = _handle_call(args)
          else:
            raise TOSDB_ValueError("invalid msg type")
        except Exception as e:          
          r = _pack_msg(_vFAILURE, _vFAIL_EXC, repr(e))
        try:
          _send_tcp(self._my_sock, r)
        except:
          raise
        finally:
          if kill:
            self.stop()
      ### --- _handle_msg --- ###         
      self._rflag = True      
      while self._rflag:                     
        try:           
          dat = _recv_tcp(self._my_sock)          
          if not dat:            
            break        
          _handle_msg(dat)
        except _socket.timeout:        
          pass
        except:
          print("Unhandled exception in _VTOS_BlockServer, terminated",
                file=_stderr)
          self._rflag = False
          self._stop_callback(self)
          raise        
      self._stop_callback(self)  
    ### --- run --- ###    
  ## --- NESTED CLASS _VTOS_BlockServer --- ##

  ## --- NESTED CLASS _VTOS_AdminServer --- ##
  class _VTOS_AdminServer(_Thread):    
    def __init__(self, conn, poll_interval):
      super().__init__(daemon=True)
      self._my_sock = conn[0]
      self._cli_addr = conn[1]
      self._poll_interval = poll_interval
      self._my_sock.settimeout(poll_interval / 1000)
      self._rflag = False
      self._globals = globals()
      
    def stop(self):
      self._rflag = False            

    def run(self):
      self._rflag = True      
      while self._rflag:                     
        try:          
          dat = _recv_tcp(self._my_sock)         
          if not dat:            
            break          
          args = _unpack_msg(dat)         
          rmsg = _pack_msg(_vFAILURE)
          try:          
            meth = self._globals[args[0].decode()]             
            uargs = _pickle.loads(args[1]) if len(args) > 1 else ()                      
            r = meth(*uargs)  
            if r is None:                    
              rmsg = _pack_msg(_vSUCCESS)
            else:
              rmsg = _pack_msg(_vSUCCESS, _pickle.dumps(r))              
          except Exception as e:            
            rmsg = _pack_msg(_vFAILURE, _vFAIL_EXC, repr(e))           
          _send_tcp(self._my_sock, rmsg)         
        except _socket.timeout:        
          pass
        except:
          print("Unhandled exception in _VTOS_AdminServer, terminated",
                file=_stderr)
          self._rflag = False
          raise

  ## --- NESTED CLASS _VTOS_Hub --- ##
  class _VTOS_Hub(_Thread):
    def __init__(self, address, poll_interval):
      super().__init__(daemon=True)  
      self._my_addr = _check_and_resolve_address(address)   
      self._rflag = False
      self._poll_interval = poll_interval
      self._my_sock = _socket.socket()
      self._my_sock.settimeout(poll_interval / 1000)
      self._my_sock.bind(address)
      self._my_sock.listen(0)
      self._virtual_block_servers = set()
      self._virtual_admin_server = None
      
    def stop(self):      
      self._rflag = False       

    def run(self):      
      def _shutdown_servers():
        while self._virtual_block_servers:
          self._virtual_block_servers.pop().stop()
        if self._virtual_admin_server:
          self._virtual_admin_server.stop()
      ### --- _shutdown_servers --- ###
      def _handle_msg(dat,conn):
        try:
          dat = _unpack_msg(dat)[0].decode()          
          if dat == _vCONN_BLOCK:           
            vserv = _VTOS_BlockServer(conn, self._poll_interval,
                      self._virtual_block_servers.discard)
            self._virtual_block_servers.add(vserv)
            vserv.start()
          elif dat == _vCONN_ADMIN:           
            if self._virtual_admin_server:
              self._virtual_admin_server.stop()            
            self._virtual_admin_server = _VTOS_AdminServer(conn,
                              self._poll_interval)
            self._virtual_admin_server.start()
          else:
            raise TOSDB_VirtualizationError("connection init"
                 " msg must be _vCONN_BLOCK or _vCONN_ADMIN")
          _send_tcp(conn[0], _pack_msg(_vSUCCESS))
        except Exception as e:
          rmsg = _pack_msg(_vFAILURE, _vFAIL_EXC, repr(e)) 
          _send_tcp(conn[0], rmsg)
          raise
      ### --- _handle_msg --- ###     
      self._rflag = True      
      while self._rflag:        
        try:          
          conn = self._my_sock.accept()        
          dat = _recv_tcp(conn[0])
          _handle_msg(dat, conn)
        except _socket.timeout:           
          continue        
        except: # anything else... shutdown the hub
          print("Unhandled exception in _VTOS_Hub, terminated",
               file=_stderr)
          _shutdown_servers()
          raise       
      _shutdown_servers()                    
    ## --- NESTED CLASS _VTOS_Hub --- ##     

  try:    
    if _virtual_hub is None:
      _virtual_hub = _VTOS_Hub(address, poll_interval)
      _virtual_hub.start()    
  except Exception as e:
    raise TOSDB_VirtualizationError("(enable) virtualization error", e)
# --- enabel_virtualization --- #

def disable_virtualization():
  global _virtual_hub
  try:
    if _virtual_hub is not None:
       _virtual_hub.stop()
       _virtual_hub = None    
  except Exception as e:
    raise TOSDB_VirtualizationError("(disable) virtualization error", e)  


def _vcall(msg, my_sock, hub_addr):
  try:     
    _send_tcp(my_sock, msg)     
    try:
      ret_b = _recv_tcp(my_sock)
    except _socket.timeout as e:
      raise TOSDB_VirtualizationError("socket timed out", 
                                      "VTOSDB_DataBlock._vcall")        
    args = _unpack_msg(ret_b)   
    status = args[0].decode()  
    if status == _vFAILURE:       
      desc = args[2].decode()       
      if args[1].decode() == _vFAIL_EXC:
        raise wrap_impl_error(eval(desc))
      else:
        raise TOSDB_VirtualizationError("failure status returned", desc)
    else:
      return (args[0],args[1]) if len(args) > 1 else (args[0],None)
  except ConnectionResetError:     
    try:
      my_sock.connect(hub_addr)      
      # ? INFINITE LOOP ? this recursive call might be the cause      
      return _vcall(msg, my_sock, hub_addr)
    except:
      raise TOSDB_VirtualizationError("failed to reconnect to hub",
                                      "VTOSDB_DataBlock._vcall")    
                        
def _dumpnamedtuple(nt):
  n = type(nt).__name__
  od = nt.__dict__
  return _pickle.dumps((n,tuple(od.keys()),tuple(od.values())))

def _loadnamedtuple(nt):
  name,keys,vals = _pickle.loads(nt)
  ty = _namedtuple(name, keys)
  return ty(*vals)

def _recv_tcp(sock):
  packedlen = _recvall_tcp(sock, 8)
  if not packedlen:
    return None
  dlen = _struct.unpack('Q', packedlen)[0]
  return _recvall_tcp(sock, dlen)

def _recvall_tcp(sock, n):
  data = b''
  while len(data) < n:
    p = sock.recv(n - len(data))
    if not p:
      return None
    data += p
  return data

def _send_tcp(sock, data):
  dl = len(data)
  msg = _struct.pack('Q',len(data)) + data
  return sock.sendall(msg)

def _pack_msg(*parts):   
  def _escape_part(part):
    enc = part.encode() if type(part) is not bytes else part         
    esc1 = _sub(_vESC, _vESC + _vEEXOR, enc) #escape the escape FIRST       
    return _sub(_vDELIM, _vESC + _vDEXOR, esc1) #escape the delim SECOND
  return _vDELIM.join([_escape_part(p) for p in parts])

def _unpack_msg(msg):  
  def _unescape_part(part):    
    unesc1 = _sub(_vESC + _vDEXOR, _vDELIM, part) #unescape the delim FIRST    
    return _sub(_vESC + _vEEXOR, _vESC, unesc1) #unescape the escape SECOND
  if not msg:
    return msg
  return [_unescape_part(p) for p in msg.strip().split(_vDELIM)]

def _check_and_resolve_address(addr): 
  if type(addr) is tuple:
    if len(addr) == 2:    
      if type(addr[0]) is str:    
        if type(addr[1]) is int:
          return (_socket.gethostbyname(addr[0]),addr[1])
  raise TOSDB_TypeError("address must be of type (str,int)")



    




        
