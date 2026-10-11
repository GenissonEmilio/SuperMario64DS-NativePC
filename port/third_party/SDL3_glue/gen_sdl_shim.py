#!/usr/bin/env python3
"""Generate sdl_imports_shim.asm + sdl_imports_tab.c from the list of Win32 imports the
static SDL3 library needs.

WHY. The port exe may import KERNEL32 only (see the import-gate: no static user32 /
xinput / dinput8 / ... because every extra static DLL chain races the 0x02000000
reservation). SDL3's Windows joystick code calls ~160 Win32 functions. Linking it
against the SDK import libraries would add USER32, GDI32, ADVAPI32, SETUPAPI, ... to the
import table. Instead this generates, for each import SDL needs that the exe does not
already import from KERNEL32, a definition of the import-address symbol itself
(__imp__Name@N) pointing at a thunk that GetProcAddress'es the function on first call.
The linker prefers a symbol defined in an object over one an import lib would supply, so
the SDK import libs are never pulled and the import table does not move.

usage: gen_sdl_shim.py <SDL3-static.lib dumpbin /symbols text> <reference exe> <sdk lib dir>
"""
import re, struct, sys, os
import pefile

symtxt, refexe, libdir = sys.argv[1:4]
here = os.path.dirname(os.path.abspath(__file__))

# value a failed call returns, by declared return type (reason in the second field)
FAIL_BY_TYPE = {
    "HRESULT":   (0x80004005, "E_FAIL; 0 would be S_OK"),
    "CONFIGRET": (0x00000013, "CR_FAILURE; 0 would be CR_SUCCESS"),
    "LSTATUS":   (0x00000078, "ERROR_CALL_NOT_IMPLEMENTED; 0 would be ERROR_SUCCESS"),
    "MMRESULT":  (0x00000061, "TIMERR_NOCANDO; 0 would be TIMERR_NOERROR"),
    "COLORREF":  (0xFFFFFFFF, "SetPixel documents -1 as failure"),
    "BOOL":      (0, "FALSE"),
    "HANDLE":    (0, "NULL (these create calls fail with NULL, not INVALID_HANDLE_VALUE)"),
    "HLOCAL":    (0, "NULL (LocalFree: NULL means freed; nothing to report)"),
    "HGDIOBJ":   (0, "NULL"),
    "HDEVNOTIFY": (0, "NULL"), "HWND": (0, "NULL"), "HDC": (0, "NULL"), "HBITMAP": (0, "NULL"),
    "HICON": (0, "NULL"), "HMODULE": (0, "NULL"),
    "HINSTANCE": (0, "NULL (ShellExecute: values up to 32 are errors)"),
    "LPWSTR*":   (0, "NULL"),
    "ATOM":      (0, "0 is the failure atom"),
    "LRESULT":   (0, "no failure code exists; 0 is the neutral answer"),
    "DWORD":     (0, "0, the documented failure value of these calls"),
    "ULONG":     (0, "0 (nothing removed)"),
    "UINT":      (0, "0"), "UINT_PTR": (0, "0 (no timer)"), "int": (0, "0"),
    "ULONGLONG": (0, "0"), "VOID": (0, "no result"),
}
FAIL_BY_NAME = {
    "GetRawInputData":       (0xFFFFFFFF, "documented (UINT)-1 on error"),
    "GetRawInputDeviceInfoA": (0xFFFFFFFF, "documented (UINT)-1 on error"),
    "GetRawInputDeviceList": (0xFFFFFFFF, "documented (UINT)-1 on error"),
    "GetMessageA": (0, "0 (WM_QUIT) ends a message loop; -1 would spin a 'while (GetMessage())' loop forever"),
    "GetMessageW": (0, "0 (WM_QUIT) ends a message loop; -1 would spin a 'while (GetMessage())' loop forever"),
}
decl = {}
for ln in open(os.path.join(here, "sdl_shim_decl.tsv")):
    if ln.strip() and not ln.startswith("#"):
        nm, ty = ln.rstrip("\r\n").split("\t")[:2]
        decl[nm] = ty

