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

""" streaming/streaming.py: Streaming functionality for tosdb *** NEW *** """

import tosdb

from sys import stderr as _stderr
from time import sleep as _sleep, perf_counter as _perf_counter
from platform import system as _system
from itertools import product as _product

DEFAULT_LATENCY = 100
DEFAULT_TIMEOUT = 0
DEFAULT_INIT_TIMEOUT = 5000

INT_MAX = 2**63 - 1

class _StreamingSession():
    INIT_TIMEOUT = DEFAULT_INIT_TIMEOUT
    
    __doc__ = """ Manage a {{session_type}} of streaming data *** NEW ***    
    
    An iterable Context Manager used to get streaming data for items and topics.
    On entry a {{block_type}} is created; and items and topics are added. The
    user then iterates over the instance to get available data. Each iteration
    will block until data is ready, or timeout.

    {{session_class}} provides two types of blocking/timeout behaviour:
    
        1) guarantee=True : returned data will all be sequential, by time. To
           insure this(because of the C implementation) each iteration will block
           until new data FROM ALL INDIVIDUAL STREAMS is available. If, for
           example, you add two topics and two items(four streams) and one of
           the items is invalid, returning no data, the whole session will block
           - indefinitely if timeout == 0.**
           
        2) guarantee=False : most all returned data will be sequential, by time,
           but occassionaly not. Each iteration blocks until new data FROM ANY
           STREAM is available. The session will be slightly more responsive and
           less likely to timeout.
    
    **To avoid this we force a timeout of {{session_class}}.INIT_TIMEOUT
    msec({init_timeout}) if NO data comes into the stream INITIALLY. Change
    this field as neeeded.
    
    Available data is returned as lists of 4-tuples, newest first, e.g.:
        [ ('spy', 'LAST', 250.00, TOSDB_DataTime), # float
          ('qqq', 'VOLUME', 1380000, TOSDB_DataTime), # int
          ('qqq', 'LASTX', '145.57 B', TOSDB_DateTime), # str
          ('spy', 'VOLUME', 24562387, TOSDBDateTime) ... ]  

    * * *
    
    {{constructor}}
    
    throws TOSDB_DataTimeError, TOSDB_IndexError, TOSDB_ValueError,
           TOSDB_DataError, TOSDB_CLibError, TOSDB_TypeError,
           TOSDB_TimeoutError      

    * * *
    
    try:
        with {{session_class}}({{example_args}}) as ss:
            d = next(ss)
            print("First Streaming Quote: ", str(d[-1]) )
            do_something_with_quotes(d)
            for d in ss:
                if not run_flag:
                    break
                do_something_with_quotes(d)
            print("Last Streaming Quote: ", str(d[0]) )
    except TOSDB_TimeoutError as e:
        print("TIMEOUT")
    """.format(init_timeout=INIT_TIMEOUT)       
    
    def __init__(self, items, topics, guarantee, timeout, latency):
        if not items:
            raise tosdb.TOSDB_ValueError("no items to stream")
        if not topics:
            raise tosdb.TOSDB_ValueError("no topics to stream")
        self._items = items
        self._topics = topics
        self._guarantee = guarantee
        self._timeout = timeout / 1000
        self._latency = latency / 1000
        self._run_flag = True

    def __enter__(self):
        try:
            self._block.add_items(*self._items)
            self._block.add_topics(*self._topics)           
            self._buffers = {k:[] for k in _product(self._items, self._topics)}            
            self._good_init = False
            self._stream_time_start = _perf_counter()
            self._cutoff_mktime = 0            
        except:
            self._block.close()
            raise     
        return self

    def __exit__(self, *args):
        self._block.close()        

    def __iter__(self):
        return self
    
    def __next__(self): 
        good = []   
        time_start = _perf_counter()
        while True:
            # look for data
            for item in self._items:
                for topic in self._topics:            
                    dat = self._block.stream_snapshot_from_marker(
                        item, topic, date_time=True, throw_if_data_lost=False)
                    if dat is not None:
                        dat.reverse()
                        self._buffers[item, topic].extend(dat)                        

            # check for empty buffers 
            bad_buffers = [k for k,v in self._buffers.items() if not v]
            need_to_sleep = bool(bad_buffers) if self._guarantee else \
                            len(bad_buffers) == len(self._buffers)
            # sleep and check for timeout if necessary
            if need_to_sleep:
                self._sleep_or_timeout(time_start, bad_buffers)
                continue
            else:
                self._good_init = True                            

            # of newest datum in each stream, find the oldest
            oldest_new_mktime = INT_MAX
            for v in self._buffers.values():
                if v and v[-1][1].mktime_micro < oldest_new_mktime:
                    oldest_new_mktime = v[-1][1].mktime_micro
            self._cutoff_mktime = oldest_new_mktime if oldest_new_mktime != INT_MAX else 0

            # remove all data older or equal to the aforementioned datum
            for k, v in self._buffers.items():             
                ngood = 0
                for vv in v:
                    if vv[1].mktime_micro <= self._cutoff_mktime: 
                        good.append( (k[0], k[1], vv[0], vv[1]) )
                        ngood += 1
                self._buffers[k] = v[ngood:]
            assert good, "'good' list is empty (PLEASE REPORT THIS BUG)"

            # sort all that and return it
            return sorted(good, key=lambda v: v[3].mktime_micro, reverse=True)

             
    def _sleep_or_timeout(self, time_start, bad_buffers):
        time_now = _perf_counter()
        if not self._good_init:
            time_left = (self.INIT_TIMEOUT/1000) - (time_now - self._stream_time_start)
            if time_left > 0:
                _sleep( min(self._latency, time_left) )
                return                       
            raise tosdb.TOSDB_TimeoutError("StreamingSession timed out trying to INIT -" +
                                           " BAD STREAMS " + str(bad_buffers))                          
        elif self._timeout: 
            time_left = self._timeout - (time_now - time_start)
            if time_left <= 0:        
                raise tosdb.TOSDB_TimeoutError("StreamingSession timed out -" +
                                               " BAD STREAMS " + str(bad_buffers))
            _sleep( min(self._latency, time_left) )                   
        else:
            _sleep(self._latency)


