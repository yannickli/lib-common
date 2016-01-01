/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

/* This script parses a TypeScript file and search for its dependencies in the
 * NODE_PATH environment variable. Then it output the list of typescript
 * declaration files the TypeScript file depends on.
 *
 * When the script finds a .d.ts files, it consider it has found the right
 * declaration file. When it finds a .ts file, it supposes the declaration
 * file will be generated by the build system in the build directory.
 */
(function() {
    /* global process:false */

    'use strict';

    var fs = require('fs');
    var typescript = require('typescript');
    var path = require('path');

    if (process.argv.lenth < 5) {
        throw new Error('missing arguments: node _get_ts_deps.js file.ts '
                      + 'path_path build_path');
    }

    var fileName = process.argv[2];
    var basePath = path.resolve(process.argv[3]) + '/';
    var buildPath = path.resolve(process.argv[4]) + '/';

    var data = fs.readFileSync(fileName, 'utf8');
    var desc = typescript.preProcessFile(data);
    var paths = process.env.NODE_PATH.split(':');
    var names = [];

    function addName(dest, mapToBuild) {
        dest = path.resolve(dest);

        if (mapToBuild && dest.substr(0, buildPath.length) !== buildPath
        &&  dest.substr(0, basePath.length) === basePath)
        {
            dest = buildPath + dest.substr(basePath.length);
        }

        if (dest.substr(0, basePath.length) === basePath) {
            dest = dest.substr(basePath.length);
        }
        names.push(dest);
    }

    desc.importedFiles.forEach(function(ref) {
        var name = ref.fileName;
        var isRaw = name.length >= 5 && (name.substr(name.length - 5) === '.json'
                                         || name.substr(name.length - 5) === '.html');

        for (var j = 0; j < paths.length; j++) {
            var searchPath = path.resolve(paths[j]) + '/';

            if (fs.existsSync(searchPath + name + '.d.ts')) {
                addName(searchPath + name + '.d.ts', true);
                return;
            } else
            if (isRaw && fs.existsSync(searchPath + name)) {
                addName(searchPath + name + '.d.ts', true);
                return;
            } else
            if (!isRaw && fs.existsSync(searchPath + name + '.ts')) {
                addName(searchPath + name + '.d.ts', true);
                return;
            }
        }
    });

    desc.referencedFiles.forEach(function(ref) {
        addName(path.dirname(fileName) + '/' + ref.fileName);
    });

    console.log(names.join(' '));
}());
