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

set NODE_EXE=%SYSCONFIG%\nodejs\node.exe
set CLI_JS=%SYSCONFIG%\dist\cli.js
set PRODUCT_JSON=%MSPM0_SDK%\.metadata\product.json

if not exist "%NODE_EXE%" (
    echo SysConfig node not found: "%NODE_EXE%"
    exit /b 1
)
if not exist "%CLI_JS%" (
    echo SysConfig cli.js not found: "%CLI_JS%"
    exit /b 1
)
if not exist "%PRODUCT_JSON%" (
    echo MSPM0 product.json not found: "%PRODUCT_JSON%"
    exit /b 1
)

"%NODE_EXE%" "%CLI_JS%" -o "%SYSCFG_DIR%" -s "%PRODUCT_JSON%" --compiler keil "%SYSCFG_DIR%\%SYSCFG_FILE%"
exit /b %ERRORLEVEL%
