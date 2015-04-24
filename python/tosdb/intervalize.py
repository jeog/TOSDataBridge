from threading import Thread as _Thread
import tosdb
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

        
class _GetOnInterval:       
    def __init__(self,block,item,topic):      
        if not isinstance(block, tosdb._TOS_DataBlock):
            raise TypeError("block must be of type tosdb.TOS_DataBlock")
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

    # active_interval storage/functionality
                 
    def start( self, run_callback, stop_callback,
               time_interval=TimeInterval.five_min, update_seconds=15 ):        
        self._check_start_args( run_callback, stop_callback, time_interval,
                                update_seconds )
        self._run_callback = run_callback
        self._stop_callback = stop_callback
        self._interval_seconds = time_interval.val        
        self._update_seconds = update_seconds
        self._rflag = True
        self._thread = _Thread( target=self._update, daemon=True )
        self._thread.start()        
        
    def stop(self):
        self._rflag = False
        self._stop_callback()

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
                dat = get_next_seg( self._item, self._topic, True)                       
                if dat and len(dat) >= 1:
                    dat += carry
                    carry = self._find_roll_points( dat )                                  
                for i in range(self._update_seconds):
                    _time.sleep( 1 )
                    if not self._rflag:
                        break
            except Exception as e:
                print( repr(e) )
                print("error in GetOnTimeInterval._update loop, stopping.")
                self.stop()
                raise
 
                
    def _find_roll_points(self, snapshot):
        last_item = snapshot[-1]        
        if self._interval_seconds <= 60:
            do_mod = lambda i: i[1].sec % self._interval_seconds       
        elif self._interval_seconds <= 3600:              
            do_mod = lambda i: i[1].min % (self._interval_seconds / 60)          
        elif self._interval_seconds <= 86400:
            do_mod = lambda i: i[1].hour % (self._interval_seconds / 3600)            
        else:
            raise ValueError("invalid TimeInterval")
        gapd = lambda t,l: (t[1].mktime - l[1].mktime) > self._interval_seconds
        riter = reversed(snapshot[:-1])
        rmndr = [last_item]       
        for this_item in riter:
            last_mod = do_mod(last_item)
            this_mod = do_mod(this_item)
            print("item", str((last_item[1].min,last_item[1].sec)),
                  ((this_item[1].min,this_item[1].sec)) )
            if last_mod > this_mod or gapd(this_item,last_item):
                # if we break into coniguous interval or 'gap' it
                print("hit", str((last_item,this_item)) )
                self._run_callback((last_item,this_item)) # (older,newer)                
                rmndr = [this_item]          
            else:                
                rmndr.insert(0,this_item)
            last_item = this_item
        # dont need all the rmndr, but will be useful for more advanced ops
        return rmndr


    @staticmethod
    def _write_header( block, item, topic, file, time_interval, update_seconds):
        file.seek(0)
        file.write(str(block.info()) + '\n')
        file.write('item: ' + item + '\n')
        file.write('topic: ' + topic + '\n')
        file.write('time_interval(sec): ' + str(time_interval.val) + '\n' )
        file.write('update_seconds: ' + str(update_seconds) + '\n\n' )       

    @classmethod
    def send_to_file( cls, block, item, topic, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15, use_pre_roll_val=True ):                
        file = open(file_path,'w')
        try:
            i = cls(block,item,topic)
            cls._write_header( block, item, topic, file, time_interval,
                               update_seconds)          
            def run_cb(x):
                x = x[0] if use_pre_roll_val else x[1]          
                file.write( str(x[1]).ljust(50) + str(x[0]) + '\n' )
                #file.write( str(x[0][1]).ljust(40) + str(x[1][1]).ljust(40) + '\n' )
            stop_cb = lambda : file.close()
            if cls._check_start_args( run_cb, stop_cb, time_interval,
                                      update_seconds):
                i.start( run_cb, stop_cb, time_interval, update_seconds)
            return i
        except Exception as e:
            print( repr(e) )
            file.close()
        
    @staticmethod
    def _check_start_args( run_callback, stop_callback, time_interval,
                           update_seconds):
        if not callable(run_callback) or not callable(stop_callback):                    
            raise TypeError( "callback must be callable")
        if type(time_interval) is not TimeInterval:
            raise TypeError( "time_interval must be of type TimeInterval")
        if divmod(time_interval.val,60)[1] != 0:
            raise ValueError( "time_interval not divisible by 60")
        if update_seconds > (time_interval.val / 2):
            raise ValueError( "update_seconds greater than half time_interval")
        if divmod(time_interval.val,update_seconds)[1] != 0:
            raise ValueError( "time_interval not divisible by update_seconds")
        return True

class GetOnTimeInterval_OHLC( GetOnTimeInterval ):
    def __init__(self,block,item):
        GetOnTimeInterval.__init__(self,block,item,'last')
        self._o = self._h = self._l = self._c = None

    @classmethod
    def send_to_file( cls, block, item, file_path,
                      time_interval=TimeInterval.five_min,
                      update_seconds=15, use_pre_roll_val=True ):
        file = open(file_path,'w')
        try:
            i = cls(block,item)
            GetOnTimeInterval._write_header( block, item, 'last', file,
                                             time_interval, update_seconds )          
            def run_cb(x):                          
                file.write( str(x[1]).ljust(50) + str(x[0]) + '\n' )                
            stop_cb = lambda : file.close()
            if cls._check_start_args( run_cb, stop_cb, time_interval,
                                      update_seconds):
                i.start( run_cb, stop_cb, time_interval, update_seconds)
            return i
        except Exception  as e:
            print( repr(e) )
            file.close()    


    def _find_roll_points(self, snapshot):
        last_item = snapshot[-1]
        if self._o is None:
            self._o = self._h = self._l = last_item[0]                
        if self._interval_seconds <= 60:
            do_mod = lambda i: i[1].sec % self._interval_seconds       
        elif self._interval_seconds <= 3600:              
            do_mod = lambda i: i[1].min % (self._interval_seconds / 60)          
        elif self._interval_seconds <= 86400:
            do_mod = lambda i: i[1].hour % (self._interval_seconds / 3600)            
        else:
            raise ValueError("invalid TimeInterval")
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
            print("item", str((last_item[1].min,last_item[1].sec)),
                  ((this_item[1].min,this_item[1].sec)) )
            if last_mod > this_mod or gapd(this_item,last_item):    
                # if we break into coniguous interval or 'gap' it
                ohlc = (self._o,self._h,self._l,this_item[0])
                self._o = self._h = self._l = this_item[0]
                print("hit", str(ohlc) )
                self._run_callback((ohlc,this_item[1])) # (older,newer)                
                rmndr = [this_item]          
            else:                
                rmndr.insert(0,this_item)
            last_item = this_item
        # dont need all the rmndr, but will be useful for more advanced ops
        return rmndr



