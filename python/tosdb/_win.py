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

"""_win.py:  The non-portable bits of the window's tosdb implementation

Please refer to the tosdb (tosdb/__init__.py) docstring for detailed information.
"""

from ._common import *
from ._common import _DateTimeStamp, _TOSDB_DataBlock, _type_switch
from .doxtend import doxtend as _doxtend

from io import StringIO as _StringIO
from uuid import uuid4 as _uuid4
from functools import partial as _partial
from math import log as _log, ceil as _ceil
from sys import maxsize as _maxsize, stderr as _stderr
from atexit import register as _on_exit
from collections import namedtuple as _namedtuple
from time import asctime as _asctime, localtime as _localtime
from platform import system as _system
from contextlib import contextmanager as _contextmanager

from os import walk as _walk, stat as _stat, curdir as _curdir, \
               listdir as _listdir, sep as _sep, path as _path

from re import compile as _compile, search as _search, match as _match, \
               split as _split

if _system() not in ["Windows","windows","WINDOWS"]: 
    print("error: tosdb/_win.py is for windows only !", file=_stderr)
    exit(1)

from ctypes import CDLL as _CDLL, \
                   cast as _cast, \
                   pointer as _pointer, \
                   create_string_buffer as _BUF_, \
                   POINTER as _PTR_, \
                   c_double as _double_, \
                   c_float as _float_, \
                   c_ulong as _ulong_, \
                   c_long as _long_, \
                   c_longlong as _longlong_, \
                   c_char_p as _str_, \
                   c_char as _char_, \
                   c_ubyte as _uchar_, \
                   c_int as _int_, \
                   c_void_p as _pvoid_, \
                   c_uint as _uint_, \
                   c_uint32 as _uint32_, \
                   c_uint8 as _uint8_
                   

_pchar_ = _PTR_(_char_)
_ppchar_ = _PTR_(_pchar_)  
_cast_cstr = lambda x: _cast(x,_str_).value.decode()

_gen_str_buffers = lambda sz, n: [_BUF_(sz) for _ in range(n)]
_gen_str_buffers_ptrs = lambda bufs: (_pchar_ * len(bufs))(*[_cast(b,_pchar_) for b in bufs])

_map_cstr = _partial(map,_cast_cstr)
_map_dt = _partial(map, TOSDB_DateTime)
_zip_cstr_dt = lambda cstr, dt: zip(_map_cstr(cstr),_map_dt(dt))

DLL_BASE_NAME = "tos-databridge"
DLL_DEPENDS1_NAME = "_tos-databridge"
SYS_ARCH_TYPE = "x64" if (_log(_maxsize * 2, 2) > 33) else "x86"
MIN_MARGIN_OF_SAFETY = 10

_REGEX_NON_ALNUM = _compile("[\W+]")
_REGEX_LETTER = _compile("[a-zA-Z]")
_VER_SFFX = '[\d]{1,2}.[\d]{1,2}'
_REGEX_VER_SFFX = _compile('-' + _VER_SFFX + '-')

_REGEX_DLL_NAME = _compile('^(' + DLL_BASE_NAME + '-)' \
                                + _VER_SFFX + '-' \
                                + SYS_ARCH_TYPE +'(.dll)$')

_REGEX_DBG_DLL_PATH = _compile('^.+(' + DLL_BASE_NAME + '-)' \
                                      + _VER_SFFX + '-' \
                                      + SYS_ARCH_TYPE + '_d(.dll)$')
           
