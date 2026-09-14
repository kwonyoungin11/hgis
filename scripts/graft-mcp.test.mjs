import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, readFile, utimes } from 'node:fs/promises';
import { join, resolve } from 'node:path';
import { execFileSync, spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { createToolService } from './graft-mcp.mjs';

test('Graft project adapter: limited tools, freshness, errors and source preservation', async () => {
  const home = resolve('build/tooling/graft-source');
  await mkdir('build/tooling/tmp', { recursive: true });
  const root = await mkdtemp(resolve('build/tooling/tmp/graft-test-'));
  execFileSync('git', ['init', '-q', root]);
  await mkdir(join(root, 'src'));
  await writeFile(join(root, '.gitignore'), 'build/\n');
  await writeFile(join(root, 'AGENTS.md'), 'Source is authoritative.\n');
  await writeFile(join(root, 'src/sample.cpp'), 'int hgis_before() { return 1; }\n');
  const stamp = new Date('2026-01-01T00:00:00Z');
  await utimes(join(root, 'src/sample.cpp'), stamp, stamp);
  const service = await createToolService(root, home);
  assert.deepEqual(service.tools.map(t => t.name).sort(), [
    'graft_check_freshness', 'graft_file_api', 'graft_find_all', 'graft_find_code',
  ]);
  assert.equal((await service.call('graft_trace_calls', {})).isError, true);
  assert.equal((await service.call('graft_find_code', {})).isError, true);
  assert.equal((await service.call('graft_file_api', { file: '../outside.cpp' })).isError, true);
  assert.equal((await service.call('graft_check_freshness', {})).isError, true, 'missing graph must fail');
  await service.index();
  assert.equal((await service.call('graft_check_freshness', {})).isError, false);
  const before = await service.call('graft_file_api', { file: 'src/sample.cpp' });
  assert.equal(before.isError, false);
  assert.match(before.content[0].text, /hgis_before/);
  // Restores and generated files can preserve size and timestamp while changing bytes.
  await writeFile(join(root, 'src/sample.cpp'), 'int hgis_after_() { return 2; }\n');
  await utimes(join(root, 'src/sample.cpp'), stamp, stamp);
  const preservedTime = await service.call('graft_file_api', { file: 'src/sample.cpp' });
  assert.match(preservedTime.content[0].text, /hgis_after_/);
  assert.doesNotMatch(preservedTime.content[0].text, /hgis_before/);
  await writeFile(join(root, 'src/sample.cpp'), 'int hgis_after_edit() { return 22; }\n');
  const after = await service.call('graft_find_all', { pattern: 'hgis_after_edit', fixed: true });
  assert.equal(after.isError, false);
  assert.match(after.content[0].text, /hgis_after_edit/);
  const api = await service.call('graft_file_api', { file: 'src/sample.cpp' });
  assert.doesNotMatch(api.content[0].text, /hgis_before/);
  assert.equal(await readFile(join(root, 'AGENTS.md'), 'utf8'), 'Source is authoritative.\n');
  assert.equal(await readFile(join(root, '.gitignore'), 'utf8'), 'build/\n');
  assert.equal(await readFile(join(root, 'src/sample.cpp'), 'utf8'), 'int hgis_after_edit() { return 22; }\n');
});

test('configured stdio entrypoint starts and speaks MCP', { timeout: 15000 }, async () => {
  const child = spawn(process.execPath, [resolve('scripts/graft-mcp.mjs')], { stdio: ['pipe', 'pipe', 'pipe'] });
  let errors = '';
  child.stderr.on('data', data => { errors += data; });
  const lines = createInterface({ input: child.stdout });
  const replies = new Map();
  lines.on('line', line => { const reply = JSON.parse(line); replies.get(reply.id)?.(reply); });
  let id = 0;
  const rpc = (method, params = {}) => new Promise(resolveReply => {
    replies.set(++id, resolveReply);
    child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
  });
  try {
    const init = await rpc('initialize', { protocolVersion: '2024-11-05' });
    assert.equal(init.result.serverInfo.name, 'hgis-graft', errors);
    const list = await rpc('tools/list');
    assert.equal(list.result.tools.length, 4);
    assert.deepEqual((await rpc('ping')).result, {});
    assert.equal((await rpc('tools/call', { name: 'graft_trace_calls', arguments: {} })).result.isError, true);
    assert.equal((await rpc('unknown-method')).error.code, -32601);
    assert.equal((await rpc('initialize', null)).error.code, -32602);
  } finally {
    child.stdin.end();
    child.kill();
    lines.close();
  }
});