class StreamingSession(_StreamingSession):
    __doc__ = _StreamingSession.__doc__.format(
        session_type="session",
        block_type="TOSDB_DataBlock",
        session_class="StreamingSession",
        example_args="['spy','qqq'], ['LAST', 'VOLUME'], True, 10000",
        constructor=\
    """__init__(self, items, topics, guarantee=True, timeout=DEFAULT_TIMEOUT,
             latency=DEFAULT_LATENCY):

    items     :: list(str) :: item strings to stream
    topics    :: list(str) :: topics to stream
    guarantee :: bool      :: streaming data guaranteed to be in time-order
    timeout   :: int       :: milliseconds to wait for data before
                              blocking iteration times out (0 == no timeout)
    latency   :: int       :: internal wait milliseconds if no data""")
                                              
    
    def __init__(self, items, topics, guarantee=True, timeout=DEFAULT_TIMEOUT,
                 latency=DEFAULT_LATENCY):
        super().__init__(items, topics, guarantee, timeout, latency)  

    def __enter__(self):
        self._block = tosdb.TOSDB_DataBlock(1000, date_time=True)
        return super().__enter__()


if _system() not in ["Windows","windows","WINDOWS"]:
    globals().pop(StreamingSession.__name__)
    

class VStreamingSession(_StreamingSession):
    __doc__ = _StreamingSession.__doc__.format(
        session_type="virtual session",
        block_type="VTOSDB_DataBlock",
        session_class="VStreamingSession",
        example_args="""['spy','qqq'], ['LAST', 'VOLUME'], ('10.0.0.1',55555), 
                               'password', True, 10000""",
        constructor=\
    """__init__(self, items, topics, address, password=None, guarantee=True,
             timeout=DEFAULT_TIMEOUT, latency=DEFAULT_LATENCY):

    items     :: list(str) :: item strings to stream
    topics    :: list(str) :: topics to stream
    address   :: (str,int) :: (host/address of the windows implementation, port)
    password  :: str       :: password for authentication(None for no authentication)
    guarantee :: bool      :: streaming data guaranteed to be in time-order
    timeout   :: int       :: milliseconds to wait for data before
                              blocking iteration times out (0 == no timeout)
    latency   :: int       :: internal wait milliseconds if no data""")
    
    def __init__(self, items, topics, address, password=None, guarantee=True,
                 timeout=DEFAULT_TIMEOUT, latency=DEFAULT_LATENCY):        
        super().__init__(items, topics, guarantee, timeout, latency)
        self._address = address
        self._password = password

    def __enter__(self): 
        self._block = tosdb.VTOSDB_DataBlock(self._address, self._password, 1000, True)
        return super().__enter__()

        
  
