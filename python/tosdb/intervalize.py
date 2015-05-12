from threading import Thread as _Thread
from sys import stderr as _stderr
from meta_enum import MetaEnum
import tosdb
import time as _time

@MetaEnum
class TimeInterval(metaclass=MetaEnum):      
  fields = { 
     'min' : 60,
     'three_min' : 180,
     'five_min' : 300,
     'ten_min' : 600,
     'fifteen_min' : 900,
     'thirty_min' : 1800,
     'hour' : 3600 
  }   
 
        
class _GetOnInterval:       
    def __init__(self,block,item,topic):      
        if not isinstance(block, tosdb._TOSDB_DataBlock):
            raise TypeError("block must be of type tosdb._TOSDB_DateBlock")
        self._block = block
        if topic.upper() not in block.topics():
            raise ValueError("block does not have topic: " + str(topic) )
        self._topic = topic
        if item.upper() not in block.items():
            raise ValueError("block does not have item: " + str(item) )
        self._item = item
        self._rflag = False
    

class GetOnTimeInterval( _GetOnInterval ):
    def __init__(self,block,item,topic):
        _GetOnInterval.__init__(self,block,item,topic)
        if block.info()['DateTime'] == 'Disabled':
            raise ValueError("block does not have datetime enabled")
        self._run_callback = None
        self._stop_callback = None

    def __del__(self):
        self.stop()

    @classmethod
    def send_to_file( cls, block, item, topic, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15, use_pre_roll_val=True ):                     
        try:
            #i = cls(block,item,topic)
            i = GetOnTimeInterval( block, item, topic )
            i._file = open(file_path,'w',1)
            cls._write_header( block, item, topic, i._file, time_interval,
                               update_seconds)          
            def run_cb(self,x):
                x = x[0] if use_pre_roll_val else x[1]          
                i._file.write( str(x[1]).ljust(50) + str(x[0]) + '\n' )               
            stop_cb = lambda : i._file.close()
            if cls._check_start_args( run_cb, stop_cb, time_interval,
                                      update_seconds):
                i.start( run_cb, stop_cb, time_interval, update_seconds)
            return i
        except Exception as e:
            print( "ohlc_daemon.py", repr(e), file=_stderr )
            i._file.close()
                 
    def start( self, run_callback, stop_callback,
               time_interval=TimeInterval.five_min, update_seconds=15 ):        
        self._check_start_args( run_callback, stop_callback, time_interval,
                                update_seconds )
        self._run_callback = run_callback
        self._stop_callback = stop_callback
        self._interval_seconds = int(time_interval.val)        
        self._update_seconds = int(update_seconds)
        self._rflag = True
        self._thread = _Thread( target=self._update, daemon=True )
        self._thread.start()        
        
    def stop(self):
        self._rflag = False
        if self._stop_callback:
            self._stop_callback()

    def join(self):
        if self._thread:
            self._thread.join()

    def _update(self):
        carry = []
        get_next_seg = getattr(self._block,'stream_snapshot_from_marker')       
        while self._rflag:
            #
            # Using a 'lazy' approach here:
            #
            # We are only looking for the roll(s) accross the interval of
            # actual (received) data-points; we don't deal with a 'data gap'
            # (by assuming a certain price or using the last price recieved).
            # This means there may be gaps in the output.
            #
            # Checking should be done at the next layer where different, though
            # less robust, strategies can be employed to fill in missing data.
            #
            try:                
                dat = get_next_seg( self._item, self._topic, date_time=True,
                                    throw_if_data_lost = False )                                     
                if dat and len(dat) >= 1:
                    dat += carry
                    carry = self._find_roll_points( dat )                                  
                for i in range(self._update_seconds):
                    _time.sleep( 1 )
                    if not self._rflag:
                        break
            except Exception as e:          
                print( "error in GetOnTimeInterval._update loop, stopping.",
                       file=_stderr)
                print( "ohlc_daemon.py", repr(e), file=_stderr )              
                self.stop()
                raise 
                
    def _find_roll_points(self, snapshot):
        last_item = snapshot[-1]        
        do_mod = self._get_do_mod()
        gapd = lambda t,l: (t[1].mktime - l[1].mktime) > self._interval_seconds
        riter = reversed(snapshot[:-1])
        rmndr = [last_item]       
        for this_item in riter:
            last_mod = do_mod(last_item)
            this_mod = do_mod(this_item)            
            if last_mod > this_mod or gapd(this_item,last_item):
                # if we break into coniguous interval or 'gap' it           
                self._run_callback(self,(last_item,this_item)) # (older,newer)                
                rmndr = [this_item]          
            else:                
                rmndr.insert(0,this_item)
            last_item = this_item
        # dont need all the rmndr, but will be useful for more advanced ops
        return rmndr

    def _get_do_mod(self):
        if self._interval_seconds <= 60:
            return lambda i: i[1].sec % self._interval_seconds       
        elif self._interval_seconds <= 3600:              
            return lambda i: i[1].min % (self._interval_seconds / 60)          
        elif self._interval_seconds <= 86400:
            return lambda i: i[1].hour % (self._interval_seconds / 3600)            
        else:
            raise ValueError("invalid TimeInterval") 
            
    @staticmethod
    def _write_header( block, item, topic, file, time_interval, update_seconds):
        file.seek(0)
        file.write(str(block.info()) + '\n')
        file.write('item: ' + item + '\n')
        file.write('topic(s): ' + topic + '\n')
        file.write('time_interval(sec): ' + str(time_interval.val) + '\n' )
        file.write('update_seconds: ' + str(update_seconds) + '\n\n' )       
        
    @staticmethod
    def _check_start_args( run_callback, stop_callback, time_interval,
                           update_seconds):
        if not callable(run_callback) or not callable(stop_callback):                    
            raise TypeError( "callback must be callable")
        if time_interval not in TimeInterval:
            raise TypeError( "time_interval must be of type TimeInterval")
        if divmod(time_interval.val,60)[1] != 0:
            raise ValueError( "time_interval not divisible by 60")
        if update_seconds > (time_interval.val / 2):
            raise ValueError( "update_seconds greater than half time_interval")
        if divmod(time_interval.val,update_seconds)[1] != 0:
            raise ValueError( "time_interval not divisible by update_seconds")
        return True

