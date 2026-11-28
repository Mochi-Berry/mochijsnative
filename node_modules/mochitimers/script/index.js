import * as os from 'mbjs:os';

const handles = new Set();

function wrap(id) {
    const handle = {
        __id: id,
        unref() { return handle; },
        ref() { return handle; },
        refresh() { return handle; },
        hasRef() { return true; },
        dispose() { handles.delete(handle); }
    };
    handles.add(handle);
    return handle;
}

function unwrap(handle) {
    if (handle && typeof handle === 'object' && handle.__id !== undefined) return handle.__id;
    return handle;
}

function delay(ms) {
    return Math.max(1, Number(ms) || 0);
}

export function setTimeout(fn, ms = 0, ...args) {
    return wrap(os.setTimeout(() => fn(...args), delay(ms)));
}

export function clearTimeout(handle) {
    handles.delete(handle);
    try { os.clearTimeout(unwrap(handle)); } catch (e) {}
}

export function setInterval(fn, ms = 0, ...args) {
    return wrap(os.setInterval(() => fn(...args), delay(ms)));
}

export function clearInterval(handle) {
    handles.delete(handle);
    try { os.clearInterval(unwrap(handle)); } catch (e) {}
}

let nextImmediate = 1;
export function setImmediate(fn, ...args) {
    const handle = wrap(nextImmediate++);
    queueMicrotask(() => {
        handles.delete(handle);
        fn(...args);
    });
    return handle;
}

export function clearImmediate(handle) {
    handles.delete(handle);
}

export function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

export function install(target = globalThis) {
    if (typeof target.setTimeout === 'undefined') target.setTimeout = setTimeout;
    if (typeof target.clearTimeout === 'undefined') target.clearTimeout = clearTimeout;
    if (typeof target.setInterval === 'undefined') target.setInterval = setInterval;
    if (typeof target.clearInterval === 'undefined') target.clearInterval = clearInterval;
    if (typeof target.setImmediate === 'undefined') target.setImmediate = setImmediate;
    if (typeof target.clearImmediate === 'undefined') target.clearImmediate = clearImmediate;
    return target;
}

install();

export default {
    setTimeout,
    clearTimeout,
    setInterval,
    clearInterval,
    setImmediate,
    clearImmediate,
    sleep,
    install
};
