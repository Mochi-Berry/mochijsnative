# 02 — ABI Reference (mochijsnative)

- Native entry point: `mbjs_init_module()`
- ABI version: `MBC1` · module version: `0.4.0` · runtime exports: 102
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
- Capability probe: `engineCap(name)` → `0|1`; JS `engineCaps()` returns a map
  covering all 56 optional engine symbols.
- Runtime state: `runtimeSetInfo`, `gcThreshold`, `setGcThreshold`,
  `runtimeUpdateStackTop`, `runGC`, `dumpFlags`, `setDumpFlags`,
  `runtimeOpaque`, `setRuntimeOpaque`, `memoryUsage`, `isLiveObject`
- Exceptions/conversions: `hasException`, `throwValue`, `toBool`,
  `toNumberValue`, `toStringValue`, `toIndex`
- Atoms/invocation: `atomNew`, `atomFree`, `atomName`, `invokeAtom`,
  `callConstructor`, `callConstructor2`
- Eval/equality: `eval2`, `evalFunction`, `evalThis`, `evalThis2`,
  `valueIsInstanceOf`, `valueIsEqual`, `valueIsStrictEqual`,
  `valueIsSameValue`, `valueIsSameValueZero`
- Descriptors/promises/symbols: `ownPropertyNames`, `ownProperty`,
  `freePropertyEnum`, `promiseState`, `promiseResult`, `isPromise`,
  `newSettledPromise`, `newSymbol`, `newPrivateSymbol`
- JIT (optional): `jitNew`, `jitInfo`, `jitStats`, `jitCompile`, `jitInvoke`,
  `jitCall`, `jitCleanup`, `jitFree`

## Ownership

`valueNew` / `valueFromJS` allocate a handle that must be released with
`valueFree` (JS `Value.free()`). Setters duplicate the value before handing it
to the engine, so JS temporaries can still be freed safely. `throwValue`
leaves the exception pending; JS `Context.raise()` keeps it pending and
`Context.throw()` retrieves it via `getException`. `ownProperty` returns value
slots for `value`/`getter`/`setter` that must be freed.

## Build variables

- `ENGINE_INC`: directory holding `mbjs.h` + `mbjs-jit.h`.
- `MOCHI_INC`: directory holding `mbjs_runtime.h`.
- `DYNALIB_DIR`: directory holding `dynalib.c`, `dynalib_abi.c`,
  `dynalib_trampoline.c`, `dynalib.h`.
- Windows link libs: none additional.
