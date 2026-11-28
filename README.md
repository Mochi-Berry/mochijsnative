# mochijsnative

JS <-> C meta-programming layer for MochiBerry/MBJS: a runtime-controlling ABI
that lets JS create engine runtimes/contexts, evaluate code, move values,
await promises, inspect the engine's exported ABI, and optionally drive a JIT
backend — all without touching the engine source.

**MochiBerry module** — one package, one repo: JS layer (`node_modules/`) +
native layer (`mochi_modules/`) + `manifest.json` at root and in both layers.
Status: `verified` · version `0.2.0` · native entry `mbjs_init_module()`.

```
mochijsnative/
├── package.json · package-lock.json · manifest.json
├── CMakeLists.txt · Makefile · LICENSE · README.md
├── mochiberry/   asset diagrams (svg)
├── docs/         01_architecture · 02_abi_reference · 03_sandbox_security
├── node_modules/ JS face: manifest.json + src/{index,native,deep,build}.js + native/ + lib/
└── mochi_modules/ native: manifest.json + include/ + src/ + native/ + script/ + lib/
```

## Build

```sh
make ENGINE_INC=/path/to/engine/native-dir
# -> mochi_modules/lib/mochijsnative_x86_64.dll   (101495 B)
```

The native source bundles `libdynalib` (dynalib.c/dynalib_abi.c/
dynalib_trampoline.c) so the trampoline/ABI parser is self-contained; the
plugin resolves every `MBJS_*` symbol from the host engine at load time
(never links `mbjs.exe`). JIT symbols are optional: an engine build without a
JIT backend loads fine and the `Jit` API reports "JIT unavailable".

## Meta-programming ABI (JS face)

```js
import { Runtime, Context, valueFromJS, detectModule,
         exportNames, exportSignature, hostBase, hostExport } from 'mochijsnative';

const rt = new Runtime().limits(1 << 20, 8 << 20);   // 1 MB stack, 8 MB heap
const ctx = rt.context(true);                         // context with std globals
ctx.set('gx', 7);
ctx.eval('40 + 2').toNumber();                        // 42
ctx.await(ctx.eval('Promise.resolve(42)')).toNumber();// 42
ctx.close(); rt.close();

exportNames();        // -> all 366 engine ABI exports
hostExport('MBJS_NewRuntime');   // -> its address in the host image
detectModule('export const a = 1');  // -> true
```

`Runtime`/`Context`/`Value` give full runtime control (eval, module eval,
JSON parse, get/set globals, call, invoke, await, loop, exception, std).
`Jit` drives a backend when the engine ships one. The top-level face also
exports `ffi`/`bind`/`call`/`callOnce` (symbol binding) and a `mochiberry`
namespace (`host`, `call`, `getVersion`).

## Verified (measured)

Run on `mbjs.exe` 7,329,331 B (366 exports, no JIT backend):

- `selftest/mochijsnative.js` → **23/23 ALL OK** (native + JS face, dual-layer
  install): version/abi, 366 exports enumerated, `MBJS_NewRuntime` address
  resolved, `Context.eval` = 42, global get/set, `JSON.parse` roundtrip,
  value marshalling (number/string/null), bits roundtrip, `Context.await`,
  `contextStd()` (std helpers absent in a no-std context, present after),
  `detectModule` returns a boolean.
- Known engine limitation (honest): `MBJS_DetectModule` is a stub in this
  engine build — it returns `true` for every source string, so it cannot
  currently discriminate ESM from script.

## Docs

[docs/01_architecture.md](docs/01_architecture.md) ·
[docs/02_abi_reference.md](docs/02_abi_reference.md) ·
[docs/03_sandbox_security.md](docs/03_sandbox_security.md)

## License

MIT — see `LICENSE`.
