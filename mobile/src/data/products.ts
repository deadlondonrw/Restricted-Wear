/* ==========================================================================
   Catalog queries — the website's RW.findProduct / RW.variantOf /
   RW.filteredProducts. The data itself is generated from the website by
   scripts/sync-site.mjs (see catalog.generated.ts).
   ========================================================================== */
import { CATEGORIES, DIRECTIONS, FUTURE_CATEGORIES, LEGACY_IDS, PRODUCTS } from './catalog.generated';
import type { CategoryInfo, DirectionInfo, Gender, Product, ProductVariant, Stream } from './types';

export { ARCHIVE, BRAND, CATEGORIES, DIRECTIONS, FUTURE_CATEGORIES, LEGACY_IDS, PRODUCTS, REDCARD } from './catalog.generated';

export const STREAMS: Stream[] = ['MAINSTREAM', 'SECONDSTREAM'];

export function findProduct(id: string | undefined): Product | undefined {
  if (!id) return undefined;
  const resolved = LEGACY_IDS[id] ?? id;
  return PRODUCTS.find((p) => p.id === resolved);
}

export function isRestricted(p: Product | undefined): boolean {
  return !!p && p.status === 'RESTRICTED';
}

/** RW.variantOf — the requested colourway, or the default one. */
export function variantOf(p: Product, key?: string | null): ProductVariant {
  return p.variants.find((v) => v.key === (key ?? '')) ?? p.variants[0];
}

export function directionBySlug(slug: string | undefined): DirectionInfo | undefined {
  return STREAMS.map((s) => DIRECTIONS[s]).find((d) => d.slug === slug);
}

export function categoryByKey(key: string): CategoryInfo | undefined {
  return CATEGORIES.find((c) => c.key === key);
}

export function categoryBySlug(slug: string): { key: string; slug: string } | undefined {
  return [...CATEGORIES, ...FUTURE_CATEGORIES].find((c) => c.slug === slug);
}

export function isFutureCategory(key: string): boolean {
  return FUTURE_CATEGORIES.some((c) => c.key === key);
}

export function featuredProducts(limit = 3): Product[] {
  return PRODUCTS.filter((p) => p.featured).slice(0, limit);
}

export function productsInStream(stream: Stream): Product[] {
  return PRODUCTS.filter((p) => p.stream === stream);
}

/** Categories with at least one piece in the given list, in catalog order. */
export function categoriesIn(products: Product[]): CategoryInfo[] {
  return CATEGORIES.filter((c) => products.some((p) => p.category === c.key));
}

/** Genders other than ALL used in the catalog — the chips only appear when there are any. */
export function genderList(): Exclude<Gender, 'ALL'>[] {
  const found = new Set<Exclude<Gender, 'ALL'>>();
  PRODUCTS.forEach((p) => {
    if (p.gender !== 'ALL') found.add(p.gender);
  });
  return [...found];
}

/* --------------------------------------------------------------------------
   Shop filters — RW.state.filters / RW.filteredProducts
   -------------------------------------------------------------------------- */
export type ShopSort = 'DEFAULT' | 'PRICE_LOW' | 'PRICE_HIGH' | 'NAME';

export const SORT_LABELS: Record<ShopSort, string> = {
  DEFAULT: 'SORT',
  PRICE_LOW: 'PRICE: LOW TO HIGH',
  PRICE_HIGH: 'PRICE: HIGH TO LOW',
  NAME: 'NAME: A–Z',
};

export type ShopFilter = {
  gender: Gender;
  category: string;
  sub: string;
  stream: Stream | 'ALL';
  sort: ShopSort;
  query: string;
};

export const DEFAULT_SHOP_FILTER: ShopFilter = {
  gender: 'ALL',
  category: 'ALL',
  sub: 'ALL',
  stream: 'ALL',
  sort: 'DEFAULT',
  query: '',
};

export function filterProducts(f: ShopFilter): Product[] {
  let list = PRODUCTS.slice();
  if (f.gender !== 'ALL') list = list.filter((p) => p.gender === f.gender || p.gender === 'ALL');
  if (f.category !== 'ALL') list = list.filter((p) => p.category === f.category);
  if (f.sub !== 'ALL') list = list.filter((p) => p.sub === f.sub);
  if (f.stream !== 'ALL') list = list.filter((p) => p.stream === f.stream);

  const q = f.query.trim().toLowerCase();
  if (q) {
    list = list.filter((p) =>
      [p.name, p.category, p.sub ?? '', p.stream, p.desc].join(' ').toLowerCase().includes(q),
    );
  }

  if (f.sort === 'PRICE_LOW') list.sort((a, b) => a.price - b.price);
  else if (f.sort === 'PRICE_HIGH') list.sort((a, b) => b.price - a.price);
  else if (f.sort === 'NAME') list.sort((a, b) => a.name.localeCompare(b.name));
  return list;
}
