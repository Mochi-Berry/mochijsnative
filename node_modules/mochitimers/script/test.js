import { setTimeout, clearTimeout, setInterval, clearInterval, setImmediate, sleep } from './index.js';

let timeoutOk = false;
setTimeout(() => { timeoutOk = true; }, 1);
await sleep(10);
if (!timeoutOk) throw new Error('setTimeout failed');
print('ok   timers.setTimeout');

let ticks = 0;
const id = setInterval(() => {
    ticks++;
    if (ticks >= 2) clearInterval(id);
}, 1);
await sleep(20);
if (ticks < 2) throw new Error('setInterval failed');
print('ok   timers.setInterval');

let immediateOk = false;
setImmediate(() => { immediateOk = true; });
await sleep(1);
if (!immediateOk) throw new Error('setImmediate failed');
print('ok   timers.setImmediate');

const cancelled = setTimeout(() => { throw new Error('cancelled timer fired'); }, 1);
clearTimeout(cancelled);
await sleep(10);
print('ok   timers.clearTimeout');
print('ALL_OK');
