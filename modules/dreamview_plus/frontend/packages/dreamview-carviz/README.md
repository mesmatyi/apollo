# [dreamview-carviz](https://github.com/hxj1234/dreamview-carviz)
[![](https://img.shields.io/badge/Powered%20by-jslib%20base-brightgreen.svg)](https://github.com/yanhaijing/jslib-base)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/hxj1234/dreamview-carviz/blob/master/LICENSE)
[![Build Status](https://travis-ci.org/hxj1234/dreamview-carviz.svg?branch=master)](https://travis-ci.org/hxj1234/dreamview-carviz)
[![Coveralls](https://img.shields.io/coveralls/hxj1234/dreamview-carviz.svg)](https://coveralls.io/github/hxj1234/dreamview-carviz)
[![npm](https://img.shields.io/badge/npm-0.1.0-orange.svg)](https://www.npmjs.com/package/dreamview-carviz)
[![NPM downloads](http://img.shields.io/npm/dm/dreamview-carviz.svg?style=flat-square)](http://www.npmtrends.com/dreamview-carviz)
[![Percentage of issues still open](http://isitmaintained.com/badge/open/hxj1234/dreamview-carviz.svg)](http://isitmaintained.com/project/hxj1234/dreamview-carviz "Percentage of issues still open")

The best scaffold for `JS|TS` third-party libraries

## :star: Features

- Supports writing source code in ES6+ or TypeScript, compiled to produce production code
- Multi-environment support (supports native browsers, AMD, CMD, Webpack, Rollup, fis, etc., and Node)
- Integrates [jsmini](https://github.com/jsmini)

> Note: If you do not use `export` and `export default` at the same time, you can turn on `legacy mode`. The module system in `legacy mode` is compatible with `ie6-8`; see the rollup configuration file

## :pill: Compatibility
Unit tests ensure support for the following environments:

| IE   | CH   | FF   | SF   | OP   | IOS  | Android   | Node  |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ----- |
| 6+   | 29+ | 55+  | 9+   | 50+  | 9+   | 4+   | 4+ |

**Note: The compiled code depends on an ES5 environment. For ie6-8, you need to include [es5-shim](http://github.com/es-shims/es5-shim/) for compatibility; see the example in [demo/demo-global.html](./demo/demo-global.html)**

## :open_file_folder: Directory Introduction

```
.
├── demo usage demo
├── dist compiled output code
├── doc project documentation
├── src source code directory
├── test unit tests
├── CHANGELOG.md changelog
└── TODO.md planned features
```

## :rocket: User Guide

Download and install the code through npm

```bash
$ npm install --save dreamview-carviz
```

If you are in a node environment

```js
var base = require('dreamview-carviz');
```

If you are in a webpack or similar environment

```js
import base from 'dreamview-carviz';
```

If you are in a requirejs environment

```js
requirejs(['node_modules/dreamview-carviz/dist/index.aio.js'], function (base) {
    // xxx
})
```

If you are in a browser environment

```html
<script src="node_modules/dreamview-carviz/dist/index.aio.js"></script>
```

## :bookmark_tabs: Documentation
[API](./doc/api.md)

## :kissing_heart: Contributor Guide
For the first run, install the dependencies first

```bash
$ npm install
```

Build the production code with one command

```bash
$ npm run build
```

Run the unit tests:

```bash
$ npm test
```

> Note: The browser environment needs to be tested manually, located in `test/browser`

Modify the version number in package.json, the version number in README.md, and CHANGELOG.md, then release the new version

```bash
$ npm run release
```

Publish the new version to npm

```bash
$ npm publish
```

## Contributors

[contributors](https://github.com/hxj1234/dreamview-carviz/graphs/contributors)

## :gear: Changelog
[CHANGELOG.md](./CHANGELOG.md)

## :airplane: Plan List
[TODO.md](./TODO.md)