class GetOnTimeInterval_C( GetOnTimeInterval ):
    def __init__(self,block,item):
        GetOnTimeInterval.__init__(self,block,item,'last')

    @classmethod
    def send_to_file( cls, block, item, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15, use_pre_roll_val=True ):   
        return super().send_to_file( block, item, 'last', file_path, 
                                     time_interval, update_seconds, 
                                     use_pre_roll_val )                         


class GetOnTimeInterval_OHLC( GetOnTimeInterval ):
    def __init__(self,block,item):
        GetOnTimeInterval.__init__(self,block,item,'last')
        self._o = self._h = self._l = self._c = None

    @classmethod
    def send_to_file( cls, block, item, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15 ): #, use_pre_roll_val=True ):     
        try:
            i = cls(block,item)
            i._file = open(file_path,'w',1)
            GetOnTimeInterval._write_header( block, item, 'last', i._file,
                                             time_interval, update_seconds )             
            def run_cb(self, x):                          
                i._file.write( str(x[1]).ljust(50) + str(x[0]) + '\n' )                
            stop_cb = lambda : i._file.close()
            if cls._check_start_args( run_cb, stop_cb, time_interval,
                                      update_seconds):
                i.start( run_cb, stop_cb, time_interval, update_seconds)
            return i
        except Exception  as e:
            print( "ohlc_daemon.py", repr(e), file=_stderr )
            i._file.close()    


    def _find_roll_points(self, snapshot):
        last_item = snapshot[-1]
        if self._o is None:
            self._o = self._h = self._l = last_item[0]                
        do_mod = self._get_do_mod()
        gapd = lambda t,l: (t[1].mktime - l[1].mktime) > self._interval_seconds
        riter = reversed(snapshot[:-1])
        rmndr = [last_item]       
        for this_item in riter:
            p = this_item[0]
            if p > self._h:
                self._h = p
            elif p < self._l:
                self._l = p           
            last_mod = do_mod(last_item)
            this_mod = do_mod(this_item)
            #print("item", str((last_item[1].min,last_item[1].sec)),
            #      ((this_item[1].min,this_item[1].sec)) )
            if last_mod > this_mod or gapd(this_item,last_item):    
                # if we break into coniguous interval or 'gap' it
                ohlc = (self._o,self._h,self._l,this_item[0])
                self._o = self._h = self._l = this_item[0]
                #print("hit", str(ohlc) )
                self._run_callback(self,(ohlc,this_item[1]))                 
                rmndr = [this_item]          
            else:                
                rmndr.insert(0,this_item)
            last_item = this_item
        # dont need all the rmndr, but will be useful for more advanced ops
        return rmndr


