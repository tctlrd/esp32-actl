var gulp = require('gulp');
var fs = require('fs-extra');
var zlib = require('zlib');
var concat = require('gulp-concat');
var gzip = require('gulp-gzip');
var flatmap = require('gulp-flatmap');
var path = require('path');
var htmlmin = require('gulp-htmlmin');
var uglify = require('gulp-uglify');
var pump = require('pump');

function createByteArray(source, destination, arrayName, cb) {
    try {
        const data = fs.readFileSync(source);
        const wstream = fs.createWriteStream(destination);
        wstream.on('error', function (err) {
            console.log(err);
        });

        wstream.write(`#define ${arrayName}_len ${data.length}\n`);
        wstream.write(`const uint8_t ${arrayName}[] PROGMEM = {`);

        for (let i = 0; i < data.length; i++) {
            if (i % 1000 === 0) wstream.write('\n');
            wstream.write('0x' + data[i].toString(16).padStart(2, '0'));
            if (i < data.length - 1) wstream.write(',');
        }

        wstream.write('\n};');
        wstream.end();
        cb();
    } catch (error) {
        console.error('Error processing gzipped file:', error);
        cb(error);
    }
}

function espRfidJsMinify (cb) {
    return pump([
            gulp.src('../../src/websrc/js/esprfid.js'),
            uglify(),
            gulp.dest('../../src/websrc/gzipped/js/'),
        ] );
}

function espRfidJsGz() {
    return gulp.src("../../src/websrc/gzipped/js/esprfid.js")
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/js/'));
}


function espRfidJsGzh(cb) {
    const source = "../../src/websrc/gzipped/js/esprfid.js.gz";
    const destination = "../../src/webh/esprfid.js.gz.h";
    createByteArray(source, destination, 'esprfid_js_gz', cb);
}

function BoardsJsMinify (cb) {
    return pump([
            gulp.src('../../src/websrc/js/boards.js'),
            uglify(),
            gulp.dest('../../src/websrc/gzipped/js/'),
        ]);
}

function BoardsJsGz() {
    return gulp.src("../../src/websrc/gzipped/js/boards.js")
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/js/'));
}

function BoardsJsGzh(cb) {
    const source = "../../src/websrc/gzipped/js/boards.js.gz";
    const destination = "../../src/webh/boards.js.gz.h";
    createByteArray(source, destination, 'boards_js_gz', cb);
}

function scriptsConcat() {
    return gulp.src([
            '../../src/websrc/3rdparty/js/jquery.min.js',
            '../../src/websrc/3rdparty/js/bootstrap.min.js',
            '../../src/websrc/3rdparty/js/footable.min.js',
            ])
        .pipe(concat({
            path: 'required.js',
            stat: {
                mode: 0o666
            }
        }))
        .pipe(gulp.dest('../../src/websrc/js/'))
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/js/'));
}

function scripts(cb) {
    const source = "../../src/websrc/gzipped/js/required.js.gz";
    const destination = "../../src/webh/required.js.gz.h";
    createByteArray(source, destination, 'required_js_gz', cb);
}

function stylesConcat() {
    return gulp.src([
            '../../src/websrc/3rdparty/css/bootstrap.min.css',
            '../../src/websrc/3rdparty/css/footable.bootstrap.min.css',
            '../../src/websrc/3rdparty/css/sidebar.css',
        ])
        .pipe(concat({
            path: 'required.css',
            stat: {
                mode: 0o666
            }
        }))
        .pipe(gulp.dest('../../src/websrc/css/'))
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/css/'));
}

function styles(cb) {
    const source = "../../src/websrc/gzipped/css/required.css.gz";
    const destination = "../../src/webh/required.css.gz.h";
    createByteArray(source, destination, 'required_css_gz', cb);
}

function fontgz(cb) {
    const sourceDir = "../../src/websrc/3rdparty/fonts";
    const destDir = "../../src/websrc/fonts/";
    const gzDir = '../../src/websrc/gzipped/fonts/';
    try {
        fs.ensureDirSync(destDir);
        fs.ensureDirSync(gzDir);
        fs.copySync(sourceDir, destDir);
        const files = fs.readdirSync(destDir);
        files.forEach(file => {
            const filePath = path.join(destDir, file);
            const gzFilePath = path.join(gzDir, file + '.gz');
            const input = fs.readFileSync(filePath);
            const output = zlib.gzipSync(input);
            fs.writeFileSync(gzFilePath, output);
        });
        cb();
    } catch (err) {
        gutil.log(err);
    }
}

function fonts() {
    return gulp.src("../../src/websrc/gzipped/fonts/*.*")
        .pipe(flatmap(function(stream, file) {
            const filename = path.basename(file.path);
            const source = file.path;
            const destination = `../../src/webh/${filename}.h`;
            const arrayName = filename.replace(/\.|-/g, "_");
            createByteArray(source, destination, arrayName, function() {});
            return stream;
        }));
}

function htmlsPrep() {
    return gulp.src('../../src/websrc/*.htm*')
        .pipe(htmlmin({collapseWhitespace: true, minifyJS: true}))
        .pipe(gulp.dest('../../src/websrc/gzipped/'))
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/'));
}

function htmlsGz() {
    return gulp.src("../../src/websrc/*.htm*")
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/'));
}

function htmls() {
    return gulp.src("../../src/websrc/gzipped/*.gz")
        .pipe(flatmap(function(stream, file) {
            const filename = path.basename(file.path);
            const source = file.path;
            const destination = `../../src/webh/${filename}.h`;
            const arrayName = filename.replace(/\.|-/g, "_");
            createByteArray(source, destination, arrayName, function() {});
            return stream;
        }));
}

async function runner() {
    const scriptTasks = gulp.series(espRfidJsMinify, espRfidJsGz, espRfidJsGzh, BoardsJsMinify, BoardsJsGz, BoardsJsGzh, scriptsConcat, scripts);
    const styleTasks = gulp.series(stylesConcat, styles);
    const fontTasks = gulp.series(fontgz, fonts);
    const htmlTasks = gulp.series(htmlsGz, htmlsPrep, htmls);
    const parallel = await gulp.parallel(scriptTasks, styleTasks, fontTasks, htmlTasks);
    return await parallel();
}

exports.default = runner;