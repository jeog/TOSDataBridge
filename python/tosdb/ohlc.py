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

"""ohlc.py:  structure tosdb data in ohlc format

"""

import tosdb
from tosdb import TOPICS, TOSDB_Error
from .doxtend import doxtend as _doxtend

from threading import Thread as _Thread, Lock as _Lock
from types import MethodType as _MethodType
from collections import deque as _Deque
from concurrent import futures as _futures
from time import time as _time, sleep as _sleep, localtime as _localtime, \
    asctime as _asctime, struct_time as _struct_time, mktime as _mktime, \
    strftime as _strftime
from itertools import groupby as _groupby

import traceback as _traceback
import time

class OHLC:    
    DATETIME_FIELDS = ('second','minute','hour','day','month','year')
    
    __slots__ = ('_ohlc','_intervals_since_epoch','_isec','_tfunc')
    
    def __init__(self, o, h, l, c, intervals_since_epoch, interval_seconds,
                 time_func=time.localtime):
        self._ohlc = (o,h,l,c)
        self._intervals_since_epoch = intervals_since_epoch
        self._isec = interval_seconds
        self._tfunc = time_func

    def update(self, h, l, c):           
        h = max(self._ohlc[1], h)
        l = min(self._ohlc[2], l)
        self._ohlc = (self._ohlc[0], h, l, c)

    def strftime(self, frmt):
        return _strftime(frmt,self._tfunc(self._intervals_since_epoch * self._isec))

    def asctime(self):
        return _asctime(self._tfunc(self._intervals_since_epoch * self._isec))

    def struct_time(self):
        return _struct_time(self._tfunc(self._intervals_since_epoch * self._isec))

    def as_dict(self):
        d = self.as_dict()
        d.update( zip(OHLC.DATETIME_FIELDS, self._datetime_tuple()) )
        return d

    def as_tuple(self):
        return self._ohlc + self._datetime_tuple()

    o = property(lambda s: s._ohlc[0])
    h = property(lambda s: s._ohlc[1])
    l = property(lambda s: s._ohlc[2])
    c = property(lambda s: s._ohlc[3])
    
    second = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_sec)
    minute = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_min)
    hour = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_hour)
    day = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_mday)
    month = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_mon)    
    year = property(lambda s: s._tfunc(s._intervals_since_epoch * s._isec).tm_year)
    
    intervals_since_epoch = property(lambda s: int(s._intervals_since_epoch))
    interval_seconds = property(lambda s: int(s._isec))

    def _datetime_tuple(self):
        return (self.second, self.minute, self.hour, self.day, self.month, self.year)
    
    def __str__(self):
        labels = ('o','h','l','c') + OHLC.DATETIME_FIELDS        
        return str(tuple(a + "=" + str(v) for a,v in zip(labels, self._as_tuple())))    

    
        
