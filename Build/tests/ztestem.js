/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

/* jshint node: true, globalstrict: true, camelcase: false */

'use strict';

var Testem = require('testem');
var path = require('path');
var util = require('util');

var testem = new Testem();
var file = process.argv[2] ? process.argv[2] : 'testem.json';

function ZReporter(silent, out) {
    this.out = out || process.stdout;
    this.id = 1;
    this.total = 0;
    this.pass = 0;
    this.results = [];
    this.logs = [];
}

ZReporter.prototype = {
    report: function(prefix, data) {
        this.display(prefix, data);
        this.total++;

        if (data.passed) {
            this.pass++;
        }
    },

    yamlDisplay: function(err, logs) {
        var failed = Object.keys(err || {})
            .filter(function(key){
                return key !== 'passed';
            })
            .map(function(key){
                return ': ' + String(err[key]);
            });
        var testLogs;

        logs = logs || [];
        testLogs = [':Log: >'].concat(logs.map(function(log){return String(log);}));

        return failed.concat(testLogs).join('\n');
    },

    resultDisplay: function(prefix, result) {
        var testsRun = this.id++,
            what = result.passed ? 'pass' : 'fail',
            tid = result.name.trim();

        return testsRun + ' ' + what + ' ' + tid;
    },

    summaryDisplay: function() {
        if (this.total === 0) {
            return util.format('no tests found in "%s"', file);
        }

        var passed = (this.pass / this.total) * 100,
            failed = 100 - passed;

        return '# ' + passed.toFixed(2) + '% passed ' + failed.toFixed(2) + '% failed';
    },

    display: function(prefix, result) {
        this.results.push(this.resultDisplay(prefix, result));

        if (result.error || result.logs.length) {
            this.results.push(this.yamlDisplay(result.error, result.logs));
        }
    },

    finish: function() {
        var groupName = file.substring(0, file.search(/.testem\.json/)) || 'jasmine';

        this.out.write('1..' + this.total + ' ' + groupName + '\n');
        this.out.write(this.results.join('\n'));
        this.out.write('\n' + this.summaryDisplay() + '\n');
    }
};

/* cf https://github.com/airportyh/testem/blob/master/docs/config_file.md#common-configuration-options */
testem.startCI({
    file: file,
    reporter: new ZReporter(),
    framework: 'jasmine',
    bail_on_uncaught_error: true,
    timeout: 30000,
    fail_on_zero_tests: true,
    launch: 'PhantomJS'
});
