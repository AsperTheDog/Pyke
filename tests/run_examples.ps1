<#
.SYNOPSIS
  Generates, configures, builds and tests every project in examples/projects with a real CMake (Windows twin of run_examples.sh).
.EXAMPLE
  cmake -S . -B build; cmake --build build --config Release
  pwsh tests/run_examples.ps1                  # finds build/Release/pyke.exe
  pwsh tests/run_examples.ps1 -Pyke build/Release/pyke.exe
  pwsh tests/run_examples.ps1 -Pyke build/Release/pyke.exe -Offline   # skip projects that fetch from GitHub
#>
param(
    [string]$Pyke = "",         # path to pyke.exe; looked for under build/ when omitted
    [switch]$Offline,
    [string]$Generator = ""     # e.g. "Ninja" or "Visual Studio 17 2022"; empty = CMake's default
)
$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Pyke) {
    $Pyke = @("build/Release/pyke.exe", "build/pyke.exe", "build/Debug/pyke.exe", "build/RelWithDebInfo/pyke.exe") |
        ForEach-Object { Join-Path $Root $_ } | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $Pyke) { Write-Host "pyke.exe not found under build/. Build it first (cmake -S . -B build; cmake --build build --config Release) or pass -Pyke <path>."; exit 2 }
}
$Pyke = (Resolve-Path $Pyke).Path
$Work = Join-Path ([System.IO.Path]::GetTempPath()) ("pyke-examples-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $Work | Out-Null
$gen = @(); if ($Generator) { $gen = @("-G", $Generator) }
$failed = 0

# Runs a native command, hiding its output unless it fails
function Step([string]$Log, [scriptblock]$Cmd) {
    $out = & $Cmd 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { Write-Host $out; return $false }
    return $true
}

foreach ($dir in Get-ChildItem (Join-Path $Root "examples/projects") -Directory) {
    $name = $dir.Name
    if ($env:CI -and (Test-Path (Join-Path $dir.FullName ".skip-in-ci"))) { Write-Host "SKIP  $name (needs a newer toolchain than CI has)"; continue }
    $source = Get-Content (Join-Path $dir.FullName "app.pyke") -Raw
    if ($Offline -and $source -match "(?m)^from github") { Write-Host "SKIP  $name (needs network)"; continue }

    $proj = Join-Path $Work $name
    Copy-Item $dir.FullName $proj -Recurse
    Push-Location $proj
    $ok = $true
    $ok = $ok -and (Step "generate" { & $Pyke app.pyke . })
    $ok = $ok -and (Step "configure" { & cmake -S . -B _b @gen })
    $ok = $ok -and (Step "build" { & cmake --build _b --config Release })
    $ok = $ok -and (Step "test" { & ctest --test-dir _b -C Release --output-on-failure })
    if ($ok -and (Test-Path consumer)) {
        $prefix = Join-Path $proj "_inst"
        $ok = $ok -and (Step "install" { & cmake --install _b --config Release --prefix $prefix })
        $ok = $ok -and (Step "consumer configure" { & cmake -S consumer -B _cb @gen "-DCMAKE_PREFIX_PATH=$prefix" })
        $ok = $ok -and (Step "consumer build" { & cmake --build _cb --config Release })
        $ok = $ok -and (Step "consumer test" { & ctest --test-dir _cb -C Release --output-on-failure })
    }
    if ($ok) {
        $again = & $Pyke app.pyke . 2>&1 | Out-String
        if ($again -notmatch "0 written") { Write-Host "regeneration was not idempotent"; $ok = $false }
    }
    Pop-Location
    if ($ok) { Write-Host "PASS  $name" } else { Write-Host "FAIL  $name"; $failed = 1 }
}

Remove-Item $Work -Recurse -Force -ErrorAction SilentlyContinue
exit $failed
