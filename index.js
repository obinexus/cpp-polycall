'use strict';

const path = require('node:path');

const fromPackageRoot = (...segments) => path.join(__dirname, ...segments);

module.exports = Object.freeze({
  root: __dirname,
  source: fromPackageRoot('src', 'polycall.cpp'),
  header: fromPackageRoot('include', 'cpp_polycall', 'polycall.hpp'),
  compatibilityHeader: fromPackageRoot('src', 'polycall.hpp'),
  ffiHeader: fromPackageRoot('generated', 'polycall', 'polycall_ffi.h'),
  cmakeLists: fromPackageRoot('CMakeLists.txt'),
  config: fromPackageRoot('cpp-polycallrc'),
  manifest: fromPackageRoot('polycall-binding.json')
});
