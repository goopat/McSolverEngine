param(
    [Parameter(Mandatory)]
    [ValidateSet("UseOcct", "NoOcct")]
    [string]$Variant,

    [string]$Version = "0.1.1",
    [string]$Configuration = "Release",
    [switch]$IncludeDotNet
)

$ErrorActionPreference = "Stop"

$repoRoot    = Resolve-Path "$PSScriptRoot\.."
$nupkgDir    = Join-Path $repoRoot "artifacts\nuget"
$buildDir    = Join-Path $repoRoot "build\nuget_$Variant"
$headersDir  = Join-Path $repoRoot "include\McSolverEngine"

$nugetExe = (Get-Command nuget.exe -ErrorAction SilentlyContinue).Source
if (-not $nugetExe) {
    $localNuget = Join-Path $repoRoot "tools\nuget.exe"
    if (Test-Path $localNuget) { $nugetExe = $localNuget }
}
if (-not $nugetExe) { throw "nuget.exe not found on PATH or in tools\." }

$packageId         = if ($IncludeDotNet) { "McSolverEngine_${Variant}_Net" } else { "McSolverEngine_$Variant" }
$buildWithOcct     = ($Variant -eq "UseOcct")
$occtDescription   = if ($buildWithOcct) { "BREP export enabled (requires OpenCASCADE)." }
                                          else { "BREP export returns OpenCascadeUnavailable." }
$dotnetDescription = if ($IncludeDotNet) { "Includes .NET wrapper (net48 / net8.0)." } else { "" }

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Building $packageId v$Version ($Configuration)" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# --- Resolve Visual Studio 2022 instance ---
# Prefer full VS2022 (Community/Professional/Enterprise) over BuildTools.
# CMAKE_GENERATOR_INSTANCE tells CMake which VS installation to use.
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsFullProducts = @(
        "Microsoft.VisualStudio.Product.Enterprise",
        "Microsoft.VisualStudio.Product.Professional",
        "Microsoft.VisualStudio.Product.Community"
    )
    $vsInstance = & $vswhere -latest -products $vsFullProducts -property installationPath 2>$null
    if ($vsInstance) {
        Write-Host "Found full Visual Studio 2022: $vsInstance" -ForegroundColor Green
        $env:CMAKE_GENERATOR_INSTANCE = $vsInstance
    } else {
        Write-Host "Full Visual Studio 2022 not found; falling back to default (may use BuildTools)." -ForegroundColor Yellow
    }
} else {
    Write-Host "vswhere not found; skipping VS2022 instance detection." -ForegroundColor Yellow
}

# --- CMake configure ---
$cmakeArgs = @(
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-B", $buildDir,
    "-S", $repoRoot,
    "-DMCSOLVERENGINE_WITH_OCCT=$(if ($buildWithOcct) { 'ON' } else { 'OFF' })"
)
Write-Host "CMake configure: cmake $cmakeArgs"
if ($env:CMAKE_GENERATOR_INSTANCE) {
    Write-Host "  CMAKE_GENERATOR_INSTANCE = $env:CMAKE_GENERATOR_INSTANCE"
}
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

# --- CMake build ---
Write-Host "CMake build..."
& cmake --build $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

# --- Collect outputs ---
$outDir = Join-Path $buildDir $Configuration
$dll       = Join-Path $outDir "mcsolverengine_native.dll"
$dllLib    = Join-Path $outDir "mcsolverengine_native.lib"
$staticLib = Join-Path $outDir "McSolverEngineCore.lib"
$zipLib    = Join-Path $outDir "McSolverEngineZip.lib"

@($dll, $dllLib, $staticLib, $zipLib) | ForEach-Object {
    if (-not (Test-Path $_)) {
        throw "Missing build output: $_"
    }
}

# --- Collect .NET outputs if needed ---
if ($IncludeDotNet) {
    $dotnetWrapperNet48 = Join-Path $repoRoot "wrapper\csharp\bin\$Configuration\net48\McSolverEngine.Wrapper.dll"
    $dotnetWrapperNet8  = Join-Path $repoRoot "wrapper\csharp\bin\$Configuration\net8.0\McSolverEngine.Wrapper.dll"

    @($dotnetWrapperNet48, $dotnetWrapperNet8) | ForEach-Object {
        if (-not (Test-Path $_)) {
            throw "Missing .NET wrapper output: $_"
        }
    }
}

# --- Create staging layout ---
$stageDir = Join-Path $buildDir "stage"
Remove-Item -Recurse -Force $stageDir -ErrorAction SilentlyContinue

$stageInclude = Join-Path $stageDir "build\native\include\McSolverEngine"
$stageLib     = Join-Path $stageDir "lib\native\x64\Release"
$stageRuntime = Join-Path $stageDir "runtimes\win-x64\native"
$stagePython  = Join-Path $stageDir "runtimes\python\mcsolverengine_py"
$stagePythonTests = Join-Path $stageDir "runtimes\python\tests"
$stageTargets = Join-Path $stageDir "build\native"
$stageBuildTargets = Join-Path $stageDir "build"

