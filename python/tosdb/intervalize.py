from tosdb import _TOS_DataBlock
from threading import Thread as _Thread
import time as _time

def _bind_class_fields_late(cls):
    cls.min         = cls(60)
    cls.three_min   = cls(180)
    cls.five_min    = cls(300)
    cls.ten_min     = cls(600)
    cls.thirty_min  = cls(1800)
    cls.hour        = cls(3600)
    cls.two_hour    = cls(7200)
    cls.four_hour   = cls(14400)                          
    return cls    
@_bind_class_fields_late
class StartInterval:
    def __init__(self,val):
        self.val = val    
                                                                                
class GetOnInterval:       
    def __init__(self,block,item,topic):      
        if type(block) != _TOS_DataBlock:
            raise TypeError("block must be of type tosdb.TOS_DataBlock")
        self._block = block
        if not block._valid_topic( topic ):
            raise ValueError("block does not have topic: " + str(topic) )
        self._topic = topic
        if not block._valid_item( item ):
            raise ValueError("block does not have item: " + str(item) )
        self._item = item
        self._rflag = False
        

class GetOnTimeInterval( GetOnInterval ):

    def start(self, interval, callback, start_interval=StartInterval.min):
        self._interval = interval
        if not callable(callback):                    
            raise TypeError("callback must be callable")
        self._callback = callback
        if type(start_interval) != StartInterval:
            raise TypeError("start_interval must be of type StartInterval")
        self._start_interval = start_interval.val
        self._rflag = True
        self._thread = _Thread( target=self._time_interval_loop )
        self._thread.start()        
        
    def stop(self):
        self._rflag = False
  
    def _time_interval_loop(self):       
        _spin_until()
        while self._rflag:
            val = self._block.get( self._item, self._topic )
            self._callback( val )
            for i in range( self._interval ):
                if not self._rflag:
                    break
                _time.sleep( 1 )

    def _spin_until( self ):
        gmt = _time.gmtime()
        gmt_sec = gmt.tm_sec + gmt.tm_min * 60
        while (gmt_sec % self.start_interval > 0) and self._rflag:
            _time.sleep(.5)
            gmt = _time.gmtime()
            gmt_sec = gmt.tm_sec + gmt.tm_min * 60
      


