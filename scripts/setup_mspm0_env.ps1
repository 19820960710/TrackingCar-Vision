param(
    [string]$Mspm0Sdk,
    [string]$SysConfig
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$Uvprojx = Join-Path $ProjectRoot 'keil\M0_Templant_FreeRTOS.uvprojx'
$SyscfgBat = Join-Path $ProjectRoot 'keil\syscfg.bat'
$SyscfgFile = Join-Path $ProjectRoot 'main.syscfg'

function Read-PathOrDefault([string]$Prompt, [string[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            $answer = Read-Host "$Prompt [`"$candidate`"]"
            if ([string]::IsNullOrWhiteSpace($answer)) { return $candidate }
            return $answer.Trim('"')
        }
    }
    do {
        $answer = Read-Host $Prompt
        $answer = $answer.Trim('"')
    } while ([string]::IsNullOrWhiteSpace($answer))
    return $answer
}

if ([string]::IsNullOrWhiteSpace($Mspm0Sdk)) {
    $Mspm0Sdk = Read-PathOrDefault 'Input MSPM0 SDK path' @(
        $env:MSPM0_SDK,
        'D:\tool\CCS\mspm0_sdk_2_10_00_04',
        'C:\ti\mspm0_sdk_2_10_00_04',
        'C:\ti\mspm0_sdk_2_10_01_00'
    )
}

if ([string]::IsNullOrWhiteSpace($SysConfig)) {
    $SysConfig = Read-PathOrDefault 'Input SysConfig install path' @(
        $env:SYSCONFIG,
        'D:\tool\CCS\sysconfig_desktop',
        'C:\ti\sysconfig_1.26.2',
        'C:\ti\sysconfig_1.25.0'
    )
}

$Mspm0Sdk = (Resolve-Path $Mspm0Sdk).Path
$SysConfig = (Resolve-Path $SysConfig).Path

$RequiredFiles = @(
    (Join-Path $Mspm0Sdk '.metadata\product.json'),
    (Join-Path $Mspm0Sdk 'source\ti\devices\msp\msp.h'),
    (Join-Path $Mspm0Sdk 'source\ti\driverlib\lib\keil\m0p\mspm0g1x0x_g3x0x\driverlib.a'),
    (Join-Path $SysConfig 'nodejs\node.exe'),
    (Join-Path $SysConfig 'dist\cli.js'),
    (Join-Path $SysConfig 'sysconfig_gui.bat')
)

foreach ($file in $RequiredFiles) {
    if (!(Test-Path $file)) {
        throw "Required file not found: $file"
    }
}

[Environment]::SetEnvironmentVariable('MSPM0_SDK', $Mspm0Sdk, 'User')
[Environment]::SetEnvironmentVariable('SYSCONFIG', $SysConfig, 'User')
$env:MSPM0_SDK = $Mspm0Sdk
$env:SYSCONFIG = $SysConfig

Write-Host '[1/3] User environment variables are set:'
Write-Host "  MSPM0_SDK=$Mspm0Sdk"
Write-Host "  SYSCONFIG=$SysConfig"

if (!(Test-Path $Uvprojx)) { throw "Keil project not found: $Uvprojx" }
$content = [System.IO.File]::ReadAllText($Uvprojx, [System.Text.Encoding]::UTF8)

$CmsisPath = Join-Path $Mspm0Sdk 'source\third_party\CMSIS\Core\Include'
$SdkSourcePath = Join-Path $Mspm0Sdk 'source'
$DriverLibPath = Join-Path $Mspm0Sdk 'source\ti\driverlib\lib\keil\m0p\mspm0g1x0x_g3x0x\driverlib.a'
$ExpectedIncludePath = "../,../Component,$CmsisPath,$SdkSourcePath,.\FreeRTOS\include,.\FreeRTOS\portable\GCC\ARM_CM0,.\FreeRTOS\portable\MemMang"

$content = [Regex]::Replace($content, '<IncludePath>\.\./,\.\./Component,.*?\.\\FreeRTOS\\portable\\MemMang</IncludePath>', "<IncludePath>$ExpectedIncludePath</IncludePath>")
$content = [Regex]::Replace($content, '<Misc>[A-Za-z]:.*?mspm0_sdk_.*?\\source\\ti\\driverlib\\lib\\keil\\m0p\\mspm0g1x0x_g3x0x\\driverlib\.a</Misc>', "<Misc>$DriverLibPath</Misc>")

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($Uvprojx, $content.TrimEnd() + "`r`n", $Utf8NoBom)
Write-Host '[2/3] Keil uvprojx SDK paths are patched.'

if (!(Test-Path $SyscfgBat)) { throw "SysConfig script not found: $SyscfgBat" }
if (!(Test-Path $SyscfgFile)) { throw "main.syscfg not found: $SyscfgFile" }

Write-Host '[3/3] Running SysConfig generation test...'
$KeilDir = Join-Path $ProjectRoot 'keil'
& $SyscfgBat $KeilDir 'main.syscfg'
if ($LASTEXITCODE -ne 0) {
    throw "SysConfig generation failed. Exit code: $LASTEXITCODE"
}

Write-Host ''
Write-Host 'Setup done. Restart Keil / VSCode, then build keil\M0_Templant_FreeRTOS.uvprojx.'
