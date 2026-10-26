#!/usr/bin/env node
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const profile = process.argv[2];
const allowed = ['economy', 'balanced', 'quality', 'restore'];
if (!allowed.includes(profile)) {
  console.error('Usage: node .claude/scripts/gsd-profile.cjs economy|balanced|quality|restore');
  process.exit(2);
}
let root;
try { root = execFileSync('git', ['rev-parse', '--show-toplevel'], { encoding: 'utf8', stdio: ['ignore','pipe','ignore'] }).trim(); }
catch { root = process.cwd(); }
const configPath = path.join(root, '.planning', 'config.json');
const backupPath = path.join(root, '.planning', 'config.json.gsd-original.bak');
function fail(message) { console.error('GSD profile: ' + message); process.exit(1); }
function readJson(file) {
  let raw;
  try { raw = fs.readFileSync(file, 'utf8'); } catch { fail('File not found: ' + file); }
  try {
    const value = JSON.parse(raw);
    if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error('Expected JSON object');
    return value;
  } catch (error) { fail('Invalid JSON in ' + file + ': ' + error.message); }
}
function writeAtomic(file, text) {
  const temp = `${file}.${process.pid}.tmp`;
  try {
    fs.writeFileSync(temp, text, { flag: 'wx', mode: 0o600 });
    fs.renameSync(temp, file);
  } finally { if (fs.existsSync(temp)) fs.unlinkSync(temp); }
}
if (!fs.existsSync(configPath)) fail('Missing .planning/config.json. Run from your project root.');
const current = readJson(configPath);
if (profile === 'restore') {
  if (!fs.existsSync(backupPath)) fail('Original backup not found: ' + backupPath);
  const original = readJson(backupPath);
  writeAtomic(configPath, JSON.stringify(original, null, 2) + '\n');
  console.log('Restored original GSD config from ' + backupPath);
  process.exit(0);
}
const profiles = {
  economy: {
    model_profile: 'budget', parallelization: false, granularity: 'coarse',
    workflow: { research: false, plan_check: false, verifier: true, nyquist_validation: true,
      auto_advance: false, node_repair: false, code_review: false, pattern_mapper: false,
      compact_content: true, auto_prune_state: true }
  },
  balanced: {
    model_profile: 'budget', parallelization: false, granularity: 'standard',
    workflow: { research: true, plan_check: true, verifier: true, nyquist_validation: true,
      auto_advance: false, node_repair: true, code_review: true, pattern_mapper: false,
      compact_content: true, auto_prune_state: false }
  },
  quality: {
    model_profile: 'quality', parallelization: true, granularity: 'standard',
    workflow: { research: true, plan_check: true, verifier: true, nyquist_validation: true,
      auto_advance: false, node_repair: true, code_review: true, pattern_mapper: true,
      compact_content: false, auto_prune_state: false }
  }
};
if (!fs.existsSync(backupPath)) {
  try { fs.writeFileSync(backupPath, fs.readFileSync(configPath), { flag: 'wx', mode: 0o600 }); }
  catch (e) { if (e.code !== 'EEXIST') fail('Could not create original backup: ' + e.message); }
}
const patch = profiles[profile];
const updated = { ...current, ...Object.fromEntries(Object.entries(patch).filter(([key]) => key !== 'workflow')),
  workflow: { ...(current.workflow || {}), ...patch.workflow } };
writeAtomic(configPath, JSON.stringify(updated, null, 2) + '\n');
readJson(configPath);
console.log(`GSD profile activated: ${profile}`);
console.log(`Config: ${configPath}`);
console.log(`Original backup: ${backupPath}`);
console.log(`model_profile=${updated.model_profile}; granularity=${updated.granularity}; parallelization=${updated.parallelization}`);
console.log('Security, API and other unlisted settings were preserved.');
