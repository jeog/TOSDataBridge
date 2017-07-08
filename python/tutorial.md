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

Be sure to use the correct version of the tos-databridge DLL. If you are using branch/version v0.6, for instance, you should be using tos-databridge-0.6-x86.dll or tos-databridge-0.6-x64.dll.
This loads the shared library into python and tries to automatically connect to the engine.

![](./../res/tosdb_tut1.png)

**Topics and Items:**

![](./../res/tosdb_tut2.png)

**TOSDB_DataBlock; adding items/topics to the block; pre-caching:**

![](./../res/tosdb_tut3.png)

**Total Frame:**

![](./../res/tosdb_tut4.png)

**Topic / Item Frame:**

![](./../res/tosdb_tut5.png)

**Block size; stream occupancy; pull individual data-points:**

![](./../res/tosdb_tut6.png)

**Pull multiple contiguous data-points:**

![](./../res/tosdb_tut7.png)

**Pull multiple contiguous data-points from an atomic marker to avoid missing data:**

![](./../res/tosdb_tut8.png)

![](./../res/tosdb_tut9.png)

**Remove topics; show C lib exception with an error code (tos_databridge.h); call clean_up() when done:**

![](./../res/tosdb_tut10.png)

 



