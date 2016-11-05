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


from ._tosdb import * 
from .meta_enum import MetaEnum 

import sys as _sys
import struct as _struct
from collections import namedtuple as _namedtuple
from abc import ABCMeta as _ABCMeta, abstractmethod as _abstractmethod

from time import mktime as _mktime, struct_time as _struct_time, \
                 asctime as _asctime, localtime as _localtime, \
                 strftime as _strftime

from ctypes import Structure as _Structure, \
                   c_long as _long_, \
                   c_int as _int_, \
                   c_double as _double_, \
                   c_float as _float_, \
                   c_longlong as _longlong_, \
                   c_char_p as _str_   

BASE_YR = 1900
NTUP_TAG_ATTR = "_dont_worry_about_why_this_attribute_has_a_weird_name_"

class _TOSDB_DataBlock(metaclass=_ABCMeta):
  """ The DataBlock interface """
  @classmethod
  def __subclasshook__(cls, C):
    if cls is _TOSDB_DataBlock:
      for ameth in cls.__abstractmethods__:
        if not any(ameth in b.__dict__ for b in C.__mro__):
          return False
      return True
    return NotImplemented

  @_abstractmethod
  def __str__(): 
    pass

  @_abstractmethod
  def info(): 
    """ Returns a more readable dict of info about the underlying block """
    pass

  @_abstractmethod
  def get_block_size(): 
    """ Returns the amount of historical data stored in the block """
    pass

  @_abstractmethod
  def set_block_size(): 
    """ Changes the amount of historical data stored in the block """
    pass

  @_abstractmethod
  def stream_occupancy(): 
    """ Returns the current number of data-points pushed into the data-stream
        
    item: any item string in the block
    topic: any topic string in the block
    """ 
    pass

  @_abstractmethod
  def items(): 
    """ Returns the items currently in the block (and not pre-cached).
    
    str_max: the maximum length of item strings returned
    returns -> list of strings 
    """
    pass

  @_abstractmethod
  def topics(): 
    """ Returns the topics currently in the block (and not pre-cached).
    
    str_max: the maximum length of topic strings returned  
    returns -> list of strings 
    """
    pass


### --- BEGIN --- Oct 30 2016

  @_abstractmethod
  def items_precached(): 
    """ Returns the items currently in the block's pre-cache.
    
    str_max: the maximum length of item strings returned
    returns -> list of strings 
    """
    pass

  @_abstractmethod
  def topics_precached(): 
    """ Returns the topics currently in the block's pre-cache.
    
    str_max: the maximum length of topic strings returned  
    returns -> list of strings 
    """
    pass

### --- END --- Oct 30 2016


  @_abstractmethod
  def add_items(): 
    """ Add items (ex. 'IBM', 'SPY') to the block.

    NOTE: if there are no topics currently in the block, these items will 
    be pre-cached and appear not to exist, until a valid topic is added.

    *items: any numer of item strings
    """  
    pass

  @_abstractmethod
  def add_topics(): 
    """ Add topics (ex. 'LAST', 'ASK') to the block.

    NOTE: if there are no items currently in the block, these topics will 
    be pre-cached and appear not to exist, until a valid item is added.

    *topics: any numer of topic strings
    """     
    pass

  @_abstractmethod
  def remove_items(): 
    """ Remove items (ex. 'IBM', 'SPY') from the block.

    NOTE: if this call removes all items from the block the remaining topics 
    will be pre-cached and appear not to exist, until a valid item is re-added.

    *items: any numer of item strings
    """
    pass

  @_abstractmethod
  def remove_topics(): 
    """ Remove topics (ex. 'LAST', 'ASK') from the block.

    NOTE: if this call removes all topics from the block the remaining items 
    will be pre-cached and appear not to exist, until a valid topic is re-added.

    *topics: any numer of topic strings
    """
    pass

  @_abstractmethod
  def get(): 
    """ Return a single data-point from the data-stream
    
    item: any item string in the block
    topic: any topic string in the block
    date_time: (True/False) attempt to retrieve a TOSDB_DateTime object   
    indx: index of data-points [0 to block_size), [-block_size to -1]
    check_indx: throw if datum doesn't exist at that particular index
    data_str_max: the maximum size of string data returned
    """
    pass

  @_abstractmethod
  def stream_snapshot(): 
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
    pass

  @_abstractmethod
  def stream_snapshot_from_marker(): 
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

    State (3) occurs when an inadequately large buffer is used. The call
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
    pass

  @_abstractmethod
  def item_frame(): 
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
    pass

  @_abstractmethod
  def topic_frame(): 
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
    pass



class TOSDB_Error(Exception):
  """ Base exception for tosdb """  
  def __init__(self,  *messages): 
    super().__init__(*messages)


class TOSDB_InitError(TOSDB_Error):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_PlatformError(TOSDB_Error):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_CLibError(TOSDB_Error):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_DateTimeError(TOSDB_Error):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_DataError(TOSDB_Error):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_ValueError(TOSDB_Error, ValueError):
  def __init__(self, *messages):
    super().__init__(*messages) 


