[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = if ($env:STORYBOARD_BUILD_DIR) { $env:STORYBOARD_BUILD_DIR } else { Join-Path $Root 'build' }
$exe = Join-Path $BuildDir 'bin\StoryBoard.exe'

if (-not (Test-Path $exe)) {
    throw "Executable not found: $exe. Run scripts\build.ps1 first."
}

# If the Qt runtime was not deployed next to the executable, fall back to the
# locally installed Qt/MinGW paths so the application still starts.
if (-not (Test-Path (Join-Path (Split-Path $exe -Parent) 'Qt6Core.dll'))) {
    $searchRoots = @('D:\Qt', 'C:\Qt', (Join-Path $env:USERPROFILE 'Qt'))
    foreach ($searchRoot in $searchRoots) {
        if (-not (Test-Path $searchRoot)) { continue }
        $qt = Get-ChildItem $searchRoot -Directory |
            Where-Object { $_.Name -match '^\d+\.\d+' } |
            Sort-Object { [version](($_.Name -split '-')[0]) } -Descending |
            ForEach-Object { Join-Path $_.FullName 'mingw_64' } |
            Where-Object { Test-Path (Join-Path $_ 'bin\qmake.exe') } |
            Select-Object -First 1
        if ($qt) {
            $toolsRoot = Join-Path (Split-Path (Split-Path $qt -Parent) -Parent) 'Tools'
            $mingw = Join-Path $toolsRoot 'mingw1310_64\bin'
            if (-not (Test-Path (Join-Path $mingw 'g++.exe'))) {
                $mingwExe = Get-ChildItem $toolsRoot -Directory -Filter 'mingw*_64' -ErrorAction SilentlyContinue |
                    ForEach-Object { Join-Path $_.FullName 'bin\g++.exe' } |
                    Where-Object { Test-Path $_ } |
                    Select-Object -First 1
                if ($mingwExe) { $mingw = Split-Path $mingwExe -Parent }
            }
            $env:Path = "$mingw;$qt\bin;$env:Path"
            break
        }
    }
}

if ($AppArgs.Count -gt 0) {
    Start-Process -FilePath $exe -ArgumentList $AppArgs
} else {
    Start-Process -FilePath $exe
}
