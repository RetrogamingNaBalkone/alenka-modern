param([string]$Manifest, [string]$Output)
$ErrorActionPreference = 'Stop'
# A Win32 .res with one RT_MANIFEST (24), resource ID 1, neutral language.
# No preprocessor is needed for this single XML payload. CMake uses alenka.rc.
# Format: https://learn.microsoft.com/windows/win32/menurc/resourceheader
$payload = [IO.File]::ReadAllBytes($Manifest)
$stream = [IO.File]::Create($Output)
$writer = [IO.BinaryWriter]::new($stream)
try {
    foreach ($entry in @(
        @{ Size = 0; Type = 0; Name = 0; Flags = 0 },
        @{ Size = $payload.Length; Type = 24; Name = 1; Flags = 0x30 }
    )) {
        $writer.Write([uint32]$entry.Size)
        $writer.Write([uint32]32)
        $writer.Write([uint16]0xffff); $writer.Write([uint16]$entry.Type)
        $writer.Write([uint16]0xffff); $writer.Write([uint16]$entry.Name)
        $writer.Write([uint32]0)
        $writer.Write([uint16]$entry.Flags); $writer.Write([uint16]0)
        $writer.Write([uint32]0); $writer.Write([uint32]0)
        if ($entry.Size) { $writer.Write($payload) }
        while ($stream.Position % 4) { $writer.Write([byte]0) }
    }
} finally { $writer.Dispose() }
