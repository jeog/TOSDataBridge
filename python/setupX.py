# Copyright (C) 2014 Jonathon Ogden < jeog.dev@gmail.com > 

from distutils.core import setup,Extension
from distutils.errors import DistutilsError
from io import StringIO
import sys

      
_tosdbExt = Extension( 
  "_tosdb2",
  sources=["_tosdb.cpp"],
  include_dirs=["..\\include"],  
  define_macros=[ ("XPLATFORM_PYTHON_CONSTS_ONLY",None) ],
  optional=True
  )

class _tosdb_error(Exception):
    def __init__(self,exc):
        Exception(exc)
        
try:
    # capture setup errors but allow setup to complete (optional=True)   
    sio = StringIO()
    se = sys.stderr
    sys.stderr = sio 
    setup( ext_modules=[_tosdbExt] )
    sys.stderr = se    
    if sio.getvalue(): 
        print( "\n+ Operation 'completed' with errors:\n")
        print( sio.getvalue() )
        print( "+ Checking on the status of the build...")
        t = None
        try:
            import _tosdb as t
            print( "+ _tosdb.pyd exists (possibly an older version?) ")
        except:        
            raise Exception("- error: _tosdb.pyd does not exist" + 
                            " (can not be imported) ")
        finally:
            if t:
                del t
except (BaseException) as err:
    print("- fatal error thrown during setup: ",type(err))
    print(err)
    
    

