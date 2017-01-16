### TOSDataBridge Service
- - -

The TOSDataBridge backend is implemented as a Windows Service. It communicates with the TOS DDE server, putting returned data into circular buffers, in shared memory, that any number of client code instances can access and share. This allows: 

- different user-space programs to share raw data to avoid redundancy 
- most of the code to run with lower privileges
- those interested the ability to write their own interfaces/bindings
- easier debugging
- the service to stay running and/or start automatically on system start-up

#### Service Commands

A Service works differently than a typical program; some useful commands:

- ```(Admin) C:\> SC start TOSDataBridge``` 
- ```(Admin) C:\> SC query TOSDataBridge``` - Display current status of the service.  
- ```(Admin) C:\> SC stop TOSDataBridge``` - Stop the service. All the data collected so far will still exist but the engine will sever its connection to TOS and exit.  It should no longer be shown as a running process and its status should be Stopped.  
- ```(Admin) C:\> SC pause TOSDataBridge``` - Pause the service. All the data collected so far will still exist but the engine will stop recording new data in the buffers. It should still be shown as a running process but its status should be Paused. **Pausing the service is not recommended.**
- ```(Admin) C:\> SC continue TOSDataBridge``` - Continue a paused service. All the data collected so far will still exist, the engine will start recording new data into the buffers, but you will have missed any streaming data while paused. The service should return to the Running state.  
- ```(Admin) C:\> SC config TOSDataBridge ...``` - Adjust the service's configuration/properties.
- ```(Admin) C:\> SC /?``` - Display help for the SC command.

#### tos-databridge-engine

Once started the service spawns a child process(tos-databridge-engine) with lower/restricted privileges that does all the leg work. Occassionally(debuging, for instance) it can be useful to run the engine directly by calling the service binary with the --noservice switch. The engine binary will then enter a 'detached' state, **requiring the user to manually kill the proces when done.**

    Example 1: (Admin) C:\>TOSDataBridge\bin\Debug\x64\> tos-databridge-serv-x64_d.exe --noservice
    Example 2: (Admin) C:\>TOSDataBridge\bin\Release\Win32\> tos-databridge-serv-x86.exe --noservice --admin   

The engine creates a number of kernel objects(mutexs, shared memory segments etc.) that require certain privileges. These privileges are set in SpawnRestrictedProcess() in service.cpp. If you attempt to run the engine binary directly, as a standard user, the creation of these objects will fail, resulting in a fatal uncaught exception. (See the comments near the top of tos_databridge.h, where NO_KGBLNS is defined, for more details.) **Running the engine directly is not recommended.** 
