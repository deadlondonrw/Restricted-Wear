/* ==========================================================================
   RESTRICTED WEAR — data
   All catalog + archive content lives here so it can be edited without
   touching render logic in app.js.
   ========================================================================== */

var RW = window.RW || {};

RW.PRODUCTS = [
  {
    id: "jessie-luxury",
    name: "JESSIE + JESSIE LUXURY",
    code: "RW-2094-01",
    category: "TEE",
    price: 185,
    status: "RESTRICTED",
    accessLevel: "02",
    desc: "A double signature run. Two archive marks printed off-register on a heavyweight base, left to fade unevenly by design. The luxury designation refers to the finish, not the price.",
    sizes: { XS: true, S: true, M: true, L: false, XL: true },
    specs: { MATERIAL: "220GSM COTTON", WEIGHT: "HEAVY", FIT: "BOXED", PRODUCTION: "LOT 003 / 2094", "ITEM CODE": "RW-2094-01" }
  },
  {
    id: "restricted-tee-01",
    name: "RESTRICTED TEE 01",
    code: "RW-2094-02",
    category: "TEE",
    price: 120,
    status: "AVAILABLE",
    accessLevel: "01",
    desc: "The base garment. No graphic, one small woven label at the hem stating what it is and what it is not. Everything else is left to the wearer.",
    sizes: { XS: true, S: true, M: true, L: true, XL: true },
    specs: { MATERIAL: "200GSM COTTON", WEIGHT: "MID", FIT: "REGULAR", PRODUCTION: "LOT 001 / 2094", "ITEM CODE": "RW-2094-02" }
  },
  {
    id: "authorization-hoodie",
    name: "AUTHORIZATION HOODIE",
    code: "RW-2094-03",
    category: "HOODIE",
    price: 340,
    status: "RESTRICTED",
    accessLevel: "03",
    desc: "Interior pocket sized for a single card. The hood is cut deep enough to obscure the face at three-quarter angle. Built for a system that does not exist outside this brand.",
    sizes: { XS: false, S: true, M: true, L: true, XL: false },
    specs: { MATERIAL: "480GSM FLEECE", WEIGHT: "HEAVY", FIT: "OVERSIZED", PRODUCTION: "LOT 002 / 2094", "ITEM CODE": "RW-2094-03" }
  },
  {
    id: "red-card-tee",
    name: "RED CARD TEE",
    code: "RW-2094-04",
    category: "TEE",
    price: 135,
    status: "AVAILABLE",
    accessLevel: "01",
    desc: "A single burgundy stripe at the collar, the only colour permitted outside the base palette. Reads as a stamp of clearance rather than a decoration.",
    sizes: { XS: true, S: true, M: true, L: true, XL: false },
    specs: { MATERIAL: "210GSM COTTON", WEIGHT: "MID", FIT: "REGULAR", PRODUCTION: "LOT 001 / 2094", "ITEM CODE": "RW-2094-04" }
  },
  {
    id: "archive-longslv",
    name: "ARCHIVE LONGSLEEVE",
    code: "RW-2094-05",
    category: "LONGSLEEVE",
    price: 210,
    status: "LOW STOCK",
    accessLevel: "02",
    desc: "Reissued from an internal reference sample with no recorded first release date. Cuffs are left raw. Treat the inconsistency as intentional.",
    sizes: { XS: true, S: false, M: true, L: true, XL: true },
    specs: { MATERIAL: "260GSM COTTON", WEIGHT: "HEAVY", FIT: "SLIM", PRODUCTION: "LOT 004 / 2094", "ITEM CODE": "RW-2094-05" }
  },
  {
    id: "restricted-hoodie",
    name: "RESTRICTED HOODIE",
    code: "RW-2094-06",
    category: "HOODIE",
    price: 365,
    status: "RESTRICTED",
    accessLevel: "03",
    desc: "The heaviest garment in the current lot. No branding on the face, one small tag at the nape listing an access level most customers will not be issued.",
    sizes: { XS: false, S: false, M: true, L: true, XL: true },
    specs: { MATERIAL: "500GSM FLEECE", WEIGHT: "HEAVY", FIT: "OVERSIZED", PRODUCTION: "LOT 002 / 2094", "ITEM CODE": "RW-2094-06" }
  }
];

RW.ARCHIVE = [
  {
    date: "PROTOCOL 2094",
    tag: "ORIGIN / UNDISCLOSED",
    title: "The number precedes the brand.",
    body: "2094 appears on the earliest surviving internal document, before the company had a name. No later record explains what it refers to. It has been printed on every item since."
  },
  {
    date: "LOT 001",
    tag: "FIRST RELEASE",
    title: "Two base garments, one label.",
    body: "The first lot shipped without imagery. A single woven tag carried the brand mark and an item code. Buyers were asked, informally, to show it before wearing it in public."
  },
  {
    date: "LOT 002",
    tag: "AUTHORIZATION INTRODUCED",
    title: "The Authoric Document is formalized.",
    body: "What began as an informal request became a printed card. Access levels were assigned to garments rather than people, a distinction the brand has never fully clarified."
  },
  {
    date: "LOT 003",
    tag: "JESSIE + JESSIE",
    title: "A signature run, deliberately off-register.",
    body: "Two marks, printed slightly out of alignment on purpose. Internal notes refer to it as 'the luxury problem' without further comment."
  },
  {
    date: "LOT 004",
    tag: "ARCHIVE REISSUE",
    title: "A reference sample re-enters circulation.",
    body: "No first release date has been located for this garment. It is catalogued as a reissue on the assumption that one exists somewhere it cannot be found."
  },
  {
    date: "STATUS",
    tag: "ONGOING",
    title: "The system remains active.",
    body: "RED CARD DOCSED continues to process authorization requests. Most are declined without stated reason. This is treated internally as consistent with policy."
  }
];

RW.BRAND = {
  name: "RESTRICTED WEAR",
  tagline: "ACCESS IS NOT GUARANTEED.",
  code: "RW-2094",
  contactDept: "RED CARD DOCSED",
  contactSub: "DOCUMENTATION / AUTHORIZATION",
  email: "redcardocsed@gmail.com"
};
