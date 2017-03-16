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
from time import time as _time, sleep as _sleep, localtime as _localtime
from itertools import groupby as _groupby

import traceback as _traceback

class OHLC:
    def __init__(self, o, h, l, c, intervals_since_epoch, interval_sec):
        self.o = o
        self.h = h
        self.l = l
        self.c = c
        self._intervals_since_epoch = intervals_since_epoch
        self._isec = interval_sec

    def update(self, h, l, c):           
        self.h = max(self.h, h)
        self.l = min(self.l, l)
        self.c = c
        
    intervals_since_epoch = property(lambda s: s._intervals_since_epoch)
    second = property(lambda s: _localtime(s._intervals_since_epoch * s._isec).tm_sec)
    minute = property(lambda s: _localtime(s._intervals_since_epoch * s._isec).tm_min)
    hour = property(lambda s: _localtime(s._intervals_since_epoch * s._isec).tm_hour)
    day = property(lambda s: _localtime(s._intervals_since_epoch * s._isec).tm_mday)
    month = property(lambda s: _localtime(s._intervals_since_epoch * s._isec).tm_mon)
    year = property(lambda s: _localtime(s._intervals_since_epoch * s._isec).tm_year)
    
    def __str__(self):
        return "{" + self.o + "," + self.h + "," + self.l + "," + self.c + "}"

        
class TOSDB_OpenHighLowClose:
    
    MIN_INTERVAL_SEC = 10
    MAX_INTERVAL_SEC = 60 * 60 * 4 # 4 hours
    BLOCK_SIZE_PER_PSEC = 10
    ALLOWED_TOPICS = [TOPICS.LAST.val, TOPICS.VOLUME.val]
    BLOCK_ATTR = ['is_thread_safe', 'stream_snapshot_from_marker', 'topics']
    
    def __init__(self, block, interval_sec, poll_sec=.5):
        TOSDB_OpenHighLowClose._check_block(block)        
        self._block = block
        TOSDB_OpenHighLowClose._check_params(interval_sec, poll_sec)
        self._isec = interval_sec
        self._psec = poll_sec
        self._restrict_block_resize(block, poll_sec)
        self._rflag = False
        self._buffers = dict()
        self._buffers_lock = _Lock()
        self._bthread = _Thread(target=self._background_worker)
        self._bthread.start()      

    def stop(self):
        self._rflag = False

    def get(self, item, topic, indx=0):
        return self._buffers[topic,item][indx]

    def stream_snapshot(self, item, topic, end=-1, beg=0):
        return self._buffers[topic,item][beg:end]

    def __del__(self):
        self.stop()
        if hasattr(self, '_old_set_block_size'):
            self._block.set_block_size = self._old_set_block_size
            
    def _background_worker(self): # can we pass a method to submit ?       
        self._rflag = True
        while self._rflag:
            tbeg = _time()
            try:
                self._manage_buffers()
                for (t,i),buf in self._buffers.items():
                    dat = self._block.stream_snapshot_from_marker(i,t,date_time=True,
                                                                  throw_if_data_lost=False)
                    if not dat:
                        continue
                             # check if we jumped the interval
                             # maintain a count of continues bad dats
                             # check if it > ._isec / ._psec
                             # if so add an empy ohlc
                    self._parse_data(t,i,dat)
            except Exception as e:
                print("exc in _background_worker:", e)
                _traceback.print_exc()
            tend = _time()
            _sleep( max(self._psec - (tend - tbeg), 0) )
         

    def _parse_data(self, topic, item, dat):
        buf = self._buffers[(topic,item)]
        ei = buf[0].intervals_since_epoch
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
            buf[0].update(max(d), min(d), d[-1])         
        if ndat:
            pos = len(dat) - fi -1
            ei = int(dat[pos][1].mktime // self._isec)
            d = [d[0] for d in ndat]
            ohlc = OHLC(d[0], max(d), min(d), d[-1], ei, self._isec)        
            buf.appendleft(ohlc)   

        
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
                self._buffers[(t,i)] = old_buffers.get((t,i),_Deque())        
        if new_buffer_ids:
            self._init_buffers_async(new_buffer_ids)        
                  

    def _init_buffers_async(self, new_buffer_ids):
        with _futures.ThreadPoolExecutor(max_workers = len(new_buffer_ids)) as e:            
            futs = {e.submit(self._init_buffer,t,i):(t,i) for t,i in new_buffer_ids}
            for f in _futures.as_completed(futs):
                t, i = futs[f]                
                dat = f.result()                
                # does groupby guarantee sorted ??
                for e, grp in _groupby(dat, lambda t: int(t[1].mktime // self._isec)):
                    d = [i[0] for i in grp]
                    ohlc = OHLC(d[0], max(d), min(d), d[-1], e)        
                    self._buffers[(t,i)].append(ohlc)  

        
    def _init_buffer(self, topic, item):
        for _ in range(int(self._isec / self._psec)):
            dat = self._block.stream_snapshot_from_marker(item,topic,date_time=True,
                                                          throw_if_data_lost=False)
            if dat:
                return dat
        raise TOSDB_Error("failed to get any data for first interval: %s, %s" % (topic,item))
                

    def _restrict_block_resize(self, block, poll_sec):
        s = self.BLOCK_SIZE_PER_PSEC * poll_sec        
        self._old_set_block_size = block.set_block_size        
        @_doxtend(type(block), func_name='set_block_size')
        def _inj_set_block_size(instance, sz):
            if sz < s:
                raise TOSDB_Error("setting a block size < %i has been restricted "
                                  "by TOSDB_OpenHighLowClose" % s)
            self._old_set_block_size(sz)            
        block.set_block_size = _MethodType(_inj_set_block_size, block)               

       
    @classmethod
    def _check_block(cls, block):
        for a in cls.BLOCK_ATTR:
            if not hasattr(block, a):
                raise TOSDB_Error("'block' does not have attr '%s'" % s)
        if not block.is_thread_safe():
            raise TOSDB_Error("'block' is not thread safe")
        if not block.is_using_datetime():
            raise TOSDB_Error("'block' is not using datetime")
     

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




    