_dll = None
_dll_depend1 = None

      
def init(dllpath=None, root="C:\\", bypass_check=False):
    """ Initialize the underlying tos-databridge DLL and try to connect.

    Returns True if library was successfully loaded, not necessarily that
    it was also able to connect. Details are sent to stdout.
    
    init(dllpath=None, root="C:\\", bypass_check=False)
    
    dllpath      :: str  :: exact path of the DLL -or-
    root         :: str  :: directory to start walking/searching to find the DLL
    bypass_check :: bool :: used by virtual layer implemenation (DO NOT SET)

    returns -> bool 

    throws TOSDB_InitError TOSDB_Error
    """  
    global _dll, _dll_depend1
    rel = set()
    if not bypass_check and dllpath is None and root == "C:\\":
        if abort_init_after_warn():
            return False

    def _remove_older_versions():
        nonlocal rel  
        getver = lambda x: _search(_REGEX_VER_SFFX,x).group().strip('-')
        vers = tuple(zip(map(getver, rel), rel))
        vers_max = max(vers)[0].split('.')[0]
        mtup = tuple(( x[0].split('.')[1],x[1]) \
                       for x in vers if x[0].split('.')[0] == vers_max )         
        mtup_max = max(mtup)[0]
        rel = set(x[1] for x in mtup if x[0] == mtup_max)

    def _get_depends1_dll_path(dllpath):        
        d = _path.dirname(dllpath)
        dbg = _match(_REGEX_DBG_DLL_PATH, dllpath)
        base = d + "/" + DLL_DEPENDS1_NAME + "-" + SYS_ARCH_TYPE
        path = base + ("_d.dll" if dbg else ".dll")       
        return path
    
    try:   
        if dllpath is None:
            matcher = _partial(_match, _REGEX_DLL_NAME)  
            for nfile in map(matcher, _listdir(_curdir)):
                if nfile: # try the current dir first             
                    rel.add(_curdir+ _sep + nfile.string)                    
            if not rel: # no luck, walk the dir tree              
                for root, dirs, files in _walk(root):  
                    for file in map(matcher, files):  
                        if file:                           
                            rel.add(root + _sep + file.string)                           
                if not rel: # if still nothing throw
                    raise TOSDB_InitError(" could not locate DLL")          
            if len(rel) > 1:  # only use the most recent version(s)
                _remove_older_versions()              
            # most recently updated
            d = dict(zip(map(lambda x: _stat(x).st_mtime, rel), rel)) 
            rec = max(d)
            dllpath = d[rec]

        dllpath_depends1 = _get_depends1_dll_path(dllpath)
        _dll_depend1 = _CDLL(dllpath_depends1)
        _dll = _CDLL(dllpath)
        
        print("+ Using Module(s) ", dllpath)
        print("                  ", dllpath_depends1)
        print("+ Last Update ", _asctime(_localtime(_stat(dllpath).st_mtime)))
        if connect():
            print("+ Succesfully Connected to Service \ Engine")       
        else:
            print("- Failed to Connect to Service \ Engine")
        return True # indicate the lib was loaded (but not if connect succeeded)
    except TOSDB_Error:
        raise
    except Exception as e:
        raise TOSDB_InitError("unable to initialize library", e)        


def connect():
    """ Attempts to connect to the engine and TOS platform

    connect()

    returns -> bool

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """            
    ret = _lib_call("TOSDB_Connect", error_check=False) 
    return (ret == 0) # 0 on success (error code)


def connected():
    """ Returns True if there is an active connection to the engine AND TOS platform

    connected()

    returns -> bool

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """
    ret = _lib_call("TOSDB_IsConnectedToEngineAndTOS", ret_type=_uint_, error_check=False)
    return bool(ret) # 1 on success (bool value)


def connection_state():
    """ Returns the connection state between the C lib, engine and TOS platform

    connection_state()
    
    if NOT connected to the engine:                         returns -> CONN_NONE
    elif connected to the engine BUT NOT the TOS platform:  returns -> CONN_ENGINE
    elif connected to the engine AND the TOS platform:      returns -> CONN_ENGINE_TOS

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """
    return _lib_call("TOSDB_ConnectionState", ret_type=_uint_, error_check=False)    
         
       
def clean_up():
    """ Clean up shared resources.  

    *** CALL BEFORE EXITING INTERPRETER ***
    
    Attempts to close the underlying block and disconnect gracefully so
    streams aren't orphaned/leaked. 
    
    clean_up()

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """
    global _dll, _dll_depend1
    if _dll is not None:       
        err = _lib_call("TOSDB_CloseBlocks", error_check=False)
        print("+ Closing Blocks" if not err else "- Error Closing Blocks")        
        _lib_call("TOSDB_Disconnect", error_check=False)
        print("+ Disconnecting From Service \ Engine")
        print("+ Closing Module(s) ", _dll._name)
        print("                    ", _dll_depend1._name)
        _dll = None
        _dll_depend1 = None
   
 
_on_exit(clean_up)  # try automatically on exit (NO GUARANTEE)


@_contextmanager
def Init(dllpath=None, root="C:\\"):
    """ Manage a 'session' with the C lib, engine, and TOS platform.

    The context manager handles initialization of the library and tries to
    connect to the engine and TOS platform. On exiting the context block it
    automatically cleans up.

    with Init(...):
        # (automatically initializes, connects etc.)
        # use admin interface, create data blocks etc.
        # (automatically cleans up, closes etc.)

    def Init(dllpath=None, root="C:\\")             
          
    dllpath :: str :: exact path of the DLL -or-
    root    :: str :: directory to start walking/searching to find the DLL    
    
    throws TOSDB_InitError    
    """
    try:
        if not init(dllpath, root):
            raise TOSDB_InitError("failed to initilize library")
        if not connected():      
            if not connect(): # try again
                raise TOSDB_InitError("failed to connect to library")
        yield
    finally:
        clean_up()


def get_block_limit():
    """ Returns the block limit imposed by the C Lib

    get_block_limit()

    returns -> int

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """
    return _lib_call("TOSDB_GetBlockLimit", ret_type=_uint32_, error_check=False)


