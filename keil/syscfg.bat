@echo off
set PROJ_DIR=%~1
set PROJ_DIR=%PROJ_DIR:'=%
set SYSCFG_FILE=%~2
set SYSCFG_FILE=%SYSCFG_FILE:'=%
if "%PROJ_DIR:~-1%"=="\" set PROJ_DIR=%PROJ_DIR:~0,-1%
pushd "%PROJ_DIR%\.."
set SYSCFG_DIR=%CD%
popd
if not defined MSPM0_SDK set MSPM0_SDK=D:\tool\CCS\mspm0_sdk_2_10_00_04
if not defined SYSCONFIG set SYSCONFIG=D:\tool\CCS\sysconfig_desktop
call "%SYSCONFIG%\sysconfig_cli.bat" -o "%SYSCFG_DIR%" -s "%MSPM0_SDK%\.metadata\product.json" --compiler keil "%SYSCFG_DIR%\%SYSCFG_FILE%"
