'use strict';

// Source package entry point: absolute paths for C++ build tooling. The
// binding itself is C++ (link it with CMake / pkg-config against the
// installed Polycall core, >= 1.1.0, binding ABI 1).
const path = require('node:path');

const fromPackageRoot = (...segments) => path.join(__dirname, ...segments);

module.exports = Object.freeze({
  root: __dirname,
  source: fromPackageRoot('src', 'polycall.cpp'),
  header: fromPackageRoot('include', 'cpp_polycall', 'polycall.hpp'),
  compatibilityHeader: fromPackageRoot('src', 'polycall.hpp'),
  cmakeLists: fromPackageRoot('CMakeLists.txt'),
  cmakeConfig: fromPackageRoot('cmake', 'cpp_polycallConfig.cmake.in'),
  config: fromPackageRoot('cpp-polycallrc'),
  manifest: fromPackageRoot('polycall-binding.json'),
  license: fromPackageRoot('LICENSE')
});