class TOSDB_TypeError(TOSDB_Error, TypeError):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_IndexError(TOSDB_Error, IndexError):
  def __init__(self, *messages):
    super().__init__(*messages)


class TOSDB_VirtualizationError(TOSDB_Error):
  def __init__(self, *messages):
    super().__init__(*messages)


def wrap_impl_error(clss):
  if not isinstance(clss, Exception):
    raise TypeError("clss must be instance of Exception")    
  our_def = """\
class TOSDB_ImplErrorWrapper({clss}):
    def __init__(self, error):
        {clss}(error)
""".format(clss=(type(clss).__name__))    
  exec(our_def)
  our_obj = eval("TOSDB_ImplErrorWrapper")(clss)
  try:
    our_obj.__module__ = _sys._getframe(1).f_globals.get('__name__')
  except:
    pass
  return our_obj

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

class _DateTimeStamp(_Structure):
  """ Our 'private' C stdlib date-time struct w/ added micro-second field """
  class _CTime(_Structure):
    """ C stdlib tm struct """
    _fields_ =  [("tm_sec", _int_),
                 ("tm_min", _int_),
                 ("tm_hour", _int_),
                 ("tm_mday", _int_),
                 ("tm_mon", _int_),
                 ("tm_year", _int_),
                 ("tm_wday", _int_),
                 ("tm_yday", _int_),
                 ("tm_isdst", _int_)]
            
  _fields_ =  [("ctime_struct", _CTime), ("micro_second", _long_)]


