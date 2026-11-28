/*
 * selftest/mochijsnative.js — meta-programming ABI (JS <-> C runtime control).
 * Run: mbjs.exe -m selftest/mochijsnative.js
 */
import { Runtime, Context, valueFromJS, detectModule, hostBase, hostExport,
         exportNames, exportSignature, bitsToNumber, numberToBits,
         engineCaps, hasEngineCap, version, engineVersion, abi,
         packageName, baseVersion }
    from '../mochi_modules/mochijsnative/script/index.js';

const results = [];
function check(name, cond) {
    results.push([name, !!cond]);
    console.log((cond ? 'PASS ' : 'FAIL ') + name);
}

async function main() {
    check('version string', typeof version === 'string' && version.length > 0);
    check('version is 0.4.0', version.indexOf('0.4.0') !== -1);
    check('engine version string', typeof engineVersion === 'string' && engineVersion.length > 0);
    check('abi has symbols', abi && abi.symbols && Object.keys(abi.symbols).length > 0);

    const names = exportNames();
    check('exportNames enumerates engine ABI', Array.isArray(names) && names.length > 100);
    check('exportNames includes MBJS_NewRuntime', names.includes('MBJS_NewRuntime'));
    check('exportSignature(MBJS_NewRuntime) non-empty', (exportSignature('MBJS_NewRuntime') || '').length > 0);
    check('hostBase is a real address', hostBase() > 0);
    check('hostExport(MBJS_NewRuntime) is an address', Number(hostExport('MBJS_NewRuntime')) > 0);

    /* runtime + context control */
    const rt = new Runtime();
    rt.limits(1 << 20, 8 << 20);                       /* 1 MB stack, 8 MB memory */
    const ctx = rt.context(true);                       /* context with std globals */
    check('Context.eval arith', ctx.eval('40 + 2').toNumber() === 42);
    check('Context.eval object json', JSON.parse(ctx.eval('JSON.stringify({a:[1,2,3]})').toStringValue()).a[1] === 2);
    ctx.set('gx', 7);
    check('Context.set/get global', ctx.eval('gx * 3').toNumber() === 21);
    check('Context.get Math is object', ctx.get('Math').type !== 'undefined');
    check('Context.jsonParse roundtrip', ctx.jsonParse('{"k":[1,2]}').json() === '{"k":[1,2]}');

    /* enriched object/array API */
    const caps = engineCaps();
    check('engineCaps enumerates optional API',
          Object.keys(caps).length >= 12 && Object.keys(caps).some(k => caps[k]));
    check('hasEngineCap(NewArray) agrees', hasEngineCap('NewArray') === caps.NewArray);
    if (caps.NewArray && caps.GetLength && caps.GetPropertyUint32) {
        const arr = ctx.array('a', 'b', 'c');
        check('Context.array + arrayLength', arr.arrayLength() === 3);
        const second = arr.arrayGet(1);
        check('Value.arrayGet', second.toStringValue() === 'b');
        second.free();
        arr.arraySet(2, 99);
        const third = arr.arrayGet(2);
        check('Value.arraySet', third.toNumber() === 99);
        third.free();
        arr.free();
    }
    if (caps.DefinePropertyValueStr) {
        const obj = ctx.eval('({})');
        check('Value.defineProp', obj.defineProp('tag', 'native') === true);
        const tag = obj.getProp('tag');
        check('defined prop readable', tag.toStringValue() === 'native');
        tag.free();
        obj.free();
    }

    /* core meta runtime 0.4.0 */
    check('engineCaps covers 0.4.0 API',
          Object.keys(caps).length >= 56 && caps.Eval2 && caps.NewAtom &&
          caps.PromiseState && caps.NewSymbol);
    check('packageName/baseVersion are strings',
          typeof packageName() === 'string' && packageName().length > 0 &&
          typeof baseVersion() === 'string');
    check('Runtime.info + dump flags', (rt.info('selftest').dumpFlags() >= 0) &&
          Number(rt.setDumpFlags(rt.dumpFlags()) !== undefined));
    check('Runtime.gcThreshold set/get', (() => {
        const before = rt.gcThreshold();
        rt.setGcThreshold(before);
        return rt.gcThreshold() === before;
    })());
    rt.updateStackTop().runGC();
    check('Runtime.memoryUsage JSON', typeof rt.memoryUsage().malloc_size === 'number');
    check('Context.toBool/toIndex', ctx.toBool(ctx.eval('1')) === true &&
          ctx.toBool(ctx.eval('0')) === false && ctx.toIndex('3') === 3);
    check('Context comparison API', ctx.isEqual(1, '1') === 1 &&
          ctx.isStrictEqual(1, 1) === true &&
          ctx.isSameValue(0, -0) === false &&
          ctx.isSameValueZero(0, -0) === true);
    if (caps.NewAtom && caps.AtomToCStringLen) {
        const atom = ctx.newAtom('meta');
        check('atom create/name', atom > 0 && ctx.atomName(atom) === 'meta');
        ctx.freeAtom(atom);
    }
    if (caps.GetOwnPropertyNames && caps.GetOwnProperty) {
        const owner = ctx.eval('({ a: 1 })');
        const props = ctx.ownPropertyNames(owner, 1);
        check('ownPropertyNames sees a', props.some(p => p.name === 'a'));
        const atom = ctx.newAtom('a');
        const prop = ctx.ownProperty(owner, atom);
        check('ownProperty descriptor', (prop.flags & 1) !== 0);
        check('ownProperty value slot', prop.value.toNumber() === 1);
        prop.value.free(); prop.getter.free(); prop.setter.free();
        ctx.freeAtom(atom);
        owner.free();
    }
    if (caps.Eval2) {
        const ev2 = ctx.eval2('6 * 7', '<eval2>', 0, 1).check();
        check('eval2', ev2.toNumber() === 42);
        ev2.free();
    }
    if (caps.EvalThis) {
        const self = ctx.eval('({ x: 9 })');
        const evThis = ctx.evalThis(self, 'this.x').check();
        check('evalThis', evThis.toNumber() === 9);
        evThis.free();
        self.free();
    }
    if (caps.CallConstructor) {
        const dateCtor = ctx.get('Date');
        const made = ctx.callConstructor(dateCtor, 0).check();
        check('callConstructor object', made.type === 'object');
        check('isInstanceOf', made.isInstanceOf(dateCtor));
        made.free();
        dateCtor.free();
    }
    if (caps.NewSymbol) {
        const sym = ctx.newSymbol('s', false);
        check('newSymbol', sym.type === 'symbol');
        sym.free();
    }
    if (caps.NewPrivateSymbol) {
        const priv = ctx.newPrivateSymbol('p');
        check('newPrivateSymbol', Number(priv.slot) >= 0);
        priv.free();
    }
    if (caps.NewSettledPromise && caps.PromiseState && caps.PromiseResult) {
        const settled = ctx.newSettledPromise(7, false);
        check('isPromise on settled', settled.isPromise());
        check('promiseState fulfilled', ctx.promiseState(settled) === 1);
        const result = ctx.promiseResult(settled);
        check('promiseResult', result.toNumber() === 7);
        result.free();
        settled.free();
    }
    if (caps.Throw && caps.HasException) {
        ctx.raise('boom');
        check('hasException after raise', ctx.hasException());
        const thrown = ctx.exception();
        check('throwValue exception value', thrown.type === 'string');
        thrown.free();
    }

    /* value marshalling */
    const vNum = valueFromJS(ctx, 42);
    check('valueFromJS number', vNum.toNumber() === 42);
    vNum.free();
    const vStr = valueFromJS(ctx, 'hello');
    check('valueFromJS string', vStr.toStringValue() === 'hello');
    vStr.free();
    const vNull = valueFromJS(ctx, null);
    check('valueFromJS null', vNull.type === 'null');
    vNull.free();

    /* bits roundtrip */
    const bits = numberToBits(3.14159);
    check('numberToBits/bitsToNumber roundtrip', Math.abs(bitsToNumber(bits) - 3.14159) < 1e-9);

    /* async: await a promise inside the realm */
    check('Context.await resolves', ctx.await(ctx.eval('Promise.resolve(20 + 22)')).toNumber() === 42);
    ctx.close();

    /* context without std + std() */
    const ctx2 = rt.context(false);
    check('no-std context omits std helpers (console)', ctx2.get('console').type === 'undefined');
    check('intrinsics present regardless of std (Math)', ctx2.get('Math').type === 'object');
    ctx2.std();
    check('contextStd() installs standard globals (console)', ctx2.get('console').type === 'object');
    ctx2.close();

    /* module detection */
    check('detectModule returns boolean', typeof detectModule('export const a = 1;') === 'boolean');
    check('detectModule true for ESM', detectModule('export const a = 1;') === true);

    rt.close();

    const fails = results.filter(r => !r[1]).length;
    console.log('---');
    if (fails === 0) { console.log('ALL OK'); return; }
    throw new Error('FAILURES: ' + fails);
}

main().catch(e => { console.log('FATAL: ' + (e && e.message || e)); throw e; });
