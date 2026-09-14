// Pinned Graft engine, with HGIS-tested retrieval only. No CLI upkeep or init hooks.
import { existsSync } from 'node:fs';
import { dirname, isAbsolute, join, relative, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createInterface } from 'node:readline';

const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const notice = 'HGIS navigation aid: indexed files only. Confirm source lines and signatures before edits. '
  + 'C++ cross-file calls and Qt signals are incomplete; this is not an impact analysis.\n\n';
const descriptions = {
  graft_find_code: 'Find candidate definitions in indexed HGIS source. Read the actual source to confirm signatures and line spans.',
  graft_file_api: 'List candidate definitions in an indexed HGIS file. Parser spans may be incomplete; confirm the source.',
  graft_find_all: 'Search indexed HGIS source text. Coverage excludes unindexed files; use rg for exhaustive textual searches.',
  graft_check_freshness: 'Refresh the local structural index if needed and verify its working-tree fingerprint. No model calls.',
};
const result = (text, isError = false) => ({ content: [{ type: 'text', text }], isError });

export async function createToolService(root = projectRoot, home = join(projectRoot, 'build/tooling/graft-source')) {
  process.env.DO_NOT_TRACK = '1';
  process.env.CI = '1';
  process.env.GRAFT_REFRESH = 'hash';
  const indexDir = join(root, 'build/tooling/graft-index');
  const load = path => import(pathToFileURL(join(home, path)).href);
  if (!existsSync(join(home, 'dist/mcp/tools.js'))) {
    throw new Error('Graft is not installed. Run scripts/setup-graft.ps1 first.');
  }
  const [{ TOOLS, callTool }, { buildGraph }, { ensureFreshGraph }, { probeDrift, isClean }] = await Promise.all([
    load('dist/mcp/tools.js'), load('dist/graph/build.js'),
    load('dist/graph/refresh.js'), load('dist/graph/fingerprint.js'),
  ]);
  const tools = TOOLS.filter(t => Object.hasOwn(descriptions, t.name)).map(t => ({
    ...t, description: descriptions[t.name],
    annotations: { readOnlyHint: true, openWorldHint: false },
  }));
  const fresh = () => {
    const drift = probeDrift(root, indexDir);
    return existsSync(join(indexDir, '.graph/wiring.json')) && drift && isClean(drift);
  };
  return {
    tools,
    index: () => buildGraph(root, { contextDir: indexDir, graphOnly: true, onlyDirs: ['src', 'tests'] }),
    async call(name, args = {}) {
      try {
        const tool = tools.find(t => t.name === name);
        if (!tool) throw new Error(`Unsupported HGIS tool: ${name}`);
        for (const key of tool.inputSchema.required ?? []) {
          if (typeof args[key] !== 'string' || !args[key].trim()) throw new Error(`${key} is required`);
        }
        for (const key of ['file', 'in']) {
          if (!args[key]) continue;
          const path = relative(root, resolve(root, args[key]));
          if (isAbsolute(args[key]) || path.startsWith('..') || isAbsolute(path)) {
            throw new Error(`${key} must stay inside the repository`);
          }
        }
        const refresh = await ensureFreshGraph(root, { contextDir: indexDir });
        if (!fresh()) throw new Error(`Index missing or stale. Run scripts/setup-graft.ps1. ${refresh.note ?? ''}`);
        if (name === 'graft_check_freshness') {
          return result(`Fresh structural index: ${indexDir}\nScope: src/, tests/. Refreshed: ${refresh.refreshed}.`);
        }
        const answer = await callTool(root, name, args, indexDir);
        if (!fresh()) throw new Error('Source changed during retrieval; retry before using these results.');
        // These pinned upstream formatter lines are promotional/tool-routing metadata,
        // not source evidence. Keep source snippets and actual failure messages intact.
        const text = answer.text.split('\n').filter(line =>
          !/^\[graft\] tokens saved ≈ /.test(line)
          && !/^\[graft\] (?:no hits|only \d+ hits?) — don't re-ask/.test(line)).join('\n').trim();
        return result(notice + text, answer.isError);
      } catch (error) {
        return result(error instanceof Error ? error.message : String(error), true);
      }
    },
    async serve() {
      // Same newline JSON-RPC stdio transport as pinned Graft's server; no updater.
      const lines = createInterface({ input: process.stdin, crlfDelay: Infinity });
      const send = message => process.stdout.write(JSON.stringify({ jsonrpc: '2.0', ...message }) + '\n');
      const fail = (id, code, message) => send({ id, error: { code, message } });
      let pending = Promise.resolve();
      lines.on('line', line => {
        if (!line.trim()) return;
        let request;
        try { request = JSON.parse(line); }
        catch { fail(null, -32700, 'Parse error'); return; }
        if (!request || Array.isArray(request) || request.jsonrpc !== '2.0' || typeof request.method !== 'string') {
          fail(request?.id ?? null, -32600, 'Invalid request'); return;
        }
        const { id, method, params = {} } = request;
        if (id === undefined) return;
        if (!params || typeof params !== 'object' || Array.isArray(params)) {
          fail(id, -32602, 'Params must be an object'); return;
        }
        const reply = result => send({ id, result });
        if (method === 'initialize') {
          reply({ protocolVersion: params.protocolVersion ?? '2024-11-05', capabilities: { tools: {} },
            serverInfo: { name: 'hgis-graft', version: '1.0.0' } });
        } else if (method === 'ping') reply({});
        else if (method === 'tools/list') reply({ tools });
        else if (method === 'tools/call') {
          // Serialize refresh/query operations to share one coherent index.
          pending = pending.then(() => this.call(params.name, params.arguments))
            .then(reply, error => fail(id, -32603, String(error)));
        } else fail(id, -32601, `Method not found: ${method}`);
      });
      lines.on('close', () => { pending.finally(() => process.exit(0)); });
    },
  };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const service = await createToolService();
    if (process.argv.includes('--index')) {
      await service.index();
      console.error('HGIS source/test structural index ready.');
    } else {
      await service.serve();
    }
  } catch (error) {
    console.error(error instanceof Error ? error.message : error);
    process.exitCode = 1;
  }
}
