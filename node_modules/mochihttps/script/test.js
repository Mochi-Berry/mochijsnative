import { get } from '../index.js';

let status = 0;
let bytes = 0;
await new Promise((resolve, reject) => {
    const req = get('https://example.com', (res) => {
        status = res.statusCode;
        (async () => {
            try {
                for await (const chunk of res) bytes += chunk.length;
                resolve();
            } catch (error) {
                reject(error);
            }
        })();
    });
    req.on('error', reject);
});
if (status !== 200 || bytes < 100) throw new Error('https.get failed');
print('ok   https.get status=' + status + ' bytes=' + bytes);
print('ALL_OK');
