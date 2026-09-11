#!/usr/bin/env node
// arm-scope.mjs — create an Unlazy scope and bind this Grok session to it.
//
//   node .grok/skills/unlazy/scripts/arm-scope.mjs <scope-id> [session-id]
//
// An unbound scope fails open: the Stop hook allows the turn and every gate in
// the ledger goes unenforced, silently. This script exists so arming is a
// verified step rather than a remembered one.
//
// It deliberately does not use shell redirection. On Windows, `echo %ID%> f`
// under cmd.exe leaves a trailing space before the redirect, and PowerShell's
// `echo`/`>` writes UTF-16 with a BOM. Either makes the hook's id comparison
// fail, which fails open — the exact outcome this skill is built to prevent.

import { mkdirSync, writeFileSync, readFileSync, existsSync } from 'node:fs';
import { join, resolve, sep } from 'node:path';

const SUCCESS = 'ARM_SCOPE_OK';
const SCOPE_RE = /^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$/;

function die(msg) {
  console.error(`arm-scope: ${msg}`);
  process.exit(1);
}

const [scopeArg, sessionArg] = process.argv.slice(2);

if (!scopeArg || scopeArg === '-h' || scopeArg === '--help') {
  console.error('usage: node arm-scope.mjs <scope-id> [session-id]');
  console.error('  session-id falls back to $GROK_SESSION_ID, then $UNLAZY_SESSION_ID.');
  process.exit(2);
}

const scope = scopeArg.trim();
if (!SCOPE_RE.test(scope)) {
  die(
    `invalid scope id ${JSON.stringify(scopeArg)}. ` +
      'Use 1-64 chars of [A-Za-z0-9._-] starting alphanumeric. ' +
      'Separators and traversal are rejected so a scope cannot escape .unlazy/.'
  );
}

const rawSession =
  sessionArg ?? process.env.GROK_SESSION_ID ?? process.env.UNLAZY_SESSION_ID ?? '';
const session = String(rawSession).trim();

if (!session) {
  die(
    'no session id. Pass it as the second argument, or set GROK_SESSION_ID. ' +
      'Refusing to create an unbound scope, which would fail open.'
  );
}
if (/[\r\n\0]/.test(session)) {
  die('session id contains a newline or NUL byte; refusing to write it.');
}

const scopeDir = resolve(process.cwd(), '.unlazy', scope);
const unlazyRoot = resolve(process.cwd(), '.unlazy');
if (scopeDir !== unlazyRoot && !scopeDir.startsWith(unlazyRoot + sep)) {
  die(`resolved scope path escapes .unlazy: ${scopeDir}`);
}

const sessionFile = join(scopeDir, 'session');

// Report a rebind rather than performing it silently: a stale binding is a
// plausible reason a previous run ended without enforcement.
let previous = null;
if (existsSync(sessionFile)) {
  try {
    previous = readFileSync(sessionFile, 'utf8').replace(/^﻿/, '').trim();
  } catch {
    previous = null;
  }
}

mkdirSync(scopeDir, { recursive: true });

// utf8, no BOM, no trailing newline — byte-exact for the hook's comparison.
writeFileSync(sessionFile, session, { encoding: 'utf8' });

const readBack = readFileSync(sessionFile, 'utf8');
if (readBack !== session) {
  die(
    `read-back mismatch. wrote ${JSON.stringify(session)}, ` +
      `read ${JSON.stringify(readBack)}. Scope is NOT armed.`
  );
}

const bytes = readFileSync(sessionFile);
if (bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) {
  die('a BOM was written despite utf8 encoding. Scope is NOT armed.');
}

if (previous !== null && previous !== session) {
  console.log(`rebound scope from previous session ${previous}`);
  console.log('If an earlier run in this scope ended without blocking, it was unbound.');
} else if (previous === session) {
  console.log('scope was already bound to this session; rewritten identically');
}

console.log(`scope   ${scope}`);
console.log(`dir     ${scopeDir}`);
console.log(`session ${session}`);
console.log(`bytes   ${bytes.length}`);
console.log('Next: write .unlazy/' + scope + '/GATES.md before doing real work.');
console.log(SUCCESS);