pe = pefile.PE(refexe, fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
have = {i.name.decode() for e in pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}

need = set()
for line in open(symtxt, errors="replace"):
    if "UNDEF" in line:
        m = re.search(r"__imp__(\w+)@(\d+)\b", line)
        if m:
            need.add((m.group(1), int(m.group(2))))
# Imports the STATIC CRT adds on SDL's behalf, which the SDL library's own undefined-symbol
# list cannot show: SDL_quit.c calls signal(), whose CRT body calls SetConsoleCtrlHandler.
# Left alone it becomes the 156th KERNEL32 import and moves the import-table hash.
need |= {("SetConsoleCtrlHandler", 8)}
shim = sorted(n for n in need if n[0] not in have)

# decorated symbol -> dll, from the short-form members of the SDK import libs
ORDER = ["user32", "gdi32", "advapi32", "shell32", "ole32", "oleaut32", "setupapi",
         "winmm", "cfgmgr32", "version", "kernel32"]
dllof = {}
for lib in ORDER:
    path = next((os.path.join(libdir, f) for f in os.listdir(libdir) if f.lower() == lib + ".lib"), None)
    data = open(path, "rb").read()
    assert data[:8] == b"!<arch>\n"
    p = 8
    while p + 60 <= len(data):
        size = int(data[p + 48:p + 58])
        body = data[p + 60:p + 60 + size]
        p += 60 + size + (size & 1)
        if len(body) > 20 and body[:4] == b"\x00\x00\xff\xff":
            rest = body[20:]
            sym, dll = rest.split(b"\0")[:2]
            dllof.setdefault(sym.decode(), dll.decode())
rows = []
for name, n in shim:
    key = "_%s@%d" % (name, n)
    if key not in dllof:
        sys.exit("no import-lib member for " + key)
    rows.append((name, n, dllof[key].lower()))

fails = []
for name, n, dll in rows:
    if name not in decl:
        sys.exit("no declared return type for %s: add it to sdl_shim_decl.tsv" % name)
    fails.append(FAIL_BY_NAME.get(name) or FAIL_BY_TYPE[decl[name]])
with open(os.path.join(here, "sdl_shim_failures.tsv"), "w", newline="\n") as f:
    T, N = "\t", "\n"
    f.write(T.join(["# index", "function", "dll", "stdcall bytes", "declared return type", "failure value", "reason"]) + N)
    for i, (name, n, dll) in enumerate(rows):
        f.write(T.join([str(i), name, dll, str(n), decl[name], "0x%08X" % fails[i][0], fails[i][1]]) + N)

asm = ["; GENERATED by gen_sdl_shim.py - do not edit. One lazy thunk per Win32 import",
       "; the static SDL3 library needs beyond the exe's existing KERNEL32 set.",
       ".686", ".model flat", "extern _sdlshim_resolve:near", ".data"]
for i, (name, n, dll) in enumerate(rows):
    asm.append("PUBLIC __imp__%s@%d" % (name, n))
    asm.append("__imp__%s@%d dd thunk_%d" % (name, n, i))
asm.append(".code")
for i, (name, n, dll) in enumerate(rows):
    asm += ["thunk_%d:" % i, "    push %d" % i, "    call _sdlshim_resolve", "    add esp, 4",
            "    test eax, eax", "    jz fail_%d" % i, "    jmp eax", "fail_%d:" % i,
            "    mov eax, 0%08Xh" % fails[i][0]] + (["    xor edx, edx"] if decl[name] == "ULONGLONG" else []) + [
            "    ret %d" % n]
asm.append("end")
open(os.path.join(here, "sdl_imports_shim.asm"), "w", newline="\n").write("\n".join(asm) + "\n")
c = ["/* GENERATED by gen_sdl_shim.py - do not edit. */",
     "const char *const sdlshim_names[] = {"]
c += ['    "%s",' % r[0] for r in rows]
c += ["};", "const char *const sdlshim_dlls[] = {"]
c += ['    "%s",' % r[2] for r in rows]
c += ["};", "const unsigned sdlshim_count = %d;" % len(rows)]
open(os.path.join(here, "sdl_imports_tab.c"), "w", newline="\n").write("\n".join(c) + "\n")
print("need %d, already imported %d, shimmed %d" % (len(need), len(need) - len(rows), len(rows)))
from collections import Counter
print(Counter(r[2] for r in rows))