def set_block_limit(new_limit):
    """ Changes the block limit imposed by the C Lib 

    set_block_limit(new_limit)

    new_limit :: int :: new block limit

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """
    _lib_call("TOSDB_SetBlockLimit",
              new_limit,
              ret_type=_uint32_, 
              arg_types=(_uint32_,),
              error_check=False)


def get_block_count():
    """ Returns the current count of instantiated blocks (according to C Lib)

    get_block_count()

    returns -> int

    throws TOSDB_CLibError
           **does not raise an exception if lib call returns error code
    """
    return _lib_call("TOSDB_GetBlockCount", ret_type=_uint32_, error_check=False)


def type_bits(topic):
    """ Returns the type bits for a particular topic

    These type bits can be logical &'d with type bit contstants (ex. QUAD_BIT)
    to determine the underlying type of the topic.

    type_bits(topic)

    topic :: str :: topic string ('LAST','ASK', etc)

    returns -> int

    throws TOSDB_CLibError
    """
    b = _uint8_()
    _lib_call("TOSDB_GetTypeBits",
              topic.upper().encode("ascii"),
              _pointer(b), 
              arg_types=(_str_,_PTR_(_uint8_)) )

    return b.value


def type_string(topic):
    """ Returns a platform-dependent string of the type of a particular topic

    type_string(topic)

    topic :: str :: topic string ('LAST','ASK', etc)

    returns -> str

    throws TOSDB_CLibError
    """
    s = _BUF_(MAX_STR_SZ + 1)
    _lib_call("TOSDB_GetTypeString",
              topic.upper().encode("ascii"),
              s,
              (MAX_STR_SZ + 1))

    return s.value.decode()



