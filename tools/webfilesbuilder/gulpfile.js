const gulp = require('gulp');
const fs = require('fs-extra');
const zlib = require('zlib');
const concat = require('gulp-concat');
const path = require('path');
const htmlmin = require('gulp-htmlmin');
const uglify = require('gulp-uglify');
const websrc = '../../src/websrc/';
const webh = `../../src/webh/`;
const tpDir = `${websrc}3rdparty/`;
const prDir = `${websrc}process/`;
const uiDir = `${websrc}ui/`;
const mnDir = `${prDir}mini/`;
const fnDir = `${prDir}final/`;
const gzDir = `${prDir}gzip/`;
const directories = [
    websrc,
    webh,
    tpDir,
    prDir,
    uiDir,
    mnDir,
    fnDir,
    gzDir
];
const mn = 'required';

// Recreate output directories (destructive)
const ensureDirs = (cb) => {
    console.log('RECREAT OUTPUT DIRECTORIES (destructive of: webh & process)');
    fs.removeSync(webh);
    fs.removeSync(prDir);
    directories.forEach(dir => {
        fs.ensureDirSync(dir);
    });
    cb();
};

// Minify JavaScript files
function minify(srcFolder, destFolder) {
    return gulp.src(`${srcFolder}*.js`).pipe(uglify()).pipe(gulp.dest(destFolder));
}

// Concat files into a single file.
function merge(srcFolder, fileType, destDir, outputFile) {
    return gulp.src(`${srcFolder}*.${fileType}`).pipe(concat({
        path: outputFile,
        stat: {
            mode: 0o666
        }
    })).pipe(gulp.dest(destDir));
}

// Gzip files
function gzip(srcFile, destFile, cb) {
    const input = fs.readFileSync(srcFile);
    const output = zlib.gzipSync(input);
    fs.writeFileSync(destFile, output);
    cb();
}

// Create byte arrays of gzipped files
function byteArray(source, destination, name, cb) {
    const arrayName = name.replace(/\.|-/g, "_");
    const data = fs.readFileSync(source);
    const wstream = fs.createWriteStream(destination);
    wstream.write(`#define ${arrayName}_len ${data.length}\n`);
    wstream.write(`const uint8_t ${arrayName}[] PROGMEM = {`);
    for (let i = 0; i < data.length; i++) {
        if (i % 1000 === 0) 
            wstream.write('\n');
        wstream.write('0x' + data[i].toString(16).padStart(2, '0') + (i < data.length - 1 ? ',' : ''));
    }
    wstream.write('\n};');
    wstream.end(cb);
}

// Task: Process assets (scripts, UI scripts, HTML, styles, fonts)
const process = (cb) => {
    console.log('PROCESS UI & 3RDPARTY FILES TO FINAL: minify, merge, copy');
    const mergeJS = () => merge(`${tpDir}js/`, 'js', `${fnDir}`, `${mn}.js`);
    const mergeCSS = () => merge(`${tpDir}css/`, 'css', `${fnDir}`, `${mn}.css`);
    const minifyUIjs = () => minify(`${uiDir}`, `${fnDir}`);
    const minifyUIhtml = () => gulp.src(`${uiDir}*.htm*`).pipe(htmlmin({collapseWhitespace: true, minifyJS: true}).on('error', console.error)).pipe(gulp.dest(`${fnDir}`));
    const copyFonts = (cb) => {
        fs.copy(`${tpDir}fonts/`, `${fnDir}`, cb);
    };
    gulp.parallel(mergeJS, minifyUIjs, minifyUIhtml, mergeCSS, copyFonts)(cb);
};

// Task: Gzip files
function gzipAll(cb) {
    console.log('GZIP FROM FINAL');
    const files = fs.readdirSync(fnDir);
    const tasks = files.map((file) => {
        const srcFile = path.join(fnDir, file);
        const destFile = path.join(gzDir, file + '.gz');
        const taskName = (done) => gzip(srcFile, destFile, done);
        Object.defineProperty(taskName, 'name', {value: `${file}.gz`});
        return taskName;
    });
    gulp.parallel(... tasks)(cb);
}

// Task: Create byte arrays from gzipped files
function byteArrayAll(cb) {
    console.log('BYTE ARRAY FROM GZIP');
    const files = fs.readdirSync(gzDir);
    const tasks = files.map(file => {
        const srcFile = path.join(gzDir, file);
        const destFile = `${webh}${file}.h`;
        const taskName = (done) => byteArray(srcFile, destFile, file, done);
        Object.defineProperty(taskName, 'name', {value: `${file}.h`});
        return taskName;
    });
    gulp.parallel(... tasks)(cb);
}

// Main runner function
function runner(cb) {
    gulp.series(ensureDirs, process, gzipAll, byteArrayAll)(cb);
}
exports.default = runner;
