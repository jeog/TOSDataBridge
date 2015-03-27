# Copyright (C) 2014 Jonathon Ogden < jeog.dev@gmail.com > 

from distutils.core import setup,Extension
from distutils.errors import DistutilsError
from math import log
from os import listdir, remove, walk
from shutil import copy2
from io import StringIO
import sys

_sysArch = "x64" if ( log( sys.maxsize * 2, 2) > 33 ) else "x86"
_sysArchD = "Win32" if _sysArch == "x86" else "x64"
_sysMinorV = sys.version_info.minor

setup_dict = {
  "name":'tosdb',
  "version":'0.1',
  "description": "Python Front-End / Wrapper for TOS-DataBridge (C/C++)",
  "author":"Jonathon Ogden",
  "author_email":"jeog.dev@gmail.com",
  "py_modules":['tosdb'] 
} 
              
_tosdbExt = Extension( 
  "_tosdb",
  sources=["_tosdb.cpp"],
  include_dirs=["..\\include"],
  library_dirs=[ "..\\bin\\Release\\"+_sysArchD ],
  libraries=["_tos-databridge-shared-"+_sysArch,
             "_tos-databridge-static-"+_sysArch],
  define_macros=[ ("THIS_IMPORTS_IMPLEMENTATION",None),
                  ("THIS_DOESNT_IMPORT_INTERFACE",None) ],
  extra_compile_args=["/EHsc"],
  extra_link_args=["/LTCG"],
  optional=True )

def try_precompiled():    
    print("+ Would you like to try again with a pre-compiled _tosdb.pyd?")
    resp = input()
    if resp not in ["y","Y","yes","Yes","YES"]:
        return
    pydF = "_tosdb-"+str(_sysArch)+"-"+str(_sysMinorV)+".pyd"
    if pydF not in listdir():
        print( "Sorry, we can't find one that matches your version of python.")
        return
    print("Found _tosdb.pyd, sending it to your python root directory")
    copy2("./"+pydF, "./_tosdb.pyd")
    setup( data_files=[(".",["_tosdb.pyd"]) ], **setup_dict )
    remove("./_tosdb.pyd")

class _tosdb_error(Exception):
    def __init__(self,exc):
        Exception(exc)
        
try:
    # capture setup errors but allow setup to complete (optional=True)   
    sio = StringIO()
    se = sys.stderr
    sys.stderr = sio 
    setup( ext_modules=[_tosdbExt], **setup_dict )
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
    try_precompiled()
    

