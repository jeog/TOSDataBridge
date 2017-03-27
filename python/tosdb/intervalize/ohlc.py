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

"""intervalize/ohlc.py:  structure streaming tosdb data in fixed-time intervals

*** IN DEVELOPMENT ***

TOSDB_FixedTimeIntervals:
    base class that pulls numerical data of a certain interval from a thread-safe
    DataBlock, putting it into buffers of fixed-time interval objects (e.g OHLC),
    that can be extracted in various ways
    
TOSDB_OpenHighLowCloseIntervals:
    extends TOSDB_FixedTimeIntervals using 'OHLC' fixed-time interval objects

TOSDB_CloseIntervals:
    extends TOSDB_FixedTimeIntervals using 'C' fixed-time interval objects

OHLC:
    object that stores open, high, low, close of a fixed-time interval along
    with datetime info

C:
    object that stores close of a fixed-time interval along with datetime info

NULL:
    base object that stores no data of a fixed-time interval but does include
    datetime info

*** IN DEVELOPMENT ***
"""

import tosdb
from tosdb import TOPICS, TOSDB_Error
from tosdb.doxtend import doxtend as _doxtend
from tosdb._common import _TOSDB_DataBlock

from threading import Thread as _Thread, Lock as _Lock
from types import MethodType as _MethodType
from collections import deque as _Deque
from concurrent import futures as _futures
from time import time as _time, sleep as _sleep, localtime as _localtime, \
    asctime as _asctime, struct_time as _struct_time, mktime as _mktime, \
    strftime as _strftime, perf_counter as _perf_counter
from itertools import groupby as _groupby
from inspect import signature as _signature

import traceback as _traceback
import time

try:
    type_bits = tosdb.type_bits
except AttributeError:
    type_bits = tosdb.vtype_bits


class NULL:
    DATETIME_FIELDS = ('second','minute','hour','day','month','year')
    
    __slots__ = ('_intervals_since_epoch','_isec','_tfunc')
    
    def __init__(self, intervals_since_epoch, interval_seconds,
                 time_func=time.localtime):        
        self._intervals_since_epoch = intervals_since_epoch
        self._isec = interval_seconds
        if not callable(time_func):
            raise TOSDB_Error("time_func not callable")
        self._tfunc = time_func         

    @classmethod
    def is_null(cls):
        return cls is NULL
        
    second = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_sec)
    minute = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_min)
    hour = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_hour)
    day = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_mday)
    month = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_mon)    
    year = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_year)
    datetime = property(lambda s: (s.second, s.minute, s.hour, s.day, s.month, s.year))
    intervals_since_epoch = property(lambda s: int(s._intervals_since_epoch))    
    interval_seconds = property(lambda s: int(s._isec))

    def strftime(self, frmt):
        return _strftime(frmt,self._tfunc(self._intervals_since_epoch * self._isec))

    def asctime(self):
        return _asctime(self._tfunc(self._intervals_since_epoch * self._isec))

    def struct_time(self):
        return _struct_time(self._tfunc(self._intervals_since_epoch * self._isec))

    def as_dict(self):        
        return dict( zip(OHLC.DATETIME_FIELDS, self.as_tuple()) )        

    def as_tuple(self):
        return self.datetime   
    
    def __str__(self):
        labels = OHLC.DATETIME_FIELDS        
        return str(tuple(a + "=" + str(v) for a,v in zip(labels, self.as_tuple())))

    
class C(NULL):   
    __slots__ = ('_c','_ticks')
    
    def __init__(self, dat, intervals_since_epoch, interval_seconds,
                 time_func=time.localtime):
        self._c = dat[-1]
        self._ticks = len(dat)
        super().__init__(intervals_since_epoch, interval_seconds, time_func)   
        
    c = property(lambda s: s._c)
    ticks = property(lambda s: s._ticks)

    def update(self, dat):              
        self._c = dat[-1]
        self._ticks += len(dat)

    def as_dict(self):
        d = {'c':self._c, 'ticks':self._ticks}
        d.update( super().as_dict() )
        return d  

    def as_tuple(self):
        return (self._c,) + super().as_tuple() + (self._ticks,)
    
    def __str__(self):
        labels = ('c',) + OHLC.DATETIME_FIELDS + ('ticks',)       
        return str(tuple(a + "=" + str(v) for a,v in zip(labels, self.as_tuple())))
    
        
