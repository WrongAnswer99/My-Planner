[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [string]$DestinationDirectory,
    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $projectRoot "build"
}
if (-not $DestinationDirectory) {
    $DestinationDirectory = Join-Path $projectRoot "dist"
}
$buildPath = [System.IO.Path]::GetFullPath($BuildDirectory)
$distPath = [System.IO.Path]::GetFullPath($DestinationDirectory)

if ($distPath -eq $projectRoot -or $distPath -eq $buildPath) {
    throw "DestinationDirectory must not be the project root or build directory."
}

if (-not (Test-Path -LiteralPath $buildPath -PathType Container)) {
    throw "Build directory does not exist: $buildPath"
}

if (-not $SkipBuild) {
    & cmake --build $buildPath --target myplan-cli myplan-gui -j 4
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}

$executables = @("myplan-gui.exe", "myplan-cli.exe")
foreach ($executable in $executables) {
    $source = Join-Path $buildPath $executable
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required executable does not exist: $source"
    }
}

$resourcesSource = Join-Path $projectRoot "resources"
if (-not (Test-Path -LiteralPath $resourcesSource -PathType Container)) {
    throw "Resources directory does not exist: $resourcesSource"
}
$readmeSource = Join-Path $projectRoot "README.md"
if (-not (Test-Path -LiteralPath $readmeSource -PathType Leaf)) {
    throw "README does not exist: $readmeSource"
}

New-Item -ItemType Directory -Path $distPath -Force | Out-Null

# Refresh generated package files while preserving myplan-data.json.
foreach ($name in @("myplan-gui.exe", "myplan-cli.exe", "README.md", "resources")) {
    $target = Join-Path $distPath $name
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

foreach ($executable in $executables) {
    Copy-Item -LiteralPath (Join-Path $buildPath $executable) -Destination $distPath
}
Copy-Item -LiteralPath $resourcesSource -Destination $distPath -Recurse
Copy-Item -LiteralPath $readmeSource -Destination $distPath

# Bundle the MinGW runtime libraries needed on machines without MinGW installed.
$runtimeLibraries = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
$compilerPath = $null
$cmakeCache = Join-Path $buildPath "CMakeCache.txt"
if (Test-Path -LiteralPath $cmakeCache -PathType Leaf) {
    $compilerEntry = Select-String -LiteralPath $cmakeCache -Pattern '^CMAKE_CXX_COMPILER:FILEPATH=(.+)$' |
        Select-Object -First 1
    if ($compilerEntry) {
        $compilerPath = $compilerEntry.Matches[0].Groups[1].Value
    }
}
if (-not $compilerPath -or -not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) {
    $compilerPath = (Get-Command g++ -ErrorAction Stop).Source
}

foreach ($library in $runtimeLibraries) {
    $oldLibrary = Join-Path $distPath $library
    if (Test-Path -LiteralPath $oldLibrary) {
        Remove-Item -LiteralPath $oldLibrary -Force
    }

    $libraryPath = (& $compilerPath "-print-file-name=$library").Trim()
    if (-not [System.IO.Path]::IsPathRooted($libraryPath) -or
        -not (Test-Path -LiteralPath $libraryPath -PathType Leaf)) {
        throw "Unable to locate MinGW runtime library: $library"
    }
    Copy-Item -LiteralPath $libraryPath -Destination $distPath
}

$buildData = Join-Path $buildPath "myplan-data.json"
$distData = Join-Path $distPath "myplan-data.json"
if (Test-Path -LiteralPath $buildData -PathType Leaf) {
    if (Test-Path -LiteralPath $distData -PathType Leaf) {
        Write-Warning "Data already exists in dist; build/myplan-data.json was left unchanged."
    } else {
        Move-Item -LiteralPath $buildData -Destination $distData
        Write-Host "Moved myplan-data.json from build to dist."
    }
}

Write-Host "Package ready: $distPath"
Get-ChildItem -LiteralPath $distPath -Recurse |
    Select-Object FullName, Length
