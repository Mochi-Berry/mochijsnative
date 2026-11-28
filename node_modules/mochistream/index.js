import { EventEmitter } from '../mochievents/index.js';

function toChunk(value) {
    if (value === undefined || value === null) return new Uint8Array(0);
    if (value instanceof Uint8Array) return value;
    if (Array.isArray(value)) return Uint8Array.from(value);
    if (typeof value === 'string') {
        const out = [];
        for (let i = 0; i < value.length; i++) out.push(value.charCodeAt(i) & 0xff);
        return Uint8Array.from(out);
    }
    if (value.buffer instanceof ArrayBuffer) return new Uint8Array(value.buffer, value.byteOffset || 0, value.byteLength);
    throw new TypeError('mochistream: chunk must be string, Uint8Array, or ArrayBuffer view');
}

function concat(chunks) {
    const size = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
    const out = new Uint8Array(size);
    let at = 0;
    for (const chunk of chunks) {
        out.set(chunk, at);
        at += chunk.length;
    }
    return out;
}

export class Readable extends EventEmitter {
    constructor(options = {}) {
        super();
        this.readable = true;
        this.writable = false;
        this._chunks = [];
        this._ended = false;
        this._reading = false;
        this._destroyed = false;
        this._readImpl = typeof options.read === 'function' ? options.read : null;
        this._rf = null;
        this._iterators = new Set();
    }

    _read() {}

    push(chunk) {
        if (this._destroyed) return false;
        if (chunk === null) {
            this._ended = true;
            this.emit('end');
            for (const iterator of this._iterators) iterator();
            return false;
        }
        const bytes = toChunk(chunk);
        if (bytes.length) {
            this._chunks.push(bytes);
            this.emit('data', bytes);
            for (const iterator of this._iterators) iterator();
        }
        return true;
    }

    read(size) {
        if (!this._chunks.length) {
            if (!this._reading && !this._ended) {
                this._reading = true;
                Promise.resolve().then(() => {
                    const fn = this._readImpl || this._read.bind(this);
                    fn.call(this, size);
                    this._reading = false;
                });
            }
            return null;
        }
        const want = Number(size) || this._chunks[0].length;
        if (this._chunks[0].length <= want) return this._chunks.shift();
        const chunk = this._chunks[0];
        this._chunks[0] = chunk.subarray(want);
        return chunk.subarray(0, want);
    }

    async *[Symbol.asyncIterator]() {
        while (true) {
            while (this._chunks.length) yield this._chunks.shift();
            if (this._ended) return;
            await new Promise((resolve) => {
                const wake = () => { this._iterators.delete(wake); resolve(); };
                this._iterators.add(wake);
                this.once('end', wake);
            });
        }
    }

    pipe(destination, options = {}) {
        const end = options.end !== false;
        const onData = (chunk) => {
            if (destination.write(chunk) === false && typeof this.pause === 'function') this.pause();
        };
        this.on('data', onData);
        this.once('end', () => {
            this.off('data', onData);
            if (end && typeof destination.end === 'function') destination.end();
        });
        this.once('error', (error) => {
            if (typeof destination.destroy === 'function') destination.destroy(error);
        });
        if (typeof this.resume === 'function') this.resume();
        return destination;
    }

    pause() { return this; }
    resume() { return this; }
    destroy(error) {
        if (this._destroyed) return this;
        this._destroyed = true;
        if (error) this.emit('error', error);
        this.emit('close');
        return this;
    }

    static from(iterable) {
        const stream = new Readable();
        (async () => {
            try {
                for await (const item of iterable) {
                    stream.push(item);
                    await Promise.resolve();
                }
                stream.push(null);
            } catch (error) {
                stream.destroy(error);
            }
        })();
        return stream;
    }

    static fromWeb(body) {
        const stream = new Readable();
        (async () => {
            try {
                const reader = body.getReader();
                for (;;) {
                    const part = await reader.read();
                    if (part.done) break;
                    stream.push(part.value);
                }
                stream.push(null);
            } catch (error) {
                stream.destroy(error);
            }
        })();
        return stream;
    }

    static toWeb(stream) {
        return {
            getReader() {
                const iterator = stream[Symbol.asyncIterator]();
                return {
                    async read() {
                        const next = await iterator.next();
                        return next.done ? { done: true, value: undefined } : { done: false, value: next.value };
                    },
                    releaseLock() {},
                };
            },
        };
    }
}

export class Writable extends EventEmitter {
    constructor(options = {}) {
        super();
        this.readable = false;
        this.writable = true;
        this._writable = true;
        this._writeImpl = typeof options.write === 'function' ? options.write : null;
        this._queue = Promise.resolve();
        this._ended = false;
    }

    _write(chunk, encoding, callback) { callback(); }

    write(chunk, encoding, callback) {
        if (typeof encoding === 'function') {
            callback = encoding;
            encoding = undefined;
        }
        const bytes = toChunk(chunk);
        this._queue = this._queue.then(() => new Promise((resolve, reject) => {
            const done = (error) => {
                if (error) {
                    if (callback) callback(error);
                    reject(error);
                } else {
                    if (callback) callback(null);
                    resolve();
                }
            };
            try {
                const fn = this._writeImpl || this._write.bind(this);
                const result = fn.call(this, bytes, encoding, done);
                if (result && typeof result.then === 'function') result.then(() => done(), done);
            } catch (error) {
                done(error);
            }
        }));
        return this._queue.length !== undefined;
    }

