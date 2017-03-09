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

"""tosdb/:  A front-end / wrapper for the TOSDataBridge C library

Please refer to README.md for an explanation of the C/C++ library, tutorial.md 
for a walk-through of the basics, and virtualization_tutorial.md for example 
usage on non-Windows systems.

class TOSDB_DataBlock(WINDOWS ONLY):
    very similar to the 'block' approach of the underlying C/C++ library except
    the interface is explicitly object-oriented. It abstracts away the underlying
    type complexity, raising TOSDB_CLibError on internal library errors.

class VTOSDB_DataBlock:
    same interface as TOSDB_DataBlock except it utilizes a thin virtualization
    layer over TCP, passing serialized method calls to a windows machine running
    the core implemenataion.

ABC _TOSDB_DataBlock:
    abstract base class of TOSDB_DataBlock and VTOSDB_DataBlock

Init:
    context manager that handles initialization and clean-up

VInit:
    version of Init for the virtual layer

admin_init():
    initializes the vitual library calls (e.g vinit(), vconnect())

init()/vinit():
    tell the windows implementation to initialize the underlying C/C++ library
    and attempt to connect

connect()/vconnect():
    tell the windows implementation to connect to the underlying C/C++ library
    if init fails to do this automatically

connected()/vconnected():
    is the windows implementation connected to the engine AND TOS platform?

connection_state()/vconnection_state()
    what connection state is the windows implementation in?

clean_up()/vclean_up() (IMPORTANT):
    de-allocates shared resources of the underlying C lib and engine.
    Resources are cleaned-up automatically on exit but there's no guarantee.
    HIGHLY RECOMMENDED YOU CALL THIS BEFORE EXITING.

getblockcount()/vgetblockcount():
    number of instantiated blocks in the C lib
    
getblocklimit()/vgetblocklimit():
    get max number of blocks allowed by C lib
    
setblocklimit()/vsetblocklimit():
    set max number of blocks allowed by C lib

* * *

Ways to initialize via virtual layer:

--------------------------------------------------------------------------------
          [ local(windows) side ]        |         [ remote side ]
--------------------------------------------------------------------------------
                      Local with no virtualization support: 
                                         |   
 1) init()                               |
 2) connect()**                          |
 3) use standard calls/objects           |
                                         |
--------------------------------------------------------------------------------
               Local (interactive) init with virtualization support:
                                         |    
 1) init()                               |
 2) connect()**                          |
 3) use standard calls/objects...        |
 4) enable_virtualization((addr,port))   |
                                         | 5a) use VTOSDB_DataBlock
 5b) use standard calls/objects...       |
                                         | 6) admin_init()
                                         | 7) use virtual admin calls
--------------------------------------------------------------------------------
               Remote (interactive) init with virtualization support:
                                         |
 1) enable_virtualization((addr,port))   |
                                         |   2) admin_init()
                                         |   3) vinit()
                                         |   4) vconnect()**
                                         |   5a) use virtual calls/objs
 5b) use standard calls/objects...       |                                         
--------------------------------------------------------------------------------
   Local (daemon/server) Remote (interactive) init with virtualization support:
                                                   |
 1) C:\...\python> python tosdb --virtual-server \ |
                   "ADDR PORT" --root PATH-TO-DLL  |
                                                   | 2) use VTOSDB_DataBlock
                                                   | 3) admin_init()
                                                   | 4) use virtual admin calls            
                            ----------------------------------------------------
                            |                 or Remote (client)
                            |                     
                            | 2) :/.../python/$ python tosdb --virtual-client \
                            |                   "ADDR PORT" 
                            | 3) use virtual calls/objs 
--------------------------------------------------------------------------------

 **only needed if init/vinit() fails to call this for us
"""

from ._common import * 
from ._common import _DateTimeStamp, _TOSDB_DataBlock, _type_switch, \
                     _recvall_tcp, _recv_tcp, _send_tcp
from ._auth import *
from .doxtend import doxtend as _doxtend

from collections import namedtuple as _namedtuple
from threading import Thread as _Thread, Lock as _Lock
from functools import partial as _partial
from platform import system as _system
from sys import stderr as _stderr, stdout as _stdout
from re import sub as _sub
from atexit import register as _on_exit
from contextlib import contextmanager as _contextmanager
from time import strftime as _strftime
from inspect import getmembers as _getmembers, isfunction as _isfunction

