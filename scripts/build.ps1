[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug', 'RelWithDebInfo')]
    [string]$Config = 'Release',

    [string]$QtDir = '',
    [string]$BuildDir = '',
    [switch]$Clean,
    [switch]$NoDeploy
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root 'build'
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)

function Test-QtDirectory([string]$Path) {
    return $Path -and (Test-Path (Join-Path $Path 'bin\qmake.exe'))
}

function Find-QtDirectory {
    if ($QtDir) {
        if (Test-QtDirectory $QtDir) { return (Resolve-Path $QtDir).Path }
        throw "Invalid Qt directory: $QtDir"
    }

    $candidates = @()
    if ($env:QT_ROOT) { $candidates += $env:QT_ROOT }

    $searchRoots = @('D:\Qt', 'C:\Qt', (Join-Path $env:USERPROFILE 'Qt'))
    foreach ($searchRoot in $searchRoots) {
        if (-not (Test-Path $searchRoot)) { continue }
        $versionDirectories = Get-ChildItem $searchRoot -Directory |
            Where-Object { $_.Name -match '^\d+\.\d+' } |
            Sort-Object { [version](($_.Name -split '-')[0]) } -Descending
        foreach ($versionDirectory in $versionDirectories) {
            $kits = Get-ChildItem $versionDirectory.FullName -Directory |
                Where-Object { $_.Name -like 'mingw*_64' }
            foreach ($kit in $kits) { $candidates += $kit.FullName }
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-QtDirectory $candidate) { return (Resolve-Path $candidate).Path }
    }

    throw 'Qt 6 MinGW installation not found. Use -QtDir, for example D:\Qt\6.12.0\mingw_64.'
}

function Find-MinGwDirectory([string]$QtPath) {
    $qtRoot = Split-Path (Split-Path $QtPath -Parent) -Parent
    $toolsRoot = Join-Path $qtRoot 'Tools'
    $candidates = @()
    foreach ($name in @('mingw1310_64', 'mingw1510_64', 'mingw810_64')) {
        $candidates += (Join-Path $toolsRoot $name)
    }
    if (Test-Path $toolsRoot) {
        $candidates += (Get-ChildItem $toolsRoot -Directory |
            Where-Object { $_.Name -like 'mingw*_64' } |
            Sort-Object Name -Descending |
            ForEach-Object { $_.FullName })
    }

    foreach ($candidate in $candidates) {
        if ((Test-Path (Join-Path $candidate 'bin\g++.exe')) -and
            (Test-Path (Join-Path $candidate 'bin\mingw32-make.exe'))) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "MinGW toolchain not found under $toolsRoot."
}

$QtPath = Find-QtDirectory
$MinGwPath = Find-MinGwDirectory $QtPath
Write-Host "Qt:    $QtPath"
Write-Host "MinGW: $MinGwPath"

$env:Path = "$MinGwPath\bin;$QtPath\bin;$env:Path"

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCommand) {
    $qtRoot = Split-Path (Split-Path $QtPath -Parent) -Parent
    $fallbackCmake = Join-Path $qtRoot 'Tools\CMake_64\bin\cmake.exe'
    if (Test-Path $fallbackCmake) {
        $cmakeCommand = Get-Item $fallbackCmake
    } else {
        throw 'cmake was not found. Add CMake to PATH first.'
    }
}
$cmake = $cmakeCommand.Source

if ($Clean) {
    $rootWithSeparator = $Root.TrimEnd('\') + '\'
    if (-not $BuildDir.StartsWith($rootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to delete a path outside the project: $BuildDir"
    }
    if (Test-Path $BuildDir) {
        Write-Host "Cleaning $BuildDir"
        Remove-Item -Recurse -Force $BuildDir
    }
}

Write-Host "Configuring ($Config)..."
& $cmake -S $Root -B $BuildDir -G 'MinGW Makefiles' `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DCMAKE_PREFIX_PATH=$QtPath" `
    "-DCMAKE_C_COMPILER=$MinGwPath\bin\gcc.exe" `
    "-DCMAKE_CXX_COMPILER=$MinGwPath\bin\g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$MinGwPath\bin\mingw32-make.exe"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }

Write-Host 'Building...'
& $cmake --build $BuildDir --parallel ([Environment]::ProcessorCount)
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

$exe = Join-Path $BuildDir 'bin\StoryBoard.exe'
if (-not (Test-Path $exe)) { throw "Executable not found: $exe" }

if (-not $NoDeploy) {
    $windeployqt = Join-Path $QtPath 'bin\windeployqt.exe'
    if (Test-Path $windeployqt) {
        Write-Host 'Deploying Qt runtime...'
        & $windeployqt --no-translations $exe
        if ($LASTEXITCODE -ne 0) { Write-Warning 'windeployqt failed; you can run it manually later.' }

        # Keep the offscreen platform plugin available for --self-test in headless sessions.
        $offscreen = Join-Path $QtPath 'plugins\platforms\qoffscreen.dll'
        $targetPlatforms = Join-Path $BuildDir 'bin\platforms'
        if ((Test-Path $offscreen) -and (Test-Path $targetPlatforms)) {
            Copy-Item $offscreen $targetPlatforms -Force
        }
    }
}

Write-Host ''
Write-Host "Build complete: $exe"
Write-Host "Run: powershell -ExecutionPolicy Bypass -File `"$Root\scripts\run.ps1`""
