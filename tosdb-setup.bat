@echo off

NET FILE 1>NUL 2>NUL &

IF %ERRORLEVEL% NEQ 0 (
    echo tosdb-setup.bat must be run as administrator.
    EXIT /B 1
)

set bCRTfiles=false

IF EXIST C:/Windows/System32/msvcr110.dll (
    IF EXIST C:/Windows/System32/msvcp110.dll (
        set bCRTfiles=true
    )
)

set servCmd=""

IF /I "%2"=="admin" ( 
    set servCmd=" --admin %3"
) else (
    set servCmd=" %2"
)

IF "%1"=="x64" (
    echo + Copying _tos-databridge-shared-x64.dll to %WINDIR% ...
    COPY /-Y .\bin\Release\x64\_tos-databridge-shared-x64.dll %WINDIR%    
    IF ERRORLEVEL 1 (
        echo - Copy failed, exiting setup...
        EXIT /B 1
    )    
    echo + Checking for VC++ Redistributable Files ...
    IF %bCRTfiles%==false (        
        echo ++ Not Found. Installing VC++ Redistributable ...
        vcredist_x64.exe 
        IF ERRORLEVEL 1 (
            echo - Installation failed, exiting setup...
            EXIT /B 1
        )
    )
    echo + Creating TOSDataBridge Service ...
    SC delete TOSDataBridge 1>NUL 2>NUL 
    SC create TOSDataBridge binPath= %cd%\\bin\\Release\\x64\\tos-databridge-serv-x64.exe%servCmd% 
    IF ERRORLEVEL 1 (
        echo - SC create failed, exiting setup...
        EXIT /B 1
    )    
) else ( 
    IF "%1"=="x86" (
        echo + Copying _tos-databridge-shared-x86.dll to %WINDIR% ...
        COPY /-Y .\bin\Release\Win32\_tos-databridge-shared-x86.dll %WINDIR%        
        IF ERRORLEVEL 1 (
            echo - Copy failed, exiting setup...
            EXIT /B 1
        )    
        echo + Checking for VC++ Redistributable Files ...
        IF EXIST C:/Windows/SysWOW64 set bCRTfiles=false
        IF EXIST C:/Windows/SysWOW64/msvcr110.dll (
            IF EXIST C:/Windows/SysWOW64/msvcp110.dll (
                set bCRTfiles=true
            )
        )
        IF %bCRTfiles%==false (        
            echo ++ Not Found. Installing VC++ Redistributable ...
            vcredist_x86.exe 
            IF ERRORLEVEL 1 (
                echo - Installation failed, exiting setup...
                EXIT /B 1
            )
        )        
        echo + Creating TOSDataBridge Service ...
        SC delete TOSDataBridge 1>NUL 2>NUL         
        SC create TOSDataBridge binPath= %cd%\bin\Release\Win32\tos-databridge-serv-x86.exe%servCmd% 
        IF ERRORLEVEL 1 (
            echo - SC create failed, exiting setup...
            EXIT /B 1
        )    
    ) else (
        echo - Invalid Command Line Argument. Use 'x86' or 'x64' to signify which build to setup.        
        EXIT /B 1
        )
)

EXIT /B 0










