const fs = require('fs');
const path = require('path');

const srcDir = path.join(__dirname, '..', 'build', 'Release');
const distDir = path.join(__dirname, '..', 'dist');

if (!fs.existsSync(distDir)) {
    fs.mkdirSync(distDir, { recursive: true });
}

if (fs.existsSync(srcDir)) {
    const files = fs.readdirSync(srcDir);
    for (const file of files) {
        if (file.endsWith('.dll') || file.endsWith('.node')) {
            fs.copyFileSync(path.join(srcDir, file), path.join(distDir, file));
            console.log('Copied ' + file + ' to dist/');
        }
    }
}
