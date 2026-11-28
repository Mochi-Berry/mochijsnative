# 02 — ABI Reference (mochijsnative)

- Native entry point: `mbjs_init_module()`
- ABI version: `MBC1` · module version: `0.3.0`
- Header ABI:
  - `mochi_modules/mochijsnative/include/mochijsnative.h`
  - `mochi_modules/mochijsnative/include/mochijsnative_abi.h`

## Export groups

All exports are registered by `mochijsnative.c` through
`MBJS_AddModuleExportList()` / `MBJS_SetModuleExportList()`.

- Meta: `version`, `abi`, `exportNames`, `exportSignature`, `hostBase`,
  `hostExport`, `engineVersion`, `lastError`
- Dynalib FFI: `bind`, `call`, `unbind`
- Runtime/context/value handles: `runtimeNew`, `runtimeFree`,
  `runtimeLimits`, `contextNew`, `contextFree`, `contextStd`, `valueNew`,
  `valueFree`, `valueType`, `valueToNumber`, `valueToInt`, `valueToString`,
  `valueJson`
- Eval/async: `eval`, `evalModule`, `detectModule`, `getException`,
  `dumpError`, `await`, `loop`, `callValue`
- Object/value access: `getGlobal`, `getProp`, `setProp`, `valueGetProp`,
  `valueSetProp`, `getPropValue`, `setPropValue`, `defineProp`, `jsonParse`
- Arrays/ArrayBuffers: `arrayNew`, `arrayLength`, `arrayGet`, `arraySet`,
  `arrayBufferCopy`, `arrayBufferData`, `arrayBufferSize`
- Capability probe: `engineCap(name)` → `0|1`; JS `engineCaps()` returns a map.
- JIT (optional): `jitNew`, `jitInfo`, `jitStats`, `jitCompile`, `jitInvoke`,
  `jitCall`, `jitCleanup`, `jitFree`

## Ownership

`valueNew` / `valueFromJS` allocate a handle that must be released with
`valueFree` (JS `Value.free()`). Setters duplicate the value before handing it
to the engine, so JS temporaries can still be freed safely.

## Build variables

- `ENGINE_INC`: directory holding `mbjs.h` + `mbjs-jit.h`.
- `MOCHI_INC`: directory holding `mbjs_runtime.h`.
- `DYNALIB_DIR`: directory holding `dynalib.c`, `dynalib_abi.c`,
  `dynalib_trampoline.c`, `dynalib.h`.
- Windows link libs: none additional.
