const gulp = require('gulp');
const fs = require('fs-extra');
const zlib = require('zlib');
const concat = require('gulp-concat');
const flatmap = require('gulp-flatmap');
const path = require('path');
const htmlmin = require('gulp-htmlmin');
const uglify = require('gulp-uglify');
const pump = require('pump');

const srcDir = '../../src/websrc/';
const webhDir = '../../src/webh/';
const uiDir = `${srcDir}ui/`;
const thDir = `${srcDir}3rdparty/`;
const prDir = `${srcDir}process/`;
const mnDir = `${prDir}mini/`;
const fnDir = `${prDir}final/`;
const gzDir = `${prDir}gzip/`;

const mn = 'required';

// minify javascript files
function minifyScripts(srcFolder, destFolder) {
    return gulp.src(`${srcFolder}*.js`)
        .pipe(uglify())
        .pipe(gulp.dest(destFolder));
}

// concate style and script files into single [.js|.css] files
function merge(srcFolder, fileType, destDir, outputFile) {
    return gulp.src(`${srcFolder}*.${fileType}`)
        .pipe(concat({
            path: outputFile,
            stat: {
                mode: 0o666
            }
        }))
        .pipe(gulp.dest(destDir));
}

// gzip all files before converting them to to byte arrays
function gzip(src, dest) {
    return function (cb) {
        fs.readdirSync(src).forEach(file => {
            const filePath = path.join(src, file);
            const gzFilePath = path.join(dest, file + '.gz');
            const input = fs.readFileSync(filePath);
            const output = zlib.gzipSync(input);
            fs.writeFileSync(gzFilePath, output);
        });
        cb();
    };
}

// create byte arrays of gzipped files outputing .gz.h header files into webh directory
// these are the final processed version of the files, and are included in the firmware
function byteArray(source, destination, name, cb) {
    const arrayName = name.replace(/\.|-/g, "_");
    try {
        const data = fs.readFileSync(source);
        const wstream = fs.createWriteStream(destination);
        wstream.on('error', console.log);

        wstream.write(`#define ${arrayName}_len ${data.length}\n`);
        wstream.write(`const uint8_t ${arrayName}[] PROGMEM = {`);

        for (let i = 0; i < data.length; i++) {
            if (i % 1000 === 0) wstream.write('\n');
            wstream.write('0x' + data[i].toString(16).padStart(2, '0') + (i < data.length - 1 ? ',' : ''));
        }

        wstream.write('\n};');
        wstream.end(cb);
    } catch (error) {
        console.error('Error processing gzipped file:', error);
        cb(error);
    }
}

function scripts (){
    let type = 'js';
    gulp.series(
        minifyScripts(`${thDir}${type}/`, `${mnDir}${type}/`)/*,
        merge(`${mnDir}${type}/`, `${type}`, `${mnDir}/${type}/`, `${mn}.${type}`),
        gzip(`${mnDir}${type}/`, `${gzDir}${type}/`),
        byteArray(`${gzDir}${type}/${mn}.${type}.gz`, `${webhDir}/${mn}.${type}.gz.h`, `${mn}_${type}_gz`)*/
    );
};

/*
const styles = gulp.series(
    concat(),
    gzip(),
    byteArray(`${gzippedDir}css/required.css.gz`, `${webhDir}required.css.gz.h`, 'required_css_gz')
);

const fonts = gulp.series(
    gzip();
    byteArray();
);

const html = gulp.series(
    function htmlsPrep() {
        return gulp.src(`${srcDir}/esp-ui/*.htm*`)
            .pipe(htmlmin({ collapseWhitespace: true, minifyJS: true }))
            .pipe(gulp.dest(gzippedDir));
    },
    function htmls() {
);
*/

exports.default = scripts() /*gulp.parallel(scripts, styles, fonts, html)*/;