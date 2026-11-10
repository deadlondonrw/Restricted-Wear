/* ==========================================================================
   RESTRICTED WEAR — design tokens (website css/style.css)
   Token names follow the CSS custom properties:

     --bg        -> bg          --line       -> line
     --fg        -> fg          --line-soft  -> lineSoft
     --surface   -> surface     --dim        -> dim
     --surface-2 -> surface2    --dim-2      -> dim2
     --accent    -> accent      --on-accent  -> onAccent
     --photo-bg  -> photoBg     --redcard    -> redcard
     --overlay   -> overlay

   Default mode is white / black / #7C0000. Inverted mode is black / white
   with the lifted oxblood accent the website uses on black.
   ========================================================================== */

export type ThemeName = 'default' | 'inverted';

export type Palette = {
  bg: string;
  fg: string;
  /** Image placeholders, form fields. */
  surface: string;
  surface2: string;
  line: string;
  lineSoft: string;
  dim: string;
  dim2: string;
  /** Brand accent — used sparingly. */
  accent: string;
  onAccent: string;
  /** Product photographs always sit on a warm off-white, in both modes. */
  photoBg: string;
  /** The Red Card document keeps its own colour in both modes. */
  redcard: string;
  overlay: string;
};

export type ColorName = keyof Palette;

export const palettes: Record<ThemeName, Palette> = {
  // :root
  default: {
    bg: '#ffffff',
    fg: '#000000',
    surface: '#f3f3f2',
    surface2: '#e9e9e8',
    line: '#d6d6d4',
    lineSoft: '#e8e8e6',
    dim: '#5e5e5c',
    dim2: '#8d8d8a',
    accent: '#7C0000',
    onAccent: '#ffffff',
    photoBg: '#F4F0E8',
    redcard: '#7C0000',
    overlay: 'rgba(0,0,0,0.32)',
  },
  // html[data-theme="inverted"]
  inverted: {
    bg: '#000000',
    fg: '#ffffff',
    surface: '#0e0e0e',
    surface2: '#171717',
    line: '#2e2e2e',
    lineSoft: '#1c1c1c',
    dim: '#a3a3a0',
    dim2: '#6e6e6b',
    accent: '#AE2424',
    onAccent: '#ffffff',
    photoBg: '#F4F0E8',
    redcard: '#7C0000',
    overlay: 'rgba(0,0,0,0.6)',
  },
};

/** Fixed inks on the photo background — the same in both modes (.pd-view-label, .zv-*). */
export const photoInk = {
  text: '#000000',
  label: '#5e5e5c',
  faint: '#8d8d8a',
} as const;

/** The website sets everything in Roboto Condensed. Loaded in src/app/_layout.tsx. */
export const fonts = {
  regular: 'RobotoCondensed_400Regular',
  medium: 'RobotoCondensed_500Medium',
  bold: 'RobotoCondensed_700Bold',
} as const;

export type Weight = keyof typeof fonts;

/** Website breakpoints (max-width media queries in css/style.css). */
export const breakpoints = {
  /** .nav-right .account-link hidden, footer 1 column */
  xs: 480,
  /** .grid 1 column, .field-row stacked */
  grid1: 520,
  /** --gutter 18px, --nav-h 52px */
  compact: 720,
  /** .home-directions 1 column, .home-lead 4:5 */
  directions: 760,
  /** footer 2 columns */
  foot2: 860,
  /** .pd, .auth-grid, .account-grid 1 column */
  stack: 900,
  /** .nav-links hidden, burger shown */
  burger: 960,
  /** .grid 2 columns */
  grid2: 980,
  /** --max-w */
  max: 1360,
} as const;

/** Motion — durations from the website's transitions. */
export const motion = {
  /** .btn / .filter-chip colour transition */
  press: 180,
  /** .page { animation: pagein .35s ease } */
  pageIn: 350,
  /** .card:hover .ph { transform: scale(1.03) } */
  mediaZoom: 700,
  /** #toast transition */
  toast: 250,
  /** RW.toast timeout */
  toastHold: 2400,
  /** #cart-drawer transform */
  drawer: 300,
} as const;
