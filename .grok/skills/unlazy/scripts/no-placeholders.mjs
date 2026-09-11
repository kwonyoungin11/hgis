#!/usr/bin/env node
// no-placeholders.mjs — turn "leave no placeholders or deferred remainder"
// from prose into a runnable oracle.
//
//   node .grok/skills/unlazy/scripts/no-placeholders.mjs <path> [<path>...]
//        [--ext .cpp,.h,.mjs] [--allow <regex>] [--control]
//
// Pass 1 of the four-pass leaf loop is the one that silently doesn't happen:
// the leaf reports done while three functions are stubs. This makes the
// unfinished remainder a gate rather than a promise.
//
// Prints NO_PLACEHOLDERS_OK and exits 0 when clean. Exits 1 with a file:line
// listing otherwise.
//
// --control injects a synthetic hit into the scan and REQUIRES the scan to
// fail. Run it once before trusting the absence: an absence check that is
// silently broken passes forever.

import { readdirSync, statSync, readFileSync } from 'node:fs';
import { join, extname, relative, resolve } from 'node:path';

const SUCCESS = 'NO_PLACEHOLDERS_OK';

const MARKERS = [
  /\bTODO\b/,
  /\bFIXME\b/,
  /\bXXX\b/,
  /\bHACK\b/,
  /\bTBD\b/,
  /\bWIP\b/,
  /\bnot[ _-]?implemented\b/i,
  /\bNotImplementedError\b/,
  /\bunimplemented\b/i,
  /\bplaceholder\b/i,
  /\bstub(bed)?\b/i,
  /\bfor now\b/i,
  /\bcoming soon\b/i,
  // No \b here: JS word boundaries are ASCII-only and never match beside Hangul.
  /(구현|작성)\s*(필요|예정|안함|미완)/,
  /미구현/,
  /미완성/,
  /임시\s*(코드|구현|처리)/,
];

const SKIP_DIRS = new Set([
  '.git', 'node_modules', '.unlazy', 'build', 'out', 'dist', 'bin', 'obj',
  '.vs', '.vscode', '.idea', 'vendor', 'third_party', '__pycache__',
]);

const DEFAULT_EXT = [
  '.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx',
  '.mjs', '.cjs', '.js', '.ts', '.tsx', '.jsx',
  '.py', '.rs', '.go', '.java', '.cs',
  '.ps1', '.psm1', '.sh', '.bat', '.cmd',
  '.cmake', '.txt', '.md', '.json', '.yml', '.yaml', '.toml',
];

const MAX_BYTES = 2 * 1024 * 1024;

// --- args -------------------------------------------------------------------

const argv = process.argv.slice(2);
const paths = [];
let exts = DEFAULT_EXT;
let allow = null;
let control = false;

for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--ext') exts = argv[++i].split(',').map((e) => (e.startsWith('.') ? e : '.' + e));
  else if (a === '--allow') allow = new RegExp(argv[++i]);
  else if (a === '--control') control = true;
  else if (a === '-h' || a === '--help') {
    console.error('usage: node no-placeholders.mjs <path>... [--ext .a,.b] [--allow <regex>] [--control]');
    process.exit(2);
  } else if (a.startsWith('--')) {
    console.error(`no-placeholders: unknown flag ${a}`);
    process.exit(2);
  } else paths.push(a);
}

if (paths.length === 0) {
  console.error('no-placeholders: no paths given. Pass the leaf\'s OWNS: paths explicitly —');
  console.error('scanning the whole tree makes the gate about other people\'s code.');
  process.exit(2);
}

// --- scan -------------------------------------------------------------------

const extSet = new Set(exts);
const hits = [];
let scanned = 0;

function walk(p) {
  let st;
  try {
    st = statSync(p);
  } catch {
    console.error(`no-placeholders: cannot stat ${p}`);
    process.exit(1);
  }
  if (st.isDirectory()) {
    for (const name of readdirSync(p)) {
      if (SKIP_DIRS.has(name)) continue;
      walk(join(p, name));
    }
    return;
  }
  if (!st.isFile()) return;
  if (!extSet.has(extname(p).toLowerCase())) return;
  if (st.size > MAX_BYTES) return;

  let text;
  try {
    text = readFileSync(p, 'utf8');
  } catch {
    return;
  }
  if (text.includes('\0')) return; // binary
  scanned++;

  text.split(/\r?\n/).forEach((line, i) => {
    if (allow && allow.test(line)) return;
    for (const m of MARKERS) {
      if (m.test(line)) {
        hits.push({ file: relative(process.cwd(), resolve(p)) || p, line: i + 1, text: line.trim().slice(0, 160) });
        return;
      }
    }
  });
}

for (const p of paths) walk(p);

if (control) {
  hits.push({ file: '<synthetic control>', line: 0, text: 'TODO injected by --control' });
}

// --- report -----------------------------------------------------------------

for (const h of hits) console.log(`${h.file}:${h.line}  ${h.text}`);

console.log(`\nscanned ${scanned} files  placeholders ${hits.length}`);

if (control) {
  if (hits.length > 0) {
    console.log('CONTROL_OK — the scan reports a known-positive, so its absence result is meaningful');
    process.exit(0);
  }
  console.log('CONTROL FAILED — the scan missed an injected marker; do not trust its clean runs');
  process.exit(1);
}

if (hits.length > 0) {
  console.log('PLACEHOLDERS REMAIN');
  process.exit(1);
}
if (scanned === 0) {
  console.log('scanned nothing — check the paths and --ext; an empty scan is not a clean scan');
  process.exit(1);
}
console.log(SUCCESS);
