param([string]$Manifest, [string]$Output, [string]$Icon = "$PSScriptRoot\..\resources\alenka.ico")
$ErrorActionPreference = 'Stop'
# A Win32 .res with the DPI manifest and multi-size application icon.
# CMake uses the equivalent alenka.rc.
# Format: https://learn.microsoft.com/windows/win32/menurc/resourceheader
$payload = [IO.File]::ReadAllBytes($Manifest)
$ico = [IO.File]::ReadAllBytes($Icon)
$count = [BitConverter]::ToUInt16($ico, 4)
$entries = @(
    @{ Data = [byte[]]@(); Type = 0; Name = 0; Flags = 0 },
    @{ Data = $payload; Type = 24; Name = 1; Flags = 0x30 }
)
$groupStream = [IO.MemoryStream]::new()
$group = [IO.BinaryWriter]::new($groupStream)
try {
    $group.Write([uint16]0); $group.Write([uint16]1); $group.Write([uint16]$count)
    for ($i = 0; $i -lt $count; $i++) {
        $entryOffset = 6 + 16 * $i
        $size = [BitConverter]::ToUInt32($ico, $entryOffset + 8)
        $offset = [BitConverter]::ToUInt32($ico, $entryOffset + 12)
        $image = [byte[]]::new($size)
        [Array]::Copy($ico, $offset, $image, 0, $size)
        $entries += @{ Data = $image; Type = 3; Name = $i + 1; Flags = 0x30 }
        $group.Write($ico, $entryOffset, 12)
        $group.Write([uint16]($i + 1))
    }
    $entries += @{ Data = $groupStream.ToArray(); Type = 14; Name = 1; Flags = 0x30 }
} finally { $group.Dispose() }
$stream = [IO.File]::Create($Output)
$writer = [IO.BinaryWriter]::new($stream)
try {
    foreach ($entry in $entries) {
        $writer.Write([uint32]$entry.Data.Length)
        $writer.Write([uint32]32)
        $writer.Write([uint16]0xffff); $writer.Write([uint16]$entry.Type)
        $writer.Write([uint16]0xffff); $writer.Write([uint16]$entry.Name)
        $writer.Write([uint32]0)
        $writer.Write([uint16]$entry.Flags); $writer.Write([uint16]0)
        $writer.Write([uint32]0); $writer.Write([uint32]0)
        if ($entry.Data.Length) { $writer.Write([byte[]]$entry.Data) }
        while ($stream.Position % 4) { $writer.Write([byte]0) }
    }
} finally { $writer.Dispose() }
