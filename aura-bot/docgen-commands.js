"use strict";

const assert = require('assert');
const fs = require('fs/promises');
const util = require('util');
const path = require('path');

const OUTPUT_PATH = `COMMANDS.md`;
const COMMAND_FILES = ['src/command.cpp'];
const aliasRegExp = /case HashCode\("([a-zA-Z0-9]+)"\):/;
const mainCmdRegExp = /case HashCode\("([a-zA-Z0-9]+)"\): \{/;
const usageRegExp = /"Usage: " \+ cmdToken \+ "([^"]+)"\);/;

async function main() {
  const aliases = new Map();
  const commandUsages = new Map();
  const seenCommands = new Set();
  for (const fileName of COMMAND_FILES) {
    const filePath = path.resolve(__dirname, fileName);
    const fileContent = await fs.readFile(filePath, 'utf8');
    let currentCommandName = '';
    let currentCommandAliases = [];
    for (const line of fileContent.split(/\r?\n/g)) {
      const trimmed = line.trim();
      let cmdMatch = mainCmdRegExp.exec(trimmed);
      if (cmdMatch) {
        if (seenCommands.has(cmdMatch[1])) {
          console.error(`Duplicate command ${cmdMatch[1]}`);
        }
        currentCommandName = cmdMatch[1];
        seenCommands.add(currentCommandName);
        aliases.set(currentCommandName, currentCommandAliases.slice());
        currentCommandAliases.length = 0;
        continue;
      }
      let aliasMatch = aliasRegExp.exec(trimmed);
      if (aliasMatch) {
        currentCommandName = '';
        currentCommandAliases.push(aliasMatch[1]);
        continue;
      }
      let usageMatch = usageRegExp.exec(trimmed);
      if (usageMatch && currentCommandName) {
        if (!usageMatch[0].includes(currentCommandName)) {
          console.error(`Usage text does not match command name for ${currentCommandName}`);
          continue;
        }
        if (!commandUsages.has(currentCommandName)) {
          commandUsages.set(currentCommandName, new Set());
        }
        commandUsages.get(currentCommandName).add(usageMatch[1]);
      }
    }
  }

  const outContents = [
    `Commands`,
    `==========`,
    `# Available commands`,
  ];
  const commandList = Array.from(seenCommands).sort();
  for (const mainCmd of commandList) {
    outContents.push('## \\`' + mainCmd + '\\`');
    if (aliases.get(mainCmd)?.length) {
      outContents.push(`- Aliases: ${aliases.get(mainCmd).join(', ')}`);
    }
    if (commandUsages.has(mainCmd)) for (const usage of commandUsages.get(mainCmd)) {
      outContents.push(`- Syntax: ${usage}`);
    }
    outContents.push(``);
  }

  await fs.writeFile(
    path.resolve(__dirname, OUTPUT_PATH),
    outContents.join(`\n`).replace(/([<>])/g, '\\$1'),
  );
}

main();
