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
from concurrent import futures

class OHLC:
    def __init__(self, o, h, l, c, epoch_intervals):
        self.o = o
        self.h = h
        self.l = l
        self.c = c
        self.epoch_intervals = epoch_intervals

# callback for 
class TOSDB_OpenHighLowClose:
    
    MIN_INTERVAL_SEC = 10
    MAX_INTERVAL_SEC = 60 * 60 * 4 # 4 hours
    BLOCK_SIZE_PER_PSEC = 10
    ALLOWED_TOPICS = [TOPICS.LAST, TOPICS.VOLUME]
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
        self._buffers_lock = Lock()
        self._bthread = _Thread(target=self._background_worker)
        self._bthread.start()      


    def _background_worker(self): # can we pass a method to submit ?       
        self._rflag = True
        while self._rflag:
            tbeg = _time()
            self._manage_buffers()
            for (t,i),b in self._buffers:
                dat = self._block.stream_snapshot_from_marker(i,t,date_time=True)
                if not dat:
                    pass # check if we jumped the interval
                         # maintain a count of continues bad dats
                         # check if it > ._isec / ._psec
                         # if so add an empy ohlc
                self._parse_data(dat)    
            tend = _time()
            _sleep( max(self._psec - (tend - tbeg), 0) )
            ### DEBUG
            return
            ### DEBUG


    def _parse_data(self, topic, item, dat):
        ei = self._buffers[(topic,item)][0].epoch_intervals
        fi =  len(dat)
        for d in dat:
            d0 = int(d[1].mktime) % self._isec                    
            if d0 == ei:
                break
            fi -= 1
        ndat = dat[fi:]
        odat = dat[:fi]
        if odat:
            self._update_ohlc(t,i,[d[0] for d in odat], ei)
        if ndat:
            pos = len(dat) - fi -1
            ei = int(dat[pos].mktime) % self._isec
            self._insert_ohlc(t, i, [d[0] for d in ndat], ei)

            
    def _update_ohlc(self, topic, item, dat, epcoh_intervals):        
        ohlc = self._buffers[(topic,item)][0]
        ohlc.h = max(ohlc.h, max(dat))
        ohlc.l = min(ohlc.l, min(dat))
        ohlc.c = dat[-1]
        ohlc.epcoh_intervals = epcoh_intervals
        

    def _insert_ohlc(self, topic, item, dat, epcoh_intervals):
        ohlc = OHLC(dat[0], max(dat), min(dat), dat[-1], epcoh_intervals)        
        self._buffers[(topic,item)].appendleft(ohlc)

        
    def _manage_buffers(self):                        
        old_buffers = dict(self._buffers)
        new_buffer_ids = []
        self._buffers.clear()
        items = self._block.items()
        for t in self._block.topics():
            if TOPICS.val_dict[t] not in cls.ALLOWED_TOPICS:
                  continue                  
            for i in times:
                if (t,i) not in old_buffers:
                    new_buffer_ids.append((t,i))
                self._buffers[(t,i)] = old_buffers.get((t,i),_Deque())
        with _futures.ThreadPoolExecutor(max_workers = len(new_buffer_ids)) as e:            
            futs = {e.submit(self._init_buffer,self,t,i):(t,i) for (t,i),b in new_buffer_ids}
            for f in _futures.as_completed(futs):
                t, i = futs[f]                
                dat = f.result()           
                self._parse_data(t, i, dat)


    def _init_buffer(self, topic, item):
        for _ in range(self._isec / self._psec):
            dat = self._block.stream_snapshot_from_marker(i,t,date_time=True)
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


    def __del__(self):
        if hasattr(self, '_old_set_block_size'):
            self._block.set_block_size = self._old_set_block_size

       
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




    
