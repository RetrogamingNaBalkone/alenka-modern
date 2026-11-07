param(
    [string]$Zig = "$env:LOCALAPPDATA\Programs\Zig\0.13.0\zig.exe",
    [string]$SDL2Root = "$env:LOCALAPPDATA\Programs\SDL2\SDL2-2.32.10\x86_64-w64-mingw32",
    [string]$Python = "$env:LOCALAPPDATA\Python\pythoncore-3.14-64\python.exe",
    [switch]$Reference
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    New-Item -ItemType Directory -Force build\bin,artifacts\releases\alenka-windows,diagnostics\validation | Out-Null
    foreach ($line in Get-Content original\SHA256SUMS) {
        if ($line -notmatch '^([a-f0-9]{64})  (ALENKA/[A-Z.]+)$') { throw 'Invalid resource manifest' }
        $expectedHash = $Matches[1]
        $resource = Join-Path "$root\original" $Matches[2]
        if ((Get-FileHash -LiteralPath $resource).Hash.ToLowerInvariant() -ne $expectedHash) {
            throw "Original resource changed: $resource"
        }
    }
    # Keep Zig's global cache shared across projects (or honor the user's override).
    $env:ZIG_LOCAL_CACHE_DIR = "$root\build\zig-cache"
    $flags = @('c++', '-std=c++17', '-O2', '-g0', '-Wall', '-Wextra', '-Werror', '-Isrc')
    & "$PSScriptRoot\manifest-resource.ps1" -Manifest "$root\resources\alenka.manifest" -Output "$root\build\bin\alenka.res"
    & $Zig @flags '-I' "$SDL2Root\include\SDL2" src\main.cpp src\original.cpp src\trainer.cpp build\bin\alenka.res "$SDL2Root\lib\libSDL2.dll.a" '-Wl,--subsystem,windows' '-o' build\bin\alenka.exe
    if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed' }
    & $Zig @flags tests\core_tests.cpp src\original.cpp '-o' build\bin\core-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & $Zig @flags tests\trainer_trace.cpp src\original.cpp src\trainer.cpp '-o' build\bin\trainer-trace.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trainer trace compilation failed' }
    & $Zig @flags tests\trainer_tests.cpp src\original.cpp src\trainer.cpp '-o' build\bin\trainer-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trainer test compilation failed' }
    Copy-Item "$SDL2Root\bin\SDL2.dll" build\bin\SDL2.dll -Force
    & $Zig @flags '-I' "$SDL2Root\include\SDL2" tests\audio_tests.cpp "$SDL2Root\lib\libSDL2.dll.a" '-o' build\bin\audio-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Audio test compilation failed' }
    & .\build\bin\audio-tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Audio checks failed' }
    $referenceArgs = @()
    if ($Reference) { $referenceArgs = @('--reference', 'diagnostics\validation\reference') }
    & $Python "$PSScriptRoot\frame_storage.py" "$root\build\bin\core-tests.exe" original\ALENKA @referenceArgs
    if ($LASTEXITCODE -ne 0) { throw 'Core checks failed' }
    & .\build\bin\trainer-tests.exe original\ALENKA
    if ($LASTEXITCODE -ne 0) { throw 'Trainer checks failed' }
    foreach ($scenario in Get-ChildItem tests\scenarios\*.trace) {
        & .\build\bin\trainer-trace.exe original\ALENKA $scenario.FullName
        if ($LASTEXITCODE -ne 0) { throw "DOS behavior reference failed: $($scenario.Name)" }
    }
    $smoke = Start-Process -FilePath "$root\build\bin\alenka.exe" -ArgumentList '--data', ('"{0}\original\ALENKA"' -f $root), '--smoke-test' -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$root\diagnostics\validation\smoke.stdout" -RedirectStandardError "$root\diagnostics\validation\smoke.stderr"
    Get-Content diagnostics\validation\smoke.stdout
    Get-Content diagnostics\validation\smoke.stderr
    if ($smoke.ExitCode -ne 0) { throw 'SDL smoke failed' }
    # Recreate the generated release so stale developer files cannot leak into it.
    $releaseRoot = [IO.Path]::GetFullPath((Join-Path $root 'artifacts\releases'))
    $package = [IO.Path]::GetFullPath((Join-Path $releaseRoot 'alenka-windows'))
    if ($package -ne (Join-Path $releaseRoot 'alenka-windows')) { throw 'Invalid package path' }
    if (Test-Path -LiteralPath $package) { Remove-Item -LiteralPath $package -Recurse -Force }
    New-Item -ItemType Directory -Force "$package\data","$package\licenses" | Out-Null
    Copy-Item build\bin\alenka.exe,build\bin\SDL2.dll artifacts\releases\alenka-windows -Force
    & $Python "$PSScriptRoot\prepare_resources.py" "$package\data"
    if ($LASTEXITCODE -ne 0) { throw 'Runtime resource export failed' }
    Copy-Item "$SDL2Root\..\LICENSE.txt" artifacts\releases\alenka-windows\licenses\SDL2.txt -Force
    Copy-Item resources\README.txt "$package\README.txt" -Force
    # Validate the shipped assets, including extracted DOS data, with the same checks.
    & $Python "$PSScriptRoot\frame_storage.py" "$root\build\bin\core-tests.exe" "$package\data" @referenceArgs
    if ($LASTEXITCODE -ne 0) { throw 'Packaged resource checks failed' }
    & .\build\bin\trainer-tests.exe "$package\data"
    if ($LASTEXITCODE -ne 0) { throw 'Packaged course checks failed' }
    $packageSmoke = Start-Process -FilePath "$package\alenka.exe" -ArgumentList '--smoke-test' -WorkingDirectory $package -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput "$root\diagnostics\validation\package-smoke.stdout" -RedirectStandardError "$root\diagnostics\validation\package-smoke.stderr"
    if ($packageSmoke.ExitCode -ne 0) { throw 'Packaged application smoke failed' }
    # Stage the common top-level folder without changing the unpacked package path.
    $archiveStage = [IO.Path]::GetFullPath((Join-Path $root 'build\release-archive'))
    if ($archiveStage -ne (Join-Path $root 'build\release-archive')) { throw 'Invalid archive staging path' }
    if (Test-Path -LiteralPath $archiveStage) { Remove-Item -LiteralPath $archiveStage -Recurse -Force }
    New-Item -ItemType Directory -Force $archiveStage | Out-Null
    try {
        Copy-Item -LiteralPath $package -Destination "$archiveStage\alenka" -Recurse
        Compress-Archive -LiteralPath "$archiveStage\alenka" -DestinationPath artifacts\releases\alenka-windows.zip -Force
    } finally {
        Remove-Item -LiteralPath $archiveStage -Recurse -Force
    }
    & $Python "$PSScriptRoot\verify_release.py" $package "$releaseRoot\alenka-windows.zip"
    if ($LASTEXITCODE -ne 0) { throw 'Release contents verification failed' }
    Write-Host 'Windows package: artifacts\releases\alenka-windows'
    Write-Host 'Windows archive: artifacts\releases\alenka-windows.zip'
} finally {
    # Linker side products are neither inputs for the next build nor deliverables.
    Get-ChildItem -LiteralPath "$root\build\bin" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in '.pdb','.lib' } |
        Remove-Item -Force
    Pop-Location
}
