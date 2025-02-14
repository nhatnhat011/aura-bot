"use strict";

const fs = require('fs/promises');
const path = require('path');
const execSync = require('child_process');

const deployFileNames = [
  'config-example.ini', 'greeting.txt', 'ip-to-country.csv',
  'LICENSE',
  ...fs.readdirSync(__dirname).filter(n => path.extname(n) === '.md')
];

const binaryFileNames = new Set(['aura.exe', 'aura']);
const manifestFileName = 'build.txt';

const buildBasePath = path.resolve(__dirname, 'dist');
const buildFolderNames = fs.readdirSync(buildBasePath);
const dynamicLibrariesBasePath = path.resolve(__dirname, 'dpp', 'official', 'dll');

const winFolderMappings = new Map([
  ['win32-compat', 'ReleaseLite'],
  ['win32-full', 'Release'],
  ['win64-compat', 'ReleaseLite-x64'],
  ['win64-full', 'Release-x64'],
]);

function padNumber(num) {
  if (num >= 10) return num;
  return `0${num}`;
}

function getDate() {
  let date = new Date();
  return [date.getUTCFullYear() % 100, date.getUTCMonth() + 1, date.getUTCDate()].map(padNumber);
}

async function main() {
  for (const folderName of buildBasePath) {
    const buildPath = path.resolve(buildBasePath, folderName);
    for (const fileName of fs.readdirSync(buildPath)) {
      if (binaryFileNames.has(fileName) || fileName === manifestFileName) {
        continue;
      }
      if (fs.statSync(path.resolve(buildPath, fileName)).isDirectory()) {
        continue;
      }
      try {
        fs.unlinkSync(path.resolve(buildPath, fileName));
      } catch (err) {}
    }

    // build.txt
    const manifestPath = path.resolve(buildPath, manifestFileName);
    const manifestContents = fs.readFileSync(manifestPath, 'utf8');
    const manifestParts = manifestContents.split('\n')[0].split(' ');
    manifestParts[4] = getDate();
    fs.writeFileSync(manifestPath, manifestParts.join(' ') + '\n');

    for (const fileName of deployFileNames) {
      fs.copySync(path.resolve(__dirname, fileName), path.resolve(buildPath, fileName));
    }

    let isWindows = winFolderMappings.has(folderName);
    let fromFolderName = winFolderMappings.get(folderName);
    if (!fromFolderName) {
      fromFolderName = linuxFolderMappings.get(folderName);
      if (!fromFolderName) throw new Error(`Unexpected folder ${folderName}`);
    }
    if (isWindows) {
      fs.copySync(
        path.resolve(__dirname, '.msvc', fromFolderName, 'aura.exe'),
        path.resolve(buildPath, 'aura.exe')
      );
      if (folderName.endsWith(`-full`)) {
        for (const dllFileName of path.resolve(dynamicLibrariesBasePath, fromFolderName)) {
          fs.copySync(
            path.resolve(dynamicLibrariesBasePath, fromFolderName, dllFileName),
            path.resolve(buildPath, dllFileName)
          );
        }
      }
    } else {
      console.log(`Update required at ${buildPath}: aura, *.so`);
    }
  }

  execSync(`git add -f dist/**/*`, {stdio: 'inherit'});
}

main();
