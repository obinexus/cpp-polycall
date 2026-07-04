'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const binding = require('..');

for (const [name, file] of Object.entries(binding)) {
  assert.equal(path.isAbsolute(file), true, `${name} must be an absolute path`);
  assert.equal(fs.existsSync(file), true, `${name} does not exist: ${file}`);
}

assert.equal(
  require.resolve('@obinexusltd/cpp-polycall/src/polycall.cpp'),
  binding.source
);
assert.equal(
  require.resolve('@obinexusltd/cpp-polycall/include/cpp_polycall/polycall.hpp'),
  binding.header
);

console.log('cpp-polycall npm package test: PASS');
