#!/usr/bin/env node
/* ==========================================================================
   RESTRICTED WEAR — site → app sync

   Reads the website at the repository root (js/data.js, img/, audio/) and
   regenerates everything the app takes from it:

     assets/products/<id>/<colour>-<view>.webp        product images (alpha kept)
     assets/products/<id>/<colour>-<view>-zoom.webp   larger files for the viewer
     assets/audio/*.mp3                               product sound
     assets/brand/*                                   R mark and wordmark
     assets/icon.png, adaptive icons, splash, favicon
     src/data/catalog.generated.ts                    catalog, archive, brand

   Run after the website changes:   npm run sync-site
   Requires ImageMagick (`magick` or `convert`) on the PATH.
   ========================================================================== */
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const APP = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SITE = path.resolve(APP, '..');
const ASSETS = path.join(APP, 'assets');

function findMagick() {
  for (const bin of ['magick', 'convert']) {
    try {
      execFileSync(bin, ['-version'], { stdio: 'ignore' });
      return bin;
    } catch {
      /* try the next name */
    }
  }
  throw new Error('ImageMagick is required (magick or convert was not found on the PATH).');
}
const MAGICK = findMagick();
const im = (args) => execFileSync(MAGICK, args, { stdio: ['ignore', 'ignore', 'inherit'] });
const mkdir = (dir) => fs.mkdirSync(dir, { recursive: true });
const posix = (p) => p.split(path.sep).join('/');

/* --------------------------------------------------------------------------
   Website data
   -------------------------------------------------------------------------- */
const sandbox = { window: {} };
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(path.join(SITE, 'js', 'data.js'), 'utf8') + '\n;window.RW = RW;', sandbox);
const RW = sandbox.window.RW;
if (!RW || !Array.isArray(RW.PRODUCTS)) throw new Error('js/data.js did not define RW.PRODUCTS');

/* --------------------------------------------------------------------------
   Products — images, zoom files, sound
   -------------------------------------------------------------------------- */
const requires = [];
const req = (file) => `require('${posix(path.relative(path.join(APP, 'src', 'data'), file))}')`;

fs.rmSync(path.join(ASSETS, 'products'), { recursive: true, force: true });
fs.rmSync(path.join(ASSETS, 'audio'), { recursive: true, force: true });
mkdir(path.join(ASSETS, 'audio'));

const products = RW.PRODUCTS.map((p) => {
  const dir = path.join(ASSETS, 'products', p.id);
  mkdir(dir);
  const labels = p.imageLabels || [];
  const sourceVariants = p.variants && p.variants.length ? p.variants : [{ key: '', label: null, images: p.images || [] }];

  const variants = sourceVariants.map((v) => {
    const images = (v.images || []).map((src, i) => {
      const view = (labels[i] || `VIEW ${i + 1}`).toUpperCase();
      const base = `${v.key || 'default'}-${view.toLowerCase().replace(/[^a-z0-9]+/g, '-')}`;
      const out = path.join(dir, `${base}.webp`);
      /* 1200×1600 at most, transparency kept: the garment sits on --photo-bg */
      im([path.join(SITE, src), '-resize', '1200x1600>', '-quality', '88', '-define', 'webp:alpha-quality=95', '-define', 'webp:method=6', out]);
      let zoom = null;
      if (v.zoom && v.zoom[i]) {
        const zOut = path.join(dir, `${base}-zoom${path.extname(v.zoom[i])}`);
        fs.copyFileSync(path.join(SITE, v.zoom[i]), zOut);
        zoom = req(zOut);
      }
      return { view, source: req(out), zoom };
    });
    return { key: v.key || '', label: v.label || null, images };
  });

  let audio = null;
  if (p.audio && p.audio.src) {
    const out = path.join(ASSETS, 'audio', path.basename(p.audio.src));
    fs.copyFileSync(path.join(SITE, p.audio.src), out);
    audio = { source: req(out), volume: typeof p.audio.volume === 'number' ? p.audio.volume : 0.5, loop: !!p.audio.loop };
  }

  return {
    id: p.id,
    name: p.name,
    stream: p.stream,
    gender: p.gender,
    category: p.category,
    sub: p.sub || null,
    price: p.price,
    status: p.status,
    featured: !!p.featured,
    desc: p.desc,
    sizes: p.sizes || {},
    sizeOrder: Object.keys(p.sizes || {}),
    specs: p.specs || {},
    variants,
    audio,
  };
});

/* --------------------------------------------------------------------------
   Brand marks
   The wordmark PNG ships on a #D9D9D9 plate. The website remaps it with two
   SVG colour matrices (index.html); the same matrices are applied here.
     default  — plate -> white, black -> black, #7C0000 -> #7C0000
     inverted — plate -> black, black -> white, #7C0000 -> #AE2424
   -------------------------------------------------------------------------- */
const BRAND_DIR = path.join(ASSETS, 'brand');
mkdir(BRAND_DIR);
fs.copyFileSync(path.join(SITE, 'img', 'rw-mark.png'), path.join(BRAND_DIR, 'mark.png'));