import struct as _struct
import socket as _socket
import pickle as _pickle
  
_SYS_IS_WIN = _system() in ["Windows","windows","WINDOWS"]

if _SYS_IS_WIN: 
    from ._win import * # import the core implementation
  
_virtual_hub = None
_virtual_hub_addr = None
_virtual_admin_sock = None # <- what happens when we exit the client side ?

_vCREATE = 'CREATE'
_vCALL = 'CALL'
_vACK = 'ACK'
_vFAILURE = 'FAILURE'
_vEXCEPTION = 'EXCEPTION'
_vSUCCESS = 'SUCCESS'
_vSUCCESS_NT = 'SUCCESS_NT'
_vCONN_BLOCK = 'CONN_BLOCK' 
_vCONN_ADMIN = 'CONN_ADMIN' 
_vREQUIRE_AUTH = 'REQUIRE_AUTH'
_vREQUIRE_AUTH_NO = 'REQUIRE_AUTH_NO'

_vALLOWED_ADMIN = ('init', 'connect', 'connected', 'connection_state', 'clean_up',
                  'get_block_limit', 'set_block_limit', 'get_block_count',
                  'type_bits', 'type_string') 

# just the name, can't get bound method with ismethod pred (why?)
_vALLOWED_METHS = tuple(m[0] for m in _getmembers(_TOSDB_DataBlock, predicate=_isfunction) \
                        if m[0][0] != '_') + ('__init__','__str__')

## !! _vDELIM MUST NOT HAVE THE SAME VALUE AS _vEEXOR !! ##
_vDELIM = b'\x7E' 
_vESC = b'\x7D'
_vDEXOR = chr(ord(_vDELIM) ^ ord(_vESC)).encode()
_vEEXOR = chr(ord(_vESC) ^ ord(_vESC)).encode() # 0


# NOTE: for time being preface virtual calls and objects with 'v' so
# we can load both on the windows side for easier debugging,

def vinit(dllpath=None, root="C:\\"):
    """ Initialize the underlying tos-databridge DLL on host, try to connect.  
    
    Returns True if library was successfully loaded, not necessarily that
    it was also able to connect. Details are sent to stdout on Windows side.
    
    vinit(dllpath=None, root="C:\\")
    
    dllpath :: str :: exact path (on host) of the DLL -or-
    root    :: str :: directory (on host) to start walking/searching to find the DLL    

    returns -> bool 

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    if dllpath is None and root == "C:\\":
        if abort_init_after_warn():
            return False
    return _admin_call('init', dllpath, root, True)


def vconnect():
    """ Attempts to connect to the underlying engine and TOS Platform on host.

    vconnect()

    returns -> bool

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    return _admin_call('connect')


def vconnected():
    """ Returns True if there is an active connection to the engine AND TOS platform on host.

    vconnected()

    returns -> bool

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    return _admin_call('connected')
    

def vconnection_state():
    """ Returns the connection state between the client lib, engine and TOS platform on host.

    vconnection_state()
    
    if NOT connected to the engine:                         returns -> CONN_NONE
    elif connected to the engine BUT NOT the TOS platform:  returns -> CONN_ENGINE
    elif connected to the engine AND the TOS platform:      returns -> CONN_ENGINE_TOS

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    return _admin_call('connection_state')


def vclean_up():
    """ Clean up shared resources on host.  

    *** CALL BEFORE EXITING INTERPRETER ***
    
    Attempts to close the underlying block and disconnect gracefully so
    streams aren't orphaned/leaked. 
    
    clean_up()

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    _admin_call('clean_up')


@_contextmanager
def VInit(address, dllpath=None, root="C:\\", password=None, timeout=DEF_TIMEOUT):
    """ Manage a virtual 'session' with a host system's C lib, engine, and TOS platform.

    The context manager handles initialization of the virtual admin interface
    and the host C Lib, then tries to connect the latter with the engine and
    TOS platform on the host. On exiting the context block it automatically
    cleans up the host system and closes down the connection.

    with VInit(...):
        # (automatically initializes, connects etc.)
        # use virtual admin interface, create data blocks etc.
        # (automatically cleans up, closes etc.)

    VInit(address, dllpath=None, root="C:\\", password=None, timeout=DEF_TIMEOUT)

    address      :: (str,int) :: (address of the host system, port)     
    dllpath      :: str       :: exact path of the DLL -or-
    root         :: str       :: directory to start walking/searching to find the DLL
    password     :: str       :: password for authentication(None for no authentication)      
    timeout      :: int       :: timeout used by the underlying socket    

    throws TOSDB_VirtualizationError TOSDB_InitError    
    """
    try:
        admin_init(address, password, timeout)
        if not vinit(dllpath, root):
            raise TOSDB_InitError("failed to initilize library (virtual)")
        if not vconnected():      
            if not vconnect(): # try again
                raise TOSDB_InitError("failed to connect to library (virtual)")
        yield
    finally:
        vclean_up()
        admin_close()


def vget_block_limit():
    """ Returns the block limit imposed by the C Lib on host.

    vget_block_limit()

    returns -> int

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    return _admin_call('get_block_limit')


