# _tosdb is how we deal with C++ header defined consts, those exported from the
# back-end libs, and '#define' compile-time consts necessary for C compatibility
from _tosdb import*  # also allows us to migrate away from ctypes when necessary
from tosdb_datetime import TOS_DateTime
from abc import ABCMeta
import pickle

virtual_CREATE = '1'
virtual_CALL = '2'
virtual_DESTROY = '3'
virtual_FAIL = '4'
virtual_SUCCESS = '5'
virtual_SUCCESS_NT = '6'

virtual_MAX_REQ_SIZE = 1000 # arbitrary for now
virtual_TYPE_ATTR = {'i':int,'s':str,'b':bool}

class TOSDB_Error(Exception):
    """ Base exception for tosdb.py """    
    def __init__(self,  *messages ):        
        Exception( *messages )



def dumpnamedtuple( nt ):
    n = type(nt).__name__
    od = nt.__dict__
    return pickle.dumps( (n,tuple(od.keys()),tuple(od.values())) )

def loadnamedtuple( nt):
    name,keys,vals = pickle.loads( nt )
    ty = namedtuple( name, keys )
    return ty( *vals )   



