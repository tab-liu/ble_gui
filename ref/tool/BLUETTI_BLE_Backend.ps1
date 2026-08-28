param(
    [Parameter(Mandatory=$true)][string]$CommandPath,
    [Parameter(Mandatory=$true)][string]$DevicePath,
    [Parameter(Mandatory=$true)][string]$StatusPath,
    [Parameter(Mandatory=$true)][string]$DataPath,
    [Parameter(Mandatory=$true)][string]$ModbusPath,
    [Parameter(Mandatory=$true)][string]$TrafficPath,
    [Parameter(Mandatory=$true)][string]$OtaStatusPath
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'
$Utf8Bom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $true
$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourcePath = Join-Path $ScriptDirectory 'BLUETTI_BLE_Bridge.cs'
$BouncyCastlePath = Join-Path $ScriptDirectory 'BouncyCastle.Crypto.dll'
$CacheDirectory = Join-Path $env:TEMP 'BLUETTI_Firmware_Studio\BLE'
$CompileLogPath = Join-Path $CacheDirectory 'BLE_Direct_V1.3.5_Compile.log'
$LauncherLogPath = Join-Path $CacheDirectory 'BLE_Direct_V1.3.5_Launcher.log'

if (-not (Test-Path -LiteralPath $CacheDirectory)) { New-Item -ItemType Directory -Path $CacheDirectory -Force | Out-Null }

function Write-Utf16Status {
    param([string]$Text)
    try { [System.IO.File]::WriteAllText($StatusPath, $Text + [Environment]::NewLine, [System.Text.Encoding]::Unicode) } catch { }
}

function Write-LauncherLog {
    param([string]$Text)
    try {
        $line = ('[{0:yyyy-MM-dd HH:mm:ss.fff}] {1}' -f [DateTime]::Now, $Text)
        [System.IO.File]::AppendAllText($LauncherLogPath, $line + [Environment]::NewLine, $Utf8Bom)
    } catch { }
}

function Resolve-CscPath {
    foreach ($candidate in @(
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe'),
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe')
    )) { if (Test-Path -LiteralPath $candidate) { return $candidate } }
    throw '未找到 .NET Framework C# 编译器 csc.exe。'
}

function Resolve-WindowsRuntimeAssemblyPath {
    $candidates = New-Object System.Collections.Generic.List[string]
    $candidates.Add((Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\System.Runtime.WindowsRuntime.dll'))
    $candidates.Add((Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\System.Runtime.WindowsRuntime.dll'))
    $pf86 = ${env:ProgramFiles(x86)}
    if (-not [String]::IsNullOrWhiteSpace($pf86)) {
        foreach ($version in @('v4.8','v4.7.2','v4.7.1','v4.7','v4.6.2','v4.6.1','v4.6','v4.5.2','v4.5.1','v4.5')) {
            $candidates.Add((Join-Path $pf86 ('Reference Assemblies\Microsoft\Framework\.NETFramework\' + $version + '\Facades\System.Runtime.WindowsRuntime.dll')))
        }
        $candidates.Add((Join-Path $pf86 'Reference Assemblies\Microsoft\Framework\.NETCore\v4.5\System.Runtime.WindowsRuntime.dll'))
    }
    foreach ($candidate in $candidates) { if (Test-Path -LiteralPath $candidate) { return $candidate } }
    throw '未找到 System.Runtime.WindowsRuntime.dll。'
}

function Resolve-OptionalReference {
    param([string]$FileName)
    $candidates = New-Object System.Collections.Generic.List[string]
    $candidates.Add((Join-Path $env:WINDIR ('Microsoft.NET\Framework\v4.0.30319\' + $FileName)))
    $candidates.Add((Join-Path $env:WINDIR ('Microsoft.NET\Framework64\v4.0.30319\' + $FileName)))
    $pf86 = ${env:ProgramFiles(x86)}
    if (-not [String]::IsNullOrWhiteSpace($pf86)) {
        foreach ($version in @('v4.8','v4.7.2','v4.7.1','v4.7','v4.6.2','v4.6.1','v4.6','v4.5.2','v4.5.1','v4.5')) {
            $candidates.Add((Join-Path $pf86 ('Reference Assemblies\Microsoft\Framework\.NETFramework\' + $version + '\Facades\' + $FileName)))
            $candidates.Add((Join-Path $pf86 ('Reference Assemblies\Microsoft\Framework\.NETFramework\' + $version + '\' + $FileName)))
        }
    }
    foreach ($candidate in $candidates) { if (Test-Path -LiteralPath $candidate) { return $candidate } }
    return $null
}

function Resolve-WinMetadataDirectory {
    $candidates = New-Object System.Collections.Generic.List[string]
    if (-not [String]::IsNullOrWhiteSpace($env:PROCESSOR_ARCHITEW6432)) { $candidates.Add((Join-Path $env:WINDIR 'Sysnative\WinMetadata')) }
    $candidates.Add((Join-Path $env:WINDIR 'System32\WinMetadata'))
    $candidates.Add((Join-Path $env:WINDIR 'SysWOW64\WinMetadata'))
    foreach ($candidate in $candidates) {
        $foundation = Join-Path $candidate 'Windows.Foundation.winmd'
        $devices = Join-Path $candidate 'Windows.Devices.winmd'
        $storage = Join-Path $candidate 'Windows.Storage.winmd'
        if ((Test-Path -LiteralPath $foundation) -and (Test-Path -LiteralPath $devices) -and (Test-Path -LiteralPath $storage)) { return $candidate }
    }
    throw '未找到 Windows 系统 WinMetadata（Foundation/Devices/Storage），无法启用 Direct BLE。'
}

try {
    Write-LauncherLog '----------------------------------------'
    Write-LauncherLog '启动 V1.3.5 Direct WinRT BLE launcher'
    if (-not (Test-Path -LiteralPath $SourcePath)) { throw '缺少 BLUETTI_BLE_Bridge.cs' }
    if (-not (Test-Path -LiteralPath $BouncyCastlePath)) { throw '缺少 BouncyCastle.Crypto.dll' }

    # V1.3.5 preflight: Windows.Storage.Streams defines Buffer too. All array copies must explicitly use System.Buffer.
    $SourceText = [System.IO.File]::ReadAllText($SourcePath, [System.Text.Encoding]::UTF8)
    if ($SourceText -match '(?<!System\.)\bBuffer\.BlockCopy\s*\(') { throw 'BLE源码预检失败：存在未限定的 Buffer.BlockCopy，请使用 System.Buffer.BlockCopy。' }
    if ($SourceText -match '\.GetEncoded\s*\(\s*(true|false)\s*\)') { throw 'BLE源码预检失败：存在版本绑定的 ECPoint.GetEncoded(bool)，请使用兼容封装。' }
    if ($SourceText -match '\.CreatePoint\s*\([^;]*new BigInteger[^;]*new BigInteger[^;]*\)') { Write-LauncherLog '提示：EC CreatePoint 已通过兼容封装调用。' }

    $CscPath = Resolve-CscPath
    $RuntimeAssembly = Resolve-WindowsRuntimeAssemblyPath
    $MetadataDirectory = Resolve-WinMetadataDirectory
    $FoundationMetadata = Join-Path $MetadataDirectory 'Windows.Foundation.winmd'
    $DevicesMetadata = Join-Path $MetadataDirectory 'Windows.Devices.winmd'
    $StorageMetadata = Join-Path $MetadataDirectory 'Windows.Storage.winmd'
    Write-LauncherLog ('编译器：' + $CscPath)
    Write-LauncherLog ('WinMetadata目录：' + $MetadataDirectory)
    Write-LauncherLog ('WindowsRuntime：' + $RuntimeAssembly)
    try { Write-LauncherLog ('BouncyCastle版本：' + ([System.Reflection.AssemblyName]::GetAssemblyName($BouncyCastlePath).Version.ToString())) } catch { Write-LauncherLog 'BouncyCastle版本：读取失败' }

    $SourceHash = 'NOHASH'
    try { $SourceHash = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash.Substring(0, 12) } catch { $SourceHash = [IO.File]::GetLastWriteTimeUtc($SourcePath).Ticks.ToString() }
    $BridgePath = Join-Path $CacheDirectory ('BLUETTI_BLE_Direct_V1.3.5_' + $SourceHash + '.exe')
    $ConfigPath = $BridgePath + '.config'
    Copy-Item -LiteralPath $BouncyCastlePath -Destination (Join-Path $CacheDirectory 'BouncyCastle.Crypto.dll') -Force

    if (-not (Test-Path -LiteralPath $BridgePath)) {
        Write-Utf16Status "READY`t蓝牙状态：正在编译 Direct WinRT BLE 后台（使用Windows系统WinMetadata）"
        $CompilerArguments = New-Object System.Collections.Generic.List[string]
        foreach ($arg in @('/nologo','/target:winexe','/platform:x86','/optimize+','/debug-','/codepage:65001','/nowarn:1701,1702')) { $CompilerArguments.Add($arg) }
        $CompilerArguments.Add('/out:' + $BridgePath)
        $CompilerArguments.Add('/reference:System.dll')
        $CompilerArguments.Add('/reference:System.Core.dll')
        $CompilerArguments.Add('/reference:System.Windows.Forms.dll')
        $CompilerArguments.Add('/reference:' + $FoundationMetadata)
        $CompilerArguments.Add('/reference:' + $DevicesMetadata)
        $CompilerArguments.Add('/reference:' + $StorageMetadata)
        $CompilerArguments.Add('/reference:' + $RuntimeAssembly)
        $CompilerArguments.Add('/reference:' + $BouncyCastlePath)
        foreach ($referenceName in @('System.Runtime.dll','System.Runtime.InteropServices.WindowsRuntime.dll')) {
            $referencePath = Resolve-OptionalReference -FileName $referenceName
            if (-not [String]::IsNullOrWhiteSpace($referencePath)) { $CompilerArguments.Add('/reference:' + $referencePath); Write-LauncherLog ('附加引用：' + $referencePath) }
        }
        $CompilerArguments.Add($SourcePath)
        $CompilerOutput = @(& $CscPath @($CompilerArguments.ToArray()) 2>&1)
        $CompilerExitCode = $LASTEXITCODE
        [string[]]$CompilerLines = @($CompilerOutput | ForEach-Object { if ($null -ne $_) { [string]$_ } })
        if ($CompilerLines.Count -eq 0) { [string[]]$CompilerLines = @('C# compiler completed without diagnostics.') }
        [System.IO.File]::WriteAllLines($CompileLogPath, $CompilerLines, $Utf8Bom)
        if ($CompilerExitCode -ne 0 -or -not (Test-Path -LiteralPath $BridgePath)) {
            [string[]]$Errors = @($CompilerLines | Where-Object { $_ -match ': error CS' } | Select-Object -First 6)
            if ($Errors.Count -eq 0) { $Errors = @($CompilerLines | Select-Object -First 10) }
            throw ('Direct BLE 后台编译失败，错误码 ' + $CompilerExitCode + '；' + [String]::Join(' | ', $Errors) + '；日志：' + $CompileLogPath)
        }
        $ConfigText = '<?xml version="1.0" encoding="utf-8"?><configuration><startup useLegacyV2RuntimeActivationPolicy="true"><supportedRuntime version="v4.0" sku=".NETFramework,Version=v4.8" /></startup></configuration>'
        [System.IO.File]::WriteAllText($ConfigPath, $ConfigText, $Utf8Bom)
        Write-LauncherLog ('Direct BLE bridge 编译成功：' + $BridgePath)
    }

    Write-Utf16Status "READY`t蓝牙状态：Direct WinRT BLE 后台已编译，正在启动"
    $ArgumentList = @(
        ('"' + $CommandPath + '"'), ('"' + $DevicePath + '"'), ('"' + $StatusPath + '"'),
        ('"' + $DataPath + '"'), ('"' + $ModbusPath + '"'), ('"' + $TrafficPath + '"'), ('"' + $OtaStatusPath + '"'), ('"' + $ScriptDirectory + '"')
    )
    $Process = Start-Process -FilePath $BridgePath -ArgumentList $ArgumentList -WindowStyle Hidden -PassThru
    Write-LauncherLog ('Direct BLE bridge PID=' + $Process.Id)
    $Process.WaitForExit()
    Write-LauncherLog ('Direct BLE bridge退出，代码=' + $Process.ExitCode)
}
catch {
    $Message = '蓝牙后台初始化失败：' + $_.Exception.Message
    Write-LauncherLog $Message
    Write-Utf16Status ("ERROR`t" + $Message)
    exit 2
}
