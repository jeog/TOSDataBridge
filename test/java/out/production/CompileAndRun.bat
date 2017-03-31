@echo off

set version=0.8
set bArch=x64
set bArchDir=x64
set jarPath=../../java/tosdatabridge.jar
set execName=TOSDataBridgeTest
set libPath=../../bin/Release/%bArchDir%/tos-databridge-%version%-%bArch%.dll

echo + x64 only! (be sure x64 engine is running)
echo + Test File: %execName%.java
echo + Version: %version%
echo + Build: %bArch%
echo + Library Path: %libPath%


javac -classpath %jarPath% %execName%.java

IF ERRORLEVEL 1 (
    echo - Failed to compile %execName%.java
    EXIT /B 1
)   

java -classpath %jarPath%;. %execName% %libPath%

IF ERRORLEVEL 1 (
    echo - Failed to run %execName%
    EXIT /B 1
) 

EXIT /B 0










