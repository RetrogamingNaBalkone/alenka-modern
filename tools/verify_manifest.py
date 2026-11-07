"""Verify that the built Windows executable contains the actual DPI manifest."""
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import sys
import xml.etree.ElementTree as ET


def verify(path):
    kernel = c.WinDLL('kernel32', use_last_error=True)
    signatures = {
        'LoadLibraryExW': ([w.LPCWSTR, w.HANDLE, w.DWORD], w.HMODULE),
        'FindResourceW': ([w.HMODULE, w.LPCWSTR, w.LPCWSTR], w.HANDLE),
        'SizeofResource': ([w.HMODULE, w.HANDLE], w.DWORD),
        'LoadResource': ([w.HMODULE, w.HANDLE], w.HANDLE),
        'LockResource': ([w.HANDLE], c.c_void_p),
        'FreeLibrary': ([w.HMODULE], w.BOOL),
    }
    for name, (args, result) in signatures.items():
        fn = getattr(kernel, name)
        fn.argtypes, fn.restype = args, result
    module = kernel.LoadLibraryExW(str(Path(path).resolve()), None, 2)
    if not module:
        raise c.WinError(c.get_last_error())
    try:
        ordinal = lambda value: c.cast(c.c_void_p(value), w.LPCWSTR)
        resource = kernel.FindResourceW(module, ordinal(1), ordinal(24))
        if not resource:
            raise c.WinError(c.get_last_error())
        handle = kernel.LoadResource(module, resource)
        pointer = kernel.LockResource(handle)
        if not pointer:
            raise c.WinError(c.get_last_error())
        data = c.string_at(pointer, kernel.SizeofResource(module, resource))
        nodes = {node.tag.split('}')[-1]: node.text for node in ET.fromstring(data.rstrip(b'\0')).iter()}
        assert nodes['dpiAwareness'] == 'PerMonitorV2, PerMonitor'
        assert nodes['dpiAware'] == 'true/pm'
        print(f'{path}: embedded Per-Monitor V2 manifest verified')
    finally:
        kernel.FreeLibrary(module)


if __name__ == '__main__':
    for path in sys.argv[1:] or ['artifacts/releases/alenka-windows/alenka.exe']:
        verify(path)
