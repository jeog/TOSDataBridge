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
               listdir as _listdir, sep as _sep

from re import compile as _compile, search as _search, match as _match, \
               split as _split

if _system() not in ["Windows","windows","WINDOWS"]: 
  print("error: tosdb/_win.py is for windows only !", file=_stderr)
  exit(1)

from ctypes import WinDLL as _WinDLL, \
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
                   c_uint_32 as _uint32_, \
                   c_uint_8 as _uint8_

_pchar_ = _PTR_(_char_)
_ppchar_ = _PTR_(_pchar_)  
_cast_cstr = lambda x: _cast(x,_str_).value.decode() 

DLL_BASE_NAME = "tos-databridge"
SYS_ARCH_TYPE = "x64" if (_log(_maxsize * 2, 2) > 33) else "x86"
MIN_MARGIN_OF_SAFETY = 10

_REGEX_NON_ALNUM = _compile("[\W+]")
_REGEX_LETTER = _compile("[a-zA-Z]")
_VER_SFFX = '[\d]{1,2}.[\d]{1,2}'
_REGEX_VER_SFFX = _compile('-' + _VER_SFFX + '-')
_REGEX_DLL_NAME = \
  _compile('^('+DLL_BASE_NAME + '-)' + _VER_SFFX + '-' + SYS_ARCH_TYPE +'(.dll)$')
           
_dll = None

## we added a lock to the _call() from VTOSDB_DataBlock
##   how do we want to handle concurrent calls at this level ?
##   do we even need to?
      
def init(dllpath=None, root="C:\\", bypass_check=False):
  """ Initialize the underlying tos-databridge DLL

  dllpath: string of the exact path of the DLL
  root: string of the directory to start walking/searching to find the DLL
  """  
  global _dll
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

  try:
    if dllpath is None:
      matcher = _partial(_match, _REGEX_DLL_NAME)  # regex match function
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
    _dll = _WinDLL(dllpath)
    print("+ Using Module ", dllpath)
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
  """ Attempts to connect to the library """ # lib returns 0 on success
  return _lib_call("TOSDB_Connect", error_check=False) == 0 


def connected():
  """ Returns true if there is an active connection to the library """
  i = _lib_call("TOSDB_IsConnected", ret_type=_uint_, error_check=False)
  return bool(i)
         
       
def clean_up():
  """ Clean up shared resources. ! CALL BEFORE EXITING INTERPRETER ! """
  global _dll
  if _dll is not None:       
    if _lib_call("TOSDB_CloseBlocks", error_check=False):
      print("- Error Closing Blocks")
    else:
      print("+ Closing Blocks")
    _lib_call("TOSDB_Disconnect", error_check=False)
    print("+ Disconnecting From Service \ Engine")        
    print("+ Closing Module ", _dll._name)
    del _dll
    _dll = None
 

@_contextmanager
def Init(dllpath=None, root="C:\\", bypass_check=False):
  try:
    if not init(dllpath, root, bypass_check):
      raise TODB_InitError("failed to initilize library")
    if not connected():      
      if not connect(): # try again
        raise TODB_InitError("failed to connect to library")
    yield
  finally:
    clean_up()


def get_block_limit():
  """ Returns the block limit of C/C++ RawDataBlock factory """
  return _lib_call("TOSDB_GetBlockLimit", ret_type=_uint32_, error_check=False)


def set_block_limit(new_limit):
  """ Changes the block limit of C/C++ RawDataBlock factory """
  _lib_call("TOSDB_SetBlockLimit", new_limit, ret_type=_uint32_, error_check=False)


def get_block_count():
  """ Returns the count of current instantiated blocks """
  return _lib_call("TOSDB_GetBlockCount", ret_type=_uint32_, error_check=False)