class TOSDB_OpenHighLowClose:
    
    MIN_INTERVAL_SEC = 10
    MAX_INTERVAL_SEC = 60 * 60 * 4 # 4 hours
    BLOCK_SIZE_PER_PSEC = 10  
    ALLOWED_TOPICS = [TOPICS.LAST.val, TOPICS.VOLUME.val]
    BLOCK_ATTR = ['is_thread_safe', 'stream_snapshot_from_marker', 'topics']
    
    def __init__(self, block, interval_sec, poll_sec=.5, time_func=time.localtime):
        TOSDB_OpenHighLowClose._check_params(interval_sec, poll_sec)
        self._isec = interval_sec
        self._psec = poll_sec
        self._min_block_size = TOSDB_OpenHighLowClose.BLOCK_SIZE_PER_PSEC * poll_sec
        self._check_block(block)        
        self._block = block
        self._ssfunc = block.stream_snapshot_from_marker
        self._restrict_block_resize(block, poll_sec)
        self._tfunc = time_func
        self._rflag = False
        self._buffers = dict()
        self._buffers_lock = _Lock()
        self._bthread = _Thread(target=self._background_worker)
        self._bthread.start()      

    def __del__(self):
        print("DEBUG", "__del__ in")
        if self._rflag:
            self.stop()
        print("DEBUG", "__del__ out")
        
    def stop(self):
        self._rflag = False
        print("DEBUG", "join IN")
        self._bthread.join()
        print("DEBUG", "join OUT")
        if hasattr(self._block, '_old_set_block_size'):
            self._block.set_block_size = self._block._old_set_block_size
            delattr(self._block, '_old_set_block_size')

    def add_items(*items):
        self._block.add_items(*items)

    def add_topics(*topics):
        for t in topics:
            if TOPICS.val_dict[t] not in TOSDB_OpenHighLowClose.ALLOWED_TOPICS:
                raise TOSDB_Error("topic %s not in 'ALLOWED_TOPICS'" % t)            
        self._block.add_topics(*topics)

    def remove_items(*items):
        self._block.remove_items(*items)

    def remove_topics(*topics):
        for t in topics:
            if TOPICS.val_dict[t] not in TOSDB_OpenHighLowClose.ALLOWED_TOPICS:
                raise TOSDB_Error("topic %s not in 'ALLOWED_TOPICS'" % t)  
        self._block.remove_topics(*topics)
        
    def occupancy(self, item, topic):
        return len(self._get_buffer(topic,item))
    
    def get(self, item, topic, indx=0):
        return self._get_buffer(topic,item)[indx]

    def get_by_datetime(self, item, topic, second, minute, hour, day, month, year):
        buf = self._get_buffer(topic,item)
        i = self._intervals_since_epoch(second, minute, hour, day, month, year)        
        off = buf[0].intervals_since_epoch - i
        if off < 0:
            raise TOSDB_Error("OHLC does not exist at this datetime")
        try:
            return buf[off]
        except IndexError:
            raise TOSDB_Error("OHLC does not exist at this datetime")

    def stream_snapshot(self, item, topic, end=-1, beg=0):
        return list(self._get_buffer(topic,item))[beg:end]

    # INCLUSIVE INDEXING !
    def stream_snapshot_between_datetimes(self, item, topic, second_beg, minute_beg,
                                          hour_beg, day_beg, month_beg, year_beg,
                                          second_end, minute_end, hour_end, day_end,
                                          month_end, year_end):
        buf = self._get_buffer(topic,item)
        i_beg = self._intervals_since_epoch(second_beg, minute_beg, hour_beg,
                                            day_beg, month_beg, year_beg)
        i_end = self._intervals_since_epoch(second_end, minute_end, hour_end,
                                            day_end, month_end, year_end)
        off_beg = buf[0].intervals_since_epoch - i_beg
        if off_beg < 0:
            raise TOSDB_Error("'beg' OHLC does not exist at this datetime")
        off_end = buf[0].intervals_since_epoch - i_end
        if off_end < 0:
            raise TOSDB_Error("'end' OHLC does not exist at this datetime")
        if off_beg < off_end:
            raise TOSDB_Error("'beg' datetime can not be less than 'end'")
        try:
            return list(buf)[off_beg:off_end + 1] #make incluseive
        except IndexError:
            raise TOSDB_Error("OHLCs do not exist between these datetimes")


    def _get_buffer(self, topic, item):
        return self._buffers[topic.upper(),item.upper()][0]
            
    def _background_worker(self):       
        self._rflag = True
        while self._rflag:
            tbeg = _time()   
            self._manage_buffers()
            for (t,i), buf_pair in self._buffers.items():
                dat = self._ssfunc(i, t, date_time=True, throw_if_data_lost=False)
                if dat:
                    self._parse_data(t,i,dat)
                    continue
                buf_pair[1] += 1
                if buf_pair[1] >= (self._isec / self._psec):                        
                    buf_pair[0].append(None)
                    buf_pair[1] = 0                                                     
            tend = _time()
            _sleep( max(self._psec - (tend - tbeg), 0) )
         

    def _parse_data(self, topic, item, dat):
        buf_pair = self._buffers[(topic,item)]
        ei = buf_pair[0][0].intervals_since_epoch
        fi =  len(dat)
        for d in dat:
            d0 = int(d[1].mktime // self._isec)                    
            if d0 == ei:
                break
            fi -= 1
        ndat = dat[fi:]
        odat = dat[:fi]
        if odat:
            d = [d[0] for d in odat]            
            buf_pair[0][0].update(max(d), min(d), d[-1])         
        if ndat:
            pos = len(dat) - fi -1
            ei = int(dat[pos][1].mktime // self._isec)
            d = [d[0] for d in ndat]
            ohlc = OHLC(d[0], max(d), min(d), d[-1], ei, self._isec, self._tfunc)        
            buf_pair[0].appendleft(ohlc)
            buf_pair[1] = 0

        
    def _manage_buffers(self):                        
        old_buffers = dict(self._buffers)
        new_buffer_ids = []
        self._buffers.clear()
        items = self._block.items()
        for t in self._block.topics():
            if TOPICS.val_dict[t] not in TOSDB_OpenHighLowClose.ALLOWED_TOPICS:
                  continue                  
            for i in items:
                if (t,i) not in old_buffers:
                    new_buffer_ids.append((t,i))
                self._buffers[(t,i)] = old_buffers.get((t,i),[_Deque(),0])        
        if new_buffer_ids:
            self._init_buffers_async(new_buffer_ids)        
                  

    def _init_buffers_async(self, new_buffer_ids):
        with _futures.ThreadPoolExecutor(max_workers = len(new_buffer_ids)) as e:            
            futs = {e.submit(self._init_buffer,t,i):(t,i) for t,i in new_buffer_ids}
            for f in _futures.as_completed(futs):
                t, i = futs[f]                
                dat = f.result()
                laste = 0
                buf = self._get_buffer(t,i)
                # does groupby guarantee sorted ??
                for e, grp in _groupby(dat, lambda t: int(t[1].mktime // self._isec)):
                    # fill in gaps                    
                    while laste and (e - laste > 1):
                        buf.append(None)                        
                        laste += 1
                        print("DEBUG INIT: ", str(laste), str(0))
                    d = [i[0] for i in grp]
                    ohlc = OHLC(d[0], max(d), min(d), d[-1], e, self._isec, self._tfunc)
                    buf.append(ohlc)
                    laste = e
                    print("DEBUG INIT: ", str(e), str(len(d)))

        
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
                                  "by TOSDB_OpenHighLowClose" % self._min_block_size)
            self._old_set_block_size(sz)            
        block.set_block_size = _MethodType(_inj_set_block_size, block)               
   
   
    def _check_block(self, block):
        for a in TOSDB_OpenHighLowClose.BLOCK_ATTR:
            if not hasattr(block, a):
                raise TOSDB_Error("'block' does not have attr '%s'" % s)
        if not block.is_thread_safe():
            raise TOSDB_Error("'block' is not thread safe")
        if not block.is_using_datetime():
            raise TOSDB_Error("'block' is not using datetime")
        if block.get_block_size() < self._min_block_size:
            print("WARN: block is too small, resize to %i" % self._min_block_size)
            block.set_block_size(self._min_block_size)
     

    @classmethod
    def _check_params(cls, interval_sec, poll_sec):      
        if interval_sec < cls.MIN_INTERVAL_SEC:
            raise TOSDB_Error("interval_sec(%i) < MIN_INTERVAL_SEC(%i)" \
                              % (interval_sec, cls.MIN_INTERVAL_SEC) )        
        if interval_sec > cls.MAX_INTERVAL_SEC:
            raise TOSDB_Error("interval_sec(%i) > MAX_INTERVAL_SEC(%i)" \
                              % (interval_sec, cls.MAX_INTERVAL_SEC) )        
        if interval_sec < (2*poll_sec):
            raise TOSDB_Error("interval_sec(%i) < 2 * poll_sec(%i)" \
                              % (interval_sec, poll_sec))            




    
