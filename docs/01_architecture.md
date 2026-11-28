# 01 — Architecture

`mochijsnative` is a **single package repo**. Installed by TGPM/npm it lands whole
as `mochijsnative`. One root `manifest.json` describes the package; the module
under repo-root `mochi_modules/mochijsnative/` carries its own manifest for the
engine loader. `node_modules/` contains only compatibility shims plus the
`mochinode` helper packages.

```
mochijsnative/
├── package.json / package-lock.json   <- package manager (npm/TGPM)
├── manifest.json                      <- package descriptor (root)
├── CMakeLists.txt / Makefile          <- build plumbing (ENGINE_INC/MOCHI_INC/DYNALIB_DIR)
├── LICENSE / README.md / .gitignore
├── mochiberry/   svg/                 <- asset & documentation diagrams (no mascots)
├── docs/         01_architecture.md · 02_abi_reference.md · 03_sandbox_security.md
├── mochi_modules/
│   └── mochijsnative/                 <- root-level module (JS face + native)
│       ├── manifest.json
│       ├── script/{index,native,deep,build}.js
│       ├── include/ · src/ · native/
│       └── lib/mochijsnative_x86_64.dll
├── node_modules/                      <- compat shims + mochinode helpers
└── selftest/mochijsnative.js
```

## Loader contract (verified from engine source + disassembly)

- a native plugin exports `mbjs_init_module`;
- the import specifier must end in `.dll` / `.so` / `.lbrct` for the loader
  to take the native path;
- `MBJS_AddModuleExportList` is called inside `mbjs_init_module`,
  `MBJS_SetModuleExportList` in the module init func;
- the plugin MUST NOT link against `mbjs.exe` (an EXE cannot be a DLL
  dependency → ERROR_MOD_NOT_FOUND 126); all `MBJS_*` entry points are
  resolved from the host image at load time via `mbjs_runtime.h`.

## Status

| field | value |
|---|---|
| status | `verified` |
| version | `0.4.0` |
| runtime exports | 102 |
| native binary | `mochi_modules/mochijsnative/lib/mochijsnative_x86_64.dll` |
| selftest | 52/52 ALL OK on export-enabled `mbjs.new.exe` |

## Build

```sh
make ENGINE_INC=E:/bcf/mochiberry \
     MOCHI_INC=E:/libmochiberry/mochinode \
     DYNALIB_DIR=E:/libmochiberry/libdynalib
# or: cmake -DENGINE_INC=... -DMOCHI_INC=... -DDYNALIB_DIR=... .
```

## Use (JS)

```js
import * as mochijsnative from 'mochijsnative';
// -> mochi_modules/mochijsnative/script/index.js
```