class GetOnTimeInterval_OHLCV( GetOnTimeInterval_OHLC ):
 
    def __init__(self,block,item):
        if 'VOLUME' not in block.topics():
            raise ValueError("block does not have topic: volume" )
        GetOnTimeInterval_OHLC.__init__(self,block,item)
        self._o = self._h = self._l = self._c = None
        self._last_vol = None

    @classmethod
    def send_to_file( cls, block, item, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15 ): #, use_pre_roll_val=True ):     
        try:
            i = cls(block,item)
            i._file = open(file_path,'w',1)
            GetOnTimeInterval_OHLC._write_header( block, item, "last, volume", 
                                             i._file, time_interval, 
                                             update_seconds )             
            def run_cb(self,x):                          
                i._file.write( str(x[1]).ljust(50) + str(x[0]) + ' ' ) 
                v = block.get(item,'volume')
                if self._last_vol:                    
                    i._file.write( str(int(v - self._last_vol)) + '\n' )
                else:
                    i._file.write( 'N/A \n' )
                self._last_vol = v
                               
            stop_cb = lambda : i._file.close()
            if cls._check_start_args( run_cb, stop_cb, time_interval,
                                      update_seconds):
                
                i.start( run_cb, stop_cb, time_interval, update_seconds)
            return i
        except Exception  as e:
            print( "ohlc_daemon.py", repr(e), file=_stderr )
            i._file.close()    


class GetOnTimeInterval_CV( GetOnTimeInterval_C ):
 
    def __init__(self,block,item):
        if 'VOLUME' not in block.topics():
            raise ValueError("block does not have topic: volume" )
        GetOnTimeInterval_C.__init__(self,block,item)     
        self._last_vol = None

    @classmethod
    def send_to_file( cls, block, item, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15, use_pre_roll_val=True ):     
        try:
            i = cls(block,item)
            i._file = open(file_path,'w',1)
            GetOnTimeInterval_C._write_header( block, item, "last, volume", 
                                                i._file, time_interval, 
                                                update_seconds )             
            def run_cb(self,x):                          
                x = x[0] if use_pre_roll_val else x[1]          
                i._file.write( str(x[1]).ljust(50) + str(x[0]) + ' ' )
                v = block.get(item,'volume')
                if self._last_vol:                    
                    i._file.write( str(int(v - self._last_vol)) + '\n' )
                else:
                    i._file.write( 'N/A \n' )
                self._last_vol = v
                               
            stop_cb = lambda : i._file.close()
            if cls._check_start_args( run_cb, stop_cb, time_interval,
                                      update_seconds):                
                i.start( run_cb, stop_cb, time_interval, update_seconds)
            return i
        except Exception  as e:
            print( "ohlc_daemon.py", repr(e), file=_stderr )
            i._file.close()    


  