class TOSDB_DateTime(
      _namedtuple("DateTime",["micro","sec","min","hour","day","month","year"])):
  """ The object used for handling DateTime values

  TOSDB_DateTime is built on top of a simplified named tuple. It can be 
  constructed from _DateTimeStamp, _struct_time from the time.py library, or 
  itself (copy construction). It can be pickled (serialized); allows for basic 
  addition and subtraction returning a new object or a DateTimeDiff tuple, 
  respectively; overloads comparison operators; and provides static utility 
  functions to convert betwen DateTimeDiff objects and microseconds.       

  obj: either a _DateTimeStamp,  time._struct_time, or TOSDB_DateTime object           
  micro_second: custom micro_second value
  """
  dtd_tuple = \
    _namedtuple("DateTimeDiff",["micro","sec","min","hour","day","sign"])
  
  _mktime = None # cache total sec from epoch to make ops easier

  def __getnewargs__(self):        
    return ((self.sec,self.min,self.hour,self.day,self.month,self.year),self.micro)

  def __getstate__(self):
    return self.__dict__
    
  def __new__(cls, obj, micro_second = 0):
    if micro_second != 0:
      micro_second %= 1000000
    _stime = None      
    if isinstance(obj, _DateTimeStamp):
      _stime = cls._to_struct_time(obj)
      micro_second = obj.micro_second
    elif isinstance(obj, _struct_time):
      _stime = obj
    elif isinstance(obj, TOSDB_DateTime):
      return super(TOSDB_DateTime, cls).__new__(cls, obj.micro, obj.sec, 
                                                obj.min, obj.hour, obj.day, 
                                                obj.month, obj.year)
    elif isinstance(obj, tuple):
      return super(TOSDB_DateTime, cls).__new__(cls, micro_second, *obj)
    else:
      raise TOSDB_DateTimeError("invalid 'object' passed to __new__")
    
    return super(TOSDB_DateTime, cls).__new__(cls, micro_second, _stime.tm_sec, 
                                              _stime.tm_min, _stime.tm_hour, 
                                              _stime.tm_mday, _stime.tm_mon, 
                                              _stime.tm_year)


  def __init__(self, obj, micro_second=0):
    if isinstance(obj, _DateTimeStamp):
      try:
        self._mktime = _mktime(TOSDB_DateTime._to_struct_time(obj))
      except:
        self._mktime = 0
    elif isinstance(obj, _struct_time):
      self._mktime = _mktime(obj)
    elif isinstance(obj, TOSDB_DateTime):
      self._mktime = obj._mktime
    else:
      raise TOSDB_DateTimeError("invalid 'object' passed to __init__")

  def __str__(self):
    ftime = _strftime("%m/%d/%y %H:%M:%S", _localtime(self._mktime))
    return ftime + " " + str(self.micro)

  def __add__(self, micro_seconds):
    if not isinstance(micro_seconds, int):            
      raise TOSDB_DateTimeError("micro_seconds must be integer")
    incr_sec = micro_seconds // 1000000
    ms_new = self.micro + (micro_seconds % 1000000 )    
    if ms_new >= 1000000:
      incr_sec += 1
      ms_new -= 1000000
    # reduce to seconds, increment, return to _struct_time    
    mk_time = self._mktime 
    mk_time += incr_sec       
    new_time = _localtime(mk_time) 
    # return a new TOSDB_DateTime 
    return TOSDB_DateTime(new_time, micro_second = ms_new)                
        
  def __sub__(self, other_or_micro_sec):
    other = other_or_micro_sec    
    if isinstance(other , int): # subtracting an integer
      if other < 0:
        return self.__add__(other * -1)
      other_sec = other // 1000000
      ms_diff = self.micro - (other % 1000000)
      if ms_diff < 0: # take a second, if needed
        other_sec += 1
        ms_diff += 1000000
      # reduce to seconds, decrement, return to stuct_time      
      mk_time = self._mktime
      mk_time -= other_sec
      new_time = _localtime(mk_time)      
      return TOSDB_DateTime(new_time, micro_second = ms_diff)            
    elif isinstance(other, TOSDB_DateTime): # subtracting another TOSDB_DateTime
      try: # try to get other's time in seconds                             
        sec_diff = self._mktime - other._mktime     
        ms_diff = self.micro - other.micro
        # convert the diff in micro_seconds to the diff-tuple
        return TOSDB_DateTime.micro_to_dtd(sec_diff * 1000000 + ms_diff)     
      except Exception as e:
        raise TOSDB_DateTimeError("invalid TOSDB_DateTime object", e)
    else:
      raise TOSDB_DateTimeError("other_or_micro_sec not TOSDB_DateTime or int")


  def __lt__(self, other):
    return self._compare(other) < 0
    
  def __ge__(self, other):
    return not self.__lt__(other)
    
  def __gt__(self, other):
    return self._compare(other) > 0
    
  def __le__(self, other):
    return not self.__gt__(other)
    
  def __eq__(self, other):
    return not self._compare(other)
    
  def __ne__(self, other):
    return self._compare(other)
    
  def _compare(self, other):
    if not isinstance(other, TOSDB_DateTime):
      raise TOSDB_DateTimeError("unable to compare; unorderable types") 
    diff = self._mktime - other._mktime
    if diff == 0:            
      diff = self.micro - other.micro   
    return (-1 if diff < 0 else 1) if diff else 0

  @property
  def mktime(self):
    return self._mktime
    
  @staticmethod
  def _to_struct_time(obj):
    return _struct_time([obj.ctime_struct.tm_year + BASE_YR,
                         obj.ctime_struct.tm_mon +1,
                         obj.ctime_struct.tm_mday,
                         obj.ctime_struct.tm_hour,
                         obj.ctime_struct.tm_min,
                         obj.ctime_struct.tm_sec,
                         obj.ctime_struct.tm_wday + 1,
                         obj.ctime_struct.tm_yday + 1,
                         obj.ctime_struct.tm_isdst ])

  @staticmethod
  def micro_to_dtd(micro_seconds):
    """ Converts micro_seconds to DateTimeDiff  """
    pos = True
    if micro_seconds < 0:
      pos = False
      micro_seconds = abs(micro_seconds)
    new_ms =  int(micro_seconds % 1000000)
    micro_seconds //= 1000000
    new_sec = int(micro_seconds % 60)
    micro_seconds //= 60
    new_min = int(micro_seconds % 60)
    micro_seconds //= 60
    new_hour = int(micro_seconds % 24)
    new_day = int(micro_seconds // 24)    

    return TOSDB_DateTime.dtd_tuple(new_ms, new_sec, new_min, new_hour, new_day, 
                                    ('+' if pos else '-'))    
    
  @staticmethod
  def dtd_to_micro(dtd):
    """ Converts DateTimeDiff to micro_seconds """
    tmp = dtd.day * 24 + dtd.hour
    tmp = tmp*60 + dtd.min
    tmp = tmp*60 + dtd.sec
    tmp *= 1000000
    tmp += dtd.micro
    if dtd.sign == '+':
      return tmp 
    elif dtd.sign == '-':
      return tmp * -1
    else:
      raise TOSDB_DateTimeError("invalid 'sign' field in DateTimeDiff")


def abort_init_after_warn():
  print("*WARNING* by not supplying --root, --path, or --noinit(-n) arguments "
        + "you are opting for a default search root of 'C:\\'. This will "
        + "attempt to search the ENTIRE disk/drive for the tos-databridge "
        + "library. It's recommended you restart the program with the library " 
        + "path (after --path) or a narrower directory root (after --root).")    
  if input("Do you want to continue anyway?") in ['y','Y','YES','yes','Yes']:
    return False
  else:
    print("- init(root='C:\\') aborted")
    return True
        
# convert type_bits to string and ctypes type
def _type_switch(type_bits):
  if type_bits == INTGR_BIT | QUAD_BIT: 
    return ("LongLong", _longlong_)
  elif type_bits == INTGR_BIT:
    return ("Long", _long_)
  elif type_bits == QUAD_BIT:
    return ("Double", _double_)
  elif type_bits == 0:
    return ("Float", _float_)
  else: # default to string
    return("String", _str_)
