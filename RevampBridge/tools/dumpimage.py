"""Start a DBXV2.exe, wait for Steam's DRM stub to decrypt it, save the image.

The exe on disk is encrypted (.bind section); the stub decrypts .text in
memory at start-up. This saves the whole module as a flat image (file offset
== RVA) and then kills the process.

usage: dumpimage.py <DBXV2.exe> <out.bin> [seconds to wait after decryption]
Steam must be running (the stub checks ownership). Close the game first.
The wait lets XV2Patcher apply its patches before the dump (default 3).
"""
import ctypes
import ctypes.wintypes as W
import math
import collections
import subprocess
import sys
import time

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
k32.OpenProcess.restype = W.HANDLE
k32.ReadProcessMemory.argtypes = [W.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
psapi.EnumProcessModulesEx.argtypes = [W.HANDLE, ctypes.POINTER(ctypes.c_void_p), W.DWORD, ctypes.POINTER(W.DWORD), W.DWORD]


def read(h, addr, n):
    buf = ctypes.create_string_buffer(n)
    got = ctypes.c_size_t()
    k32.ReadProcessMemory(h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got))
    return buf.raw[:got.value]


def entropy(b):
    c = collections.Counter(b)
    return -sum(v / len(b) * math.log2(v / len(b)) for v in c.values())


def main():
    exe, out = sys.argv[1], sys.argv[2]
    import os
    p = subprocess.Popen([exe], cwd=os.path.dirname(os.path.abspath(exe)))
    try:
        h = k32.OpenProcess(0x0410, False, p.pid)
        base = None
        for _ in range(120):
            time.sleep(1)
            mods = (ctypes.c_void_p * 1)()
            need = W.DWORD()
            if psapi.EnumProcessModulesEx(h, mods, ctypes.sizeof(mods), ctypes.byref(need), 3) and mods[0]:
                base = mods[0]
                text = read(h, base + 0x1000, 0x10000)
                if len(text) == 0x10000 and entropy(text) < 7.0:
                    break
        else:
            sys.exit('timed out waiting for .text to be decrypted (is Steam running?)')
        time.sleep(float(sys.argv[3]) if len(sys.argv) > 3 else 3)
        hdr = read(h, base, 0x1000)
        pe = int.from_bytes(hdr[0x3C:0x40], 'little')
        size = int.from_bytes(hdr[pe + 24 + 56:pe + 24 + 60], 'little')  # SizeOfImage
        img = bytearray()
        for off in range(0, size, 0x10000):
            chunk = read(h, base + off, min(0x10000, size - off))
            img += chunk.ljust(min(0x10000, size - off), b'\0')
        open(out, 'wb').write(img)
        print(f'base {base:#x}, image {size:#x} bytes, .text entropy {entropy(img[0x1000:0x11000]):.2f} -> {out}')
    finally:
        p.kill()


if __name__ == '__main__':
    main()
