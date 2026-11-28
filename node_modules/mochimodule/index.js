import { require as mochiRequire, resolvePackage, cwd } from '../mochinode/index.js';
export async function createRequire(from) {
  const base = typeof from === 'string' ? from : (from && from.pathname ? from.pathname : cwd());
  return (spec) => mochiRequire(spec, base);
}
export default { createRequire };
