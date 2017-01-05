### Installation Details
- - -

1. Move the unzipped tos-databridge folder to its permanent location. If you change it in the future you'll need to redo some of these steps because the Service module relies on an absolute path.

2. Determine the appropriate build you'll need: 32-bit(x86) or 64-bit(x64). ***Make sure you don't mix and match in future steps, python will automatically look for a version that matches its own build.***
 
3. (***Optional***) Compile the appropriate binaries if [building from source](README.md#build-optional).  
   
4. Determine if your TOS platform runs under elevated privileges (does it ask for an admin password before starting?)
   
5. Determine if you need to run under a custom Session. The tos-databridge-engine.exe[] binary needs to run in the same session as the ThinkOrSwim platform. **MOST USERS SHOULDN'T WORRY ABOUT THIS** unless they plan to run in a non-standard environment (e.g an EC2 instance). 
  
6. Open a command shell with Admin Privileges (right-click on cmd.exe and click 'run as administrator'). Navigate to the tos-databridge root directory and run the tosdb-setup.bat setup script with the info from steps #2, #4, and #5:
    
    ```
    Usage: C:\TOSDataBridge\> tosdb-setup.bat   [x86|x64]   [admin]   [session]
     
    Example 1: C:\TOSDataBridge\> tosdb-setup.bat x86
    Example 2: C:\TOSDataBridge\> tosdb-setup.bat x64 admin
    Example 3: C:\TOSDataBridge\> tosdb-setup.bat x64 admin 2
    ```

7. The setup script is going to do a few things:     
    - make sure you have the appropriate run-time libraries installed; if not the appropriate redist executable will be run to install them. (If this fails you can do this manually by downloading the most recent VC++ Redistributable from Microsoft); 
    - make sure you have the appropriate -serv and -engine binaries in /bin
    - create the Windows Service.
 
8. Before we continue it's a good idea, possibly necessary, to add the tos-databridge binaries, or the whole directory, to your anti-virus's 'white-list'. ***There's a good chance that if you don't do this tos-databridge-engine[].exe, at the very least, may be stopped and/or deleted for 'suspicious' behavior.***
  
9.  After the Windows Service has been successfully created run: 

    ```(Admin) C:\> SC start TOSDataBridge```  

    Returned text and various system utilities should confirms the service is running, (see screen-shot below). Let's also configure the service to begin automatically on system start-up:
    
    ```(Admin) C:\> SC config TOSDataBridge start= auto``` (notice the space between '=' and 'auto')
    
    [More information about the service.](README_SERVICE.md)

    (note: some of the particulars in these (older) screen-shots may be different for newer versions)    
    ![](./res/SCss1.png)

10. (***C/C++ ONLY***) Include the tos_databridge.h header in your code ( if its C++ make sure the compiler can find containers.hpp, generic.hpp, and exceptions.hpp.) and adjust the link settings to import the tos-databridge-0.6-[].lib stub. (If you run into trouble review the VisualStudio settings for tos-databridge-shell[].exe as they should resemble what you're trying to do.)
   
11. (***C/C++ ONLY***) Add appropriate [API calls](README_API.md) to your code.
   
12. Make sure the TOS Platform is running, execute your program or start the [python wrapper](README_PYTHON.md)

