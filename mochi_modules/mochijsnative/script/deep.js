import { nativeHandle as host } from './native.js';

function id(value) {
    return Number(value);
}

export function bitsToNumber(bits) {
    const view = new DataView(new ArrayBuffer(8));
    view.setBigUint64(0, BigInt(bits), true);
    return view.getFloat64(0, true);
}

export function numberToBits(value) {
    const view = new DataView(new ArrayBuffer(8));
    view.setFloat64(0, Number(value), true);
    return view.getBigUint64(0, true);
}

export class Value {
    constructor(context, slot) {
        this.context = context;
        this.slot = id(slot);
        this.released = false;
    }

    get type() {
        return host.valueType(this.slot);
    }

    isException() {
        return Number(host.valueIsException(this.slot)) !== 0;
    }

    toNumber() {
        return bitsToNumber(host.valueToNumber(this.slot));
    }

    toInt(wide = false) {
        return Number(host.valueToInt(this.slot, wide ? 1 : 0));
    }

    toStringValue() {
        return host.valueToString(this.slot);
    }

    json() {
        return host.valueJson(this.slot);
    }

    arrayLength() {
        return Number(host.arrayLength(this.context.id, this.slot));
    }

    arrayGet(index) {
        return new Value(this.context, host.arrayGet(this.context.id, this.slot, index));
    }

    arraySet(index, value) {
        const stored = valueFromJS(this.context, value);
        try {
            return Number(host.arraySet(this.context.id, this.slot, index, stored.slot));
        } finally {
            if (!(value instanceof Value)) stored.free();
        }
    }

    getProp(key) {
        return new Value(this.context, host.valueGetProp(this.context.id, this.slot, String(key)));
    }

    setProp(key, value) {
        const stored = valueFromJS(this.context, value);
        try {
            return Number(host.valueSetProp(this.context.id, this.slot, String(key), stored.slot));
        } finally {
            if (!(value instanceof Value)) stored.free();
        }
    }

    getPropValue(key) {
        const stored = valueFromJS(this.context, key);
        try {
            return new Value(this.context, host.getPropValue(this.context.id, this.slot, stored.slot));
        } finally {
            if (!(key instanceof Value)) stored.free();
        }
    }

    setPropValue(key, value, flags = 0) {
        const storedKey = valueFromJS(this.context, key);
        const storedValue = valueFromJS(this.context, value);
        try {
            return Number(host.setPropValue(this.context.id, this.slot,
                storedKey.slot, storedValue.slot, flags));
        } finally {
            if (!(key instanceof Value)) storedKey.free();
            if (!(value instanceof Value)) storedValue.free();
        }
    }

    defineProp(key, value, flags = 7) {
        const stored = valueFromJS(this.context, value);
        try {
            return Number(host.defineProp(this.context.id, this.slot, String(key), stored.slot, flags)) !== 0;
        } finally {
            if (!(value instanceof Value)) stored.free();
        }
    }

    arrayBufferData() {
        return host.arrayBufferData(this.context.id, this.slot);
    }

    arrayBufferSize() {
        return Number(host.arrayBufferSize(this.context.id, this.slot));
    }

    check() {
        if (!this.isException()) return this;
        const message = this.toStringValue();
        this.free();
        throw new Error('mochijsnative: ' + message);
    }

    free() {
        if (this.released) return;
        host.valueFree(this.slot);
        this.released = true;
    }
}

export function valueFromJS(context, value) {
    if (value === null) return new Value(context, host.valueNew(context.id, 'null'));
    if (value === undefined) return new Value(context, host.valueNew(context.id, 'undefined'));
    if (typeof value === 'boolean') return new Value(context, host.valueNew(context.id, 'bool', value ? 1 : 0));
    if (typeof value === 'number') return new Value(context, host.valueNew(context.id, 'float', numberToBits(value)));
    if (typeof value === 'bigint') return new Value(context, host.valueNew(context.id, 'int', Number(value)));
    if (typeof value === 'string') return new Value(context, host.valueNew(context.id, 'string', value));
    if (value instanceof Value) return value;
    if (value && typeof value.json === 'function')
        return context.jsonParse(value.json());
    throw new TypeError('mochijsnative: unsupported value ' + typeof value);
}

