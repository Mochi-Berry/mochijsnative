/*
 * node_modules/dynalib — the dynamic loader API for the SEVM stack.
 *
 *   const target = dynalib(file, reg_or_offset, symbol, abi_sig, opts);
 *
 * Two modes, exactly as specified:
 *
 *   1. direct   (symbol or offset given) -> a callable handle carrying
 *      metadata: fn.ptr, fn.base, fn.offset, fn.abi, fn.close()
 *
 *   2. module   (both null) -> a module handle like dlopen():
 *      mod.sym(name, [offset], [abi_sig]), mod.exports(), mod.base, mod.close()
 *
 * The heavy lifting happens in the native plugin
 * `mochi_modules/mochinode_dynalib.dll`, which embeds the C99 libdynalib
 * subsystem (PE/ELF export tables, ASLR-aware offsets, JIT trampolines).
 */

import { loadNative, resolveNativePath } from '../mochinode/index.js';

const native = await loadNative('mochinode_dynalib.dll');

function fail(what) {
    const e = new Error('dynalib: ' + native.lastError());
    e.stage = what;
    throw e;
}

/*
 * Short names ("sqlite3") are searched in mochi_modules/ (JS side) before the
 * loader gets them; libdynalib's own C convention looks in ./mochimodule/.
 */
function locate(file) {
    const s = String(file);
    if (s.includes('/') || s.includes('\\'))
        return s;
    const found = resolveNativePath(s);
    return found || s;
}

function makeModule(id) {
    const mod = {
        _id: id,

        get base() {
            return native.moduleBase(id);
        },

        /* mod.sym(name, [offset], [abi_sig]) */
        sym(name, offset, abiSig) {
            const sid = native.resolveSymbol(id, offset || 0, name,
                                             abiSig || null);
            if (sid < 0) fail('sym(' + name + ')');
            return makeSymbol(id, sid);
        },

        /* mod.exports() -> [{ name, rva, ordinal }, ...] */
        exports() {
            return JSON.parse(native.moduleExports(id));
        },

        close() {
            native.closeModule(id);
        },
    };
    return mod;
}

function makeSymbol(modId, symId) {
    const sym = {
        _id: symId,

        get ptr() { return native.symbolPtr(symId); },
        get base() { return native.symbolBase(symId); },
        get offset() { return native.symbolOffset(symId); },
        get abi() { return native.symbolAbi(symId); },

        /* fn.call(a, b, ...) -> number */
        call(...args) {
            const argv = [symId, args.length];
            for (const a of args) argv.push(a);
            return native.callSymbol.apply(null, argv);
        },

        close() {
            native.freeSymbol(symId);
            native.closeModule(modId);
        },
    };
    return sym;
}

/*
 * dynalib(file, reg_or_offset, symbol, abi_sig, opts)
 *
 *   file          path or short name ("sqlite3" -> sqlite3.dll/.so/.lbrct)
 *   reg_or_offset number (RVA, ASLR-compensated) | null
 *   symbol        export name | null
 *   abi_sig       type contract, e.g. "i32(i32,i32)", "ptr()"
 *   opts          { rawAddr: false }
 */
export function dynalib(file, regOrOffset, symbol, abiSig, opts) {
    const options = opts || {};

    const modId = native.openModule(locate(file));
    if (modId < 0) fail('open(' + file + ')');

    if (regOrOffset == null && symbol == null)
        return makeModule(modId);

    const symId = native.resolveSymbol(
        modId,
        regOrOffset == null ? 0 : regOrOffset,
        symbol == null ? null : symbol,
        abiSig == null ? null : abiSig);
    if (symId < 0) {
        native.closeModule(modId);
        fail('resolve(' + symbol + ')');
    }

    if (options.rawAddr) {
        const ptr = native.symbolPtr(symId);
        native.freeSymbol(symId);
        native.closeModule(modId);
        return ptr;
    }

    return makeSymbol(modId, symId);
}

/* parse an ABI spec without loading anything */
export function parseAbi(spec) {
    return JSON.parse(native.abiParse(spec));
}

export default { dynalib, parseAbi };
