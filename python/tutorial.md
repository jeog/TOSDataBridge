### tosdb/ tutorial 
---

This tutorial attempts to show example usage of the core tosdb package. Please refer to *\_win.py*, *\_common.py* and *\__init__.py* for more details on the core package; *\__init__.\__doc__* for an explanation of the virtual layer; *\__main__.py* for running the package outside of the interactive interpreter; *intervalize/ohlc.py* for fixed-time intervals(e.g OHLC); and *cli_scripts/* for code built on top of the core package.

Comments inside the screen-shots help explain what we are doing and why.

**IMPORTANT:** Before using tosdb/ be sure to:

1. build/install the backend C/C++ modules with the appropriate build(x86 vs. x64) for your version of python, 
2. start the TOSDataBridge Service(SC Start TOSDataBridge),
3. have the TOS platform running, with the correct privileges (i.e if you run as admin you should have passed 'admin' to the tosdb-setup.bat script),
4. install the tosdb/ package (python setup.py install)

---

**Initialize the library with the init() call:**  

This loads the shared library into python and tries to automatically connect to the engine. Be sure to use the correct version of the tos-databridge DLL. If you are using branch/version v0.7 and 64-bit python, for instance, you should be using tos-databridge-0.7-x64.dll.  
![](./../res/tosdb_tut1.png)

**Topics and Items:**
![](./../res/tosdb_tut2.png)

**TOSDB_DataBlock:**
![](./../res/tosdb_tut3.png)

**Total Frame:**
![](./../res/tosdb_tut4.png)

**Topic / Item Frames:**
![](./../res/tosdb_tut5.png)

**Historical Data:**
![](./../res/tosdb_tut6.png)

**Historical Data (2):**
![](./../res/tosdb_tut7.png)

**Historical Data (3):**
![](./../res/tosdb_tut8.png)
![](./../res/tosdb_tut9.png)

**Miscellany & Clean-Up:**
![](./../res/tosdb_tut10.png)

 



