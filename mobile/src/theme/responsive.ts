import { useWindowDimensions } from 'react-native';

import { METRIC_CHARS, METRIC_WIDTHS } from './metrics.generated';
import { breakpoints, type Weight } from './tokens';

/* ==========================================================================
   Responsive layout and one-line text fitting.

   Layout follows the website's own breakpoints, so a phone, a large phone, a
   tablet and a desktop window each get the layout the website gives that
   width.

   Labels never wrap onto a second line and never leave the screen: the width
   of a string is calculated from Roboto Condensed's real glyph widths, and
   the font size is reduced just enough for the whole string to fit the width
   it has.
   ========================================================================== */

const INDEX: Record<string, number> = {};
for (let i = 0; i < METRIC_CHARS.length; i += 1) INDEX[METRIC_CHARS[i]] = i;
/** Width used for a glyph outside the table (wide enough to be safe). */
const FALLBACK = 640;

/** Width of `text` in em, including letter-spacing (em per glyph). */
export function measureEm(text: string, weight: Weight = 'regular', tracking = 0): number {
  const table = METRIC_WIDTHS[weight];
  let units = 0;
  let glyphs = 0;
  for (const ch of text) {
    const i = INDEX[ch];
    units += i === undefined ? FALLBACK : table[i];
    glyphs += 1;
  }
  return units / 1000 + glyphs * tracking;
}

/** Headroom for rendering differences between platforms. */
const FIT_SAFETY = 0.965;
/** Nothing is ever set smaller than this. */
export const FIT_FLOOR = 7;

/**
 * Largest font size ≤ `max` at which the whole of `text` fits on ONE line of
 * `width` px, taking the system text-size setting into account.
 */
export function fitOneLine({
  text,
  max,
  width,
  weight = 'regular',
  tracking = 0,
  fontScale = 1,
  maxScale = 1,
}: {
  text: string;
  max: number;
  width: number;
  weight?: Weight;
  tracking?: number;
  fontScale?: number;
  maxScale?: number;
}): number {
  const em = measureEm(text, weight, tracking);
  if (em <= 0 || width <= 0) return max;
  const scale = Math.max(1, Math.min(fontScale, maxScale));
  const fitted = (width * FIT_SAFETY) / (em * scale);
  if (fitted >= max) return max;
  return Math.max(FIT_FLOOR, Math.floor(fitted * 4) / 4);
}

const clamp = (n: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, n));

export type Layout = ReturnType<typeof useLayout>;

/**
 * Window-derived layout values, mirroring css/style.css.
 *   gutter    --gutter (18px up to 720px wide, 32px above)
 *   gutterLg  --gutter-lg
 *   navH      --nav-h
 *   frame     .wrap outer width (max 1360px, centred)
 *   content   .wrap inner width — what a full-width label actually has
 *   columns   .grid columns (1 / 2 / 3)
 *   vw()      CSS clamp(min, n vw, max)
 */
export function useLayout() {
  const { width, height, fontScale } = useWindowDimensions();
  const compact = width <= breakpoints.compact;
  const gutter = compact ? 18 : 32;
  const gutterLg = compact ? 32 : 64;
  const navH = compact ? 52 : 56;
  const frame = Math.min(width, breakpoints.max);
  const content = frame - gutter * 2;
  const columns = width <= breakpoints.grid1 ? 1 : width <= breakpoints.grid2 ? 2 : 3;
  const vw = (min: number, percent: number, max: number) =>
    Math.round(clamp((width * percent) / 100, min, max) * 2) / 2;
  return {
    width,
    height,
    fontScale,
    compact,
    gutter,
    gutterLg,
    navH,
    frame,
    content,
    columns,
    vw,
    /** .pd / .auth-grid / .account-grid collapse to one column */
    stacked: width <= breakpoints.stack,
    /** burger menu instead of the inline nav links */
    burger: width <= breakpoints.burger,
  };
}
