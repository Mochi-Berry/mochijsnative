import * as os from 'mbjs:os';
import * as std from 'mbjs:std';

function cwd() {
  try { const v = os.getcwd(); return Array.isArray(v) ? String(v[0]) : String(v); } catch { return '.'; }
}

/*
 * Architecture is not exposed by the engine. Report the host override when the
 * launcher sets globalThis.__MOCHI_ARCH__, otherwise default to the ABI the
 * shipped engine/DLLs are built for (x86_64 here). Treat this as "reported",
 * not independently verified.
 */
export const platform = String(globalThis.__MOCHI_PLATFORM__ || 'win32');
export const arch = String(globalThis.__MOCHI_ARCH__ || 'x64');

export const env = globalThis.__MOCHI_ENV__ || {};
export const argv = globalThis.__MOCHI_ARGV__ || ['mbjs', 'script.js'];
/* real PID via dynalib -> kernel32.GetCurrentProcessId(); fall back to 1 */
let realPid = 1;
try {
  const { dynalib } = await import('../dynalib/index.js');
  const mod = dynalib('kernel32.dll', null, null, null);
  const fn = mod.sym('GetCurrentProcessId', 0, 'u32()');
  const v = fn.call();
  if (typeof v === 'number' && v > 0) realPid = v;
} catch (e) { /* dynalib or DLL unavailable — keep fallback */ }
export const pid = realPid;
export const version = 'v20.0.0-mochi';
export const versions = { node: '20.0.0-mochi', mochi: '0.0.3' };
export const browser = false;
export const execPath = globalThis.__MOCHI_EXEC_PATH__ || 'mbjs.exe';

const _startMs = Date.now();

export function cwdFn() { return cwd(); }
export { cwdFn as cwd };

export function exit(code = 0) {
  if (typeof std.exit === 'function') { std.exit(code); return; }
  throw new Error('process.exit(' + code + ')');
}

export function nextTick(fn, ...args) { queueMicrotask(() => fn(...args)); }
export function hrtime(prev) {
  const now = Date.now();
  const s = Math.floor(now / 1000);
  const ns = (now % 1000) * 1e6;
  if (!prev) return [s, ns];
  let ds = s - prev[0]; let dns = ns - prev[1];
  if (dns < 0) { ds -= 1; dns += 1e9; }
  return [ds, dns];
}
hrtime.bigint = () => BigInt(Date.now()) * 1000000n;

export function uptime() { return (Date.now() - _startMs) / 1000; }

/*
 * memoryUsage: the engine exposes a real RSS/heap snapshot via mbjs:os
 * memoryInfo(). cpuUsage: the engine exposes no process CPU-time counter
 * (no GetProcessTimes equivalent), so it stays a zero-filled shape with an
 * honest note — not an invented number.
 */
export function memoryUsage() {
  try {
    const mi = os.memoryInfo();
    return {
      rss: Number(mi.rss || 0),
      heapTotal: Number(mi.heapSize || 0),
      heapUsed: Number(mi.heapUsed || 0),
      external: Number(mi.privateBytes || 0),
      arrayBuffers: 0,
    };
  } catch (e) {
    return { rss: 0, heapTotal: 0, heapUsed: 0, external: 0, arrayBuffers: 0 };
  }
}
export function cpuUsage() { return { user: 0, system: 0 }; }

/**
 * A minimal EventEmitter-shaped stream for process.stdout/stderr/stdin. The
 * engine has no native stream objects, so write() is the real sink and the
 * event surface (on/once/off/emit/pipe) exists so Node code that pipes or
 * listens on process streams does not crash. No data events are emitted here
 * (nothing produces them); the surface is for compatibility.
 */
function makeStream(writeFn) {
    const listeners = {};
    const s = {
        write: writeFn,
        isTTY: false,
        on(ev, fn) { if (!listeners[ev]) listeners[ev] = []; listeners[ev].push(fn); return s; },
        addListener(ev, fn) { return s.on(ev, fn); },
        once(ev, fn) {
            if (!listeners[ev]) listeners[ev] = [];
            const w = (...a) => { s.off(ev, w); fn(...a); };
            listeners[ev].push(w);
            return s;
        },
        off(ev, fn) { if (listeners[ev]) listeners[ev] = listeners[ev].filter(f => f !== fn); return s; },
        removeListener(ev, fn) { return s.off(ev, fn); },
        emit(ev, ...a) { (listeners[ev] || []).forEach(f => { try { f(...a); } catch (e) {} }); return s; },
        pipe() { return s; },
        ref() { return s; },
        unref() { return s; },
    };
    return s;
}

export const stdout = makeStream((...a) => { try { console.log(...a); } catch {} return true; });
export const stderr = makeStream((...a) => { try { console.error(...a); } catch {} return true; });
export const stdin = makeStream(() => true);
export function on(){} export function once(){} export function off(){} export function emit(){}
export function kill(){} export function umask(){ return 0; }

const proc = { env, argv, pid, platform, arch, version, versions, browser, execPath,
  cwd, exit, nextTick, hrtime, uptime, memoryUsage, cpuUsage,
  stdout, stderr, stdin, on, once, off, emit, kill, umask };
if (!globalThis.process) {
  try { globalThis.process = proc; } catch { Object.defineProperty(globalThis, 'process', { value: proc, configurable: true, writable: true }); }
}
export default proc;