def type_bits(topic):
  """ Returns the type bits for a particular 'topic'

  topic: string representing a TOS data field('LAST','ASK', etc)
  returns -> value that can be logical &'d with type bit contstants 
  (ex. QUAD_BIT)
  """
  tybits = _uint8_()
  _lib_call("TOSDB_GetTypeBits", topic.upper().encode("ascii"), _pointer(tybits))

  return tybits.value


def type_string(topic):
  """ Returns a platform-dependent string of the type of a particular 'topic'

  topic: string representing a TOS data field('LAST','ASK', etc)
  """
  tystr = _BUF_(MAX_STR_SZ + 1)

  _lib_call("TOSDB_GetTypeString", 
            topic.upper().encode("ascii"), 
            tystr, 
            (MAX_STR_SZ + 1))

  return tystr.value.decode()


####################
_on_exit(clean_up) #
####################

class TOSDB_DataBlock(_TOSDB_DataBlock):
  """ The main object for storing TOS data.    

  size: how much historical data to save
  date_time: should block include date-time stamp with each data-point?
  timeout: how long to wait for responses from TOS-DDE server 

  Please review the attached README.html for details.
  """    
  def __init__(self, size=1000, date_time=False, timeout=DEF_TIMEOUT):        
    self._name = (_uuid4().hex).encode("ascii")
    self._valid = False
    _lib_call("TOSDB_CreateBlock", self._name, size, date_time, timeout)                 
    self._valid= True
    self._block_size = size
    self._timeout = timeout
    self._date_time = date_time
    self._items = []   
    self._topics = []        
  

  def __del__(self): 
    # for convenience, no guarantee
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
    
  # private _valid calls assume .upper() already called

  def _valid_item(self, item):
    if not self._items: # in case items came out of pre-cache
      self._items = self.items()
    if item not in self._items:
      raise TOSDB_ValueError("item " + str(item) + " not found")


  def _valid_topic(self, topic):
    if not self._topics: # in case topics came out of pre-cache
      self._topics = self.topics()
    if topic not in self._topics:
      raise TOSDB_ValueError("topic " + str(topic) + " not found")


  def _item_count(self):       
    i_count = _uint32_()
    _lib_call("TOSDB_GetItemCount", self._name, _pointer(i_count))
    return i_count.value


  def _topic_count(self):        
    t_count = _uint32_()
    _lib_call("TOSDB_GetTopicCount", self._name, _pointer(t_count))
    return t_count.value


  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def info(self):      
    return {"Name": self._name.decode('ascii'), 
            "Items": self.items(),
            "Topics": self.topics(), 
            "Size": self._block_size,
            "DateTime": "Enabled" if self._date_time else "Disabled",
            "Timeout": self._timeout}
    

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def get_block_size(self):          
    b_size = _uint32_()
    _lib_call("TOSDB_GetBlockSize", self._name, _pointer(b_size))
    return b_size.value
    

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def set_block_size(self, sz):  
    _lib_call("TOSDB_SetBlockSize", self._name, sz)
    self._block_size = sz
       

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock     
  def stream_occupancy(self, item, topic):              
    item = item.upper()
    topic = topic.upper()
    self._valid_item(item)
    self._valid_topic(topic)
    occ = _uint32_()
    
    _lib_call("TOSDB_GetStreamOccupancy", 
              self._name, 
              item.encode("ascii"),
              topic.encode("ascii"), 
              _pointer(occ),
              arg_list=[_str_, _str_, _str_, _PTR_(_uint32_)])
    
    return occ.value
    

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def items(self, str_max=MAX_STR_SZ):  
    size = self._item_count()  
    strs = [_BUF_(str_max + 1) for _ in range(size)]      
    strs_array = (_pchar_* size)(*[ _cast(s, _pchar_) for s in strs]) 
    
    _lib_call("TOSDB_GetItemNames", 
              self._name, 
              strs_array, 
              size, 
              str_max + 1, 
              arg_list=[_str_, _ppchar_, _uint32_, _uint32_])
    
    return [_cast_cstr(s) for s in strs_array]            
       
       
  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def topics(self,  str_max=MAX_STR_SZ):
    size = self._topic_count()
    strs = [_BUF_(str_max + 1) for _ in range(size)]     
    strs_array = (_pchar_* size)(*[ _cast(s, _pchar_) for s in strs])   
        
    _lib_call("TOSDB_GetTopicNames", 
              self._name, 
              strs_array, 
              size, 
              str_max + 1, 
              arg_list=[_str_,  _ppchar_, _uint32_, _uint32_])               
    
    return [_cast_cstr(s) for s in strs_array] 
      
 
  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock   
  def add_items(self, *items):              
    mtup = tuple(s.encode("ascii").upper() for s in items)
    items_type = _str_ * len(mtup)
    citems = items_type(*mtup)
    _lib_call("TOSDB_AddItems", self._name, citems, len(mtup), 
              arg_list=[_str_,  _ppchar_, _uint32_])    
            
    if not self._items and not self._topics:
      self._topics = self.topics() # in case topics came out of pre-cache
    self._items = self.items()
       

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def add_topics(self, *topics):        
    mtup = tuple(s.encode("ascii").upper() for s in topics)
    topics_type = _str_ * len(mtup)
    ctopics = topics_type(*mtup)
    _lib_call("TOSDB_AddTopics", self._name, ctopics, len(mtup), 
              arg_list=[_str_,  _ppchar_, _uint32_])  

    if not self._items and not self._topics:
      self._items = self.items() # in case items came out of pre-cache
    self._topics = self.topics()


  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def remove_items(self, *items):
    for item in items:
      _lib_call("TOSDB_RemoveItem", self._name, item.encode("ascii").upper())     
    self._items = self.items()


  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def remove_topics(self, *topics):
    for topic in topics:
      _lib_call("TOSDB_RemoveTopic", self._name, topic.encode("ascii").upper())
    self._topics = self.topics()
        

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def get(self, item, topic, date_time=False, indx=0, check_indx=True, 
          data_str_max=STR_DATA_SZ):
     
    item = item.upper()
    topic = topic.upper()
    
    if date_time and not self._date_time:
      raise TOSDB_DateTimeError("date_time not available for this block")

    self._valid_item(item)
    self._valid_topic(topic)
    
    if indx < 0:
      indx += self._block_size
    if indx >= self._block_size:
      raise TOSDB_IndexError("invalid index value passed to get()")   
    if check_indx and indx >= self.stream_occupancy(item, topic):
      raise TOSDB_DataError("data not available at this index yet " +
                            "(disable check_indx to avoid this error)")

    dts = _DateTimeStamp()      
    tbits = type_bits(topic)
    tytup = _type_switch(tbits)
    
    if tytup[0] == "String":
      ret_str = _BUF_(data_str_max + 1)
      _lib_call("TOSDB_GetString", 
                self._name, 
                item.encode("ascii"), 
                topic.encode("ascii"), 
                indx, 
                ret_str, 
                data_str_max + 1,
                _pointer(dts) if date_time else _PTR_(_DateTimeStamp)(),
                arg_list=[_str_,  _str_, _str_, _long_, _pchar_,
                          _uint32_, _PTR_(_DateTimeStamp)])   
   
      if date_time :
        return (ret_str.value.decode(), TOSDB_DateTime(dts))
      else:
        return ret_str.value.decode()

    else:
      val = tytup[1]()
      _lib_call("TOSDB_Get"+tytup[0], 
                self._name,
                item.encode("ascii"), 
                topic.encode("ascii"),
                indx, 
                _pointer(val),
                _pointer(dts) if date_time else _PTR_(_DateTimeStamp)(),
                arg_list=[_str_, _str_, _str_, _long_, _PTR_(tytup[1]),
                          _PTR_(_DateTimeStamp)])     

      if date_time:
        return (val.value, TOSDB_DateTime(dts))
      else:
        return val.value


  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def stream_snapshot(self, item, topic, date_time=False, end=-1, beg=0, 
                      smart_size=True, data_str_max=STR_DATA_SZ):

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
    
    dtss = (_DateTimeStamp * size)()
    tbits = type_bits(topic)
    tytup = _type_switch(tbits)
    
    if tytup[0] == "String":      
      strs = [_BUF_( data_str_max +1) for _ in range(size)]              
      strs_array = (_pchar_ * size)(*[ _cast(s, _pchar_) for s in strs]) 

      _lib_call("TOSDB_GetStreamSnapshotStrings", 
                self._name,
                item.encode("ascii"), 
                topic.encode("ascii"),
                strs_array, 
                size, 
                data_str_max + 1,                
                dtss if date_time else _PTR_(_DateTimeStamp)(),
                end, 
                beg,
                arg_list=[_str_, _str_, _str_, _ppchar_, _uint32_, _uint32_, 
                          _PTR_(_DateTimeStamp), _long_, _long_ ])   

      if date_time:
        adj_dts = [TOSDB_DateTime(x) for x in dtss]         
        return [sd for sd in zip(map(_cast_cstr, strs_array), adj_dts)]        
      else:        
        return [_cast_cstr(s) for s in strs_array]

    else: 
      num_array = (tytup[1] * size)()   
      _lib_call("TOSDB_GetStreamSnapshot"+tytup[0]+"s", 
                self._name,
                item.encode("ascii"), 
                topic.encode("ascii"),
                num_array, 
                size,
                dtss if date_time else _PTR_(_DateTimeStamp)(),
                end, 
                beg,
                arg_list=[_str_, _str_, _str_, _PTR_(tytup[1]), _uint32_, 
                          _PTR_(_DateTimeStamp), _long_, _long_ ]) 
     
      if date_time:
        adj_dts = [TOSDB_DateTime(x) for x in dtss]
        return [nd for nd in zip(num_array,adj_dts)]       
      else:
        return [n for n in num_array]


  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def stream_snapshot_from_marker(self, item, topic, date_time=False, beg=0, 
                                  margin_of_safety=100, throw_if_data_lost=True,
                                  data_str_max=STR_DATA_SZ):
    
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
    _lib_call("TOSDB_IsMarkerDirty", 
              self._name,
              item.encode("ascii"), 
              topic.encode("ascii"),
              _pointer(is_dirty), 
              arg_list=[_str_, _str_, _str_, _PTR_(_uint_)]) 

    if is_dirty and throw_if_data_lost:
      raise TOSDB_DataError("marker is already dirty")
    
    mpos = _longlong_()
    _lib_call("TOSDB_GetMarkerPosition", 
              self._name,
              item.encode("ascii"), 
              topic.encode("ascii"),
              _pointer(mpos), 
              arg_list=[_str_, _str_, _str_, _PTR_(_longlong_)])    
    
    cur_sz = mpos.value - beg + 1
    if cur_sz < 0:
      return None
    
    safe_sz = cur_sz + margin_of_safety
    dtss = (_DateTimeStamp * safe_sz)()
    tbits = type_bits(topic)
    tytup = _type_switch(tbits)
    get_size = _long_()
    
    if tytup[0] == "String":
      strs = [ _BUF_( data_str_max +1) for _ in range(safe_sz) ]   
      strs_array = (_pchar_ * safe_sz)(*[_cast(s,_pchar_) for s in strs]) 

      _lib_call("TOSDB_GetStreamSnapshotStringsFromMarker", 
                self._name,
                item.encode("ascii"), 
                topic.encode("ascii"),
                strs_array, 
                safe_sz, 
                data_str_max + 1,
                dtss if date_time else _PTR_(_DateTimeStamp)(),     
                beg, 
                _pointer(get_size),
                arg_list=[_str_, _str_, _str_, _ppchar_, _uint32_, _uint32_,
                          _PTR_(_DateTimeStamp), _long_, _PTR_(_long_)]) 
         
      get_size = get_size.value
      if get_size == 0:
        return None
      elif get_size < 0:
        if throw_if_data_lost:
          raise TOSDB_DataError("data lost behind the 'marker'")
        else:
          get_size *= -1
      if date_time:
        adj_dts = [TOSDB_DateTime(x) for x in dtss[:get_size]]      
        return [sd for sd in zip(map(_cast_cstr, strs_array[:get_size]), adj_dts)]    
      else:
        return [_cast_cstr(s) for s in strs_array[:get_size]]

    else:
      num_array = (tytup[1] * safe_sz)()   
      _lib_call("TOSDB_GetStreamSnapshot" + tytup[0] + "sFromMarker", 
                self._name,
                item.encode("ascii"), 
                topic.encode("ascii"),
                num_array, 
                safe_sz,
                dtss if date_time else _PTR_(_DateTimeStamp)(),     
                beg, 
                _pointer(get_size),
                arg_list=[_str_, _str_, _str_, _PTR_(tytup[1]), _uint32_, 
                          _PTR_(_DateTimeStamp), _long_, _PTR_(_long_)]) 
    
      get_size = get_size.value
      if get_size == 0:
        return None
      elif get_size < 0:
        if throw_if_data_lost:
          raise TOSDB_DataError("data lost behind the 'marker'")
        else:
          get_size *= -1            
      if date_time:
        adj_dts = [TOSDB_DateTime(x) for x in dtss[:get_size]]
        return [nd for nd in zip(num_array[:get_size], adj_dts)]       
      else:
        return [n for n in num_array[:get_size]]    
    

  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def item_frame(self, topic, date_time=False, labels=True, 
                 data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
   
    topic = topic.upper()
    
    if date_time and not self._date_time:
      raise TOSDB_DateTimeError("date_time not available for this block")
    
    self._valid_topic(topic)
    
    size = self._item_count()
    dtss = (_DateTimeStamp * size)()  
    labs = [_BUF_(label_str_max+1) for _ in range(size)] 
    labs_array = (_pchar_ * size)(*[_cast(s, _pchar_) for s in labs])  
    tbits = type_bits(topic)
    tytup = _type_switch(tbits)
    
    if tytup[0] is "String":      
      strs = [_BUF_( data_str_max + 1) for _ in range(size)]
      strs_array = (_pchar_ * size)(*[_cast(s, _pchar_) for s in strs]) 

      _lib_call("TOSDB_GetItemFrameStrings", 
                self._name, 
                topic.encode("ascii"),
                strs_array, 
                size, 
                data_str_max + 1,
                labs_array if labels else _ppchar_(), 
                label_str_max + 1,
                dtss if date_time else _PTR_(_DateTimeStamp)(),
                arg_list=[_str_, _str_, _ppchar_, _uint32_, _uint32_,
                          _ppchar_, _uint32_, _PTR_(_DateTimeStamp)])       

      if labels:
        l_map = map(_cast_cstr, labs_array)
        _nt_ = _gen_namedtuple(_str_clean(topic)[0], _str_clean(*l_map)) 
        if date_time:
          adj_dts = [TOSDB_DateTime(x) for x in dtss]
          return _nt_(*zip(map(_cast_cstr, strs_array),adj_dts))
        else:
          return _nt_(*map(_cast_cstr, strs_array))    
      else:
        if date_time:
          adj_dts = [TOSDB_DateTime(x) for x in dtss]
          return list(zip(map(_cast_cstr, strs_array),adj_dts))
        else:
          return list(map(_cast_cstr, strs_array))  
   
    else: 
      num_array = (tytup[1] * size)()   
      _lib_call("TOSDB_GetItemFrame"+tytup[0]+"s", 
                self._name, 
                topic.encode("ascii"), 
                num_array, 
                size,
                labs_array if labels else _ppchar_(), 
                label_str_max + 1,
                dtss if date_time else _PTR_(_DateTimeStamp)(),       
                arg_list=[_str_, _str_, _PTR_(tytup[1]), _uint32_, _ppchar_, 
                          _uint32_, _PTR_(_DateTimeStamp)])    
  
      if labels:   
        l_map = map(_cast_cstr, labs_array)
        _nt_ = _gen_namedtuple(_str_clean(topic)[0], _str_clean(*l_map)) 
        if date_time:
          adj_dts = [TOSDB_DateTime(x) for x in dtss]
          return _nt_(*zip(num_array,adj_dts))
        else:
          return _nt_(*num_array)   
      else:
        if date_time:
          adj_dts = [TOSDB_DateTime(x) for x in dtss]
          return list(zip(num_array,adj_dts))
        else:
          return [n for n in num_array]    


  @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
  def topic_frame(self, item, date_time=False, labels=True, 
                  data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
   
    item = item.upper()
    
    if date_time and not self._date_time:
      raise TOSDB_DateTimeError("date_time not available for this block")
    
    self._valid_item( item)
    
    size = self._topic_count()
    dtss = (_DateTimeStamp * size)()          
    labs = [_BUF_(label_str_max + 1) for _ in range(size)] 
    strs = [_BUF_(data_str_max + 1) for _ in range(size)]    
    labs_array = (_pchar_ * size)(*[_cast(s, _pchar_) for s in labs]) 
    strs_array = (_pchar_ * size)(*[_cast(s, _pchar_) for s in strs])
        
    _lib_call("TOSDB_GetTopicFrameStrings", 
              self._name, 
              item.encode("ascii"),
              strs_array, 
              size, 
              data_str_max + 1,                                            
              labs_array if labels else _ppchar_(), 
              label_str_max + 1,
              dtss if date_time else _PTR_(_DateTimeStamp)(),
              arg_list=[_str_, _str_, _ppchar_, _uint32_, _uint32_,
                        _ppchar_, _uint32_, _PTR_(_DateTimeStamp)])    

    if labels:
      l_map = map(_cast_cstr, labs_array)
      _nt_ = _gen_namedtuple(_str_clean(item)[0], _str_clean(*l_map))        
      if date_time:
        adj_dts = [TOSDB_DateTime(x) for x in dtss]
        return _nt_(*zip(map(_cast_cstr, strs_array), adj_dts))                
      else:
        return _nt_(*map(_cast_cstr, strs_array))            
    else:
      if date_time:
        adj_dts = [TOSDB_DateTime(x) for x in dtss]
        return list(zip(map(_cast_cstr, strs_array), adj_dts))         
      else:
        return list(map(_cast_cstr, strs_array))


  def total_frame(self, date_time=False, labels=True, data_str_max=STR_DATA_SZ, 
                  label_str_max=MAX_STR_SZ):
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
    p = _partial(self.topic_frame, date_time=date_time, labels=labels, 
                 data_str_max=data_str_max, label_str_max=label_str_max)
    if labels:
      return {x : p(x) for x in self.items()}    
    else:               
      return [p(x) for x in self.items()]
            

# how we access the underlying calls
def _lib_call(f, *args, ret_type=_int_, arg_list=None, error_check=True):        
  if not _dll:
    raise TOSDB_CLibError("tos-databridge DLL is currently not loaded")
    return # in case exc gets ignored on clean_up
  try:        
    attr = getattr(_dll, str(f))
    if ret_type:
      attr.restype = ret_type
    if isinstance(arg_list, list):
      attr.argtypes = arg_list
    elif arg_list is not None:
      raise ValueError('arg_list should be list or None')
    ret = attr(*args) # <- THE ACTUAL LIBRRY CALL
    if error_check and ret:
      raise TOSDB_CLibError("error [%i] returned from library call" % ret, str(f))        
    return ret        
  except Exception as e:
    status = " Not Connected"
    if f != "TOSDB_IsConnected": # avoid infinite recursion
     if connected():
       status = " Connected"               
    raise TOSDB_CLibError("Unable to execute library function", f, status, e)

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
