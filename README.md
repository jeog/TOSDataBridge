## TOSDataBridge  
- - -
TOSDataBridge (TOSDB) is an open-source collection of resources for 'scraping' real-time streaming data off of TDAmeritrade's ThinkOrSwim(TOS) platform, providing C, C++, and Python interfaces. 

TOSDB uses TOS's antiquated, yet still useful, DDE feature, directly through the Windows API. The C / C++ interfaces are implemented as a shared library that communicates with a backend Windows Service. The Python interface wraps this library in a more object-oriented, user-friendly format.

The core implementation is not portable, but the python interface does provides a thin virtualization layer over TCP. A user running Windows in a Virtual Machine, for instance, can expose the exact same python interface to a different host system running python3. 

![](./res/main_diagram.png)

### Requirements
- - -
- Windows for the core implementation. (The python interface is available to any system running python3.)
- TDAmeritrade's ThinkOrSwim(TOS) platform that exposes DDE functionality (the Window's verion)
- VC++ 2012 Redistributable (included)

### Quick Setup
- - -
- tosdb-setup.bat will attempt to install the necessary modules/dependencies for you but you should refer to [Installation Details](README_INSTALL.md) for a more detailed explanation
- Be sure to know what build you need(x86 vs x64); it should match your system, all the modules you'll need, and your version of Python(if you plan on using the python wrapper)

    ##### Core C/C++ Libraries
    ```
    (Admin) C:\[...TOSDataBridge]\tosdb-setup.bat   [x86|x64]   [admin]   [session]
    ```

    - [x86|x64] : the version to build (required)
    - [admin] : does your TOS platform require elevation? (optional) 
    - [session] : override the service's attempt to determine the session id when exiting from session-0 isolation. The tos-databridge-engine.exe[] binary needs to run in the same session as the ThinkOrSwim platform. **Most users shouldn't worry about this** unless they plan to run in a non-standard environment (e.g an EC2 instance).  [An explanation of Sessions, Desktops, and Stations.](https://blogs.technet.microsoft.com/askperf/2007/07/24/sessions-desktops-and-windows-stations/) (optional)
 
    ```
    Example 1: C:\TOSDataBridge\> tosdb-setup.bat x86
    Example 2: C:\TOSDataBridge\> tosdb-setup.bat x64 admin
    Example 3: C:\TOSDataBridge\> tosdb-setup.bat x64 admin 2
    ```

    ##### Python Wrapper (optional)
    ```
    C:\[...TOSDataBridge]\python\python setup.py install
    ```
    - Core C/C++ libs (above) must be installed first to use the (non-virtual) interface
    - C++ python extensions have been converted to pure python to avoid portability/build issues. (tosdb/_tosdb.py is now generated automatically by setup.py)

### Quick Start
- - -
1. You may need to white-list some of these files (specifically tos-databridge-engine-\[x86|x64].exe) in your Anti-Virus software before proceeding.
2. Start the service:

    ```
    (Admin) C:\> SC start TOSDataBridge
    ```
   (consider having the service begin automatically on startup to avoid this step in the future; see #9 in the [Installation Details](README_INSTALL.md).)
3. Log on to your TOS platform

##### For C/C++:
- Include tos_databridge.h header in your code 
- Use the library calls detailed in [C/C++ API](README_API.md)
- Link with *tos-databridge-0.6-[x86|x64].dll*
- Build
- Run

##### For Python:
- import tosdb
- [(view tutorial)](python/tutorial.md)


### Build (optional)
- - -
- The recommended way to build from source is to use VisualStudio. Open the .sln file in /VisualStudioBuild, go to the appropriate build, and select 'build'. 

- Object code and logs are placed into an intermediate sub-directory of /VisualStudioBuild of form /$(Configuration)/$(Platform). After the object code is linked the binaries are sent to a sub-directory of /bin of form /$(Configuration)/$(Platform) and the symbol files (if applicable) are sent to /symbols/$(Configuration).

- There are 32 and 64 bit (Win32 and x64) binaries included along with the relevant configurations in the VisualStudio solution. Debug versions have a "_d" suffix to avoid collisions. It's up to the user to choose and use the correct builds for ALL modules. The python library will search for the underlying DLL (-x64 vs. -x86) that matches the build of that version of python.


### Contents
- - -
- **/include** - C/C++ header files; your C/C++ code must include the tos-databridge.h header; C++ code needs to be sure the compiler can find containers.hpp, generic.hpp, exceptions.hpp.

- **/src** - C/C++ source files; if building from source simply open the .sln file inside /VisualStudioBuild, select the configuration/platform, and build.

- **/VisualStudioBuild** - The complete Visual Studio solution with properties, pre-processor options, relative links etc. set, ready to be built.

- **/bin** - Compiled (release only) binaries by build type. **(master branch may, or may not, contain all, or any)**

    - *tos-databridge-serv-[x86|x64].exe* : The service process that spawns and controls the main engine described below. This program is run as a typical windows service with SYSTEM privileges; as such its intended role is very limited. 
    
    - *tos-databridge-engine-[x86|x64].exe* : The main engine - spawned from tos-databridge-serv.exe - that interacts with the TOS platform and our DLL(below). It runs with a lower(ed) integrity level and reduced privileges. 
    
    - *tos-databridge-0.6-[x86|x64].dll* : The library/interface that client code uses to access TOSDB. Review tos-databridge.h, and the sections below, for all the necessary calls, types, and objects.
    
    - *_tos-databridge-[x86|x64].dll* : A back-end library that provides custom concurrency and IPC objects; logging and utilities; as well as the Topic-String mapping. 
    
    - *tos-databridge-shell-[x86|x64]* : A shell used to interact directly with the library calls.

- **/Symbols** - Symbol (.pdb) files **(master branch may, or may not, contain all, or any)**

- **/python** - Files relevant to the python wrapper.

    - **tosdb/** : A python package that serves as a wrapper around *tos-databridge-0.6-[x86|x64].dll*. It provides a more object oriented, simplified means of accessing the core functionality.

    - **tosdb/cli_scripts/** : Python scripts built on top of the python wrapper.

- **/sigs** - The detached signature for each binary; sha256 checksums for binaries, signatures, and the jeog.dev public key; **(master branch may, or may not, contain all, or any)**

- **/res** - Miscellaneous resources

- **/log** - All log files and dumps

- **/prov** - Provisional code that is being worked on outside the main build tree


### [Installation Details](README_INSTALL.md)
- - -

### [C/C++ API](README_API.md)
- - -

### [TOSDataBridge Service](README_SERVICE.md)
- - -

### [Python Wrapper (tosdb/)](README_PYTHON.md)
- - -

### [Details, Provisos & Source Glossary](README_DETAILS.md)
- - -

### Contributions
---
This project grew out of personal need and is maintained by a single developer. Contributions - testing, bug fixes, suggestions, extensions, whatever - are always welcome. If you want to contribute something non-trivial it's recommended you communicate the intention first to avoid unnecessary and/or conflicting work.

Simply reporting bugs or questioning seemingly idiotic or unintuitive interface design can be very helpful.


### License & Terms
- - -
*TOSDB is released under the GNU General Public License(GPL); a copy (LICENSE.txt) should be included. If not, see http://www.gnu.org/licenses. The author reserves the right to issue current and/or future versions of TOSDB under other licensing agreements. Any party that wishes to use TOSDB, in whole or in part, in any way not explicitly stipulated by the GPL, is thereby required to obtain a separate license from the author. The author reserves all other rights.*

*This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.*

*By choosing to use the software - under the broadest interpretation of the term "use" \- you absolve the author of ANY and ALL responsibility, for ANY and ALL damages incurred; including, but not limited to, damages arising from the accuracy and/or timeliness of data the software does, or does not, provide.*

*Furthermore, TOSDB is in no way related to TDAmeritrade or affiliated parties; users of TOSDB must abide by their terms of service and are solely responsible for any violations therein.*

- - -

*Copyright (C) 2014 Jonathon Ogden*


