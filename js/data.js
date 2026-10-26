/* ==========================================================================
   RESTRICTED WEAR — data (Version 1.2)
   All prices are example prices in EUR until real pricing is finalized.
   ========================================================================== */
var RW = window.RW || {};

/* --------------------------------------------------------------------------
   Categories
   `key` is stored on products; `slug` is used in URLs (#/shop/<slug>).
   -------------------------------------------------------------------------- */
RW.CATEGORIES = [
  { key: "T-SHIRTS", slug: "t-shirts", subs: ["OVERSIZED", "SLIM", "CLASSIC"] },
  { key: "HOODIES", slug: "hoodies", subs: [] },
  { key: "ZIP HOODIES", slug: "zip-hoodies", subs: [] },
  { key: "JEANS", slug: "jeans", subs: ["LOOSE", "BAGGY", "BELL-BOTTOMS"] }
];

/* Shown as STILL DEVELOPING. No products exist in these yet. */
RW.FUTURE_CATEGORIES = [
  { key: "SNEAKERS", slug: "sneakers" },
  { key: "JEWELRY", slug: "jewelry" },
  { key: "BAGS", slug: "bags" },
  { key: "HEADWEAR", slug: "headwear" }
];

/* --------------------------------------------------------------------------
   Creative directions
   -------------------------------------------------------------------------- */
RW.DIRECTIONS = {
  MAINSTREAM: {
    key: "MAINSTREAM",
    slug: "mainstream",
    desc: "The primary direction of Restricted Wear, developed by its founder."
  },
  SECONDSTREAM: {
    key: "SECONDSTREAM",
    slug: "secondstream",
    desc: "The secondary direction. A freer relationship between colour and emotion."
  }
};

/* --------------------------------------------------------------------------
   Products — real Restricted Wear works.
   Names, directions and prices exactly as supplied by the designer.
   variants: colourways. Each has images [FRONT, BACK] in img/products/<id>/<colour>-<view>.png
             The first variant is the default (cards, cart thumbnails).
             Mainstream T-shirts are rebuilt on the master templates (tshirt_white/black_front/back).
             Secondstream keeps its own PNG mockups (exposure-normalised only).
   stream:   MAINSTREAM | SECONDSTREAM
   status:   AVAILABLE | RESTRICTED | LOW STOCK
             RESTRICTED pieces require an active Red Card to purchase.
   featured: shown on the Home page.
   audio:    optional { src, volume, loop }. Plays while the product page is open.
   -------------------------------------------------------------------------- */
