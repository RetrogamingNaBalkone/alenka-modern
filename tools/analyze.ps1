param(
    [string]$GhidraRoot = "$env:LOCALAPPDATA\Programs\Ghidra\ghidra_12.1.2_PUBLIC",
    [string]$JavaHome = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'diagnostics\analysis\ghidra'
$project = Join-Path $out 'project'
$workParent = [IO.Path]::GetFullPath((Join-Path $root 'build\work'))
$work = [IO.Path]::GetFullPath((Join-Path $workParent ('ghidra-' + [guid]::NewGuid().ToString('N'))))
if (-not $work.StartsWith($workParent + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid Ghidra work directory' }
New-Item -ItemType Directory -Force $out,$project,$work | Out-Null
try {
$jvm = @(
    "-Duser.home=$work\home",
    "-Dapplication.settingsdir=$work\settings",
    "-Dapplication.cachedir=$work\cache",
    '-Djava.system.class.loader=ghidra.GhidraClassLoader', '-Dfile.encoding=UTF8', '-Xmx2G',
    '-cp', "$GhidraRoot\Ghidra\Framework\Utility\lib\Utility.jar",
    'ghidra.Ghidra', 'ghidra.app.util.headless.AnalyzeHeadless'
)
foreach ($name in @('ALLE.EXE', 'ALLE.LIB', 'ALLE.FLD')) {
    $opts = @($project, 'Alenka', '-import', "$root\original\ALENKA\$name", '-overwrite', '-scriptPath', "$PSScriptRoot\ghidra", '-log', "$out\analysis.log", '-scriptlog', "$out\script.log")
    if ($name -eq 'ALLE.EXE') {
        $opts += @('-postScript', 'RecoverSwitches.java', '-postScript', 'ExportAnalysis.java', $out)
    } else {
        $opts += @('-loader', 'BinaryLoader', '-processor', 'x86:LE:16:Real Mode', '-noanalysis', '-postScript', 'ResourceAnalysis.java', $out)
    }
    & "$JavaHome\bin\java.exe" @jvm @opts
    if ($LASTEXITCODE -ne 0) { throw "Ghidra failed: $name" }
}
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
}
