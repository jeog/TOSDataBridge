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


# _tosdb is how we deal with C++ header defined consts, those exported from the
# back-end libs, and '#define' compile-time consts necessary for C compatibility
from _tosdb import *  # also allows us to migrate away from ctypes when necessary
import sys as _sys
from collections import namedtuple as _namedtuple
from time import mktime as _mktime, struct_time as _struct_time, \
     asctime as _asctime, localtime as _localtime, strftime as _strftime
from ctypes import Structure as _Structure, c_long as _long_, c_int as _int_

BASE_YR = 1900
NTUP_TAG_ATTR = "_dont_worry_about_why_this_attribute_has_a_weird_name_"

class TOSDB_Error( Exception ):
    """ Base exception for tosdb.py """    
    def __init__(self,  *messages ):        
        Exception( *messages )

class TOSDB_PlatformError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_CLibError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_DateTimeError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_DataError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

class TOSDB_ValueError( TOSDB_Error, ValueError ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )
        ValueError( *messages )

class TOSDB_TypeError( TOSDB_Error, TypeError ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )
        TypeError( *messages )

class TOSDB_IndexError( TOSDB_Error, IndexError ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )
        IndexError( *messages )

class TOSDB_VirtualizationError( TOSDB_Error ):
    def __init__( self, *messages ):
        TOSDB_Error( *messages )

def wrap_impl_error( clss ):
    if not isinstance( clss, Exception):
        raise TypeError( "clss must be instance of Exception" )
    
    our_def = """\
class TOSDB_ImplErrorWrapper( {clss} ):
    def __init__( self, error ):
        {clss}( error )
""".format(clss=(type(clss).__name__))
    
    exec(our_def)
    our_obj = eval("TOSDB_ImplErrorWrapper")(clss)
    try:
        our_obj.__module__ = _sys._getframe(1).f_globals.get('__name__')
    except:
        pass
    return our_obj

class _DateTimeStamp(_Structure):
    """ Our 'private' C stdlib date-time struct w/ added micro-second field """
    class _CTime(_Structure):
        """ C stdlib tm struct """
        _fields_ =  [ ("tm_sec", _int_),
                      ("tm_min", _int_),
                      ("tm_hour", _int_),
                      ("tm_mday", _int_),
                      ("tm_mon", _int_),
                      ("tm_year", _int_),
                      ("tm_wday", _int_),
                      ("tm_yday", _int_),
                      ("tm_isdst", _int_) ]
            
    _fields_ =  [ ("ctime_struct", _CTime), ("micro_second", _long_) ]