RW.PRODUCTS = [
  {
    id: "posing-tonight",
    name: "posing tonight",
    stream: "MAINSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    sub: null,
    price: 299.99,
    status: "AVAILABLE",
    featured: true,
    desc: "Printed on the front and the back.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: {},
    variants: [
      { key: "white", label: "WHITE", images: ["img/products/posing-tonight/white-front.png", "img/products/posing-tonight/white-back.png"] }
    ],
    imageLabels: ["FRONT", "BACK"],
    /* Plays while this product's page is open. volume: 0 to 1 (0.5 = medium). */
    audio: { src: "audio/posing-tonight.mp3", volume: 0.5 }
  },
  {
    id: "jessie-jessie-luxury",
    name: "jessie + jessie luxury",        /* Frame 44 */
    stream: "MAINSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    sub: null,
    price: 399.99,
    status: "AVAILABLE",
    featured: true,
    desc: "Printed on the front, plain on the back.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: {},
    variants: [
      { key: "burgundy", label: "BURGUNDY", images: ["img/products/jessie-jessie-luxury/burgundy-front.png", "img/products/jessie-jessie-luxury/burgundy-back.png"] },
      { key: "white", label: "WHITE", images: ["img/products/jessie-jessie-luxury/white-front.png", "img/products/jessie-jessie-luxury/white-back.png"] }
    ],
    imageLabels: ["FRONT", "BACK"]
  },
  {
    id: "paris-hilton",
    name: "paris hilton",                  /* Frame 45 */
    stream: "MAINSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    sub: null,
    price: 299.99,
    status: "AVAILABLE",
    featured: true,
    desc: "Printed on the front and the back.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: {},
    variants: [
      { key: "white", label: "WHITE", images: ["img/products/paris-hilton/white-front.png", "img/products/paris-hilton/white-back.png"] }
    ],
    imageLabels: ["FRONT", "BACK"],
    /* Plays while this product's page is open. volume: 0 to 1 (0.5 = medium). */
    audio: { src: "audio/paris-hilton.mp3", volume: 0.5 }
  },
  {
    id: "restricted-wear-erd",
    name: "restricted wear erd",           /* Frame 46 */
    stream: "MAINSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    sub: null,
    price: 249.99,
    status: "AVAILABLE",
    featured: false,
    desc: "Printed on the front and the back.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: {},
    variants: [
      { key: "white", label: "WHITE", images: ["img/products/restricted-wear-erd/white-front.png", "img/products/restricted-wear-erd/white-back.png"] },
      { key: "black", label: "BLACK", images: ["img/products/restricted-wear-erd/black-front.png", "img/products/restricted-wear-erd/black-back.png"] }
    ],
    imageLabels: ["FRONT", "BACK"]
  },
  {
    id: "alex-g7",
    name: "alex g7#",                      /* Frame 47 */
    stream: "SECONDSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    sub: null,
    price: 249.99,
    status: "AVAILABLE",
    featured: true,
    desc: "White T-shirt. Printed on the front, plain on the back.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: {},
    variants: [
      { key: "white", label: "WHITE", images: ["img/products/alex-g7/white-front.png", "img/products/alex-g7/white-back.png"] }
    ],
    imageLabels: ["FRONT", "BACK"]
  },
  {
    id: "jagermejsterholic",
    name: "jagermejsterholic",             /* Frame 48 */
    stream: "SECONDSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    sub: null,
    price: 449.99,
    status: "AVAILABLE",
    featured: false,
    desc: "White T-shirt. Printed on the front and on the sleeve, plain on the back.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: {},
    variants: [
      { key: "white", label: "WHITE", images: ["img/products/jagermejsterholic/white-front.png", "img/products/jagermejsterholic/white-back.png"] }
    ],
    imageLabels: ["FRONT", "BACK"]
  }
];

/* Product ids from earlier versions — keeps saved bags working where possible. */
RW.LEGACY_IDS = {
  "jessie-luxury-tee": "jessie-jessie-luxury"
};

/* --------------------------------------------------------------------------
   Editorial imagery (optional). Set paths when photography is available.
   -------------------------------------------------------------------------- */
RW.EDITORIAL = {
  lead: null,
  mainstream: null,
  secondstream: null
};

/* --------------------------------------------------------------------------
   Archive
   -------------------------------------------------------------------------- */
RW.ARCHIVE = [
  {
    label: "DIRECTIONS",
    title: "Mainstream and Secondstream.",
    body: "The work is divided into two directions. They are presented separately and belong to the same project."
  },
  {
    label: "RED CARD",
    title: "The Red Card is introduced.",
    body: "Purchasing is tied to an account. Selected pieces also require an active Red Card."
  },
  {
    label: "SIGNATURE",
    title: "A signature print, left off-register.",
    body: "Two marks printed slightly out of alignment on purpose. The effect was kept."
  },
  {
    label: "REISSUE",
    title: "A reference sample returns.",
    body: "The zip hoodie is reissued from an internal sample, with raw cuffs and a two-way zip. No earlier release date is recorded."
  }
];

/* --------------------------------------------------------------------------
   Red Card identifier
   94 digits: [4-digit prefix] + [90 random digits]. Digits only.
   With a small probability the prefix itself is random.
   -------------------------------------------------------------------------- */
RW.REDCARD = {
  LENGTH: 94,
  PREFIXES: ["2094", "8888", "6666", "4546", "9999"],
  RANDOM_PREFIX_CHANCE: 0.08
};

/* --------------------------------------------------------------------------
   Brand / contact
   -------------------------------------------------------------------------- */
RW.BRAND = {
  name: "RESTRICTED WEAR",
  businessEmail: "restrictedwearofficial@gmail.com",
  redCardEmail: "redcardocsed@gmail.com",
  telegram: "@restrictedwear",
  telegramUrl: "https://t.me/restrictedwear",
  currency: "EUR"
};
