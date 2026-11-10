import type { ImageSourcePropType } from 'react-native';

/** RW.DIRECTIONS — the two creative directions. */
export type Stream = 'MAINSTREAM' | 'SECONDSTREAM';

/** "ALL" on a product means it is cut for everyone. */
export type Gender = 'ALL' | 'MEN' | 'WOMEN';

/** AVAILABLE | RESTRICTED | LOW STOCK. RESTRICTED pieces require an active Red Card. */
export type ProductStatus = 'AVAILABLE' | 'RESTRICTED' | 'LOW STOCK';

export type CategoryInfo = { key: string; slug: string; subs: string[] };
export type FutureCategoryInfo = { key: string; slug: string };
export type DirectionInfo = { key: Stream; slug: string; desc: string };

export type ProductImage = {
  /** FRONT / BACK — the website's imageLabels. */
  view: string;
  source: ImageSourcePropType;
  /** Larger file used only by the zoom viewer, when the website has one. */
  zoom: ImageSourcePropType | null;
};

/** A colourway. The first variant is the default (cards, bag thumbnails). */
export type ProductVariant = {
  key: string;
  label: string | null;
  images: ProductImage[];
};

/** Plays while the product page is open. volume: 0 to 1. */
export type ProductAudio = {
  source: number;
  volume: number;
  loop: boolean;
};

export type Product = {
  id: string;
  name: string;
  stream: Stream;
  gender: Gender;
  category: string;
  sub: string | null;
  /** EUR — money() prints "€ 399.99". */
  price: number;
  status: ProductStatus;
  /** Shown on Home under SELECTED. */
  featured: boolean;
  desc: string;
  /** true = size in stock. T-shirts XS–XL, jeans by waist in inches. */
  sizes: Record<string, boolean>;
  /** Display order of the sizes. */
  sizeOrder: string[];
  specs: Record<string, string>;
  variants: ProductVariant[];
  audio: ProductAudio | null;
};

export type ArchiveEntry = {
  label: string;
  title: string;
  body: string;
};
