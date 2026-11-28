import { request as httpRequest, get as httpGet } from '../mochihttp/index.js';

function unsupported(name) {
    const err = new Error(`mochihttps: ${name} is unavailable on MochiBerry runtime`);
    err.code = 'MOCHI_UNSUPPORTED';
    return err;
}

function normalizeUrl(url, options = {}) {
    let text;
    if (typeof url === 'string' || (url && typeof url.href === 'string')) {
        text = String(url);
    } else {
        const host = options.hostname || options.host || 'localhost';
        const port = options.port ? ':' + String(options.port) : '';
        const path = options.path || '/';
        text = 'https://' + host + port + path;
    }
    if (text.indexOf('http://') === 0) text = 'https://' + text.slice(7);
    return text;
}

function args(url, options, callback) {
    if (typeof options === 'function') {
        callback = options;
        if (url && typeof url === 'object' && typeof url.href !== 'string') {
            options = url;
            url = '';
        } else {
            options = {};
        }
    }
    return [normalizeUrl(url, options || {}), options || {}, callback];
}

export function request(url, options, callback) {
    const parts = args(url, options, callback);
    return httpRequest(parts[0], parts[1], parts[2]);
}

export function get(url, options, callback) {
    const parts = args(url, options, callback);
    return httpGet(parts[0], parts[1], parts[2]);
}

export function createServer() { throw unsupported('https.createServer'); }

export * from '../mochihttp/index.js';
export default { request, get, createServer };
