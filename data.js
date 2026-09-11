/* ==========================================================================
   RESTRICTED WEAR — data
   ========================================================================== */
var RW = window.RW || {};

RW.PRODUCTS = [
  {
    id: "jessie-luxury-tee",
    name: "JESSIE LUXURY TEE",
    code: "RW-01",
    stream: "MAINSTREAM",
    gender: "ALL",
    category: "T-SHIRTS",
    price: 185,
    status: "RESTRICTED",
    desc: "A double signature run. Two archive marks printed off-register on a heavyweight base, left to fade unevenly by design.",
    sizes: { XS: true, S: true, M: true, L: false, XL: true },
    specs: { MATERIAL: "220GSM COTTON", WEIGHT: "HEAVY", FIT: "BOXED", "ITEM CODE": "RW-01" }
  },
  {
    id: "restricted-tee-01",
    name: "BASE TEE 01",
    code: "RW-02",
    stream: "MAINSTREAM",
    gender: "MEN",
    category: "T-SHIRTS",
    price: 120,
    status: "AVAILABLE",
    desc: "The base garment. Minimal branding, one small woven label at the hem stating what it is and what it is not.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: { MATERIAL: "200GSM COTTON", WEIGHT: "MID", FIT: "REGULAR", "ITEM CODE": "RW-02" }
  },
  {
    id: "authorization-hoodie",
    name: "AUTHORIZATION HOODIE",
    code: "RW-03",
    stream: "MAINSTREAM",
    gender: "ALL",
    category: "HOODIES",
    price: 340,
    status: "RESTRICTED",
    desc: "Interior pocket sized for a single card. Built for the Red Card authorization system.",
    sizes: { XS: false, S: true, M: true, L: true, XL: false },
    specs: { MATERIAL: "480GSM FLEECE", WEIGHT: "HEAVY", FIT: "OVERSIZED", "ITEM CODE": "RW-03" }
  },
  {
    id: "baggy-fit-jeans",
    name: "BAGGY FIT JEANS",
    code: "RW-04",
    stream: "SECONDSTREAM",
    gender: "WOMEN",
    category: "JEANS",
    price: 410,
    status: "RESTRICTED",
    desc: "Exaggerated proportions with structured denim holding the silhouette away from the leg. Restrained red stitching on the inseam.",
    sizes: { XS: true, S: true, M: true, L: true, XL: false },
    specs: { MATERIAL: "14OZ DENIM", WEIGHT: "HEAVY", FIT: "BAGGY", "ITEM CODE": "RW-04" }
  },
  {
    id: "archive-zip-hoodie",
    name: "ARCHIVE ZIP HOODIE",
    code: "RW-05",
    stream: "SECONDSTREAM",
    gender: "MEN",
    category: "HOODIES",
    price: 365,
    status: "LOW STOCK",
    desc: "Reissued from an internal reference sample. Cuffs are left raw. Two-way hardware zip.",
    sizes: { XS: true, S: false, M: true, L: true, XL: true },
    specs: { MATERIAL: "500GSM FLEECE", WEIGHT: "HEAVY", FIT: "SLIM", "ITEM CODE": "RW-05" }
  }
];

RW.ARCHIVE = [
  {
    date: "INTERNAL RECORD",
    title: "The Authoric Document is formalized.",
    body: "What began as an informal request became the Red Card system. Access is assigned to specific garments rather than uniformly across the collection."
  },
  {
    date: "SIGNATURE RUN",
    title: "A signature run, deliberately off-register.",
    body: "Two marks, printed slightly out of alignment on purpose. Internal notes refer to it as 'the luxury problem' without further comment."
  },
  {
    date: "REFERENCE ARCHIVE",
    title: "A reference sample re-enters circulation.",
    body: "No first release date has been located for this specific garment wash. It is catalogued as a reissue on the assumption that an original exists in the vault."
  },
  {
    date: "STATUS: ACTIVE",
    title: "The authorization system remains active.",
    body: "Most unverified purchase requests are declined immediately. This restriction is treated internally as an uncompromisable standard for the brand."
  }
];

RW.BRAND = {
  name: "RESTRICTED WEAR",
  tagline: "ACCESS IS NOT GUARANTEED.",
  code: "RESTRICTED",
  contactDept: "RED CARD",
  email: "redcardocsed@gmail.com"
};