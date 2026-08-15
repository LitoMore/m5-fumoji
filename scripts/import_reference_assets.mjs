#!/usr/bin/env node

import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from 'node:fs'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptDirectory = dirname(fileURLToPath(import.meta.url))
const projectRoot = resolve(scriptDirectory, '..')

function usage() {
  console.error('Usage: node scripts/import_reference_assets.mjs <baye-fmj-app checkout>')
  console.error('   or: node scripts/import_reference_assets.mjs <fmj.lib.js> <font.js>')
  process.exit(2)
}

const inputs = process.argv.slice(2)
if (inputs.length < 1 || inputs.length > 2) usage()

let romJavaScript
let fontJavaScript
const first = resolve(inputs[0])
if (existsSync(first) && statSync(first).isDirectory()) {
  romJavaScript = join(first, 'Fmj/fmj_offline/js/libs/fmj.lib.js')
  fontJavaScript = join(first, 'Fmj/fmj_offline/js/font.js')
} else if (inputs.length === 2) {
  romJavaScript = first
  fontJavaScript = resolve(inputs[1])
} else {
  usage()
}

function extractHex(path, key) {
  if (!existsSync(path)) throw new Error(`Missing source file: ${path}`)
  const source = readFileSync(path, 'utf8')
  const escapedKey = key.replaceAll('.', '\\.')
  const expression = new RegExp(`fmj\\.rom\\["${escapedKey}"\\]\\s*=\\s*"([0-9a-fA-F]+)"`)
  const match = source.match(expression)
  if (!match) throw new Error(`Could not find ${key} in ${path}`)
  if ((match[1].length & 1) !== 0) throw new Error(`${key} has an odd hex length`)
  return Buffer.from(match[1], 'hex')
}

const outputDirectory = join(projectRoot, 'data')
mkdirSync(outputDirectory, { recursive: true })
for (const [name, sourcePath] of [
  ['FMJ.LIB', romJavaScript],
  ['HZK16', fontJavaScript],
  ['ASC16', fontJavaScript],
]) {
  const bytes = extractHex(sourcePath, name)
  writeFileSync(join(outputDirectory, name), bytes)
  console.log(`${name}: ${bytes.length} bytes`)
}