class TOSDB_DataBlock(_TOSDB_DataBlock):
    """ The main object for storing TOS data.    

    __init__(self, size=1000, date_time=False, timeout=DEF_TIMEOUT)

    size      :: int  :: how much historical data can be inserted
    date_time :: bool :: should block include date-time with each data-point?
    timeout   :: int  :: how long to wait for responses from engine, TOS-DDE server,
                         and/or internal IPC/Concurrency mechanisms (milliseconds)

    throws TOSDB_CLibError
    """    
    def __init__(self, size=1000, date_time=False, timeout=DEF_TIMEOUT):        
        self._name = (_uuid4().hex).encode("ascii")
        self._block_size = size
        self._timeout = timeout
        self._date_time = date_time
        self._items = []   
        self._topics = []
        self._items_precached = []   
        self._topics_precached = []    
        self._valid = False
        _lib_call("TOSDB_CreateBlock",
                  self._name,
                  size,
                  date_time,
                  timeout,
                  arg_types=(_str_,_uint32_,_int_,_uint32_))                 
        self._valid= True
       

    def __del__(self): # for convenience, no guarantee
        # cleaning up can be problematic if we exit python abruptly:
        #   1) need to be sure _lib_call is still around to call the C Lib with
        #   2) need to be sure block creation was actually successful using the
        #      _valid flag (__del__ can be called if __init__ fails/throws)
        #   3) need to be sure the DLL object is still around 
        if _lib_call is not None and self._valid and _dll is not None:
            try:
                _lib_call("TOSDB_CloseBlock", self._name)      
            except:      
                print("WARN: block[" + self._name.decode() + "] __del__ failed "
                      "to call TOSDB_CloseBlock - leak possible")        


    def __str__(self):      
        sio = _StringIO() # ouput buffer
        tf = self.total_frame() # get the frame
        count = 0
        mdict = {"col0":0}       
        for k in tf:  # first, find the min column sizes not to truncate
            val = tf[k]
            if count == 0:
                mdict.update({(k,len(k)) for k in val._fields})      
            if len(k) > mdict["col0"]:
                mdict["col0"] = len(k)
            for f in val._fields:
                l = len(str(getattr(val,f)))           
                if l > mdict[f]:
                    mdict[f] = l
            count += 1  
        count = 0       
        for k in tf:  # next, 'print' the formatted frame data
            val = tf[k]
            if count == 0:
                print(" " * mdict["col0"], end=' ', file=sio)
                for f in val._fields:               
                    print(f.ljust(mdict[f]),end=' ', file=sio) 
                print('',file=sio)
            print(k.ljust(mdict["col0"]),end=' ', file=sio)
            for f in val._fields:
                print(str(getattr(val,f)).ljust(mdict[f]), end=' ', file=sio) 
            print('',file=sio)
            count += 1              
        sio.seek(0)
        return sio.read()


    def _item_count(self):       
        i = _uint32_()
        _lib_call("TOSDB_GetItemCount",
                  self._name,
                  _pointer(i),
                  arg_types=(_str_,_PTR_(_uint32_)))
        return i.value


    def _topic_count(self):        
        t = _uint32_()
        _lib_call("TOSDB_GetTopicCount",
                  self._name,
                  _pointer(t),
                  arg_types=(_str_,_PTR_(_uint32_)))
        return t.value


    def _item_precached_count(self):       
        i = _uint32_()
        _lib_call("TOSDB_GetPreCachedItemCount",
                  self._name,
                  _pointer(i),
                  arg_types=(_str_,_PTR_(_uint32_)))
        return i.value


    def _topic_precached_count(self):        
        t = _uint32_()
        _lib_call("TOSDB_GetPreCachedTopicCount",
                  self._name,
                  _pointer(t),
                  arg_types=(_str_,_PTR_(_uint32_)))
        return t.value

    
    def _get_items(self, str_max=MAX_STR_SZ):
        size = self._item_count()
        strs = _gen_str_buffers(str_max+1, size)
        pstrs = _gen_str_buffers_ptrs(strs) 
        
        _lib_call("TOSDB_GetItemNames", 
                  self._name, 
                  pstrs, 
                  size, 
                  str_max + 1, 
                  arg_types=(_str_, _ppchar_, _uint32_, _uint32_))
        
        return list(map(_cast_cstr,pstrs))                     

    
    def _get_topics(self, str_max=MAX_STR_SZ):
        size = self._topic_count()
        strs = _gen_str_buffers(str_max+1, size)
        pstrs = _gen_str_buffers_ptrs(strs)   
            
        _lib_call("TOSDB_GetTopicNames", 
                  self._name, 
                  pstrs, 
                  size, 
                  str_max + 1, 
                  arg_types=(_str_,  _ppchar_, _uint32_, _uint32_))               
        
        return list(map(_cast_cstr,pstrs))     

    
    def _get_items_precached(self, str_max=MAX_STR_SZ):
        size = self._item_precached_count()  
        strs = _gen_str_buffers(str_max+1, size)
        pstrs = _gen_str_buffers_ptrs(strs) 
        
        _lib_call("TOSDB_GetPreCachedItemNames", 
                  self._name, 
                  pstrs, 
                  size, 
                  str_max + 1, 
                  arg_types=(_str_, _ppchar_, _uint32_, _uint32_))
        
        return list(map(_cast_cstr,pstrs))                 


    def _get_topics_precached(self,  str_max=MAX_STR_SZ):
        size = self._topic_precached_count()
        strs = _gen_str_buffers(str_max+1, size)
        pstrs = _gen_str_buffers_ptrs(strs)   
            
        _lib_call("TOSDB_GetPreCachedTopicNames", 
                  self._name, 
                  pstrs, 
                  size, 
                  str_max + 1, 
                  arg_types=(_str_,  _ppchar_, _uint32_, _uint32_))               
        
        return list(map(_cast_cstr,pstrs))


    def _sync_items_topics(self):
        self._items = self._get_items()
        self._topics = self._get_topics()       
        self._items_precached = self._get_items_precached()
        self._topics_precached = self._get_topics_precached()


    def _items_topics_are_synced(self):
        return self._items == self._get_items() \
            and self._topics == self._get_topics() \
            and self._items_precached == self._get_items_precached() \
            and self._topics_precached == self._get_topics_precached()


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def info(self):      
        return {"Name": self._name.decode('ascii'), 
                "Items": list(self._items),
                "Topics": list(self._topics),
                "ItemsPreCached": list(self._items_precached),
                "TopicsPreCached": list(self._topics_precached), 
                "Size": self._block_size,
                "DateTime": "Enabled" if self._date_time else "Disabled",
                "Timeout": self._timeout}
      

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def get_block_size(self):          
        b = _uint32_()
        _lib_call("TOSDB_GetBlockSize",
                  self._name,
                  _pointer(b),
                  arg_types=(_str_,_PTR_(_uint32_)))
        return b.value
      

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def set_block_size(self, sz):
        try:
            _lib_call("TOSDB_SetBlockSize",
                      self._name,
                      sz,
                      arg_types=(_str_,_uint32_))
        finally:
            try:
                self._block_size = self.get_block_size()
            except Exception as e:
                raise TOSDB_Error("unable to sync block size: " + str(e))
              

    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def items(self, str_max=MAX_STR_SZ):
        return list(self._items)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def topics(self, str_max=MAX_STR_SZ):
        return list(self._topics)

    
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def items_precached(self, str_max=MAX_STR_SZ):
        return list(self._items_precached)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def topics_precached(self,  str_max=MAX_STR_SZ):
        return list(self._topics_precached)         


    def _in_block_or_precache(self, is_item, e):
        return e in (self._items + self._items_precached) if is_item else \
               e in (self._topics + self._topics_precached)                    


    def _add_remove(self, cname, hfunc, elems):
        if not self._items_topics_are_synced():
            raise TOSDB_Error("item/topics not synced with C lib")
        remove = 'Remove' in cname
        fails = {}
        for elem in elems:
            try:
                el = hfunc(elem, False)                                
                if remove and not self._in_block_or_precache('Item' in cname, el):
                    fails[str(elem)] = "item not in block or precache: " + el
                    continue
                _lib_call(cname,
                          self._name,
                          el.encode("ascii"),
                          arg_types=(_str_,_str_))
            except Exception as e:
                fails[str(elem)] = e.args[0]
        self._sync_items_topics()
        if fails:
            raise TOSDB_Error("error(s) adding/removing items/topics", str(fails))
        
        
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock   
    def add_items(self, *items):
        self._add_remove('TOSDB_AddItem', self._handle_raw_item, items)

            
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def add_topics(self, *topics):
        self._add_remove('TOSDB_AddTopic', self._handle_raw_topic, topics)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def remove_items(self, *items):
        self._add_remove('TOSDB_RemoveItem', self._handle_raw_item, items)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def remove_topics(self, *topics):
        self._add_remove('TOSDB_RemoveTopic', self._handle_raw_topic, topics)  

 
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock     
    def stream_occupancy(self, item, topic):        
        item = self._handle_raw_item(item)
        topic = self._handle_raw_topic(topic)       
        occ = _uint32_()
        
        _lib_call("TOSDB_GetStreamOccupancy", 
                  self._name, 
                  item.encode("ascii"),
                  topic.encode("ascii"), 
                  _pointer(occ),
                  arg_types=(_str_, _str_, _str_, _PTR_(_uint32_)))
        
        return occ.value

    
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def get(self, item, topic, date_time=False, indx=0, check_indx=True, 
            data_str_max=STR_DATA_SZ):
        
        item = self._handle_raw_item(item)
        topic = self._handle_raw_topic(topic)  
        
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        
        if indx < 0:
            indx += self._block_size
        if indx >= self._block_size:
            raise TOSDB_IndexError("invalid index value passed to get()")   
        if check_indx and indx >= self.stream_occupancy(item, topic):
            raise TOSDB_DataError("data not available at this index yet " +
                                  "(disable check_indx to avoid this error)")
                
        tytup = _type_switch( type_bits(topic) )       
        if tytup[0] == "String":
            return self._get_string(item, topic, date_time, indx, data_str_max)
        else:
            return self._get_number(tytup, item, topic, date_time, indx)


    def _get_string(self, item, topic, date_time, indx, data_str_max):
        dt = _DateTimeStamp() 
        s = _BUF_(data_str_max + 1)
        
        _lib_call("TOSDB_GetString", 
                  self._name, 
                  item.encode("ascii"), 
                  topic.encode("ascii"), 
                  indx, 
                  s, 
                  data_str_max + 1,
                  _pointer(dt) if date_time else _PTR_(_DateTimeStamp)(),
                  arg_types=(_str_,  _str_, _str_, _long_, _pchar_,
                             _uint32_, _PTR_(_DateTimeStamp)))

        s = s.value.decode()
        return (s, TOSDB_DateTime(dt)) if date_time else s


    def _get_number(self, tytup, item, topic, date_time, indx):
        dt = _DateTimeStamp()
        n = tytup[1]()
        
        _lib_call("TOSDB_Get"+tytup[0], 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  indx, 
                  _pointer(n),
                  _pointer(dt) if date_time else _PTR_(_DateTimeStamp)(),
                  arg_types=(_str_, _str_, _str_, _long_, _PTR_(tytup[1]),
                             _PTR_(_DateTimeStamp)))     

        return (n.value, TOSDB_DateTime(dt)) if date_time else n.value       

       
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def stream_snapshot(self, item, topic, date_time=False, end=-1, beg=0,
                        smart_size=True, data_str_max=STR_DATA_SZ):
        
        item = self._handle_raw_item(item)
        topic = self._handle_raw_topic(topic)  
        
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        
        if end < 0:
            end += self._block_size
        if beg < 0:
            beg += self._block_size         
        size = (end - beg) + 1
        if beg < 0 \
           or end < 0 \
           or beg >= self._block_size \
           or end >= self._block_size \
           or size <= 0:
            raise TOSDB_IndexError("invalid 'beg' and/or 'end' index value(s)")

        if smart_size:
            so = self.stream_occupancy(item, topic)
            if so == 0 or so <= beg:
                return []
            end = min(end, so - 1)
            beg = min(beg, so - 1)
            size = (end - beg) + 1    
   
        tytup = _type_switch( type_bits(topic) )        
        if tytup[0] == "String":      
            return self._stream_snapshot_strings(item, topic, date_time, end,
                                                 beg, size, data_str_max)
        else:
            return self._stream_snapshot_numbers(tytup, item, topic, date_time,
                                                 end, beg, size)


    def _stream_snapshot_strings(self, item, topic, date_time, end, beg, size,
                                 data_str_max):
        strs = _gen_str_buffers(data_str_max+1, size)
        pstrs = _gen_str_buffers_ptrs(strs) 
        dts = (_DateTimeStamp * size)()
                                 
        _lib_call("TOSDB_GetStreamSnapshotStrings", 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  pstrs, 
                  size, 
                  data_str_max + 1,                
                  dts if date_time else _PTR_(_DateTimeStamp)(),
                  end, 
                  beg,
                  arg_types=(_str_, _str_, _str_, _ppchar_, _uint32_, _uint32_, 
                             _PTR_(_DateTimeStamp), _long_, _long_))   
        
        return list(_zip_cstr_dt(pstrs,dts) if date_time else _map_cstr(pstrs)) 

        
    def _stream_snapshot_numbers(self, tytup, item, topic, date_time, end, beg, size):
        nums = (tytup[1] * size)()
        dts = (_DateTimeStamp * size)()
                                 
        _lib_call("TOSDB_GetStreamSnapshot"+tytup[0]+"s", 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  nums, 
                  size,
                  dts if date_time else _PTR_(_DateTimeStamp)(),
                  end, 
                  beg,
                  arg_types=(_str_, _str_, _str_, _PTR_(tytup[1]), _uint32_, 
                             _PTR_(_DateTimeStamp), _long_, _long_)) 
        
        return list(zip(nums,_map_dt(dts)) if date_time else nums)

                                 
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def stream_snapshot_from_marker(self, item, topic, date_time=False, beg=0, 
                                    margin_of_safety=100, throw_if_data_lost=True,
                                    data_str_max=STR_DATA_SZ):
        
        item = self._handle_raw_item(item)
        topic = self._handle_raw_topic(topic)  
        
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")
        
        if beg < 0:
            beg += self._block_size   
        if beg < 0 or beg >= self._block_size:
            raise TOSDB_IndexError("invalid 'beg' index value")
        
        if margin_of_safety < MIN_MARGIN_OF_SAFETY:
            raise TOSDB_ValueError("margin_of_safety < MIN_MARGIN_OF_SAFETY")
        
        is_dirty = _uint_()
        _lib_call("TOSDB_IsMarkerDirty", 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  _pointer(is_dirty), 
                  arg_types=(_str_, _str_, _str_, _PTR_(_uint_))) 

        if is_dirty and throw_if_data_lost:
            raise TOSDB_DataError("marker is already dirty")
        
        mpos = _longlong_()
        _lib_call("TOSDB_GetMarkerPosition", 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  _pointer(mpos), 
                  arg_types=(_str_, _str_, _str_, _PTR_(_longlong_)))    
        
        cur_sz = mpos.value - beg + 1
        if cur_sz < 0:
            return None
        
        tytup = _type_switch( type_bits(topic) )       
        if tytup[0] == "String":
            return self._stream_snapshot_from_marker_strings(item, topic,
                                date_time, beg, cur_sz + margin_of_safety,
                                throw_if_data_lost, data_str_max)
        else:
            return self._stream_snapshot_from_marker_numbers(tytup, item,
                                topic, date_time, beg, cur_sz + margin_of_safety,
                                throw_if_data_lost)
       

    def _stream_snapshot_from_marker_strings(self, item, topic, date_time, beg,
                                             size, throw_if_data_lost, data_str_max):
        strs = _gen_str_buffers(data_str_max+1, size)
        pstrs = _gen_str_buffers_ptrs(strs) 
        dts = (_DateTimeStamp * size)()
        g = _long_()
                                 
        _lib_call("TOSDB_GetStreamSnapshotStringsFromMarker", 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  pstrs, 
                  size, 
                  data_str_max + 1,
                  dts if date_time else _PTR_(_DateTimeStamp)(),     
                  beg, 
                  _pointer(g),
                  arg_types=(_str_, _str_, _str_, _ppchar_, _uint32_, _uint32_,
                             _PTR_(_DateTimeStamp), _long_, _PTR_(_long_))) 
           
        g = g.value
        if g == 0:
            return None
        elif g < 0:
            if throw_if_data_lost:
                raise TOSDB_DataError("data lost behind the 'marker'")
            else:
                g *= -1

        return list(_zip_cstr_dt(pstrs[:g],dts[:g]) if date_time else _map_cstr(pstrs[:g]))
     
                                 
    def _stream_snapshot_from_marker_numbers(self, tytup, item, topic, date_time, beg,
                                             size, throw_if_data_lost):
        nums = (tytup[1] * size)()
        dts = (_DateTimeStamp * size)()
        g = _long_()
                                 
        _lib_call("TOSDB_GetStreamSnapshot" + tytup[0] + "sFromMarker", 
                  self._name,
                  item.encode("ascii"), 
                  topic.encode("ascii"),
                  nums, 
                  size,
                  dts if date_time else _PTR_(_DateTimeStamp)(),     
                  beg, 
                  _pointer(g),
                  arg_types=(_str_, _str_, _str_, _PTR_(tytup[1]), _uint32_, 
                             _PTR_(_DateTimeStamp), _long_, _PTR_(_long_))) 
      
        g = g.value
        if g == 0:
            return None
        elif g < 0:
            if throw_if_data_lost:
                raise TOSDB_DataError("data lost behind the 'marker'")
            else:
                g *= -1

        return list(zip(nums[:g], _map_dt(dts[:g])) if date_time else nums[:g])                        
                                    
                                 
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def item_frame(self, topic, date_time=False, labels=True, 
                   data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
        
        topic = self._handle_raw_topic(topic)  
        
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")       
        
        tytup = _type_switch( type_bits(topic) )        
        if tytup[0] is "String":
            return self._item_frame_strings(topic, date_time, labels, self._item_count(),
                                            data_str_max, label_str_max)
        else:
            return self._item_frame_numbers(tytup, topic, date_time, labels,
                                            self._item_count(), label_str_max) 


    def _item_frame_strings(self, topic, date_time, labels, size, data_str_max,
                            label_str_max):
        strs = _gen_str_buffers(data_str_max+1, size)
        labs = _gen_str_buffers(label_str_max+1, size)                
        pstrs = _gen_str_buffers_ptrs(strs)
        plabs = _gen_str_buffers_ptrs(labs)  
        dts = (_DateTimeStamp * size)()
                                 
        _lib_call("TOSDB_GetItemFrameStrings", 
                  self._name, 
                  topic.encode("ascii"),
                  pstrs, 
                  size, 
                  data_str_max + 1,
                  plabs if labels else _ppchar_(), 
                  label_str_max + 1,
                  dts if date_time else _PTR_(_DateTimeStamp)(),
                  arg_types=(_str_, _str_, _ppchar_, _uint32_, _uint32_,
                             _ppchar_, _uint32_, _PTR_(_DateTimeStamp)))       

        dat = list(_zip_cstr_dt(pstrs, dts) if date_time else _map_cstr(pstrs))
        if labels:            
            nt = _gen_namedtuple(_str_clean(topic)[0], _str_clean(*_map_cstr(plabs)))            
            return nt(*dat)
        else:
            return dat                                


    def _item_frame_numbers(self, tytup, topic, date_time, labels, size, label_str_max):        
        nums = (tytup[1] * size)()
        dts = (_DateTimeStamp * size)()
        labs = _gen_str_buffers(label_str_max+1, size) 
        plabs = _gen_str_buffers_ptrs(labs)
                                 
        _lib_call("TOSDB_GetItemFrame"+tytup[0]+"s", 
                  self._name, 
                  topic.encode("ascii"), 
                  nums, 
                  size,
                  plabs if labels else _ppchar_(), 
                  label_str_max + 1,
                  dts if date_time else _PTR_(_DateTimeStamp)(),       
                  arg_types=(_str_, _str_, _PTR_(tytup[1]), _uint32_, _ppchar_, 
                             _uint32_, _PTR_(_DateTimeStamp)))    

        dat = list(zip(nums,_map_dt(dts)) if date_time else nums)
        if labels:               
            nt = _gen_namedtuple(_str_clean(topic)[0], _str_clean(*_map_cstr(plabs)))
            return nt(*dat)        
        else:
            return dat 
        
                                 
    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def topic_frame(self, item, date_time=False, labels=True, 
                    data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
        
        item = self._handle_raw_item(item)
         
        if date_time and not self._date_time:
            raise TOSDB_DateTimeError("date_time not available for this block")       
        
        size = self._topic_count()
        strs = _gen_str_buffers(data_str_max+1, size)
        labs = _gen_str_buffers(label_str_max+1, size)                
        pstrs = _gen_str_buffers_ptrs(strs)
        plabs = _gen_str_buffers_ptrs(labs)  
        dts = (_DateTimeStamp * size)()
            
        _lib_call("TOSDB_GetTopicFrameStrings", 
                  self._name, 
                  item.encode("ascii"),
                  pstrs, 
                  size, 
                  data_str_max + 1,                                            
                  plabs if labels else _ppchar_(), 
                  label_str_max + 1,
                  dts if date_time else _PTR_(_DateTimeStamp)(),
                  arg_types=(_str_, _str_, _ppchar_, _uint32_, _uint32_,
                             _ppchar_, _uint32_, _PTR_(_DateTimeStamp)))    

        dat = list(_zip_cstr_dt(pstrs,dts) if date_time else _map_cstr(pstrs))        
        if labels:      
            nt = _gen_namedtuple(_str_clean(item)[0], _str_clean(*_map_cstr(plabs)))            
            return nt(*dat)                     
        else:
            return dat


    # total_frame not party of abstract base, so include doc string
    def total_frame(self, date_time=False, labels=True, data_str_max=STR_DATA_SZ, 
                    label_str_max=MAX_STR_SZ):      
        """ Return ALL of the block's most recent values:

        total_frame(self, date_time=False, labels=True, data_str_max=STR_DATA_SZ, 
                    label_str_max=MAX_STR_SZ)              
     
        date_time     :: bool :: include TOSDB_DateTime objects 
        labels        :: bool :: include item and topic labels
        data_str_max  :: int  :: maximum length of string data returned
        label_str_max :: int  :: maximum length of label strings returned 

        if labels == date_time == True:  returns -> dict of namedtuple of 2-tuple**
        elif labels == True:             returns -> dict of namedtuple**
        elif date_time == True:          returns -> list of list of 2-tuple**
        else:                            returns -> list of list**

        **data are ALL of type str (frame can contain topics of different type)

        throws TOSDB_DataTimeError, TOSDB_CLibError     
        """
        p = _partial(self.topic_frame, date_time=date_time, labels=labels, 
                     data_str_max=data_str_max, label_str_max=label_str_max)
        if labels:
            return {x : p(x) for x in self._items}    
        else:               
            return list(map(p,self._items))


    def _handle_raw(self, s):       
        if len(s) < 1 or len(s) > MAX_STR_SZ:            
            raise TOSDB_ValueError("invalid str len: " + str(len(s)))
        return s.upper()

        
    def _handle_raw_item(self, item, throw_if_not_in_block=True):
        if type(item) is not str:
            raise TOSDB_TypeError("item must be str")

        item = self._handle_raw(item)                                  
        if throw_if_not_in_block and item not in self._items:
            raise TOSDB_ValueError("item '" + str(item) + "' not in block")
        
        return item

        
    def _handle_raw_topic(self, topic, throw_if_not_in_block=True):
        if topic in TOPICS:
            topic = topic.val
        elif type(topic) is not str:
            raise TOSDB_TypeError("topic must be TOPICS enum or str")

        topic = self._handle_raw(topic)
        if topic not in TOPICS.val_dict:        
            raise TOSDB_ValueError("invalid topic: " + topic)
                                  
        if throw_if_not_in_block and topic not in self._topics:
            raise TOSDB_ValueError("topic '" + str(topic) + "' not in block")
        
        return topic
    


### HOW WE ACCESS THE UNDERLYING C CALLS ###
def _lib_call(f, *fargs, ret_type=_int_, arg_types=None, error_check=True):        
    if not _dll:
        raise TOSDB_CLibError("tos-databridge DLL is currently not loaded")
        return # in case exc gets ignored on clean_up
    ret = None
    err = None
    try:        
        attr = getattr(_dll, str(f))
        if ret_type:
            attr.restype = ret_type
        if isinstance(arg_types, tuple):
            attr.argtypes = arg_types
        elif arg_types is not None:
            raise ValueError('arg_types should be tuple or None')
        ret = attr(*fargs) # <- THE ACTUAL LIBRRY CALL      
    except BaseException as e:
        err = e

    if err:
        raise TOSDB_CLibError("unable to execute library function [%s]" % f, err)
    elif error_check and ret:               
        raise TOSDB_CLibError("library function [%s] returned error code [%i,%s]" \
                              % (f, ret, _lookup_error_name(ret)))
    
    return ret  


def _lookup_error_name(e):    
    try:
        return ERROR_LOOKUP[e]        
    except:
        dbase = min(ERROR_LOOKUP.keys())
        if e < dbase:
            return "ERROR_DECREMENT_BASE(" + str(e - dbase) + ")"
        return '***unrecognized error code***'    


# create a custom namedtuple with an i.d tag for special pickling
def _gen_namedtuple(name, attrs):
    nt = _namedtuple(name, attrs)
    setattr(nt, NTUP_TAG_ATTR, True)
    return nt


# clean strings for namedtuple fields
def _str_clean(*strings):    
    fin = []
    for s in strings:               
        tmp = ''
        if not _match(_REGEX_LETTER, s):
            s = 'X_' + s
        for sub in _split(_REGEX_NON_ALNUM, s):
            tmp += sub
        fin.append(tmp)
    return fin