export class Context {
    constructor(runtime, withStd = true) {
        this.runtime = runtime;
        this.id = id(host.contextNew(runtime.id, withStd ? 1 : 0));
        this.closed = false;
        if (this.id < 0) throw new Error(host.lastError());
    }

    value(kind, ...args) {
        return new Value(this, host.valueNew(this.id, kind, ...args));
    }

    number(value) {
        return valueFromJS(this, value);
    }

    arrayNew() {
        return new Value(this, host.arrayNew(this.id));
    }

    array(...values) {
        const out = this.arrayNew();
        for (let i = 0; i < values.length; i++) out.arraySet(i, values[i]);
        return out;
    }

    arrayBufferFrom(ptr, size) {
        return new Value(this, host.arrayBufferCopy(this.id, ptr, size)).check();
    }

    evalRaw(source, filename = '<eval>', flags = 0) {
        return new Value(this, host.eval(this.id, String(source), String(filename), flags));
    }

    eval(source, filename = '<eval>', flags = 0) {
        return this.evalRaw(source, filename, flags).check();
    }

    evalModuleRaw(source, filename) {
        return new Value(this, host.evalModule(this.id, String(source), String(filename)));
    }

    evalModule(source, filename) {
        return this.evalModuleRaw(source, filename).check();
    }

    jsonParse(text) {
        return new Value(this, host.jsonParse(this.id, String(text))).check();
    }

    get global() {
        return new Value(this, host.getGlobal(this.id));
    }

    get(name) {
        const globalObject = this.global;
        try {
            return new Value(this, host.getProp(this.id, globalObject.slot, String(name)));
        } finally {
            globalObject.free();
        }
    }

    set(name, value) {
        const globalObject = this.global;
        const stored = valueFromJS(this, value);
        try {
            return Number(host.setProp(this.id, globalObject.slot, String(name), stored.slot));
        } finally {
            globalObject.free();
            stored.free();
        }
    }

    call(name, ...args) {
        const fn = this.get(name);
        const values = args.map((arg) => valueFromJS(this, arg));
        try {
            return new Value(this, host.callValue(this.id, fn.slot, 0, ...values.map((v) => v.slot))).check();
        } finally {
            fn.free();
            values.forEach((v) => v.free());
        }
    }

    invoke(value, thisValue, ...args) {
        const fn = value instanceof Value ? value : valueFromJS(this, value);
        const self = thisValue === undefined || thisValue === null ? null : valueFromJS(this, thisValue);
        const values = args.map((arg) => valueFromJS(this, arg));
        try {
            return new Value(this, host.callValue(this.id, fn.slot, self ? self.slot : 0,
                ...values.map((v) => v.slot))).check();
        } finally {
            if (!(value instanceof Value)) fn.free();
            if (self) self.free();
            values.forEach((v) => v.free());
        }
    }

    await(value) {
        const stored = value instanceof Value ? value : valueFromJS(this, value);
        try {
            const slot = host.await(this.id, stored.slot);
            stored.released = true;
            return new Value(this, slot).check();
        } finally {
            if (!(value instanceof Value)) stored.free();
        }
    }

    exception() {
        return new Value(this, host.getException(this.id));
    }

    dumpError() {
        host.dumpError(this.id);
    }

    loop(once = false) {
        return Number(host.loop(this.id, once ? 1 : 0));
    }

    /* prepare standard globals on a context that was created without std. */
    std() {
        const r = Number(host.contextStd(this.id));
        if (r !== 0) throw new Error('mochijsnative: contextStd failed');
        return this;
    }

    close() {
        if (this.closed) return;
        host.contextFree(this.id);
        this.closed = true;
    }
}

