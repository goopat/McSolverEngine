param(
    [Parameter(Mandatory)]
    [ValidateSet("UseOcct", "NoOcct")]
    [string]$Variant,

    [string]$Version = "0.1.0",
    [string]$Configuration = "Release"
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

$packageId         = "McSolverEngine_$Variant"
$buildWithOcct     = ($Variant -eq "UseOcct")
$occtDescription   = if ($buildWithOcct) { "BREP export enabled (requires OpenCASCADE)." }
                                          else { "BREP export returns OpenCascadeUnavailable." }

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Building $packageId v$Version ($Configuration)" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# --- CMake configure ---
$cmakeArgs = @(
    "-B", $buildDir,
    "-S", $repoRoot,
    "-DMCSOLVERENGINE_WITH_OCCT=$(if ($buildWithOcct) { 'ON' } else { 'OFF' })"
)
Write-Host "CMake configure: cmake $cmakeArgs"
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

@($dll, $dllLib, $staticLib) | ForEach-Object {
    if (-not (Test-Path $_)) {
        throw "Missing build output: $_"
    }
}

# --- Create staging layout ---
$stageDir = Join-Path $buildDir "stage"
Remove-Item -Recurse -Force $stageDir -ErrorAction SilentlyContinue

$stageInclude = Join-Path $stageDir "build\native\include\McSolverEngine"
$stageLib     = Join-Path $stageDir "lib\native\x64\Release"
$stageRuntime = Join-Path $stageDir "runtimes\win-x64\native"
$stageTargets = Join-Path $stageDir "build\native"

New-Item -ItemType Directory -Force -Path $stageInclude, $stageLib, $stageRuntime, $stageTargets | Out-Null

Copy-Item "$headersDir\*.h"  $stageInclude
Copy-Item $staticLib         $stageLib
Copy-Item $dllLib            $stageLib
Copy-Item $dll               $stageRuntime
Copy-Item "$PSScriptRoot\McSolverEngine.targets" (Join-Path $stageTargets "$packageId.targets")

# --- Generate nuspec ---
$nuspecPath = Join-Path $stageDir "$packageId.nuspec"

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

      Provides C ABI (mcsolverengine_native.dll) and C++ static library (McSolverEngineCore.lib).
      Build variant: $Variant.
    </description>
    <license type="expression">LGPL-2.1-or-later</license>
    <projectUrl>https://github.com/goopat/McSolverEngine</projectUrl>
    <tags>native C++ CAD constraint-solver geometric-solver FreeCAD sketcher BREP OCCT</tags>
    <dependencies>
      <group targetFramework="native0.0" />
    </dependencies>
  </metadata>
  <files>
    <file src="build\native\include\McSolverEngine\*.h"  target="build\native\include\McSolverEngine\" />
    <file src="lib\native\x64\Release\*.lib"              target="lib\native\x64\Release\" />
    <file src="runtimes\win-x64\native\*.dll"             target="runtimes\win-x64\native\" />
    <file src="build\native\$packageId.targets"             target="build\native\$packageId.targets" />
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