    end(chunk, encoding, callback) {
        if (typeof chunk === 'function') {
            callback = chunk;
            chunk = undefined;
        } else if (typeof encoding === 'function') {
            callback = encoding;
            encoding = undefined;
        }
        const finish = () => {
            this._ended = true;
            this.emit('finish');
            if (callback) callback();
        };
        if (chunk !== undefined) this.write(chunk, encoding);
        this._queue.then(finish, (error) => this.emit('error', error));
        return this;
    }

    destroy(error) {
        if (error) this.emit('error', error);
        this.emit('close');
        return this;
    }
}

export class Duplex extends Readable {
    constructor(options = {}) {
        super(options);
        this.writable = true;
        this._writable = true;
        this._writeImpl = typeof options.write === 'function' ? options.write : null;
        this._writeQueue = Promise.resolve();
        this._writableEnded = false;
    }

    _write(chunk, encoding, callback) { callback(); }

    write(chunk, encoding, callback) {
        if (typeof encoding === 'function') {
            callback = encoding;
            encoding = undefined;
        }
        const bytes = toChunk(chunk);
        this._writeQueue = this._writeQueue.then(() => new Promise((resolve, reject) => {
            const done = (error) => error ? reject(error) : resolve();
            try {
                const fn = this._writeImpl || this._write.bind(this);
                fn.call(this, bytes, encoding, (error) => {
                    if (error) {
                        if (callback) callback(error);
                        done(error);
                    } else {
                        if (callback) callback(null);
                        done();
                    }
                });
            } catch (error) {
                if (callback) callback(error);
                reject(error);
            }
        }));
        return true;
    }

    end(chunk, encoding, callback) {
        if (typeof chunk === 'function') {
            callback = chunk;
            chunk = undefined;
        } else if (typeof encoding === 'function') {
            callback = encoding;
            encoding = undefined;
        }
        if (chunk !== undefined) this.write(chunk, encoding);
        this._writeQueue.then(() => {
            this._writableEnded = true;
            this.emit('finish');
            if (callback) callback();
        });
        return this;
    }
}

export class Transform extends Duplex {
    constructor(options = {}) {
        super(options);
        this._transformImpl = typeof options.transform === 'function'
            ? options.transform
            : (typeof options.transform === 'object' && typeof options.transform.transform === 'function'
                ? options.transform.transform
                : null);
        const flush = options.flush || (typeof options.transform === 'object' ? options.transform.flush : null);
        this._flushImpl = typeof flush === 'function' ? flush : null;
    }

    _transform(chunk, encoding, callback) { callback(null, chunk); }

    _write(chunk, encoding, callback) {
        const fn = this._transformImpl || this._transform.bind(this);
        let settled = false;
        const done = (error, output) => {
            if (settled) return;
            settled = true;
            if (error) return callback(error);
            if (output !== undefined && output !== null) this.push(output);
            callback();
        };
        try {
            const result = fn.call(this, chunk, encoding, done);
            if (result && typeof result.then === 'function') result.then((output) => done(null, output), done);
        } catch (error) {
            done(error);
        }
    }

    _final(callback) {
        if (!this._flushImpl) return callback();
        let settled = false;
        const done = (error, output) => {
            if (settled) return;
            settled = true;
            if (error) return callback(error);
            if (output !== undefined && output !== null) this.push(output);
            callback();
        };
        try {
            const result = this._flushImpl.call(this, done);
            if (result && typeof result.then === 'function') result.then((output) => done(null, output), done);
        } catch (error) {
            done(error);
        }
    }
}

export class PassThrough extends Transform {}

export function finished(stream, callback) {
    const done = (error) => {
        if (callback) callback(error);
    };
    if (stream._ended || stream._writableEnded) {
        Promise.resolve().then(() => done());
        return;
    }
    stream.once('finish', () => done());
    stream.once('end', () => done());
    stream.once('error', done);
}

export function pipeline(...args) {
    const callback = typeof args[args.length - 1] === 'function' ? args.pop() : null;
    const streams = args;
    const run = async () => {
        for (let i = 0; i < streams.length - 1; i++) streams[i].pipe(streams[i + 1], { end: i + 1 < streams.length - 1 });
        await Promise.all(streams.map((stream) => new Promise((resolve, reject) => {
            if (stream._ended || stream._writableEnded) return resolve();
            stream.once('finish', resolve);
            stream.once('end', resolve);
            stream.once('error', reject);
        })));
    };
    if (callback) {
        run().then(() => callback(), callback);
        return undefined;
    }
    return run();
}

export async function collect(stream) {
    const chunks = [];
    for await (const chunk of stream) chunks.push(toChunk(chunk));
    return concat(chunks);
}

export { concat, toChunk };
export default { Readable, Writable, Duplex, Transform, PassThrough, finished, pipeline, collect, concat, toChunk };


export const promises = {
  pipeline(...args) {
    return new Promise((resolve, reject) => pipeline(...args, (err) => err ? reject(err) : resolve()));
  },
  finished(stream) {
    return new Promise((resolve, reject) => finished(stream, (err) => err ? reject(err) : resolve()));
  },
};