class TOS_DateTime( _namedtuple( "DateTime",
                    ["micro","sec","min","hour","day","month","year"])):
    """ The object used for handling DateTime values

        TOS_DateTime is built on top of a simplified named tuple. It can be 
        constructed from _DateTimeStamp, _struct_time from the time.py library,
        or itself (copy construction). It can be pickled (serialized); allows
        for basic addition and subtraction returning a new object or a
        DateTimeDiff tuple, respectively; overloads comparison operators;
        and provides static utility functions to convert betwen DateTimeDiff
        objects and microseconds.       

        object: either a _DateTimeStamp( ideally from the impl. ), a 
        time._struct_time           
        micro_second: optional if adding a time._struct_time that lacks a 
        micro_second field
    """
    dtd_tuple = _namedtuple( "DateTimeDiff",
                            ["micro","sec","min","hour","day","sign"] )
    _mktime = None # cache total sec from epoch to make ops easier
    
    @staticmethod
    def _createSTime( object ):
        return _struct_time( [ object.ctime_struct.tm_year + BASE_YR,
                              object.ctime_struct.tm_mon +1,
                              object.ctime_struct.tm_mday,
                              object.ctime_struct.tm_hour,
                              object.ctime_struct.tm_min,
                              object.ctime_struct.tm_sec,
                              object.ctime_struct.tm_wday + 1,
                              object.ctime_struct.tm_yday + 1,
                              object.ctime_struct.tm_isdst ] )

    def __getnewargs__(self):        
        return ( (self.sec,self.min,self.hour,self.day,self.month,self.year),
                 self.micro ) 
    
    def __new__( cls, object, micro_second = 0 ):
        if micro_second != 0:
            micro_second %= 1000000
        _stime = None      
        if isinstance( object, _DateTimeStamp ):
              _stime = cls._createSTime( object )
              micro_second = object.micro_second
        elif isinstance( object, _struct_time ):
            _stime = object
        elif isinstance( object, TOS_DateTime):
            return super(TOS_DateTime, cls).__new__( cls, object.micro,
                                                     object.sec, object.min,
                                                     object.hour, object.day,
                                                     object.month, object.year )
        elif isinstance( object, tuple ):
            return super(TOS_DateTime, cls).__new__( cls, micro_second, *object)
        else:
            raise TOSDB_DateTimeError( "invalid 'object' passed to __new__" )
        
        return super(TOS_DateTime, cls).__new__( cls, micro_second,
                                                 _stime.tm_sec, _stime.tm_min,
                                                 _stime.tm_hour, _stime.tm_mday,
                                                 _stime.tm_mon, _stime.tm_year )

    def __init__(self, object, micro_second=0):
        if isinstance( object, _DateTimeStamp ):
              self._mktime = _mktime( TOS_DateTime._createSTime( object ) )          
        elif isinstance( object, _struct_time ):
            self._mktime = _mktime(object)
        elif isinstance( object, TOS_DateTime):
            self._mktime = object._mktime
        else:
            raise TOSDB_DateTimeError( "invalid 'object' passed to __init__" )

    def __str__( self ):
        return _strftime( "%m/%d/%y %H:%M:%S", 
                          _localtime( self._mktime ) ) + " " + str(self.micro)

    def __add__(self, micro_seconds ):
        if not isinstance(micro_seconds, int):                
            raise TOSDB_DateTimeError( "micro_seconds must be integer" )
        incr_sec = micro_seconds // 1000000
        ms_new = self.micro + ( micro_seconds % 1000000  )        
        if ms_new >= 1000000:
            incr_sec += 1
            ms_new -= 1000000
        # reduce to seconds, increment, return to _struct_time        
        mk_time = self._mktime 
        mk_time += incr_sec       
        new_time = _localtime( mk_time ) 
        # return a new TOS_DateTime 
        return TOS_DateTime( new_time, micro_second = ms_new )                
        
    def __sub__(self, other_or_micro_sec ):
        other = other_or_micro_sec
        # if we are subtracting an integer
        if isinstance( other , int ):
            if other < 0:
                return self.__add__( other * -1 )
            other_sec = other // 1000000
            ms_diff = self.micro - ( other % 1000000 )
            if ms_diff < 0: # take a second, if needed
                other_sec += 1
                ms_diff += 1000000
            # reduce to seconds, decrement, return to stuct_time          
            mk_time = self._mktime
            mk_time -= other_sec
            new_time = _localtime( mk_time )
            # return a new TOS_DateTime 
            return TOS_DateTime( new_time, micro_second = ms_diff )            
        # if we are subtracting another TOS_DateTime
        elif isinstance( other, TOS_DateTime):
            try: # try to get other's time in seconds                                         
                sec_diff = self._mktime - other._mktime         
                ms_diff = self.micro - other.micro
                # convert the diff in micro_seconds to the diff-tuple
                return TOS_DateTime.micro_to_dtd( sec_diff * 1000000 + ms_diff )     
            except Exception as e:
                raise TOSDB_DateTimeError( "invalid TOS_DateTime object", e )
        else:
            raise TOSDB_DateTimeError( "other_or_micro_sec must be " +
                                       "TOS_DateTime or int" )

    def __lt__( self, other ):
        return (self._compare( other ) < 0)
    
    def __ge__( self, other ):
        return (not self.__lt__( other))
    
    def __gt__( self, other):
        return (self._compare( other ) > 0)
    
    def __le__( self, other ):
        return (not self.__gt__( other))
    
    def __eq__( self, other ):
        return (not self._compare( other ))
    
    def __ne__( self, other ):
        return (self._compare( other ))
    
    def _compare( self, other ):
        if not isinstance( other, TOS_DateTime ):
            raise TOSDB_DateTimeError("unable to compare; unorderable types") 
        diff = self._mktime - other._mktime
        if diff == 0:            
            diff = self.micro - other.micro   
        return (-1 if diff < 0 else 1) if diff else 0         

    @staticmethod
    def micro_to_dtd( micro_seconds ):
        """ Converts micro_seconds to DateTimeDiff  """
        pos = True
        if micro_seconds < 0:
            pos = False
            micro_seconds = abs( micro_seconds )
        new_ms =  int(micro_seconds % 1000000)
        micro_seconds //= 1000000
        new_sec = int(micro_seconds % 60)
        micro_seconds //= 60
        new_min = int(micro_seconds % 60)
        micro_seconds //= 60
        new_hour = int(micro_seconds % 24)
        new_day = int(micro_seconds // 24)        
        return TOS_DateTime.dtd_tuple( new_ms, new_sec, new_min, new_hour,
                                       new_day, ('+' if pos else '-') )        
        
    @staticmethod
    def dtd_to_micro( dtd ):
        """ Converts DateTimeDiff to micro_seconds """
        tmp = dtd.day * 24 + dtd.hour
        tmp = tmp*60 + dtd.min
        tmp = tmp*60 + dtd.sec
        tmp *= 1000000
        tmp += dtd.micro
        if dtd.sign == '+':
            return tmp 
        elif dtd.sign == '-':
            return tmp * -1
        else:
            raise TOSDB_DateTimeError( "invalid 'sign' field in DateTimeDiff" )


def abort_init_after_warn():
    print( "*WARNING* by not supplying --root, --path, or " +
           "--noinit( -n ) arguments you are opting for a default " +
           "search root of 'C:\\'. This will attempt to search " +
           "the ENTIRE disk/drive for the tos-databridge library. " +
           "It's recommended you restart the program with the " +
           "library path (after --path) or a narrower directory " +
           "root (after --root)." )                
    if input( "Do you want to continue anyway?") in ['y','Y','YES','yes','Yes']:
        return False
    else:
        print("- init(root='C:\\') aborted")
        return True
        
# convert typebits to string and ctypes type
def _type_switch( typeB ):
    if typeB == INTGR_BIT + QUAD_BIT:
        return ( "LongLong", _longlong_ )
    elif typeB == INTGR_BIT:
        return ( "Long", _long_ )
    elif typeB == QUAD_BIT:
        return ( "Double", _double_ )
    elif typeB == 0:
        return ( "Float", _float_ )
    else: # default to string
        return( "String", _str_ )