def vset_block_limit(new_limit):
    """ Changes the block limit imposed by the C Lib on host.

    vset_block_limit(new_limit)

    new_limit :: int :: new block limit

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    _admin_call('set_block_limit', new_limit) 


def vget_block_count():
    """ Returns the current count of instantiated blocks (according to C Lib) on host.

    vget_block_count()

    returns -> int

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """
    return _admin_call('get_block_count')


def vtype_bits(topic):
    """ Returns the type bits for a particular topic from host.

    These type bits can be logical &'d with type bit contstants (ex. QUAD_BIT)
    to determine the underlying type of the topic.

    vtype_bits(topic)

    topic :: str :: topic string ('LAST','ASK', etc)

    returns -> int

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """  
    return _admin_call('type_bits', topic)


def vtype_string(topic):
    """ Returns a platform-dependent string of the type of a particular topic from host.

    vtype_string(topic)

    topic :: str :: topic string ('LAST','ASK', etc)

    returns -> str

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper    
    """
    return _admin_call('type_string', topic) 


# map function objects to names
_vALLOWED_ADMIN_CALLS = dict(zip(_vALLOWED_ADMIN, \
    (eval(('' if _SYS_IS_WIN else 'v') + c) for c in _vALLOWED_ADMIN)))


def admin_close(): # do we need to signal the server ?
    """ Close admin connection created by admin_init """  
    global _virtual_hub_addr, _virtual_admin_sock
    if _virtual_admin_sock:
        _virtual_admin_sock.close()
    _virtual_hub_addr = ''
    _virtual_admin_sock = None 


def admin_init(address, password=None, timeout=DEF_TIMEOUT):
    """ Initialize virtual admin calls. 

    This establishes a stateful connection with the host system, allowing
    the admin calls(e.g vinit(), vconnect()) to communicate with their
    siblings(e.g init(), connect()) on the host side.
    
    admin_init(address, password=None, timeout=DEF_TIMEOUT)
    
    address  :: (str,int) :: (host/address of the windows implementation, port)
    password :: str       :: password for authentication(None for no authentication)
    timeout  :: int       :: timeout used by the underlying socket
    
    returns -> bool

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper 
    """  
    global _virtual_hub_addr, _virtual_admin_sock
    if _virtual_admin_sock:
        raise TOSDB_VirtualizationError("virtual admin socket already exists")  
    if password is not None:
        check_password(password)
    _virtual_hub_addr = _check_and_resolve_address(address)
    _virtual_admin_sock = _socket.socket()
    _virtual_admin_sock.settimeout(timeout / 1000) 
    try: 
        _virtual_admin_sock.connect(_virtual_hub_addr) 
        _handle_req_from_server(_virtual_admin_sock,password)
        _vcall(_pack_msg(_vCONN_ADMIN), _virtual_admin_sock, _virtual_hub_addr)
    except:
        admin_close()
        raise
    return True


def _admin_call(method, *arg_buffer):
    if not _virtual_admin_sock:
        raise TOSDB_VirtualizationError("no virtual admin socket, call admin_init")
    if method not in _vALLOWED_ADMIN:
        raise TOSDB_VirtualizationError("method '%s' not in _vALLOWED_ADMIN" % method)
    a = (method,)
    if arg_buffer:
        a += (_pickle.dumps(arg_buffer),) 
    ret_b = _vcall(_pack_msg(*a), _virtual_admin_sock, _virtual_hub_addr) 
    if ret_b[1]:
        return _pickle.loads(ret_b[1])


def _handle_req_from_server(sock,password):
    ret = _recv_tcp(sock)
    if ret is None:
        raise TOSDB_VirtualizationError("no response from server")
    _send_tcp(sock, _vACK.encode()) # ack
    r = ret.decode()
    if r == _vREQUIRE_AUTH:     
        if password is not None:     
            try_import_pycrypto()
            good_auth = handle_auth_cli(sock,password)
            if not good_auth:
                raise TOSDB_VirtualizationError("authentication failed")
        else:
            raise TOSDB_VirtualizationError("server requires authentication")
    elif r != _vREQUIRE_AUTH_NO:
        raise TOSDB_VirtualizationError("invalid (auth) request from server")
          

class VTOSDB_DataBlock(_TOSDB_DataBlock):
    """ The main object for storing TOS data. (VIRTUAL)   

    __init__(self, address, password=None, size=1000, date_time=False, 
             timeout=DEF_TIMEOUT)
                 
    address   :: (str,int) :: (host/address of the windows implementation, port)
    password  :: str       :: password for authentication(None for no authentication)
    size      :: int       :: how much historical data can be inserted
    date_time :: bool      :: should block include date-time with each data-point?
    timeout   :: int       :: how long to wait for responses from engine,
                              TOS-DDE server, internal IPC/Concurrency mechanisms,
                              and network communication (milliseconds)

    throws TOSDB_VirtualizationError TOSDB_ImplErrorWrapper
    """  
    def __init__(self, address, password=None, size=1000, date_time=False, 
                 timeout=DEF_TIMEOUT):
        self._valid = False
        self._hub_addr = _check_and_resolve_address(address)
        self._my_sock = _socket.socket()
        self._my_sock.settimeout(timeout / 1000)
        if password is not None:
            check_password(password)
        self._my_sock.connect(self._hub_addr)  
        try:
            _handle_req_from_server(self._my_sock,password)
        except:
            self._my_sock.close()
            raise
        self._call_LOCK = _Lock()
        _vcall(_pack_msg(_vCONN_BLOCK), self._my_sock, self._hub_addr)      
        # in case __del__ is called during socket op
        self._call(_vCREATE, '__init__', size, date_time, timeout) 
        self._valid = True


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def close(self):
        if self._valid:
            self._call(_vCALL, 'close')
            self._valid = False
        if self._my_sock:    
            self._my_sock.close()        

    
    def __del__(self):
        try:
            self.close()
        except:
            pass


    def __str__(self):
        s = self._call(_vCALL, '__str__')  
        return s if s else ''

    
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def info(self):    
        return self._call(_vCALL, 'info')
    

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def get_block_size(self):    
        return self._call(_vCALL, 'get_block_size')
    

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def set_block_size(self, sz):    
        self._call(_vCALL, 'set_block_size', sz)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def stream_occupancy(self, item, topic):          
        return self._call(_vCALL, 'stream_occupancy', item, topic)  


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def items(self, str_max = MAX_STR_SZ):
        return self._call(_vCALL, 'items', str_max)         


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def topics(self,  str_max = MAX_STR_SZ):
        return self._call(_vCALL, 'topics', str_max) 


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def items_precached(self, str_max = MAX_STR_SZ):
        return self._call(_vCALL, 'items_precached', str_max)         


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def topics_precached(self,  str_max = MAX_STR_SZ):
        return self._call(_vCALL, 'topics_precached', str_max) 


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def add_items(self, *items):       
        self._call(_vCALL, 'add_items', *items)     


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def add_topics(self, *topics):    
        self._call(_vCALL, 'add_topics', *topics)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def remove_items(self, *items):
        self._call(_vCALL, 'remove_items', *items)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def remove_topics(self, *topics):
        self._call(_vCALL, 'remove_topics', *topics)
      

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def get(self, item, topic, date_time=False, indx = 0, check_indx=True, 
            data_str_max=STR_DATA_SZ):

        return self._call(_vCALL, 'get', item, topic, date_time, indx,
                          check_indx, data_str_max) 


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def stream_snapshot(self, item, topic, date_time=False, end=-1, beg=0, 
                        smart_size=True, data_str_max=STR_DATA_SZ):

        return self._call(_vCALL, 'stream_snapshot', item, topic, date_time,
                          end, beg, smart_size, data_str_max) 


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def stream_snapshot_from_marker(self, item, topic, date_time=False, beg=0, 
                                    margin_of_safety=100, throw_if_data_lost=True,
                                    data_str_max=STR_DATA_SZ):

        return self._call(_vCALL, 'stream_snapshot_from_marker', item, topic,
                          date_time, beg, margin_of_safety, throw_if_data_lost,
                          data_str_max) 
    

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def item_frame(self, topic, date_time=False, labels=True, 
                   data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):

        return self._call(_vCALL, 'item_frame', topic, date_time, labels,
                          data_str_max, label_str_max)   


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def topic_frame(self, item, date_time=False, labels=True, 
                    data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):

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
        if method not in _vALLOWED_METHS:
            raise TOSDB_VirtualizationError("method '%s' not in _vALLOWED_METHS" % method)
        if virt_type == _vCREATE:
            req_b = _pack_msg(_vCREATE, _pickle.dumps(arg_buffer))
        elif virt_type == _vCALL:
            a = (_vCALL, method) + ((_pickle.dumps(arg_buffer),) if arg_buffer else ())
            req_b = _pack_msg(*a)       
        else:
            raise TOSDB_VirtualizationError("invalid virt_type")
        with self._call_LOCK:
            ret_b = _vcall(req_b, self._my_sock, self._hub_addr)
            if virt_type == _vCREATE:
                return True
            elif virt_type == _vCALL and ret_b[1]:
                if ret_b[0].decode() == _vSUCCESS_NT:
                    return _loadnamedtuple(ret_b[1])
                else:
                    return _pickle.loads(ret_b[1])
                     

            
def enable_virtualization(address, password=None, timeout=DEF_TIMEOUT, verbose=True):
    """ enable virtualization, making *this* the host system.

    This will start a background thread (_VTOS_Hub) to listen for virtual
    'clients' looking to use the admin calls (via Thread _VTOS_AdminServer)
    or data blocks (via Thread(s) _VTOS_BlockServer).

    The virtual client call will serlialize it's arguments, sending them
    to the host. On success the local/host version of the call will be
    executed by its respective Server. Finally, a return value/exception
    will be serialized and sent back to the client.
    
    enable_virtualization(address, password=None, timeout=DEF_TIMEOUT, verbose=True)
    
    address  :: (str,int) :: (address of the host system, port)
    password :: str       :: password for authentication(None for no authentication)
    timeout  :: int       :: timeout(s) used by the underlying socket(s)
    verbose  :: bool      :: extra information to stdout about client connection(s)
    
    throws TOSDB_VirtualizationError 
    """  
    global _virtual_hub   

    if password is not None:
        try_import_pycrypto()  

    try:    
        if _virtual_hub is None:
            if password is not None:
                check_password(password)
            _virtual_hub = _VTOS_Hub(address, password, timeout, verbose)
            _virtual_hub.start()    
    except Exception as e:
        raise TOSDB_VirtualizationError("enable virtualization error", e)


def disable_virtualization():
    """ disable virtualization on host system.

    disable_virtualization()

    throws TOSDB_VirtualizationError
    """ 
    global _virtual_hub
    try:
        if _virtual_hub is not None:
            _virtual_hub.stop()
            _virtual_hub = None    
    except Exception as e:
        raise TOSDB_VirtualizationError("disable virtualization error", e)  


def log_conn(msg, addr, file=_stdout, **info):
    t = _strftime("%m/%d/%Y %H:%M:%S")
    msg = msg[:40].ljust(40)  
    s = "+" if file == _stdout else "-"
    print(s, t, " " + s + " ", msg, str(addr), file=file)
    for k in info:
        print('    ', k + ":", str(info[k]), file=file)


class _VTOS_BlockServer(_Thread):    
    def __init__(self, conn, timeout, stop_callback, verbose):
        super().__init__(daemon=True)
        self._my_sock = conn[0]
        self._cli_addr = conn[1]
        self._timeout = timeout
        self._my_sock.settimeout(timeout / 1000)
        self._blk = None
        self._rflag = False
        self._stop_callback = stop_callback
        self._verbose = verbose
        if self._verbose:
            log_conn("OPEN BLOCK SERVER",self._cli_addr)

    def __del__(self):
        if self._verbose:
            log_conn("CLOSE BLOCK SERVER",self._cli_addr)
        if self._my_sock:
            self._my_sock.close()
            
    def stop(self):
        self._rflag = False            

    def run(self):   
        self._rflag = True      
        while self._rflag:                     
            try:           
                dat = _recv_tcp(self._my_sock)          
                if not dat:            
                    break        
                ret, kill = self._handle_msg(dat)
                try:
                    _send_tcp(self._my_sock, ret)      
                finally:
                    if kill:
                        self.stop()
            except _socket.timeout:        
                pass
            except ConnectionResetError:
                log_conn("CONNECTION RESET IN BLOCK SERVER",self._cli_addr, file=_stderr)  
                break 
            except Exception as e:
                log_conn("FATAL: UNHANDLED EXCEPTION IN BLOCKSERVER", self._cli_addr,
                         file=_stderr, exception=e) 
                self._close()
                raise    
        self._close()    

    def _handle_msg(self,dat):                   
        r = None
        kill = False
        args = _unpack_msg(dat)        
        msg_t = args[0].decode()        
        try:
            if msg_t == _vCREATE:
                uargs = _pickle.loads(args[1])         
                self._blk = TOSDB_DataBlock(*uargs)
                if self._verbose:
                    log_conn("BLOCK CREATED", self._cli_addr, name=self._blk._name.decode(),
                             block_size=self._blk._block_size, date_time=self._blk._date_time,
                             timeout=self._blk._timeout)
                r = _pack_msg(_vSUCCESS)
                if not r:
                    kill = True                                  
            elif msg_t == _vCALL:   
                r = self._handle_call(args)
            else:
                raise TOSDB_ValueError("invalid msg type")
        except Exception as e:  
            kill = True        
            r = _pack_msg(_vFAILURE, _vEXCEPTION, repr(e))
        return (r, kill)
                
    def _handle_call(self,args):       
        try:
            m = args[1].decode()
            if m not in _vALLOWED_METHS:
                raise TOSDB_VirtualizationError("method '%s' not in _vALLOWED_METHS" % m)
            meth = getattr(self._blk, m)
            uargs = _pickle.loads(args[2]) if len(args) > 2 else ()            
            ret = meth(*uargs)        
            if ret is None: # None is still a success
                return _pack_msg(_vSUCCESS)        
            elif hasattr(ret,NTUP_TAG_ATTR): #special namedtuple tag
                return _pack_msg(_vSUCCESS_NT, _dumpnamedtuple(ret))
            else:
                return _pack_msg(_vSUCCESS, _pickle.dumps(ret))   
        except Exception as e:         
            return _pack_msg(_vFAILURE, _vEXCEPTION, repr(e))

    def _close(self):
        del self._blk
        self._rflag = False          
        self._my_sock.close()
        self._stop_callback(self)
        
  
class _VTOS_AdminServer(_Thread):    
    def __init__(self, conn, timeout, stop_callback, verbose):
        super().__init__(daemon=True)
        self._my_sock = conn[0]
        self._cli_addr = conn[1]
        self._timeout = timeout
        self._my_sock.settimeout(timeout / 1000)
        self._rflag = False       
        self._stop_callback = stop_callback
        self._verbose = verbose
        if self._verbose:
            log_conn("OPEN ADMIN SERVER",self._cli_addr)

    def __del__(self):
        if self._verbose:
            log_conn("CLOSE ADMIN SERVER",self._cli_addr)
        if self._my_sock:
            self._my_sock.close()
        
    def stop(self):
        self._rflag = False            

    def run(self):
        self._rflag = True      
        while self._rflag:                     
            try:          
                dat = _recv_tcp(self._my_sock)         
                if not dat:            
                    break          
                ret = self._handle_call(dat)
                _send_tcp(self._my_sock, ret)
            except _socket.timeout:        
                pass
            except ConnectionResetError:
                log_conn("CONNECTION RESET IN ADMIN SERVER",self._cli_addr, file=_stderr)  
                break 
            except Exception as e:
                log_conn("FATAL: UNHANDLED EXCEPTION IN ADMIN SERVER", self._cli_addr,
                         file=_stderr, exception=e) 
                self._close()
                raise
        self._close()

    def _handle_call(self,dat):
        args = _unpack_msg(dat)         
        rmsg = _pack_msg(_vFAILURE)
        try:
            m = args[0].decode()
            if m not in _vALLOWED_ADMIN:
                raise TOSDB_VirtualizationError("method '%s' not in _vALLOWED_ADMIN" % m)
            meth = _vALLOWED_ADMIN_CALLS[m]             
            uargs = _pickle.loads(args[1]) if len(args) > 1 else ()                      
            r = meth(*uargs)
            msg = (_vSUCCESS,) if (r is None) else (_vSUCCESS,_pickle.dumps(r))
            rmsg = _pack_msg(*msg)                      
        except Exception as e:            
            rmsg = _pack_msg(_vFAILURE, _vEXCEPTION, repr(e))           
        return rmsg

    def _close(self):
        self._rflag = False
        self._my_sock.close()
        self._stop_callback(self)        

  
class _VTOS_Hub(_Thread):
    def __init__(self, address, password, timeout, verbose):
        super().__init__(daemon=True)  
        self._my_addr = _check_and_resolve_address(address)   
        self._rflag = False
        self._password = password
        self._timeout = timeout
        self._my_sock = _socket.socket()
        self._my_sock.settimeout(timeout / 1000)
        self._my_sock.bind(self._my_addr)
        self._my_sock.listen(0)
        self._virtual_block_servers = set()
        self._virtual_admin_servers = set()
        self._verbose = verbose
      
    def stop(self):      
        self._rflag = False      
        
    def run(self):      
        self._rflag = True      
        while self._rflag:        
            try:
                conn = None
                try:
                    conn = self._my_sock.accept()
                except _socket.timeout:
                    continue                
                conn[0].settimeout(self._timeout / 1000)                 
                # indicate the response/handshake auth protocol required
                if not self._ack_auth_protocol(conn):
                    conn[0].close()
                    continue                
                # use auth mechanims if we got a password arg
                if self._password is not None and not self._authenticate(conn):
                    conn[0].close()
                    continue
                if self._verbose:
                    log_conn("CLIENT CONNECTED",conn[1])               
                # get the type of server to start(_vCONN_BLOCK, _vCONN_ADMIN)
                dat = self._recv_msg(conn)
                if not dat:
                    conn[0].close()
                    continue                 
                try:                    
                    self._handle_msg(dat, conn) # (DONT CLOSE CONN AFTER THIS POINT)
                    rmsg = _pack_msg(_vSUCCESS)
                except TOSDB_VirtualizationError as e:
                    log_conn("FAILED TO HANDLE MESSAGE",conn[1], file=_stderr, exception=e) 
                    rmsg = _pack_msg(_vFAILURE, _vEXCEPTION, repr(e))
                    continue
                except Exception as e:
                    log_conn("FAILED TO HANDLE MESSAGE",conn[1], file=_stderr, exception=e)
                    rmsg = _pack_msg(_vFAILURE, _vEXCEPTION, repr(e))
                    raise
                finally:
                    _send_tcp(conn[0], rmsg)
            except ConnectionResetError:                
                log_conn("CONNECTION RESET IN HUB SERVER", conn[1], file=_stderr)  
                continue 
            except Exception as e: # anything else... shutdown the hub
                addr = conn[1] if conn else "(no client address)"
                log_conn("FATAL: UNHANDLED EXCEPTION IN HUB SERVER", addr, file=_stderr, exception=e)
                self._shutdown_servers()
                raise                        
        self._shutdown_servers()

    def _recv_msg(self, conn):
        dat = None
        try:
            dat = _recv_tcp(conn[0])
            if not dat:
                log_conn("DID NOT RECEIVE VALID MESSAGE", conn[1], file=_stderr)                 
        except _socket.timeout:
            log_conn("TIMED OUT WAITING FOR MESSAGE", conn[1], file=_stderr)            
        return dat
                       
    def _ack_auth_protocol(self, conn):
        amsg = _vREQUIRE_AUTH_NO
        if self._password:
            amsg = _vREQUIRE_AUTH
            if self._verbose:
                log_conn("TELL CLIENT TO AUTHENTICATE", conn[1])       
        try:
            _send_tcp(conn[0], amsg.encode())
        except _socket.timeout:
            log_conn("TIMED OUT SENDING PROTOCOL MESSAGE", conn[1], file=_stderr)            
            return False
        # get an ack or timeout
        try:
            r = _recv_tcp(conn[0])
            if r != _vACK.encode(): 
                log_conn("BAD ACK TOKEN RECEIVED", conn[1], file=_stderr)                
                return False
        except _socket.timeout:
            log_conn("TIMED OUT WAITING FOR ACK TOKEN", conn[1], file=_stderr)             
            return False
        return True
    
    #TODO: add delay/throttle mechanism
    def _authenticate(self, conn):
        try:                
            good_auth = handle_auth_serv(conn,self._password)                       
            if good_auth:
                if self._verbose:
                    log_conn("CLIENT AUTHENTICATION SUCCEEDED", conn[1]) 
                return True
            log_conn("CLIENT AUTHENTICATION FAILED", conn[1], file=_stderr)                                              
        except TOSDB_VirtualizationError as e:
            log_conn("HANDSHAKE FAILED", conn[1], file=_stderr, exception=e)
        except Exception as e:
            log_conn("HANDSHAKE FAILED", conn[1], file=_stderr, exception=e)
            raise
        return False
    
    def _handle_msg(self,dat,conn):            
        dat = _unpack_msg(dat)[0].decode()          
        if dat == _vCONN_BLOCK:         
            vserv = _VTOS_BlockServer(conn, self._timeout,
                                      self._virtual_block_servers.discard, self._verbose)
            self._virtual_block_servers.add(vserv)
            vserv.start()
        elif dat == _vCONN_ADMIN:                      
            vserv = _VTOS_AdminServer(conn, self._timeout,
                                      self._virtual_admin_servers.discard, self._verbose)
            self._virtual_admin_servers.add(vserv)
            vserv.start()
        else:
            conn[0].close()
            raise TOSDB_VirtualizationError("init message not _vCONN_BLOCK or _vCONN_ADMIN")   
        
    def _shutdown_servers(self):
        if self._verbose:
            log_conn("SHUTDOWN BLOCK & ADMIN SERVERS", "")
        while self._virtual_block_servers:
            self._virtual_block_servers.pop().stop()
        while self._virtual_admin_servers:
            self._virtual_admin_servers.pop().stop()

        
def _vcall(msg, my_sock, hub_addr, rcnt=3):
    try:
        #clear any stale data in the stream(e.g our last call timed-out midway)
        old_timeout = my_sock.gettimeout()
        my_sock.settimeout(0) #set to non-blocking
        try:
            while True: #could we get stuck in this loop ?
                my_sock.recv(4096)            
        except BlockingIOError:
            pass
        my_sock.settimeout(old_timeout)
        #initiate new call
        _send_tcp(my_sock, msg)               
        ret_b = _recv_tcp(my_sock)
        args = _unpack_msg(ret_b)
        status = None
        try:               
            status = args[0].decode()
        except Exception as e:            
            raise TOSDB_VirtualizationError("server failed to return (valid) message",
                                            "_vcall", str(e))
        if status == _vFAILURE:       
            desc = args[2].decode()       
            if args[1].decode() == _vEXCEPTION:
                raise wrap_impl_error(eval(desc))
            else:
                raise TOSDB_VirtualizationError("failure status returned", desc)
        else:
            return (args[0],args[1]) if len(args) > 1 else (args[0],None)
    except _socket.timeout as e:
        raise TOSDB_VirtualizationError("socket timed out", "_vcall")     
    except ConnectionResetError:        
        try:                             
            if rcnt > 0: # attemp rcnt retries via recursion
                my_sock.connect(hub_addr) 
                return _vcall(msg, my_sock, hub_addr, rcnt-1)
            else:
                raise TOSDB_VirtualizationError("_vcall recursion limit hit")
        except:
            raise TOSDB_VirtualizationError("failed to reconnect to hub","_vcall")    
       
                 
def _dumpnamedtuple(nt):
    n = type(nt).__name__
    od = nt.__dict__
    return _pickle.dumps((n,tuple(od.keys()),tuple(od.values())))


def _loadnamedtuple(nt):
    name,keys,vals = _pickle.loads(nt)
    ty = _namedtuple(name, keys)
    return ty(*vals)


def _pack_msg(*parts):   
    def _escape_part(part):
        enc = part.encode() if type(part) is not bytes else part         
        esc1 = _sub(_vESC, _vESC + _vEEXOR, enc)    #escape the escape FIRST       
        return _sub(_vDELIM, _vESC + _vDEXOR, esc1) #escape the delim SECOND
    return _vDELIM.join([_escape_part(p) for p in parts])


def _unpack_msg(msg):  
    def _unescape_part(part):    
        unesc1 = _sub(_vESC + _vDEXOR, _vDELIM, part) #unescape the delim FIRST    
        return _sub(_vESC + _vEEXOR, _vESC, unesc1)   #unescape the escape SECOND
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



    




        
