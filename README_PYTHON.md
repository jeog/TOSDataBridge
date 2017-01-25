### Python Wrapper (tosdb/)
- - -

The python wrapper is a simpler way to get started with the underlying library. The tosdb package will use ctypes.py to load the library and wrap the calls, mostly as tosdb.TOSDB_DataBlock methods. 

> **IMPORTANT:** tosdb was only written to be compatible with python3

> **IMPORTANT:** You still must [install the underlying C/C++ modules.](README_INSTALL.md) You also must be sure the build of these modules matches your python build. Open a python shell and look to see if it says 32 bit or 64 bit on the top - 32 bit needs x86 modules; 64 bit needs x64. If they don't match redo the installation.

#### Install

`C:\TOSDataBridge\python\> python setup.py install`

#### Getting Started

tosdb/ is structured as a package with the bulk of its code in \__init__.py and \_win.py , the latter holding the non-portable parts that \__init__.py will import if it determines it's being run on a windows sytem. 
```
# import the package
import tosdb

# (or) import an individual module
from tosdb import intervalize
```

Once imported you'll have to initialize it so it can connect to the underlying C/C++ library. This requires the path of tos-databridge-[].dll or the root directory it's going to search in for the latest version. 

```
tosdb.init(root="C:/TOSDataBridge/bin/Release")
```

Please see [python/tutorial.md](./python/tutorial.md) for a walk-through with screen-shots.

#### Virtual Layer

To use TOSDataBridge from a non-windows systems you can use the virtual interface. To do this you'll still need to install the C/C++ modules and the tosdb package on a (physically or virtually) networked windows sytem.

The virtual interface consists of, more-or-less, the same objects/calls, with a prepended 'v' (i.e VTOSDB_DataBlock, vinit()). See the detailed explanation in tosdb/\_\_init\_\_.\_\_doc\_\_ or the [virtual tutorial](./python/virtualization_tutorial.md) for example usage.

> **IMPORTANT:** We recently added a (provisional) authentication mechanism to the virtual layer. ***Unless you know what you're doing and can review the code (tosdb/\_\_init\_\_.py and tosdb/\_auth.py) it's prudent to assume it not secure, possibly exploitable for remote code execution.*** (If anyone out there can give any feedback it would be helpful.) Currently it's recommended for internal networks. To use: 1) install the pycrypto package if you don't have it(pip install pycrypto), 2) pass a password to the enable_virtualization call on the server side and (the same password) to the admin_init call and/or the VTOSDB_DataBlock constructor on the client side.   

#### Cleanup

There is a minor issue with how python uses the underlying library to deallocate shared resources, mostly because of python's use of refcounts. WE STRONGLY RECOMMEND you call clean_up() before exiting to be sure all shared resources have been properly dealt with. If this is not possible - the program terminates abruptly, for instance - there's a chance you've got dangling/orphaned resources. 

You can dump the state of these resources to a file in /log using tos-databridge-shell-[].exe: 
``` 
   [--> Connect
   [--> DumpBufferStatus
```
