TOSDataBridge (TOSDB) is an open-source collection of resources for 'scraping' real-time streaming data off of TDAmeritrade's ThinkOrSwim(TOS) platform, providing C, C++, and Python interfaces. Users of the TOS platform - with some basic programming/scripting knowledge - can use these tools to populate their own databases with market data; perform specialized, low-level, and efficient analysis on large data-sets in real-time; test and debug other financial apps; or even build extensions on top of it.

TOSDB uses TOS's antiquated, yet still useful, DDE feature, directly through the Windows API. The C / C++ interfaces are implemented as a shared library(DLL) that communicates with a back-end Windows Service. The Python interface wraps this library in a more object-oriented, user-friendly format, providing an interactive environment for easy access to the low(er)-level calls. 

[Complete Documentation ( docs/README.html ) ](https://raw.githubusercontent.com/jeog/TOSDataBridge/master/docs/README.html)

###+ Requirements
- Windows (this has been lightly tested on Windows 7 SP1 and Vista SP2)
- TDAmeritrade's ThinkOrSwim(TOS) platform that exposes DDE functionality (it should)
- VC++ 2012 Redistributable (included)
- Some basic Windows knowledge; some basic C, C++, or Python programming knowledge
  
###+ Installation
- tosdb-setup.bat will attempt to install the necessary modules/dependencies for you but you should refer to README.html for a more detailed explanation.
- Be sure to know what build you need(x86 vs x64); it should match your system, all the modules you'll need, and your version of Python(if you plan on using the python wrapper).

 ####++ Core C/C++ Libraries
 ```
(Admin) C:\[...TOSDataBridge]\tosdb-setup.bat   [x86|x64]   [admin]
```
 - [x86|x64] : the version to build (required)
 - [admin] : does your TOS platform require elevation? (leave blank if no)

 ####++ Python Wrapper/Front-End (optional)
 ```
C:\[...TOSDataBridge]\python\python setup.py install
```
 - C/C++ libs must be installed first.
 - Building the _tosdb.pyd extension is problematic on some systems - please refer to README.html for troubleshooting information.


###+ Start
1. (You may need to white-list some of these files in your Anti-Virus software before proceeding.) Start the service:  
```
(Admin) C:\> SC start TOSDataBridge
```
2. Include tos_databridge.h header in your code (if C++ make sure containers.hpp and generic.hpp can be found by the compiler), add the necessary lib calls, build it.
3. Log on to the TOS platform and start your program or the python wrapper(see the python tutorial in \docs for a walk-through).


###LICENSING & WARRANTY

*TOSDB is released under the GNU General Public License(GPL); a copy (LICENSE.txt) should be included. If not, see http://www.gnu.org/licenses. The author reserves the right to issue current and/or future versions of TOSDB under other licensing agreements. Any party that wishes to use TOSDB, in whole or in part, in any way not explicitly stipulated by the GPL, is thereby required to obtain a separate license from the author. The author reserves all other rights.*

*This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. By choosing to use the software - under the broadest interpretation of the term 'use' - you absolve the author of ANY and ALL responsibility, for ANY and ALL damages incurred; including, but not limited to, damages arising from the accuracy and/or timeliness of data the software does, or does not, provide.*     

*Furthermore, TOSDB is in no way related to TDAmeritrade or affiliated parties; users of TOSDB must abide by their terms of service and are solely responsible for any violations therein.*
