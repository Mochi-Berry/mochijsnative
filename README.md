# mochijsnative

JS <-> C meta-programming layer for MochiBerry/MBJS: a runtime-controlling ABI
that lets JS create engine runtimes/contexts, evaluate code, move values,
await promises, inspect the engine's exported ABI, bind dynalib symbols, and
drive a JIT backend — all without touching the engine source.

**MochiBerry module** — one package, one repo: the JS face and native plugin
both live under repo-root `mochi_modules/mochijsnative/`; `node_modules/`
contains only compatibility shims and the `mochinode` helper packages.
Status: `verified` · version `0.3.0` · native entry `mbjs_init_module()`.

```
mochijsnative/
├── package.json · package-lock.json · manifest.json
├── CMakeLists.txt · Makefile · LICENSE · README.md
├── mochiberry/   asset diagrams (svg)
├── docs/         01_architecture · 02_abi_reference · 03_sandbox_security
├── mochi_modules/
│   └── mochijsnative/            <-- root-level module (JS face + native)
│       ├── manifest.json
│       ├── script/{index,native,deep,build}.js
│       ├── include/{mochijsnative.h,mochijsnative_abi.h}
│       ├── src/mochijsnative.c
│       ├── lib/mochijsnative_x86_64.dll
│       └── native/
├── node_modules/                 <-- compat shims + mochinode helpers
└── selftest/mochijsnative.js
```

## Build

```sh
make ENGINE_INC=E:/bcf/mochiberry \
     MOCHI_INC=E:/libmochiberry/mochinode \
     DYNALIB_DIR=E:/libmochiberry/libdynalib
# -> mochi_modules/mochijsnative/lib/mochijsnative_x86_64.dll
```

The native source bundles `libdynalib` (dynalib.c/dynalib_abi.c/
dynalib_trampoline.c) so the trampoline/ABI parser is self-contained; the
plugin resolves every `MBJS_*` symbol from the host engine at load time
(never links `mbjs.exe`). JIT and the enriched array/object/ArrayBuffer API
are optional: `engineCap(name)` / `engineCaps()` report what the running
engine actually exports.

## Meta-programming ABI (JS face)

```js
import { Runtime, valueFromJS, engineCaps, ffi, bind, call }
    from 'mochijsnative';

const rt = new Runtime().limits(1 << 20, 8 << 20);   // 1 MB stack, 8 MB heap
const ctx = rt.context(true);                         // context with std globals
ctx.set('gx', 7);
ctx.eval('40 + 2').toNumber();                        // 42
ctx.await(ctx.eval('Promise.resolve(42)')).toNumber();// 42

const arr = ctx.array('a', 'b', 'c');                 // native array
arr.arrayLength();                                    // 3
arr.arrayGet(1).toStringValue();                      // 'b'
arr.arraySet(2, 99);
arr.free();

const obj = ctx.eval('({})');
obj.defineProp('tag', 'native');                      // true
obj.getProp('tag').toStringValue();                   // 'native'
obj.free();

const add = ffi('MBJS_GetVersion', 'ptr()');          // dynalib symbol binding
console.log(engineCaps());                            // optional API map
ctx.close(); rt.close();
```

`Runtime`/`Context`/`Value` give full runtime control (eval, module eval,
JSON parse, get/set globals, call, invoke, await, loop, exception, std).
Version 0.3.0 adds `arrayNew/arrayLength/arrayGet/arraySet`, `getProp/
setProp/getPropValue/setPropValue/defineProp`, `arrayBufferCopy/
arrayBufferData/arrayBufferSize`, plus `engineCap/engineCaps`. The top-level
face also exports `ffi`/`bind`/`call`/`callOnce` and a `mochiberry` namespace.

## Verified (measured)

Run on the export-enabled `mbjs.new.exe` engine build:

- `selftest/mochijsnative.js` → **30/30 ALL OK** (native + root-level JS face):
  version/abi, engine exports enumerated, `MBJS_NewRuntime` address resolved,
  `Context.eval` = 42, global get/set, `JSON.parse` roundtrip, native
  array get/set, `defineProp` + `getProp`, value marshalling, bits roundtrip,
  `Context.await`, `contextStd()`, and `detectModule`.
- Known engine limitation (honest): `MBJS_DetectModule` is a stub in some
  engine builds — it returns `true` for every source string.

## Docs

[docs/01_architecture.md](docs/01_architecture.md) ·
[docs/02_abi_reference.md](docs/02_abi_reference.md) ·
[docs/03_sandbox_security.md](docs/03_sandbox_security.md)

## License

MIT — see `LICENSE`.
