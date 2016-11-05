@echo off

setlocal enableDelayedExpansion 

set "OURsrc=test.c"
set "RELdir=..\..\bin\Release"
set "INCLdir=..\..\include"
set "BINbase=TestBuild"

IF [%3] == [] (
  echo Usage: "%0  [x86|x64]  [C|CPP]  [VS_TOOLS_NUM]"
  echo        "[x86|x64]      - build" 
  echo        "[C|CPP]        - language"
  echo        "[VS_TOOLS_NUM] - integer part of VS...COMNTOOLS variable (e.g VS110COMNTOOLS -> 110)"
  EXIT /B 1  
) else (    
  set "VScpath=!VS%3%COMNTOOLS!..\..\VC" 
  if NOT EXIST !VScpath! ( 
    echo fatal: can't find VS%3%COMNTOOLS; or it contains an invalid path
    EXIT /B 1
  )
) 

IF "%1"=="x86" (
  set "CMDpath=\bin"
  set "OURlibdir=!RELdir!\Win32"
  set "OURlibstub=!RELdir!\Win32\tos-databridge-0.5-x86.lib"
  set "OURdllname=tos-databridge-0.5-x86.dll"
  set "OURdll2name=_tos-databridge-x86.dll"
  set "OURdllpath=!RELdir!\Win32\!OURdllname!"
  set "OURdll2path=!RELdir!\Win32\!OURdll2name!"
  set "VCvars=bin\VCvars32.bat"
  set "MACHarg=X86"
  set "OURobj=!BINbase!32.obj"
  set "OURexec=!BINbase!32.exe"
) else (
  IF "%1"=="x64" (
    set "CMDpath=\bin\x86_amd64"    
    set "OURlibdir=!RELdir!\x64"
    set "OURlibstub=!RELdir!\x64\tos-databridge-0.5-x64.lib"
    set "OURdllname=tos-databridge-0.5-x64.dll"
    set "OURdll2name=_tos-databridge-x64.dll"
    set "OURdllpath=!RELdir!\x64\!OURdllname!"
    set "OURdll2path=!RELdir!\x64\!OURdll2name!"
    set "VCvars=bin\x86_amd64\VCvarsx86_amd64.bat"
    set "MACHarg=X64"
    set "OURobj=!BINbase!64.obj"
    set "OURexec=!BINbase!64.exe"
  ) else (
    echo Usage: "%0  [x86|x64]  [C|CPP]  [VS_TOOLS_NUM]"
    echo        "[x86|x64]      - build" 
    echo        "[C|CPP]        - language"
    echo        "[VS_TOOLS_NUM] - integer part of VS...COMNTOOLS variable (e.g VS110COMNTOOLS -> 110)"
    EXIT /B 1
  )
)

setlocal disableDelayedExpansion

IF "%2"=="C" (
  set "LANGflag=/TC"
  set "LANG=C"
) else (
  IF "%2"=="CPP" (
    set "LANGflag=/TP"
    set "LANG=C++"
  ) else (
    echo Usage: "%0  [x86|x64]  [C|CPP]  [VS_TOOLS_NUM]"
    echo        "[x86|x64]      - build" 
    echo        "[C|CPP]        - language"
    echo        "[VS_TOOLS_NUM] - integer part of VS...COMNTOOLS variable (e.g VS110COMNTOOLS -> 110)"
    EXIT /B 1
  )
)
echo --------------------------------------------------------------------------
echo MACHarg:     %MACHarg%
echo LANG:        %LANG%
echo VCvars:      %VCvars%
echo VScpath:     %VScpath%
echo CMDpath:     %CMDpath%
echo OURsrc:      %OURsrc%
echo OURobj:      %OURobj%
echo OURexec:     %OURexec%
echo INCLdir:     %INCLdir%
echo OURlibdir:   %OURlibdir%
echo OURlibstub:  %OURlibstub%
echo OURdllname:  %OURdllname%
echo OURdllpath:  %OURdllpath%
echo OURdll2name: %OURdll2name%
echo OURdll2path: %OURdll2path%
echo -------------------------------------------------------------------------------


if NOT EXIST "%VSINSTALLDIR%" (
  echo.
  echo Setting up environment...  
  CALL "%VScpath%\%VCvars%"  
  IF %ERRORLEVEL% NEQ 0 ( 
    echo fatal: error setting up environment
    EXIT /B 1 
  )
)

IF NOT EXIST "%VSINSTALLDIR%" ( 
  echo fatal: can't find 'VSINSTALLDIR'
  EXIT /B 1
)

echo.
echo Clearing Files...
del *.exe *.obj *.dll *.lib *.pdb 2>NUL

echo.
echo Compiling...
@echo on

"%VSINSTALLDIR%\VC\%CMDpath%\CL.exe" /c /Fo"%OURobj%" /IC:%INCLdir% /Zi /nologo ^
/W3 /WX- /sdl /O2 /Oi /GL /D _MBCS /Gm- /EHsc /MD /GS /Gy /fp:precise /Zc:wchar_t ^
 /Zc:forScope /Gd %LANGflag% /errorReport:prompt %OURsrc%

@echo off

IF %ERRORLEVEL% NEQ 0 ( 
  echo fatal: compilation error
  EXIT /B 1 
)

echo.
echo Linking...
@echo on

"%VSINSTALLDIR%\VC\%CMDpath%\link.exe" /ERRORREPORT:PROMPT /OUT:%OURexec% /NOLOGO ^
/LIBPATH:"%OURlibdir%" "%OURlibstub%" kernel32.lib user32.lib gdi32.lib ^
winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib ^
odbc32.lib odbccp32.lib /OPT:REF /OPT:ICF /LTCG /TLBID:1 /DYNAMICBASE /NXCOMPAT ^
/MACHINE:%MACHarg% %OURobj%

@echo off

IF %ERRORLEVEL% NEQ 0 ( 
  echo fatal: link error
  EXIT /B 1 
)

echo.
echo Checking Dependencies...
IF NOT EXIST %OURdllname% ( COPY %OURdllpath% . )
IF NOT EXIST %OURdll2name% ( COPY %OURdll2path% . )

IF %ERRORLEVEL% NEQ 0 ( EXIT /B 1 )

echo.
echo Running %OURexec%...
@echo on
"%OURexec%"
@echo off

IF %ERRORLEVEL% NEQ 0 (
  echo fatal: error running %OURexec%
  EXIT /B 1
) else (
  echo + Success!
)