export class Runtime {
    constructor() {
        this.id = id(host.runtimeNew());
        if (this.id < 0) throw new Error(host.lastError());
    }

    limits(stackBytes = 0, memoryBytes = 0) {
        host.runtimeLimits(this.id, stackBytes, memoryBytes);
        return this;
    }

    context(withStd = true) {
        return new Context(this, withStd);
    }

    close() {
        host.runtimeFree(this.id);
    }
}

export class Jit {
    constructor(options = {}) {
        const opts = options || {};
        this.id = id(host.jitNew(
            opts.backend === undefined ? 0 : opts.backend,
            opts.cc || '',
            opts.ccFlags || '',
            opts.incDir || 'E:/bcf/mochiberry',
            opts.libDir || 'E:/bcf/mochiberry/build_manual',
            opts.hotThreshold || 0,
            opts.tmpDir || '',
            opts.cacheDir || ''
        ));
        if (this.id < 0) throw new Error(host.lastError());
    }

    get backend() {
        return host.jitInfo(this.id, 0);
    }

    get lastError() {
        return host.jitInfo(this.id, 1);
    }

    get stats() {
        return JSON.parse(host.jitStats(this.id));
    }

    compile(context, source, name = '<jit>') {
        const slot = host.jitCompile(this.id, context.id, String(source), String(name));
        if (id(slot) < 0) throw new Error(this.lastError || host.lastError());
        return { id: id(slot), context };
    }

    invoke(compiled, thisValue, ...args) {
        const values = args.map((arg) => valueFromJS(compiled.context, arg));
        const self = thisValue === undefined || thisValue === null ? null : valueFromJS(compiled.context, thisValue);
        try {
            const slot = host.jitInvoke(compiled.id, self ? self.slot : 0, ...values.map((v) => v.slot));
            return new Value(compiled.context, slot).check();
        } finally {
            if (self) self.free();
            values.forEach((v) => v.free());
        }
    }

    call(context, source, name, thisValue, ...args) {
        const values = args.map((arg) => valueFromJS(context, arg));
        const self = thisValue === undefined || thisValue === null ? null : valueFromJS(context, thisValue);
        try {
            const slot = host.jitCall(this.id, context.id, String(source), String(name),
                self ? self.slot : 0, ...values.map((v) => v.slot));
            return new Value(context, slot).check();
        } finally {
            if (self) self.free();
            values.forEach((v) => v.free());
        }
    }

    cleanup(context) {
        host.jitCleanup(this.id, context.id);
    }

    close() {
        host.jitFree(this.id);
    }
}

/* meta-programming: detect whether a source string is an ES module. */
export function detectModule(source) {
    return Number(host.detectModule(String(source))) !== 0;
}

export function hostBase() {
    return Number(host.hostBase());
}

export function hostExport(name) {
    return host.hostExport(String(name));
}

export function lastError() {
    return host.lastError();
}

export function exportNames() {
    return JSON.parse(host.exportNames());
}

export function exportSignature(name) {
    return host.exportSignature(String(name));
}

export const ENGINE_CAPS = [
    'GetVersion', 'NewArray', 'GetPropertyUint32', 'SetPropertyUint32',
    'GetPropertyValue', 'SetPropertyValue', 'DefinePropertyValueStr',
    'GetLength', 'IsArray', 'NewArrayBufferCopy', 'GetArrayBuffer',
    'IsArrayBuffer'
];

export function hasEngineCap(name) {
    return Number(host.engineCap(String(name))) !== 0;
}

export function engineCaps() {
    const out = {};
    for (const name of ENGINE_CAPS) out[name] = hasEngineCap(name);
    return out;
}

export default {
    Value,
    valueFromJS,
    Context,
    Runtime,
    Jit,
    exportNames,
    exportSignature,
    ENGINE_CAPS,
    engineCaps,
    hasEngineCap,
    detectModule,
    hostBase,
    hostExport,
    lastError,
    bitsToNumber,
    numberToBits
};
