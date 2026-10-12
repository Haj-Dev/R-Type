param(
    [ValidateSet("debug", "release")]
    [string]$Config = "debug"
)

$ErrorActionPreference = "Stop"
$BuildDir = "build"

cmake -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
cmake --build $BuildDir --parallel

Copy-Item "$BuildDir/compile_commands.json" -Destination "." -Force