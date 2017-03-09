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

from ._win import TOSDB_DataBlock, DEF_TIMEOUT, MAX_STR_SZ, STR_DATA_SZ
from .doxtend import doxtend as _doxtend

from threading import RLock as _RLock


class TOSDB_DataBlockThreadSafe(TOSDB_DataBlock):
    """ The main object for storing TOS data with thread-safe access    

    __init__(self, size=1000, date_time=False, timeout=DEF_TIMEOUT)

    size      :: int  :: how much historical data can be inserted
    date_time :: bool :: should block include date-time with each data-point?
    timeout   :: int  :: how long to wait for responses from engine, TOS-DDE server,
                         and/or internal IPC/Concurrency mechanisms (milliseconds)

    throws TOSDB_CLibError
    """    
    def __init__(self, size=1000, date_time=False, timeout=DEF_TIMEOUT):
        self._rlock = _RLock()
        super().__init__(size, date_time, timeout)


    def __str__(self):
        with self._rlock:
            return super().__str__()


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def is_thread_safe(self):
        return True

        
    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def close(self):
        with self._rlock:
            super().close()


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def info(self):
        with self._rlock:
            return super().info()


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def get_block_size(self):          
        with self._rlock:
            return super().get_block_size()      


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def set_block_size(self, sz):
        with self._rlock:
            super().set_block_size(sz)              


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def items(self, str_max=MAX_STR_SZ):
        with self._rlock:
            return super().items(str_max)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def topics(self, str_max=MAX_STR_SZ):
        with self._rlock:
            return super().topics(str_max)

    
    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def items_precached(self, str_max=MAX_STR_SZ):
        with self._rlock:
            return super().items_precached(str_max)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def topics_precached(self, str_max=MAX_STR_SZ):
        with self._rlock:
            return super().topics_precached(str_max)

    
    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock   
    def add_items(self, *items):
        with self._rlock:
            super().add_items(*items)

            
    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def add_topics(self, *topics):
        with self._rlock:
            super().add_topics(*topics)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def remove_items(self, *items):
        with self._rlock:
            super().remove_items(*items)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def remove_topics(self, *topics):
        with self._rlock:
            super().remove_topics(*topics)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock     
    def stream_occupancy(self, item, topic):
        with self._rlock:
            return super().stream_occupancy(item, topic)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def get(self, item, topic, date_time=False, indx=0, check_indx=True, 
            data_str_max=STR_DATA_SZ):
        
        with self._rlock:
            return super().get(item, topic, date_time, indx, check_indx, data_str_max)

            
    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def stream_snapshot(self, item, topic, date_time=False, end=-1, beg=0,
                        smart_size=True, data_str_max=STR_DATA_SZ):
        
        with self._rlock:
            return super().stream_snapshot(item, topic, date_time, end, beg,
                                           smart_size, data_str_max)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def stream_snapshot_from_marker(self, item, topic, date_time=False, beg=0, 
                                    margin_of_safety=100, throw_if_data_lost=True,
                                    data_str_max=STR_DATA_SZ):
        
        with self._rlock:
            return super().stream_snapshot_from_marker(item, topic, date_time, beg,
                                                       margin_of_safety, throw_if_data_lost,
                                                       data_str_max)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def item_frame(self, topic, date_time=False, labels=True, 
                   data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
        
        with self._rlock:
            return super().item_frame(topic, date_time, labels, data_str_max, label_str_max)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def topic_frame(self, item, date_time=False, labels=True, 
                    data_str_max=STR_DATA_SZ, label_str_max=MAX_STR_SZ):
        
        with self._rlock:
            return super().topic_frame(item, date_time, labels, data_str_max, labels_str_max)


    @_doxtend(TOSDB_DataBlock) # __doc__ from TOSDB_DataBlock
    def total_frame(self, date_time=False, labels=True, data_str_max=STR_DATA_SZ, 
                    label_str_max=MAX_STR_SZ):
        
        with self._rlock:
            return super().total_frame(date_time, labels, data_str_max, label_str_max)