$stageDirs = @($stageInclude, $stageLib, $stageRuntime, $stagePython, $stagePythonTests, $stageTargets)
if ($IncludeDotNet) {
    $stageDirs += $stageBuildTargets
}
New-Item -ItemType Directory -Force -Path $stageDirs | Out-Null

Copy-Item "$headersDir\*.h"  $stageInclude
Copy-Item $staticLib         $stageLib
Copy-Item $zipLib            $stageLib
Copy-Item $dllLib            $stageLib
Copy-Item $dll               $stageRuntime
Copy-Item "$PSScriptRoot\McSolverEngine.targets" (Join-Path $stageTargets "$packageId.targets")

# License
Copy-Item "$repoRoot\License.md" $stageDir

# Python wrapper
$pythonSrc = Join-Path $repoRoot "wrapper\python\mcsolverengine_py"
Get-ChildItem -Path $pythonSrc -File -Filter "*.py" | ForEach-Object {
    Copy-Item $_.FullName $stagePython
}

# Python wrapper tests
$pythonTests = Join-Path $repoRoot "wrapper\python\tests"
if (Test-Path $pythonTests) {
    Get-ChildItem -Path $pythonTests -File -Filter "*.py" | ForEach-Object {
        Copy-Item $_.FullName $stagePythonTests
    }
}

# --- .NET wrapper files ---
if ($IncludeDotNet) {
    $stageNet48 = Join-Path $stageDir "lib\net48"
    $stageNet8  = Join-Path $stageDir "lib\net8.0"
    New-Item -ItemType Directory -Force -Path $stageNet48, $stageNet8 | Out-Null
    Copy-Item $dotnetWrapperNet48 $stageNet48
    Copy-Item $dotnetWrapperNet8  $stageNet8

    # For .NET projects using PackageReference, targets under build/ are auto-imported.
    # Use the .NET-specific targets file (no C++ compilation settings).
    Copy-Item "$PSScriptRoot\McSolverEngine_Net.targets" (Join-Path $stageBuildTargets "$packageId.targets")
}

# --- Generate nuspec ---
$nuspecPath = Join-Path $stageDir "$packageId.nuspec"

$dotnetFiles = ""
$dotnetDeps  = ""
if ($IncludeDotNet) {
    $dotnetFiles = @"
    <file src="build\$packageId.targets"                   target="build\$packageId.targets" />
    <file src="lib\net48\*.dll"                              target="lib\net48\" />
    <file src="lib\net8.0\*.dll"                             target="lib\net8.0\" />
"@
    $dotnetDeps = @"
    <dependencies>
      <group targetFramework="native0.0" />
      <group targetFramework="net48" />
      <group targetFramework="net8.0" />
    </dependencies>
"@
} else {
    $dotnetDeps = @"
    <dependencies>
      <group targetFramework="native0.0" />
    </dependencies>
"@
}

$nuspec = @"
<?xml version="1.0" encoding="utf-8"?>
<package xmlns="http://schemas.microsoft.com/packaging/2013/05/nuspec.xsd">
  <metadata>
    <id>$packageId</id>
    <version>$Version</version>
    <title>McSolverEngine ($Variant)</title>
    <authors>McSolverEngine Contributors</authors>
    <requireLicenseAcceptance>false</requireLicenseAcceptance>
    <description>
      Standalone extraction of FreeCAD Sketcher GCS constraint solver for Windows x64.
      $occtDescription
      $dotnetDescription

      Provides C ABI (mcsolverengine_native.dll) and C++ static library (McSolverEngineCore.lib).
      Build variant: $Variant.
    </description>
    <license type="expression">LGPL-2.1-or-later</license>
    <projectUrl>https://github.com/goopat/McSolverEngine</projectUrl>
    <tags>native C++ CAD constraint-solver geometric-solver FreeCAD sketcher BREP OCCT</tags>
    $dotnetDeps
  </metadata>
  <files>
    <file src="License.md"                                target="License.md" />
    <file src="build\native\include\McSolverEngine\*.h"  target="build\native\include\McSolverEngine\" />
    <file src="lib\native\x64\Release\*.lib"              target="lib\native\x64\Release\" />
    <file src="runtimes\win-x64\native\*.dll"             target="runtimes\win-x64\native\" />
    <file src="build\native\$packageId.targets"             target="build\native\$packageId.targets" />
    $dotnetFiles
    <file src="runtimes\python\mcsolverengine_py\*.py"     target="runtimes\python\mcsolverengine_py\" />
    <file src="runtimes\python\tests\*.py"                 target="runtimes\python\tests\" />
  </files>
</package>
"@

$nuspec | Set-Content -Path $nuspecPath -Encoding UTF8

# --- Pack ---
New-Item -ItemType Directory -Force -Path $nupkgDir | Out-Null

Write-Host "Packing NuGet..."
& $nugetExe pack $nuspecPath -OutputDirectory $nupkgDir -NoDefaultExcludes
if ($LASTEXITCODE -ne 0) {
    throw "nuget pack failed (exit $LASTEXITCODE)."
}

$nupkgFile = Join-Path $nupkgDir "$packageId.$Version.nupkg"
Write-Host "============================================" -ForegroundColor Green
Write-Host "Package created: $nupkgFile" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
