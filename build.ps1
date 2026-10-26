param(
    [ValidateSet("debug", "release")]
    [string]$Config = "debug"
)

$ErrorActionPreference = "Stop"
$BuildDir = "build"

cmake -B "$BuildDir/$Config" -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
cmake --build "$BuildDir/$Config" --parallel

Copy-Item "$BuildDir/$Config/compile_commands.json" -Destination "." -Force
