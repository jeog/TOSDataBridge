# Copyright (C) 2014 Jonathon Ogden < jeog.dev@gmail.com > 

from distutils.core import setup,Extension
from distutils.errors import DistutilsError
from math import log
from os import listdir, remove, walk
from shutil import copy2
from io import StringIO
from platform import system
import sys

_isWinSys = system() in ["Windows","windows"]
_sysArch = "x64" if ( log( sys.maxsize * 2, 2) > 33 ) else "x86"
_sysArchD = "Win32" if _sysArch == "x86" else "x64"
_sysMinorV = sys.version_info.minor

setup_dict = {
  "name":'tosdb',
  "version":'0.1',
  "description": "Python Front-End / Wrapper for TOS-DataBridge (C/C++)",
  "author":"Jonathon Ogden",
  "author_email":"jeog.dev@gmail.com",
  "packages": ['tosdb'] 
}      
         
# the cross platfrom stub
ext_stub = Extension( "_tosdb", sources=[ "_tosdb.cpp" ], 
                      include_dirs=[ "../include" ], optional=True )

ext_win = Extension( **ext_stub.__dict__ )

# add/override for Win
ext_win.library_dirs       =  [ "../bin/Release/"+_sysArchD ]
ext_win.libraries          =  [ "_tos-databridge-shared-"+_sysArch,
                                "_tos-databridge-static-"+_sysArch ]
ext_win.define_macros      =  [ ("THIS_IMPORTS_IMPLEMENTATION",None),
                                ("THIS_DOESNT_IMPORT_INTERFACE",None) ]
ext_win.extra_compile_args =  ["/EHsc"]
ext_win.extra_link_args    =  ["/LTCG"]  
        
try: # capture setup errors but allow setup to complete (optional=True)   
    sio = StringIO()
    se = sys.stderr
    sys.stderr = sio  
    setup( ext_modules=[ ext_win if _isWinSys else ext_stub], **setup_dict )  
    sys.stderr = se    
    if sio.getvalue(): 
        print( '\n', "+ Operation 'completed' with errors:\n")
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
    if _isWindSys:           
        print("+ Would you like to try again with a pre-compiled _tosdb.pyd?")         
        if input() in ["y","Y","yes","Yes","YES"]:            
            pydF = "_tosdb-"+str(_sysArch)+"-"+str(_sysMinorV)+".pyd"
            if pydF in listdir():           
                print("Found _tosdb.pyd, sending it to python root directory")
                copy2("./"+pydF, "./_tosdb.pyd")
                setup( data_files=[(".",["_tosdb.pyd"]) ], **setup_dict_win )
                remove("./_tosdb.pyd")
            else:
                print("Sorry, can't find one that matches your python version.")
    

