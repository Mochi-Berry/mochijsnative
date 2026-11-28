import * as os from 'mbjs:os';

function envString(name) {
    try {
        return typeof os.getenv === 'function' ? os.getenv(name) : '';
    } catch (e) {
        return '';
    }
}

export const platform = () => os.platform || 'win32';
export const arch = () => globalThis.process?.arch || 'x64';
export const release = () => envString('OS') || 'unknown';
export const hostname = () => envString('COMPUTERNAME') || envString('HOSTNAME') || 'localhost';
export const tmpdir = () => (envString('TEMP') || envString('TMP') || envString('TMPDIR') || '.').replace(/\\/g, '/');
export const homedir = () => (envString('USERPROFILE') || envString('HOME') || '.').replace(/\\/g, '/');
export const endianness = () => 'LE';
export const type = () => os.platform === 'win32' ? 'Windows_NT' : 'Unknown';
export const cpus = () => [];
export const totalmem = () => { try { return Number(os.memoryInfo().systemTotal || 0); } catch { return 0; } };
export const freemem = () => { try { return Number(os.memoryInfo().systemFree || 0); } catch { return 0; } };
export const EOL = os.platform === 'win32' ? '\r\n' : '\n';
export const availableParallelism = () => 1;
export const machine = () => 'unknown';
export const constants = { signals: { SIGINT: 2, SIGTERM: 15 } };

/**
 * networkInterfaces(): the shim has no portable way to enumerate real NICs
 * (that needs netlink/getifaddrs native calls). Return only the loopback,
 * which is real and always present; code that needs a non-internal address
 * finds none and correctly falls back to localhost instead of binding to a
 * fabricated IP. Do NOT invent a LAN address here.
 */
export function networkInterfaces() {
    return {
        lo: [{
            address: '127.0.0.1', netmask: '255.0.0.0', family: 'IPv4',
            mac: '00:00:00:00:00:00', internal: true, cidr: '127.0.0.1/8',
        }],
    };
}

/**
 * userInfo(): Node shape without a native getpwuid. uid/gid are unknown on the
 * engine (no passwd backend), so report -1 (Node's "unknown" convention).
 */
export function userInfo() {
    return {
        username: envString('USERNAME') || envString('USER') || 'user',
        uid: -1,
        gid: -1,
        shell: null,
        homedir: homedir(),
    };
}

export default {
    platform,
    arch,
    release,
    hostname,
    tmpdir,
    homedir,
    endianness,
    type,
    cpus,
    totalmem,
    freemem,
    EOL,
    availableParallelism,
    machine,
    constants,
    networkInterfaces,
    userInfo,
};
