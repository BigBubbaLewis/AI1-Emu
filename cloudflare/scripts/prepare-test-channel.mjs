// Builds a self-contained deploy folder for the separate "ai1emu-test" Worker.
// It holds only the current build (app + five emulators) and never touches the
// live ai1emu-updates site or its assets.
//   node cloudflare/scripts/prepare-test-channel.mjs <output folder>
// Then, in the output folder: npm install && npx wrangler deploy
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
const root = path.resolve(import.meta.dirname, '../..');
const out = path.resolve(process.argv[2] || '');
if (!process.argv[2]) throw Error('Usage: node prepare-test-channel.mjs <output folder>');
if (out.startsWith(path.join(root, 'cloudflare', 'public'))) throw Error('Refusing to write into the live site assets folder');
const version = fs.readFileSync(path.join(root, 'source/app_version.h'), 'utf8').match(/AI1EMU_VERSION "([0-9.]+)"/)[1];
const hash = data => crypto.createHash('sha256').update(data).digest('hex');
const pub = path.join(out, 'public');
fs.rmSync(pub, {recursive: true, force: true});
fs.mkdirSync(path.join(out, 'src'), {recursive: true});
fs.mkdirSync(pub, {recursive: true});
fs.copyFileSync(path.join(root, 'cloudflare/src/worker.mjs'), path.join(out, 'src/worker.mjs'));
fs.writeFileSync(path.join(out, 'wrangler.jsonc'), JSON.stringify({
 name: 'ai1emu-test', main: 'src/worker.mjs', compatibility_date: '2026-11-03', compatibility_flags: ['nodejs_compat'],
 assets: {directory: './public', binding: 'ASSETS', run_worker_first: true}, observability: {enabled: true, head_sampling_rate: 1}, workers_dev: true
}, null, 1));
if (!fs.existsSync(path.join(out, 'package.json'))) {
 const wrangler = JSON.parse(fs.readFileSync(path.join(root, 'cloudflare/package.json'), 'utf8')).devDependencies.wrangler;
 fs.writeFileSync(path.join(out, 'package.json'), JSON.stringify({name: 'ai1emu-test', private: true, type: 'module', scripts: {deploy: 'wrangler deploy'}, devDependencies: {wrangler}}, null, 1));
}

// App: served as a normal asset (under the 25 MiB asset limit).
const nro = fs.readFileSync(path.join(root, 'AI1Emu.nro'));
if (nro.toString('ascii', 16, 20) !== 'NRO0') throw Error('Invalid AI1Emu.nro');
const appPath = `/releases/${version}/AI1Emu.nro`;
fs.mkdirSync(path.dirname(path.join(pub, appPath)), {recursive: true});
fs.writeFileSync(path.join(pub, appPath), nro);
const manifest = JSON.stringify({schema: 1, version, path: appPath, size: nro.length, sha256: hash(nro)}, null, 2);
fs.writeFileSync(path.join(pub, 'latest.json'), manifest);
fs.writeFileSync(path.join(pub, 'latest-multicore.json'), manifest);

// Emulators: streamed from parts below 25 MiB, listed in cores.txt.
// CORES_VERSION keeps an unchanged emulator set at its earlier version, so devices that
// already installed it are not asked to download it again.
const coresVersion = process.env.CORES_VERSION || version;
if (!/^\d+\.\d+\.\d+$/.test(coresVersion)) throw Error('Bad CORES_VERSION');
const index = {};
const lines = [`version ${coresVersion}`];
// dekopon.nro (3DS) is the unmodified upstream release (GPL-2.0), so it is read from
// DEKOPON_NRO when set; its source revision is recorded in licenses/Dekopon-NOTICE.txt.
// CORES_DIR points at a build folder outside the repo (e.g. when building to another drive).
const coresDir = process.env.CORES_DIR || path.join(root, 'build/switch/cores');
const coreFile = name => name === 'dekopon' && process.env.DEKOPON_NRO ? process.env.DEKOPON_NRO : path.join(coresDir, `${name}.nro`);
for (const name of ['fceumm', 'snes9x', 'genesis-plus-gx', 'melonds', 'mupen64plus-next', 'dekopon']) {
 const data = fs.readFileSync(coreFile(name));
 if (data.toString('ascii', 16, 20) !== 'NRO0') throw Error(`Invalid ${name}.nro`);
 const sha = hash(data), url = `/releases/${coresVersion}/cores/${name}.nro`, parts = [];
 for (let at = 0, i = 0; at < data.length; at += 22 * 1024 * 1024, i++) {
  const part = `/_downloads/${sha}/part-${i}.bin`;
  fs.mkdirSync(path.dirname(path.join(pub, part)), {recursive: true});
  fs.writeFileSync(path.join(pub, part), data.subarray(at, at + 22 * 1024 * 1024));
  parts.push(part);
 }
 index[url] = {parts, size: data.length, sha256: sha, type: 'application/octet-stream', filename: `${name}.nro`};
 lines.push(`${name}.nro ${data.length} ${sha} ${url}`);
}
fs.writeFileSync(path.join(pub, 'download-index.json'), JSON.stringify(index));
fs.writeFileSync(path.join(pub, 'cores.txt'), lines.join('\n') + '\n');
fs.writeFileSync(path.join(pub, 'index.html'), `<!doctype html><meta charset="utf-8"><title>AI1 Emu test channel</title><body style="font-family:system-ui;max-width:640px;margin:40px auto;padding:0 16px"><h1>AI1 Emu test channel</h1><p>Test builds for the owner's Switch. Version ${version}, not hardware-verified.</p><p>3DS support uses the unmodified <a href="https://github.com/PalindromicBreadLoaf/dekopon/releases/tag/v3.0.0-RC1">Dekopon v3.0.0-RC1</a> release (GPL-2.0-or-later). Its complete source is at <a href="https://github.com/PalindromicBreadLoaf/dekopon">github.com/PalindromicBreadLoaf/dekopon</a>.</p><p>The public downloads are at <a href="https://ai1emu-updates.freegamez.workers.dev/">ai1emu-updates</a>.</p></body>`);
for (const file of fs.readdirSync(pub, {recursive: true})) {
 const full = path.join(pub, String(file));
 if (fs.statSync(full).isFile() && fs.statSync(full).size > 25 * 1024 * 1024) throw Error('Asset too large: ' + file);
}
console.log(`Prepared test channel ${version} in ${out}`);
console.log(lines.join('\n'));