const WORDMARK = path.join(SITE, 'img', 'rw-wordmark.png');
const matrix = (rows) => rows.map((r) => r.join(' ')).join(' ');
im([
  WORDMARK, '-resize', '1410x',
  '-color-matrix', matrix([
    [1, 0.08756, 0.08756, 0, 0, 0],
    [0, 0.58756, 0.58756, 0, 0, 0],
    [0, 0.58756, 0.58756, 0, 0, 0],
    [0, 0, 0, 1, 0, 0],
    [0, 0, 0, 0, 1, 0],
    [0, 0, 0, 0, 0, 1],
  ]),
  '-strip', path.join(BRAND_DIR, 'wordmark-default.png'),
]);
im([
  WORDMARK, '-resize', '1410x',
  '-color-matrix', matrix([
    [-0.6532, -0.26096, -0.26096, 0, 0, 1],
    [-1.7661, 0.29549, 0.29549, 0, 0, 1],
    [-1.7661, 0.29549, 0.29549, 0, 0, 1],
    [0, 0, 0, 1, 0, 0],
    [0, 0, 0, 0, 1, 0],
    [0, 0, 0, 0, 0, 1],
  ]),
  '-strip', path.join(BRAND_DIR, 'wordmark-inverted.png'),
]);

/* App icons from the website icon (dark red R on transparent). */
const ICON = path.join(SITE, 'img', 'icons', 'icon-512.png');
im([ICON, '-resize', '1024x1024', '-background', 'white', '-gravity', 'center', '-extent', '1024x1024', '-flatten', '-strip', path.join(ASSETS, 'icon.png')]);
im([ICON, '-resize', '620x620', '-background', 'none', '-gravity', 'center', '-extent', '1024x1024', '-strip', path.join(ASSETS, 'adaptive-icon.png')]);
im([path.join(SITE, 'img', 'rw-mark.png'), '-resize', '620x620', '-background', 'none', '-gravity', 'center', '-extent', '1024x1024', '-strip', path.join(ASSETS, 'adaptive-icon-monochrome.png')]);
im([ICON, '-resize', '512x512', '-strip', path.join(ASSETS, 'splash-icon.png')]);
im([ICON, '-resize', '48x48', '-strip', path.join(ASSETS, 'favicon.png')]);

/* --------------------------------------------------------------------------
   Generated catalog module
   -------------------------------------------------------------------------- */
const lit = (value, indent = 0) => {
  const pad = '  '.repeat(indent);
  const inner = '  '.repeat(indent + 1);
  if (value === null || value === undefined) return 'null';
  if (typeof value === 'string') return value.startsWith("require('") ? value : JSON.stringify(value);
  if (typeof value !== 'object') return String(value);
  if (Array.isArray(value)) {
    if (!value.length) return '[]';
    if (value.every((v) => typeof v !== 'object' || v === null)) return `[${value.map((v) => lit(v)).join(', ')}]`;
    return `[\n${value.map((v) => inner + lit(v, indent + 1)).join(',\n')},\n${pad}]`;
  }
  const keys = Object.keys(value);
  if (!keys.length) return '{}';
  const key = (k) => (/^[A-Za-z_$][\w$]*$/.test(k) ? k : JSON.stringify(k));
  const flat = keys.every((k) => typeof value[k] !== 'object' || value[k] === null);
  if (flat && keys.length <= 6) return `{ ${keys.map((k) => `${key(k)}: ${lit(value[k])}`).join(', ')} }`;
  return `{\n${keys.map((k) => `${inner}${key(k)}: ${lit(value[k], indent + 1)}`).join(',\n')},\n${pad}}`;
};

const out = `/* ==========================================================================
   GENERATED by scripts/sync-site.mjs from the website (js/data.js).
   Do not edit by hand — change the website data and run:  npm run sync-site
   ========================================================================== */
import type { ArchiveEntry, CategoryInfo, DirectionInfo, FutureCategoryInfo, Product, Stream } from './types';

export const CATEGORIES: CategoryInfo[] = ${lit(RW.CATEGORIES)};

/** Shown as STILL DEVELOPING. No products exist in these yet. */
export const FUTURE_CATEGORIES: FutureCategoryInfo[] = ${lit(RW.FUTURE_CATEGORIES)};

export const DIRECTIONS: Record<Stream, DirectionInfo> = ${lit(RW.DIRECTIONS)};

export const PRODUCTS: Product[] = ${lit(products)};

/** Product ids from earlier versions — keeps saved bags working where possible. */
export const LEGACY_IDS: Record<string, string> = ${lit(RW.LEGACY_IDS || {})};

export const ARCHIVE: ArchiveEntry[] = ${lit(RW.ARCHIVE)};

/** Red Card identifier: [4-digit prefix] + random digits, ${RW.REDCARD.LENGTH} digits in all. */
export const REDCARD = ${lit({ LENGTH: RW.REDCARD.LENGTH, PREFIXES: RW.REDCARD.PREFIXES, RANDOM_PREFIX_CHANCE: RW.REDCARD.RANDOM_PREFIX_CHANCE })};

export const BRAND = ${lit(RW.BRAND)};
`;
fs.writeFileSync(path.join(APP, 'src', 'data', 'catalog.generated.ts'), out);

const count = products.reduce((n, p) => n + p.variants.reduce((m, v) => m + v.images.length, 0), 0);
console.log(`Synced ${products.length} products, ${count} images, ${products.filter((p) => p.audio).length} tracks.`);
