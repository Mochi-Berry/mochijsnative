# mochijsnative (mochi_modules) — root-level module

This directory is the root-level MochiBerry module for `mochijsnative`. Both
the JS face and the native plugin live under `mochijsnative/`; `node_modules/`
holds only compatibility shims and the `mochinode` helper packages.

## Layout

```text
mochi_modules/
├── README.md                  this file
└── mochijsnative/
    ├── manifest.json          {"layer": "native_core"} — targets, exports, sources
    ├── CMakeLists.txt         builds lib/mochijsnative_x86_64.dll from src/
    ├── include/               public ABI headers
    ├── src/                   C sources (engine plugin)
    ├── script/                JS face (index/native/deep/build)
    ├── native/                extra native sources, if any
    └── lib/                   prebuilt mochijsnative_x86_64.dll
```

## ABI contract

* The plugin exports `mbjs_init_module`.
* Runtime `MBJS_*` symbols are resolved from the host engine at load time via
  `mbjs_runtime_resolve()` — never link against `mbjs.exe` (importing it
  fails with error 126 on Windows).
* C functions that throw MUST return `MBJS_EXCEPTION`, never `MBJS_UNDEFINED`.
* Specifier imports must end in `.dll`/`.so`/`.dylib` (arch-suffixed name).

## Build

```bash
make ENGINE_INC=E:/bcf/mochiberry \
     MOCHI_INC=E:/libmochiberry/mochinode \
     DYNALIB_DIR=E:/libmochiberry/libdynalib
# output: mochijsnative/lib/mochijsnative_x86_64.dll
```
