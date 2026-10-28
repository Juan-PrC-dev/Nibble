param([string]$Zig = 'zig')
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & $Zig c++ src/main.cpp -std=c++17 -Os -static -municode '-Wl,--subsystem,windows' -luser32 -lgdi32 -lshell32 -lwtsapi32 -lpsapi -lwindowscodecs -lole32 -luuid -lmsimg32 -o Nibble.exe
    if ($LASTEXITCODE) { throw 'Nibble build failed' }
    & $Zig c++ src/tests.cpp -std=c++17 -Os -static -o tests.exe
    if ($LASTEXITCODE) { throw 'Test build failed' }
    & ./tests.exe
    if ($LASTEXITCODE) { throw 'Tests failed' }
    & $Zig c++ src/probe.cpp -std=c++17 -Os -static -lpsapi -o probe.exe
    if ($LASTEXITCODE) { throw 'Probe build failed' }
    & $Zig c++ src/fixture-server.cpp -std=c++17 -Os -static -o fixture-server.exe
    if ($LASTEXITCODE) { throw 'Fixture build failed' }
    & $Zig c++ src/connection-tests.cpp -std=c++17 -Os -static -lpsapi -o connection-tests.exe
    if ($LASTEXITCODE) { throw 'Integration test build failed' }
    & ./connection-tests.exe ./fixture-server.exe
    if ($LASTEXITCODE) { throw 'Integration tests failed' }
} finally { Pop-Location }
