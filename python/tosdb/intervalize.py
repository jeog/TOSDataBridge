from tosdb import _TOS_DataBlock
from threading import Thread as _Thread
import time as _time

def _bind_class_fields_late_for_time_interval(cls):
    cls.min         = cls(60)
    cls.three_min   = cls(180)
    cls.five_min    = cls(300)
    cls.ten_min     = cls(600)
    cls.thirty_min  = cls(1800)
    cls.hour        = cls(3600)
    cls.two_hour    = cls(7200)
    cls.four_hour   = cls(14400) 
    return cls    
@_bind_class_fields_late_for_time_interval
class TimeInterval:
    def __init__(self,val):
        self.val = val
        
class GetOnInterval:       
    def __init__(self,block,item,topic):      
        if not isinstance(block, _TOS_DataBlock):
            raise TypeError("block must be of type tosdb.TOS_DataBlock")
        self._block = block
        if topic not in block.topics():
            raise ValueError("block does not have topic: " + str(topic) )
        self._topic = topic
        if item not in block.items():
            raise ValueError("block does not have item: " + str(item) )
        self._item = item
        self._rflag = False    

class GetOnTimeInterval( GetOnInterval ):
    def __init__(self,block,item,topic):
        GetOnInterval.__init__(block,item,topic)
        if block.info()['DateTime'] == 'Disabled':
            raise ValueError("block does not have datetime enabled")            

    def start( self, callback, time_interval=TimeInterval.five_min,
               update_seconds=15):        
        if not callable(callback):                    
            raise TypeError("callback must be callable")
        self._callback = callback
        if type(time_interval) is not TimeInterval:
            raise TypeError("time_interval must be of type TimeInterval")
        if divmod(time_interval,60)[1] != 0:
            raise ValueError("time_interval value must be divisible by 60") 
        self._time_interval = time_interval.val
        if update_seconds > (time_interval.val / 2):
            raise ValueError("update_seconds greater than half time_interval")
        self._update_seconds = update_seconds
        self._rflag = True
        self._thread = _Thread( target=self._update )
        self._thread.start()        
        
    def stop(self):
        self._rflag = False

    def _update(self):
        carry = None
        while self._rflag:
            dat = self._block.stream_snapshot_from_marker( self._item,
                                                           self._topic, True)
            if dat >= 1:
                roll = self._find_last_roll( dat + carry if carry else [])
                if roll:
                    self._callback( roll )
                carry = dat[0] # carry to the back of next snapshot
            for i in range(self._update_seconds):
                _time.sleep( 1 )
                if not self._rflag:
                    break

    def _find_last_roll(self, snapshot):
        last_item = snapshot[0]
        if self._time_interval.val <= 60:            
            for this_item in snapshot[1:]:               
                if last_item[1].min > this_item[1].min:
                    return last_item
                else:
                    last_item = this_item                      
        elif self._time_interval.val <= 3600:
            intrvl_min = self._time_interval.val / 60          
            for this_item in snapshot[1:]:
                last_mod = last_item[1].min % intrvl_min
                this_mod = this_item[1].min % intrvl_min
                if last_mod == 0 and this_mod != 0:
                    return last_item
                else:
                    last_item = this_item           
        elif self._time_interval.val <= 86400:
            intrvl_hour = self._time_interval.val / 3600            
            for this_item in snapshot[1:]:
                last_mod = last_item[1].hour % intrvl_hour
                this_mod = this_item[1].hour % intrvl_hour
                if last_mod == 0 and this_mod != 0:
                    return last_item
                else:
                    last_item = this_item
  
      