class OHLC(C):        
    __slots__ = ('_o', '_h', '_l')
    
    def __init__(self, dat, intervals_since_epoch, interval_seconds,
                 time_func=time.localtime):
        super().__init__(dat, intervals_since_epoch, interval_seconds, time_func)
        self._o = dat[0]
        self._h = max(dat)
        self._l = min(dat) 

    o = property(lambda s: s._o)
    h = property(lambda s: s._h)
    l = property(lambda s: s._l)
    
    def update(self, dat):           
        self._h = max(self._h, max(dat))
        self._l = min(self._l, min(dat))
        super().update(dat)        
    
    def as_dict(self):
        d = {'o':self._o, 'h':self._h, 'l':self._l}
        d.update( super().as_dict() )
        return d
  
    def as_tuple(self):
        return (self._o, self._h, self._l) + super().as_tuple()   
  
    def __str__(self):
        labels = ('o','h','l','c') + self.DATETIME_FIELDS + ('ticks',)     
        return str(tuple(a + "=" + str(v) for a,v in zip(labels, self.as_tuple())))    


class TOSDB_FixedTimeIntervals:
    """Base object that pulls data from a thread-safe DataBlock in fixed-time intervals

    This class takes streaming data from a 'DataBlock' object (_win.py/__init__.py),
    breaks it up along fixed-time intervals, transforms the data into a custom objects,
    and stores those objects in internal buffers that can be accessed via get(...)
    and stream_snapshot(...) methods.

    1) Create a thread-safe 'DataBlock' (e.g tosdb.TOSDB_ThreadSafeDataBlock)
    
    2) Add 'items' and 'topics' directly through the 'DataBlock' interface, or
       use the methods provided
       i) 'topics' that return string data will be ignored (e.g DESCRIPTION, LASTX)
       ii) all valid items/topics added/removed to/from the 'DataBlock' will
           immediately affect *this* object (be careful!)
           
    3) Define a custom class that extends 'NULL' to represent data in each interval
       (see OHLC and C above).
        i) its constructor should have the following prototype:
            def __init__(self, data, intervals_since_epoch, interval_seconds, time_func):
                super().__init__(self, intervals_since_epoch, interval_seconds, time_func)
                # handle 'data'
        ii) it should define an 'update' method to handle new data coming in:
            def update(self, data):
                # handle 'data'
        iii) consider defining '__slots__' to limit memory usage
        
    4) Create a TOSDB_FixedTimeIntervals object with:
        i)   the block we created in #1
        ii)  the class we created in #3 (the actuall class, not an instance)
        iii) the length of the interval in seconds (>= 10, <= 14,400)
        iv)  (optional) how often each retrieval/collection operations should occur
             (i.e interval_sec=30, poll_sec=1 means 30 operations per interval)
        v)   (optional) custom time function to translate time data stored in
             interval_object to time.struct_time
        vi)  (optional) callback function that will pass the item, topic, and
             last completed interval as its arguments

    5) Use methods to access stored data:
        get(...) 
        get_by_datetime(...) 
        stream_snapshot(...) 
        stream_snapshot_between_datetimes(...)

    6) WHEN DONE: call stop() method

    NOTE: after stop() is called the 'DataBlock' object you passed into the constructor
          is still valid and in the same state.
    
    __init__(self, block, interval_obj, interval_sec, poll_sec=1,
             interval_cb=None, time_func=time.localtime)

    block        :: object :: object that exposes certain _TOSDB_DataBlock methods                                                             
    interval_obj :: class  :: object used to stored data over a fixed-time interval 
    interval_sec :: int    :: size of fixed-time interval in seconds 
    poll_sec     :: int    :: time in seconds of each data retrieval/collection operation
    interval_cb  :: func   :: callback for each 'completed' interval
                              (def func(item, topic, interval_obj instance))
    time_func    :: func   :: custom time function that returns time.struct_time    
    
    throws TOSDB_Error
    """
    
    MIN_INTERVAL_SEC = 10
    MAX_INTERVAL_SEC = 60 * 60 * 4 # 4 hours
    MIN_POLL_SEC = .10
    BLOCK_SIZE_PER_PSEC = 10
    WAIT_ADJ_DOWN = .999  
    WAIT_ADJ_THRESHOLD = .01
    WAIT_ADJ_THRESHOLD_FATAL = .10
    BLOCK_ATTR = ['is_thread_safe', 'stream_snapshot_from_marker', 'topics', 'items',
                  'add_items', 'add_topics', 'remove_items', 'remove_topics']
    
    def __init__(self, block, interval_obj, interval_sec, poll_sec=1,
                 interval_cb=None, time_func=time.localtime):
        self._rflag = False
        self._check_params(interval_obj, interval_sec, poll_sec, interval_cb, time_func)
        self._iobj = interval_obj
        self._isec = interval_sec
        self._psec = round(interval_sec / round(interval_sec/poll_sec), 5)
        self._min_block_size = self.BLOCK_SIZE_PER_PSEC * poll_sec
        self._check_block(block)        
        self._block = block
        self._texcludes = set()
        self._ssfunc = block.stream_snapshot_from_marker
        self._restrict_block_resize(block, poll_sec)
        self._tfunc = time_func
        self._interval_cb = interval_cb
        self._wait_adj_down_exp = 0
        self._buffers = dict()
        self._buffers_lock = _Lock()
        self._bthread = _Thread(target=self._background_worker)
        self._bthread.start()      


    def __del__(self):        
        if self._rflag:
            self.stop()        

        
    def stop(self):
        """ Stop retrieving data from the underlying 'DataBlock' """
        self._rflag = False        
        self._bthread.join()        
        if hasattr(self._block, '_old_set_block_size'):
            self._block.set_block_size = self._block._old_set_block_size
            delattr(self._block, '_old_set_block_size')


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def add_items(*items):
        with self._buffers_lock:
            self._block.add_items(*items)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def add_topics(*topics):
        for t in topics:
           if type_bits(t) & tosdb.STRING_BIT:
                raise TOSDB_Error("topic %s returns string data" % t)
        with self._buffers_lock:
            self._block.add_topics(*topics)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def remove_items(*items):
        with self._buffers_lock:
            self._block.remove_items(*items)


    @_doxtend(_TOSDB_DataBlock) # __doc__ from ABC _TOSDB_DataBlock
    def remove_topics(*topics):     
        with self._buffers_lock:
            self._block.remove_topics(*topics)

        
    def occupancy(self, item, topic):
        """ Returns the current number of interval objects being held

        stream_occupancy(self, item, topic)

        item  :: str  :: any item string in the block
        topic :: str  :: any topic string in the block
        
        returns -> int      
        """ 
        with self._buffers_lock:
            return len(self._get_buffer_deque(topic,item))

    
    def get(self, item, topic, indx=0):
        """ Return a single interval object 

        get(self, item, topic, indx=0)
            
        item  :: str :: any item string in the block
        topic :: str :: any topic string in the block      
        indx  :: int :: index/position of interval to get                                 
        """
        with self._buffers_lock:
            return self._get_buffer_deque(topic,item)[indx]


    def get_by_datetime(self, item, topic, datetime):
        """ Return a single interval object by its datetime tuple

        get(self, item, topic, datetime)
            
        item     :: str :: any item string in the block
        topic    :: str :: any topic string in the block      
        datetime :: int :: datetime tuple of the interval object
                           (second, minute, hour, day, month, year)

        throws TOSDB_Error if interval object doesn't exist at that datetime
        """
        with self._buffers_lock:
            b = self._get_buffer_deque(topic,item)
            i = self._intervals_since_epoch(*datetime)        
            off = b[0].intervals_since_epoch - i
            if off < 0:
                raise TOSDB_Error("OHLC does not exist at this datetime")
            try:
                return b[off]
            except IndexError:
                raise TOSDB_Error("OHLC does not exist at this datetime")


    def stream_snapshot(self, item, topic, end=-1, beg=0):
        """ Return multiple interval objects

        stream_snapshot(self, item, topic, end=-1, beg=0)
                        
        item  :: str :: any item string in the block
        topic :: str :: any topic string in the block
        end   :: int :: index/position of least recent interval to get  
        beg   :: int :: index/position of most recent interval to get   
        """
        with self._buffers_lock:
            return list(self._get_buffer_deque(topic,item))[beg:end]


    # INCLUSIVE INDEXING !
    def stream_snapshot_between_datetimes(self, item, topic, end_datetime=tuple(),
                                          beg_datetime=tuple()):
        """ Return multiple interval objects between datetime tuples

        stream_snapshot(self, item, topic, end_datetime=tuple(), beg_datetime=tuple())
                        
        item         :: str :: any item string in the block
        topic        :: str :: any topic string in the block
        end_datetime :: int :: datetime tuple of least recent interval to get
                               (second, minute, hour, day, month, year)
        beg_datetime :: int :: datetime tuple of most recent interval to get
                               (second, minute, hour, day, month, year)

        throws TOSDB_Error if interval objects don't exist between the datetime tuples
        """
        with self._buffers_lock:
            b = self._get_buffer_deque(topic,item)
            if beg_datetime:
                i_beg = self._intervals_since_epoch(*beg_datetime)
                off_beg = b[0].intervals_since_epoch - i_beg
                if off_beg < 0:
                    raise TOSDB_Error("'beg' OHLC does not exist at this datetime")               
            else:
                off_beg = 0       
            if end_datetime:
                i_end = self._intervals_since_epoch(*end_datetime)           
                off_end = b[0].intervals_since_epoch - i_end
                if off_end < 0:
                    raise TOSDB_Error("'end' OHLC does not exist at this datetime")
                off_end += 1 # make range incluseive
            else:
                off_end=-1
            try:
                return list(b)[off_beg:off_end] 
            except IndexError:
                raise TOSDB_Error("OHLCs do not exist between these datetimes")

    class _Buffer:
        def __init__(self):
            self.deque = _Deque()
            self.reset()
        def incr(self):
            self.count += 1
        def reset(self):
            self.count = 0
          
    def _get_buffer_object(self, topic, item):
        return self._buffers[topic.upper(),item.upper()]
    
    def _get_buffer_deque(self, topic, item):
        return self._get_buffer_object(topic,item).deque

    def _callback(self, item, topic, d, i=1):
        if self._interval_cb:
            self._interval_cb(item, topic, d[i]) 
            
    def _background_worker(self):        
        ni = self._isec / self._psec
        itbeg = _perf_counter()    
        count = 0
        self._rflag = True
        while self._rflag:
            tbeg = _perf_counter()               
            with self._buffers_lock:
                self._manage_buffers()
                for (t,i), b in self._buffers.items():
                    b.incr()
                    dat = self._ssfunc(i, t, date_time=True, throw_if_data_lost=False)                    
                    if dat:
                        self._parse_data(t,i,dat)
                    if b.count == 0:
                        continue
                    if (b.count % ni) == 0:
                        #print("b.count mod interval: %i" % b.count)
                        self._handle_null_interval(t, i, b)
            count += 1
            tend = _perf_counter()
            trem = self._psec - (tend - tbeg)
            if trem < 0:
                ## TODO :: this will create problems handling nulls as we wont be able
                ##         to speed up by using WAIT_ADJ_DOWN (below)
                ##         considering adjusting _psec 
                print("WARN: _background_worker taking longer than _psec (%i) seconds"
                      % self._psec)
            _sleep( max(trem,0) * (self.WAIT_ADJ_DOWN ** self._wait_adj_down_exp) )
            if (count % ni) == 0:
                self._tune_background_worker(count,ni,itbeg)


    def _tune_background_worker(self, count, ni, itbeg):
        tnow = _perf_counter()
        adjbeg = itbeg + (count // ni) * self._isec
        terr = tnow - adjbeg
        #print("DEBUG RUNNING ERROR SECONDS: ", str(terr))
        if terr < 0:
            #print("DEBUG RUNNING ERROR SLEEP: ", str(terr))
            if self._wait_adj_down_exp > 0:
                self._wait_adj_down_exp -= 1
            _sleep(abs(terr))
        elif abs(terr) > (self._isec * self.WAIT_ADJ_THRESHOLD_FATAL):
            TOSDB_Error("_background worker entered fatal wait/sync states: %f, %i" \
                        % (terr, self._wait_adj_down_exp))
        elif terr > (self._isec * self.WAIT_ADJ_THRESHOLD):
            self._wait_adj_down_exp += 1
            #print("DEBUG RUNNING ERROR SPEED UP: %f, %i" % (terr,self._wait_adj_down_exp))        


    def _handle_null_interval(self, t, i, b):                       
        ei = b.deque[0].intervals_since_epoch + 1
        obj = NULL(ei, self._isec, self._tfunc)
        b.deque.appendleft(obj)
        self._callback(i,t,b.deque) # callback with penultimate interval
        #print("DEBUG WORK NULL: %s %i" % (str((t,i)),ei))


    def _parse_data(self, topic, item, dat):
        b = self._get_buffer_object(topic,item)        
        ei_old = int(dat[-1][1].mktime // self._isec)    
        ei_new = ei_old + 1 # making big assumption here
        fi = 0
        for d in dat:
            ei_last = int(d[1].mktime // self._isec)                    
            if ei_last == ei_old:
                break
            fi += 1
        vodat = [d[0] for d in dat[fi:]]
        vndat = [d[0] for d in dat[:fi]]
        if vodat:
            self._insert_data(ei_old, vodat, b, True, topic, item)                                 
        if vndat:
            self._insert_data(ei_new, vndat, b, False, topic, item)
            

    def _insert_data(self, ei, vdat, b, old, topic, item):
        ei_current = b.deque[0].intervals_since_epoch
        tstr = "old" if old else "new"
        if ei > ei_current:
            #print("VDAT (%s > current) %i (%i) %i" % (tstr, ei, len(vdat), ei_current))
            obj = self._iobj(vdat, ei, self._isec, self._tfunc)        
            b.deque.appendleft(obj)
            b.reset()
            self._callback(item, topic, b.deque) # callback with penultimate interval
        else:
            #print("VDAT (%s <= current) %i (%i) %i" % (tstr, ei, len(vdat), ei_current))                      
            for i in range(len(b.deque)):
                if b.deque[i].intervals_since_epoch == ei:
                    if type(b.deque[i]) is NULL:
                        b.deque[i] = self._iobj(vdat, ei, self._isec, self._tfunc)
                    else:
                        b.deque[i].update(vdat)
                    return
            raise TOSDB_Error("can not find place for %s data" % tstr)

            
    def _manage_buffers(self):       
        old_buffers = dict(self._buffers)       
        new_buffer_ids = []
        self._buffers.clear()
        items = self._block.items()     
        for t in self._block.topics():            
            if t in self._texcludes:
                continue
            elif type_bits(t) & tosdb.STRING_BIT:           
                print("WARN: topic %s returns string data - excluding" % t)
                self._texcludes.add(t)
                continue
            for i in items:
                if (t,i) not in old_buffers:
                    new_buffer_ids.append((t,i))                             
                self._buffers[(t,i)] = old_buffers.get((t,i),self._Buffer())        
        if new_buffer_ids:
            self._init_buffers_async(new_buffer_ids)        
                      

    def _init_buffers_async(self, new_buffer_ids):
        with _futures.ThreadPoolExecutor(max_workers = len(new_buffer_ids)) as e:            
            futs = {e.submit(self._init_buffer,t,i):(t,i) for t,i in new_buffer_ids}
            for f in _futures.as_completed(futs):
                t, i = futs[f]                
                dat = f.result()
                laste = 0
                b = self._get_buffer_deque(t,i)
                cb_count = 0
                # does groupby guarantee sorted ??
                for e, grp in _groupby(dat, lambda t: int(t[1].mktime // self._isec)):
                    # fill in gaps                 
                    while laste and (e - laste > 1):
                        obj = NULL(laste + 1, self._isec, self._tfunc)
                        b.append(obj)                        
                        cb_count += 1
                        laste += 1                        
                    d = [i[0] for i in grp]
                    obj = self._iobj(d, e, self._isec, self._tfunc)
                    b.append(obj)
                    cb_count += 1
                    laste = e
                # we sort new to old so need do all callbacks after appends
                for n in range(cb_count-1,0,-1):
                    self._callback(i,t,b,n) 

        
    def _init_buffer(self, topic, item):
        for _ in range(int(self._isec / self._psec)):
            dat = self._ssfunc(item, topic, date_time=True, throw_if_data_lost=False)
            if dat:
                return dat
            _sleep(self._psec)
        raise TOSDB_Error("failed to get any data for first interval: %s, %s" % (topic,item))


    def _intervals_since_epoch(self, second, minute, hour, day, month, year):
        is_dst = self._tfunc().tm_isdst
        sfe = _mktime(_struct_time([year,month,day,hour,minute,second,0,0,is_dst]))
        if sfe % self._isec:
            raise TOSDB_Error("invalid interval constructed")        
        return int(sfe // self._isec)
    

    def _restrict_block_resize(self, block, poll_sec):             
        block._old_set_block_size = block.set_block_size        
        @_doxtend(type(block), func_name='set_block_size')
        def _inj_set_block_size(instance, sz):
            if sz < self._min_block_size:
                raise TOSDB_Error("setting a block size < %i has been restricted "
                                  "by TOSDB_FixedTimeIntervals" % self._min_block_size)
            block._old_set_block_size(sz)            
        block.set_block_size = _MethodType(_inj_set_block_size, block)               
   
   
    def _check_block(self, block):
        for a in TOSDB_FixedTimeIntervals.BLOCK_ATTR:
            if not hasattr(block, a):
                raise TOSDB_Error("'block' does not have attr '%s'" % a)
        if not block.is_thread_safe():
            raise TOSDB_Error("'block' is not thread safe")
        if not block.is_using_datetime():
            raise TOSDB_Error("'block' is not using datetime")
        if block.get_block_size() < self._min_block_size:
            print("WARN: block is too small, resize to %i" % self._min_block_size)
            block.set_block_size(self._min_block_size)
     

    @classmethod
    def _check_params(cls, interval_obj, interval_sec, poll_sec, interval_cb, time_func):
        if not hasattr(interval_obj, 'update'):
            raise TOSDB_Error("'interval obj' has no 'update' function/attribute")
        try:
            test = interval_obj([0], 30, 1)
        except Exception as e:
            raise TOSDB_Error("could not instantiate 'interval_obj': %s" % str(e))
        if interval_sec < cls.MIN_INTERVAL_SEC:
            raise TOSDB_Error("interval_sec(%i) < MIN_INTERVAL_SEC(%i)" \
                              % (interval_sec, cls.MIN_INTERVAL_SEC) )        
        if interval_sec > cls.MAX_INTERVAL_SEC:
            raise TOSDB_Error("interval_sec(%i) > MAX_INTERVAL_SEC(%i)" \
                              % (interval_sec, cls.MAX_INTERVAL_SEC) )        
        if interval_sec < (2*poll_sec):
            raise TOSDB_Error("interval_sec(%i) < 2 * poll_sec(%i)" \
                              % (interval_sec, poll_sec))
        if poll_sec < cls.MIN_POLL_SEC:
            raise TOSDB_Error("poll_sec(%i) < MIN_POLL_SEC(%i)" \
                              % (poll_sec, cls.MIN_POLL_SEC) )
        if interval_cb is not None:
            if not callable(interval_cb):
                raise TOSDB_Error("'interval_cb' not callable")
            if len(_signature(interval_cb).parameters) != 3:
                raise TOSDB_Error("'interval_cb' signature requires 3 paramaters")
        if not callable(time_func):
            raise TOSDB_Error("'time_func' not callable")


class TOSDB_OpenHighLowCloseIntervals(TOSDB_FixedTimeIntervals):
    """Pulls data from a thread-safe DataBlock in fixed-time ohlc.OHLC intervals

    This class takes streaming data from a 'DataBlock' object (_win.py/__init__.py),
    breaks it up along fixed-time intervals, transforms the data into an ohlc.OHLC object,
    and stores those objects in internal buffers that can be accessed via get(...)
    and stream_snapshot(...) methods.

    1) Create a thread-safe 'DataBlock' (e.g tosdb.TOSDB_ThreadSafeDataBlock)
    
    2) Add 'items' and 'topics' directly through the 'DataBlock' interface, or
       use the methods provided
       i) 'topics' that return string data will be ignored (e.g DESCRIPTION, LASTX)
       ii) all valid items/topics added/removed to/from the 'DataBlock' will
           immediately affect *this* object (be careful!)

    3) Create a TOSDB_OpenHighLowCloseIntervals object with:
        i)   the block we created in #1    
        ii)  the length of the interval in seconds (>= 10, <= 14,400)
        iii) (optional) how often each retrieval/collection operations should occur
             (i.e interval_sec=30, poll_sec=1 means 30 operations per interval)
        iv)  (optional) custom time function to translate time data stored in
             interval_object to time.struct_time
        vi)  (optional) callback function that will pass the item, topic, and
             last completed interval as its arguments
             
    5) Use methods to access stored data:
        get(...) 
        get_by_datetime(...) 
        stream_snapshot(...) 
        stream_snapshot_between_datetimes(...)

    6) WHEN DONE: call stop() method

    NOTE: after stop() is called the 'DataBlock' object you passed into the constructor
          is still valid and in the same state.
    
    __init__(self, block, interval_sec, poll_sec=1, interval_cb=None,
             time_func=time.localtime)

    block        :: object :: object that exposes certain _TOSDB_DataBlock methods                                                             
    interval_sec :: int    :: size of fixed-time interval in seconds 
    poll_sec     :: int    :: time in seconds of each data retrieval/collection operation
    interval_cb  :: func   :: callback for each 'completed' interval
                              (def func(item, topic, interval_obj instance))
    time_func    :: func   :: custom time function that returns time.struct_time
    
    throws TOSDB_Error
    """
    def __init__(self, block, interval_sec, poll_sec=1, interval_cb=None,
                 time_func=time.localtime):
        super().__init__(block, OHLC, interval_sec, poll_sec, interval_cb, time_func)


class TOSDB_CloseIntervals(TOSDB_FixedTimeIntervals):
    """Pulls data from a thread-safe DataBlock in fixed-time ohlc.C intervals

    This class takes streaming data from a 'DataBlock' object (_win.py/__init__.py),
    breaks it up along fixed-time intervals, transforms the data into a ohlc.C object,
    and stores those objects in internal buffers that can be accessed via get(...)
    and stream_snapshot(...) methods.

    1) Create a thread-safe 'DataBlock' (e.g tosdb.TOSDB_ThreadSafeDataBlock)
    
    2) Add 'items' and 'topics' directly through the 'DataBlock' interface, or
       use the methods provided
       i) 'topics' that return string data will be ignored (e.g DESCRIPTION, LASTX)
       ii) all valid items/topics added/removed to/from the 'DataBlock' will
           immediately affect *this* object (be careful!)

    3) Create a TOSDB_CloseIntervals object with:
        i)   the block we created in #1    
        ii)  the length of the interval in seconds (>= 10, <= 14,400)
        iii) (optional) how often each retrieval/collection operations should occur
             (i.e interval_sec=30, poll_sec=1 means 30 operations per interval)
        iv)  (optional) custom time function to translate time data stored in
             interval_object to time.struct_time
        vi)  (optional) callback function that will pass the item, topic, and
             last completed interval as its arguments
             
    5) Use methods to access stored data:
        get(...) 
        get_by_datetime(...) 
        stream_snapshot(...) 
        stream_snapshot_between_datetimes(...)

    6) WHEN DONE: call stop() method

    NOTE: after stop() is called the 'DataBlock' object you passed into the constructor
          is still valid and in the same state.
    
    __init__(self, block, interval_sec, poll_sec=1, interval_cb=None,
             time_func=time.localtime)

    block        :: object :: object that exposes certain _TOSDB_DataBlock methods                                                             
    interval_sec :: int    :: size of fixed-time interval in seconds 
    poll_sec     :: int    :: time in seconds of each data retrieval/collection operation
    interval_cb  :: func   :: callback for each 'completed' interval
                              (def func(item, topic, interval_obj instance))
    time_func    :: func   :: custom time function that returns time.struct_time
    
    throws TOSDB_Error
    """
    def __init__(self, block, interval_sec, poll_sec=1, interval_cb=None,
                 time_func=time.localtime):
        super().__init__(block, C, interval_sec, poll_sec, interval_cb, time_func)



    
