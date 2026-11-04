/* ==========================================================================
   RESTRICTED WEAR — application (Version 1.2)
   Accounts, Red Cards and order history are stored on the server
   (Cloudflare Pages Functions + KV, see functions/api/) when it is available,
   and in this browser otherwise. Checkout is a visual prototype: no payment
   is ever processed and no card data is stored.
   ========================================================================== */
var RW = window.RW || {};

/* --------------------------------------------------------------------------
   Constants
   -------------------------------------------------------------------------- */
RW.CART_KEY = "rw_cart_v2";                 /* unchanged from V1.1 */
RW.THEME_KEY = "rw_theme_v2";               /* "default" | "inverted" */
RW.ACCOUNTS_KEY = "rw_accounts_v1";
RW.SESSION_KEY = "rw_session_v1";
RW.REDCARD_REGISTRY_KEY = "rw_redcard_ids_v1";
RW.LEGACY_KEYS = ["rw_redcard_auth_v1", "rw_theme_v1"];
RW.DEFAULT_ROUTE = "home";

/* --------------------------------------------------------------------------
   State
   -------------------------------------------------------------------------- */
RW.state = {
  cart: [],
  theme: "default",
  filters: { gender: "ALL", category: "ALL", sub: "ALL", stream: "ALL", sort: "DEFAULT", query: "" },
  collectionCat: { MAINSTREAM: "ALL", SECONDSTREAM: "ALL" },
  pd: { id: null, size: null, qty: 1, thumb: 0 },
  accountTab: "signin",
  next: null
};

/* --------------------------------------------------------------------------
   Generic helpers
   -------------------------------------------------------------------------- */
RW.qs = function (sel, ctx) { return (ctx || document).querySelector(sel); };
RW.qsa = function (sel, ctx) { return Array.prototype.slice.call((ctx || document).querySelectorAll(sel)); };

/* EUR, e.g. "€ 499.99" */
RW.money = function (n) {
  return "€ " + Number(n || 0).toLocaleString("en-US", { minimumFractionDigits: 2, maximumFractionDigits: 2 });
};

RW.findProduct = function (id) { return RW.PRODUCTS.filter(function (p) { return p.id === id; })[0]; };
RW.availableSizes = function (p) { return Object.keys(p.sizes || {}); };
RW.isRestricted = function (p) { return !!p && p.status === "RESTRICTED"; };
RW.categoryBySlug = function (slug) {
  return RW.CATEGORIES.concat(RW.FUTURE_CATEGORIES).filter(function (c) { return c.slug === slug; })[0];
};
RW.categoryByKey = function (key) {
  return RW.CATEGORIES.concat(RW.FUTURE_CATEGORIES).filter(function (c) { return c.key === key; })[0];
};
RW.isFutureCategory = function (key) {
  return RW.FUTURE_CATEGORIES.some(function (c) { return c.key === key; });
};

RW.escapeHtml = function (str) {
  return String(str === null || str === undefined ? "" : str).replace(/[&<>"']/g, function (c) {
    var map = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
    return map[c];
  });
};

RW.formatDate = function (iso) {
  var d = iso ? new Date(iso) : new Date();
  var months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
  return months[d.getMonth()] + " " + d.getDate() + ", " + d.getFullYear();
};

RW.toast = function (msg) {
  var el = RW.qs("#toast");
  if (!el) return;
  el.textContent = msg;
  el.classList.add("show");
  clearTimeout(RW._toastTimer);
  RW._toastTimer = setTimeout(function () { el.classList.remove("show"); }, 2400);
};

RW.go = function (route) { window.location.hash = "#/" + route; };

/* Safe storage — every read/write is wrapped; the site works without it. */
RW.store = {
  get: function (key, fallback) {
    try {
      var raw = localStorage.getItem(key);
      return raw === null ? fallback : JSON.parse(raw);
    } catch (e) { return fallback; }
  },
  set: function (key, value) {
    try { localStorage.setItem(key, JSON.stringify(value)); } catch (e) {}
  },
  remove: function (key) {
    try { localStorage.removeItem(key); } catch (e) {}
  }
};

/* --------------------------------------------------------------------------
   Randomness (crypto where available)
   -------------------------------------------------------------------------- */
RW.randomDigits = function (n) {
  var out = "";
  var c = window.crypto || window.msCrypto;
  while (out.length < n) {
    if (c && c.getRandomValues) {
      var buf = new Uint8Array(Math.max(8, n - out.length + 8));
      c.getRandomValues(buf);
      for (var i = 0; i < buf.length && out.length < n; i++) {
        if (buf[i] < 250) out += String(buf[i] % 10); /* rejection sampling: uniform 0–9 */
      }
    } else {
      out += String(Math.floor(Math.random() * 10));
    }
  }
  return out;
};

RW.randomUnit = function () {
  var c = window.crypto || window.msCrypto;
  if (c && c.getRandomValues) {
    var b = new Uint32Array(1);
    c.getRandomValues(b);
    return b[0] / 4294967296;
  }
  return Math.random();
};

/* --------------------------------------------------------------------------
   Red Card identifier — exactly 94 digits, no separators or prefixes.
   [4-digit controlled prefix, or rarely a random one] + [90 random digits]
   Local mode: uniqueness is enforced against every identifier issued in this
   browser. Server mode generates identifiers in functions/api/ instead.
   -------------------------------------------------------------------------- */
RW.isValidRedCardId = function (id) {
  return typeof id === "string" && id.length === RW.REDCARD.LENGTH && /^[0-9]+$/.test(id);
};

RW.redCardPrefix = function () {
  var list = RW.REDCARD.PREFIXES;
  if (RW.randomUnit() < RW.REDCARD.RANDOM_PREFIX_CHANCE) return RW.randomDigits(4);
  return list[Math.floor(RW.randomUnit() * list.length)];
};

RW.generateRedCardId = function () {
  var registry = RW.store.get(RW.REDCARD_REGISTRY_KEY, []);
  if (!Array.isArray(registry)) registry = [];
  var accounts = RW.accounts();
  Object.keys(accounts).forEach(function (k) {
    var rc = accounts[k].redCard;
    if (rc && rc.id && registry.indexOf(rc.id) === -1) registry.push(rc.id);
  });

  var id;
  do {
    id = RW.redCardPrefix() + RW.randomDigits(RW.REDCARD.LENGTH - 4);
  } while (!RW.isValidRedCardId(id) || registry.indexOf(id) !== -1);

  registry.push(id);
  RW.store.set(RW.REDCARD_REGISTRY_KEY, registry);
  return id;
};

/* --------------------------------------------------------------------------
   Accounts

   Two storage modes, chosen automatically at start-up:
   • server — the site is running on Cloudflare Pages with the RW_DB KV
     namespace bound (see functions/api/[[path]].js). Accounts, Red Cards and
     orders are stored on the server, so they are the same in every browser
     and on every device.
   • local  — no API is available (opened as a file, or KV not bound yet).
     The V1.2 in-browser prototype is used: everything stays in this browser.

   In both modes a Red Card is issued once per account and kept permanently:
   its identifier and issue date never change. Revoking only switches the
   status; reactivating restores the same document.
   -------------------------------------------------------------------------- */
RW.API_BASE = "/api/";
RW.TOKEN_KEY = "rw_token_v1";
RW.backend = { mode: "local", user: null };

/* --- local (in-browser) prototype storage ------------------------------- */
/* Passwords are hashed with a non-cryptographic hash purely so they are not
   kept as plain text. This is not a security measure. */
RW.demoHash = function (str) {
  var h1 = 0xdeadbeef, h2 = 0x41c6ce57;
  for (var i = 0, ch; i < str.length; i++) {
    ch = str.charCodeAt(i);
    h1 = Math.imul(h1 ^ ch, 2654435761);
    h2 = Math.imul(h2 ^ ch, 1597334677);
  }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507);
  h1 ^= Math.imul(h2 ^ (h2 >>> 13), 3266489909);
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507);
  h2 ^= Math.imul(h1 ^ (h1 >>> 13), 3266489909);
  return (4294967296 * (2097151 & h2) + (h1 >>> 0)).toString(16);
};

RW.normEmail = function (e) { return String(e || "").trim().toLowerCase(); };

RW.accounts = function () {
  var a = RW.store.get(RW.ACCOUNTS_KEY, {});
  return a && typeof a === "object" ? a : {};
};
RW.saveAccounts = function (map) { RW.store.set(RW.ACCOUNTS_KEY, map); };

RW.currentUser = function () {
  if (RW.backend.mode === "server") return RW.backend.user;
  var email = RW.store.get(RW.SESSION_KEY, null);
  if (!email) return null;
  return RW.accounts()[email] || null;
};

RW.saveUser = function (user) {
  var map = RW.accounts();
  map[user.email] = user;
  RW.saveAccounts(map);
};

/* A Red Card made earlier in this browser, for the same email and password.
   It is carried over to the server unchanged (same identifier, same date). */
RW.legacyRedCardFor = function (email, password) {
  email = RW.normEmail(email);
  var u = RW.accounts()[email];
  if (!u || u.pass !== RW.demoHash(email + "::" + password)) return null;
  if (!u.redCard || !RW.isValidRedCardId(u.redCard.id)) return null;
  return { id: u.redCard.id, issued: u.redCard.issued, status: u.redCard.status };
};

RW.local = {
  createAccount: function (name, email, password) {
    email = RW.normEmail(email);
    var map = RW.accounts();
    if (map[email]) return { error: "AN ACCOUNT WITH THIS EMAIL ALREADY EXISTS" };
    var user = {
      name: String(name || "").trim(),
      email: email,
      pass: RW.demoHash(email + "::" + password),
      created: new Date().toISOString(),
      profile: { phone: "", country: "", city: "", address: "", postal: "" },
      redCard: null,
      orders: []
    };
    map[email] = user;
    RW.saveAccounts(map);
    RW.store.set(RW.SESSION_KEY, email);
    return { user: user };
  },
  signIn: function (email, password) {
    email = RW.normEmail(email);
    var user = RW.accounts()[email];
    if (!user || user.pass !== RW.demoHash(email + "::" + password)) return { error: "EMAIL OR PASSWORD IS INCORRECT" };
    RW.store.set(RW.SESSION_KEY, email);
    return { user: user };
  },
  signOut: function () { RW.store.remove(RW.SESSION_KEY); return {}; },
  saveProfile: function (name, profile) {
    var u = RW.currentUser();
    if (!u) return { error: "SIGN IN TO CONTINUE" };
    if (name !== undefined) u.name = name;
    u.profile = u.profile || {};
    Object.keys(profile || {}).forEach(function (k) { u.profile[k] = profile[k]; });
    RW.saveUser(u);
    return { user: u };
  },
  issueRedCard: function (name, country) {
    var u = RW.currentUser();
    if (!u) return { error: "SIGN IN TO CONTINUE" };
    if (name) u.name = name;
    if (country !== undefined) { u.profile = u.profile || {}; u.profile.country = country; }
    if (u.redCard && RW.isValidRedCardId(u.redCard.id)) {
      u.redCard.status = "ACTIVE";               /* same identifier, same issue date */
    } else {
      u.redCard = { id: RW.generateRedCardId(), holder: u.name, issued: new Date().toISOString(), status: "ACTIVE" };
    }
    RW.saveUser(u);
    return { user: u };
  },
  revokeRedCard: function () {
    var u = RW.currentUser();
    if (!u) return { error: "SIGN IN TO CONTINUE" };
    if (u.redCard) u.redCard.status = "REVOKED";  /* the document is kept */
    RW.saveUser(u);
    return { user: u };
  },
  placeOrder: function (items, profile) {
    var u = RW.currentUser();
    if (!u) return { error: "SIGN IN TO CONTINUE" };
    if (profile) { u.profile = u.profile || {}; Object.keys(profile).forEach(function (k) { u.profile[k] = profile[k]; }); }
    var order = {
      ref: RW.randomDigits(10),
      date: new Date().toISOString(),
      total: items.reduce(function (s, i) { return s + i.price * i.qty; }, 0),
      items: items
    };
    u.orders = u.orders || [];
    u.orders.push(order);
    RW.saveUser(u);
    return { order: order, user: u };
  }
};

/* --- server storage ------------------------------------------------------ */
RW.api = function (method, path, body, timeoutMs) {
  var headers = { "Accept": "application/json" };
  var token = RW.store.get(RW.TOKEN_KEY, null);
  if (token) headers["Authorization"] = "Bearer " + token;
  if (body !== undefined) headers["Content-Type"] = "application/json";

  var ctrl = typeof AbortController === "function" ? new AbortController() : null;
  var timer = ctrl ? setTimeout(function () { ctrl.abort(); }, timeoutMs || 12000) : null;

  return fetch(RW.API_BASE + path, {
    method: method,
    headers: headers,
    body: body !== undefined ? JSON.stringify(body) : undefined,
    credentials: "same-origin",
    cache: "no-store",
    signal: ctrl ? ctrl.signal : undefined
  }).then(function (res) {
    return res.json().catch(function () { return {}; }).then(function (data) {
      return { ok: res.ok, status: res.status, data: data || {} };
    });
  }).catch(function () {
    return { ok: false, status: 0, data: { error: "CONNECTION FAILED. TRY AGAIN." } };
  }).then(function (r) {
    if (timer) clearTimeout(timer);
    return r;
  });
};

/* Turns an API answer into { user, ... } or { error }, and keeps the cached account in step. */
RW.apiResult = function (r) {
  if (!r.ok) {
    if (r.status === 401 && RW.backend.user) { RW.backend.user = null; RW.store.remove(RW.TOKEN_KEY); }
    return { error: (r.data && r.data.error) || "SOMETHING WENT WRONG. TRY AGAIN." };
  }
  if (r.data.token) RW.store.set(RW.TOKEN_KEY, r.data.token);
  if (r.data.user) RW.backend.user = r.data.user;
  return r.data;
};

/* Decides the storage mode and restores the signed-in account. */
RW.backend.init = function () {
  if (!window.fetch || window.location.protocol === "file:") { RW.backend.mode = "local"; return Promise.resolve(); }
  return RW.api("GET", "health", undefined, 4000).then(function (r) {
    RW.backend.mode = r.ok && r.data && r.data.storage ? "server" : "local";
    if (RW.backend.mode !== "server") return;
    RW.backend.user = null;
    if (!RW.store.get(RW.TOKEN_KEY, null)) return;
    return RW.api("GET", "me", undefined, 6000).then(function (me) {
      if (me.ok) RW.backend.user = me.data.user;
      else if (me.status === 401) RW.store.remove(RW.TOKEN_KEY);
    });
  });
};

/* Quietly re-reads the account (changes made on another device) and
   re-renders the open account page if anything changed. */
RW.backend.refresh = function () {
  if (RW.backend.mode !== "server" || !RW.backend.user || RW.backend._refreshing) return;
  RW.backend._refreshing = true;
  var before = JSON.stringify(RW.backend.user);
  RW.api("GET", "me", undefined, 6000).then(function (r) {
    RW.backend._refreshing = false;
    if (r.ok) RW.backend.user = r.data.user;
    else if (r.status === 401) { RW.backend.user = null; RW.store.remove(RW.TOKEN_KEY); }
    else return;
    if (JSON.stringify(RW.backend.user) !== before && !RW.qs("#modal-overlay.open")) RW.refreshAccountViews();
  });
};

/* --- the interface used by the pages (always returns a Promise) ---------- */
RW.account = {
  createAccount: function (name, email, password) {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.createAccount(name, email, password));
    return RW.api("POST", "register", {
      name: name, email: email, password: password,
      legacyRedCard: RW.legacyRedCardFor(email, password)
    }).then(RW.apiResult);
  },
  signIn: function (email, password) {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.signIn(email, password));
    return RW.api("POST", "login", {
      email: email, password: password,
      legacyRedCard: RW.legacyRedCardFor(email, password)
    }).then(RW.apiResult);
  },
  signOut: function () {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.signOut());
    return RW.api("POST", "logout", {}).then(function () {
      RW.store.remove(RW.TOKEN_KEY);
      RW.backend.user = null;
      return {};
    });
  },
  saveProfile: function (name, profile) {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.saveProfile(name, profile));
    return RW.api("PUT", "profile", { name: name, profile: profile }).then(RW.apiResult);
  },
  issueRedCard: function (name, country) {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.issueRedCard(name, country));
    return RW.api("POST", "redcard", { name: name, country: country }).then(RW.apiResult);
  },
  revokeRedCard: function () {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.revokeRedCard());
    return RW.api("DELETE", "redcard").then(RW.apiResult);
  },
  placeOrder: function (items, profile) {
    if (RW.backend.mode !== "server") return Promise.resolve(RW.local.placeOrder(items, profile));
    return RW.api("POST", "orders", { items: items, profile: profile || null }).then(RW.apiResult);
  }
};

/* --------------------------------------------------------------------------
   Authorization
   Browsing is public. Any purchase requires a signed-in account.
   RESTRICTED pieces additionally require an active Red Card.
   -------------------------------------------------------------------------- */
RW.hasRedCard = function (user) {
  user = user || RW.currentUser();
  return !!(user && user.redCard && user.redCard.status === "ACTIVE" && RW.isValidRedCardId(user.redCard.id));
};

/* "ACTIVE" | "REVOKED" | "NOT ISSUED" */
RW.redCardStatus = function (user) {
  var rc = user && user.redCard;
  if (!rc || !RW.isValidRedCardId(rc.id)) return "NOT ISSUED";
  return rc.status === "ACTIVE" ? "ACTIVE" : "REVOKED";
};

/* Returns null when the items may be purchased, otherwise "ACCOUNT" or "REDCARD". */
RW.purchaseBlocker = function (items) {
  var user = RW.currentUser();
  if (!user) return "ACCOUNT";
  var needsCard = (items || []).some(function (i) { return RW.isRestricted(RW.findProduct(i.id)); });
  if (needsCard && !RW.hasRedCard(user)) return "REDCARD";
  return null;
};

RW.redirectForAuthorization = function (blocker, next) {
  RW.state.next = next || null;
  if (blocker === "ACCOUNT") {
    RW.toast("SIGN IN TO CONTINUE");
    RW.state.accountTab = "signin";
    RW.go("account");
  } else {
    RW.toast("AN ACTIVE RED CARD IS REQUIRED");
    RW.go("authorization");
  }
};

RW.continueAfterAuth = function (fallback) {
  var next = RW.state.next;
  RW.state.next = null;
  if (next) { RW.go(next); return; }
  if (fallback) RW.go(fallback);
};

/* Re-renders whichever account-related page is open. */
RW.refreshAccountViews = function () {
  RW.renderCart();
  if (RW.qs(".auth-page")) RW.renderAuthorization();
  else if (RW.qs(".account-page")) RW.renderAccount();
};

/* Disables a button while a request is running. */
RW.busy = function (btn, label) {
  if (!btn) return function () {};
  var original = btn.textContent;
  btn.disabled = true;
  if (label) btn.textContent = label;
  return function () { btn.disabled = false; btn.textContent = original; };
};

/* --------------------------------------------------------------------------
   Persistence — cart / theme
   -------------------------------------------------------------------------- */
RW.loadCart = function () {
  var raw = RW.store.get(RW.CART_KEY, []);
  if (!Array.isArray(raw)) raw = [];
  RW.state.cart = raw.map(function (i) {
    if (i && RW.LEGACY_IDS[i.id]) i.id = RW.LEGACY_IDS[i.id];
    return i;
  }).filter(function (i) { return i && RW.findProduct(i.id) && i.qty > 0; }).map(function (i) {
    i.variant = RW.variantOf(RW.findProduct(i.id), i.variant).key;
    return i;
  });
  RW.saveCart();
};
RW.saveCart = function () { RW.store.set(RW.CART_KEY, RW.state.cart); };

RW.loadTheme = function () {
  var t = null;
  try { t = localStorage.getItem(RW.THEME_KEY); } catch (e) {}
  RW.state.theme = t === "inverted" ? "inverted" : "default";
  RW.applyTheme();
};
RW.applyTheme = function () {
  var inv = RW.state.theme === "inverted";
  if (inv) document.documentElement.setAttribute("data-theme", "inverted");
  else document.documentElement.removeAttribute("data-theme");
  var meta = RW.qs('meta[name="theme-color"]');
  if (meta) meta.setAttribute("content", inv ? "#000000" : "#ffffff");
  var btn = RW.qs("#theme-toggle");
  if (btn) btn.setAttribute("aria-pressed", inv ? "true" : "false");
};
RW.toggleTheme = function () {
  RW.state.theme = RW.state.theme === "inverted" ? "default" : "inverted";
  try { localStorage.setItem(RW.THEME_KEY, RW.state.theme); } catch (e) {}
  RW.applyTheme();
};

/* --------------------------------------------------------------------------
   Media — product images when present, otherwise a quiet placeholder
   -------------------------------------------------------------------------- */
/* Colour variants. A product may define `variants: [{ key, label, images: [front, back, ...] }]`.
   Products without variants behave exactly as before (one default image set). */
RW.variantsOf = function (p) {
  if (p && p.variants && p.variants.length) return p.variants;
  return [{ key: "", label: null, images: (p && p.images) || [] }];
};
RW.variantOf = function (p, key) {
  var vs = RW.variantsOf(p);
  return vs.filter(function (v) { return v.key === (key || ""); })[0] || vs[0];
};
RW.imagesOf = function (p, key) { return RW.variantOf(p, key).images || []; };

RW.mediaHtml = function (p, index, alt, variantKey) {
  var src = p && RW.imagesOf(p, variantKey)[index || 0];
  if (src) {
    var label = p.imageLabels && p.imageLabels[index || 0] ? " \u2014 " + p.imageLabels[index || 0].toLowerCase() : "";
    return '<div class="ph has-photo"><img class="ph-photo" src="' + RW.escapeHtml(src) + '" alt="' + RW.escapeHtml(alt || (p ? p.name + label : "")) + '" loading="lazy"></div>';
  }
  return '<div class="ph"><img class="ph-mark" src="img/rw-mark.png" alt=""></div>';
};

RW.editorialHtml = function (src) {
  if (src) return '<div class="ph has-photo"><img class="ph-photo" src="' + RW.escapeHtml(src) + '" alt=""></div>';
  return '<div class="ph"><img class="ph-mark" src="img/rw-mark.png" alt=""></div>';
};

/* --------------------------------------------------------------------------
   Cart logic
   -------------------------------------------------------------------------- */
RW.sameLine = function (i, id, size, variant) {
  return i.id === id && i.size === size && (i.variant || "") === (variant || "");
};

RW.addToCart = function (id, size, qty, variant) {
  qty = qty || 1;
  var existing = RW.state.cart.filter(function (i) { return RW.sameLine(i, id, size, variant); })[0];
  if (existing) { existing.qty += qty; } else { RW.state.cart.push({ id: id, size: size, qty: qty, variant: variant || "" }); }
  RW.saveCart();
  RW.updateBagCount();
  RW.renderCart();
};

RW.removeFromCart = function (id, size, variant) {
  RW.state.cart = RW.state.cart.filter(function (i) { return !RW.sameLine(i, id, size, variant); });
  RW.saveCart();
  RW.updateBagCount();
  RW.renderCart();
};

RW.changeCartQty = function (id, size, delta, variant) {
  var item = RW.state.cart.filter(function (i) { return RW.sameLine(i, id, size, variant); })[0];
  if (!item) return;
  item.qty += delta;
  if (item.qty < 1) { RW.removeFromCart(id, size, variant); return; }
  RW.saveCart();
  RW.updateBagCount();
  RW.renderCart();
};

RW.cartCount = function () {
  return RW.state.cart.reduce(function (sum, i) { return sum + i.qty; }, 0);
};
RW.cartSubtotal = function () {
  return RW.state.cart.reduce(function (sum, i) {
    var p = RW.findProduct(i.id);
    return sum + (p ? p.price * i.qty : 0);
  }, 0);
};

RW.updateBagCount = function () {
  var el = RW.qs("#cart-count");
  if (el) el.textContent = String(RW.cartCount());
};

RW.renderCart = function () {
  var container = RW.qs("#cart-items-container");
  if (!container) return;

  if (!RW.state.cart.length) {
    container.innerHTML = '<div class="cart-empty">YOUR BAG IS EMPTY.</div>';
  } else {
    container.innerHTML = RW.state.cart.map(function (item) {
      var p = RW.findProduct(item.id);
      if (!p) return "";
      var sizeAttr = RW.escapeHtml(item.size || "") + '" data-variant="' + RW.escapeHtml(item.variant || "");
      var v = RW.variantOf(p, item.variant);
      return (
        '<div class="cart-item">' +
          '<a class="ci-thumb" href="#/product/' + p.id + '">' + RW.mediaHtml(p, 0, null, item.variant) + '</a>' +
          '<div class="ci-info">' +
            '<div class="ci-name">' + RW.escapeHtml(p.name) + '</div>' +
            '<div class="ci-meta">' + (v.label ? RW.escapeHtml(v.label) + ' &middot; ' : "") + 'SIZE ' + RW.escapeHtml(item.size || "-") + ' &middot; ' + RW.escapeHtml(p.stream) +
              (RW.isRestricted(p) ? ' &middot; <span class="restricted">RESTRICTED</span>' : "") + '</div>' +
            '<div class="ci-bottom">' +
              '<div class="qty-control">' +
                '<button type="button" data-cart-qty="-1" data-id="' + p.id + '" data-size="' + sizeAttr + '" aria-label="Decrease quantity">&minus;</button>' +
                '<span>' + item.qty + '</span>' +
                '<button type="button" data-cart-qty="1" data-id="' + p.id + '" data-size="' + sizeAttr + '" aria-label="Increase quantity">+</button>' +
              '</div>' +
              '<span>' + RW.money(p.price * item.qty) + '</span>' +
            '</div>' +
            '<button type="button" class="ci-remove" data-cart-remove data-id="' + p.id + '" data-size="' + sizeAttr + '">REMOVE</button>' +
          '</div>' +
        '</div>'
      );
    }).join("");
  }

  var subtotal = RW.cartSubtotal();
  var subEl = RW.qs("#cart-subtotal-price");
  var totEl = RW.qs("#cart-total-price");
  if (subEl) subEl.textContent = RW.money(subtotal);
  if (totEl) totEl.textContent = RW.money(subtotal);

  var count = RW.cartCount();
  var subtitle = RW.qs("#cart-status-subtitle");
  if (subtitle) subtitle.textContent = count ? count + (count === 1 ? " PIECE" : " PIECES") : "EMPTY";

  var authLine = RW.qs("#cart-auth-line");
  if (authLine) {
    var blocker = RW.state.cart.length ? RW.purchaseBlocker(RW.state.cart) : null;
    authLine.textContent = blocker === "ACCOUNT"
      ? "PURCHASING REQUIRES AN ACCOUNT"
      : blocker === "REDCARD" ? "RESTRICTED PIECES REQUIRE AN ACTIVE RED CARD" : "";
    authLine.style.display = blocker ? "" : "none";
  }

  RW.qsa("[data-cart-qty]", container).forEach(function (btn) {
    btn.addEventListener("click", function () {
      RW.changeCartQty(btn.getAttribute("data-id"), btn.getAttribute("data-size"), parseInt(btn.getAttribute("data-cart-qty"), 10), btn.getAttribute("data-variant"));
    });
  });
  RW.qsa("[data-cart-remove]", container).forEach(function (btn) {
    btn.addEventListener("click", function () {
      RW.removeFromCart(btn.getAttribute("data-id"), btn.getAttribute("data-size"), btn.getAttribute("data-variant"));
    });
  });
  RW.qsa(".ci-thumb", container).forEach(function (a) { a.addEventListener("click", RW.closeCart); });
};

RW.openCart = function () {
  var overlay = RW.qs("#cart-overlay");
  if (overlay) overlay.classList.add("open");
};
RW.closeCart = function () {
  var overlay = RW.qs("#cart-overlay");
  if (overlay) overlay.classList.remove("open");
};

RW.bindCheckoutButton = function () {
  var btn = RW.qs("#checkout-btn");
  if (!btn) return;
  btn.addEventListener("click", function () {
    if (!RW.state.cart.length) { RW.toast("YOUR BAG IS EMPTY"); return; }
    RW.closeCart();
    var blocker = RW.purchaseBlocker(RW.state.cart);
    if (blocker) { RW.redirectForAuthorization(blocker, "checkout"); return; }
    RW.go("checkout");
  });
};

/* --------------------------------------------------------------------------
   Modal
   -------------------------------------------------------------------------- */
RW.openModal = function (html) {
  var box = RW.qs("#modal-box-content");
  var overlay = RW.qs("#modal-overlay");
  if (box) box.innerHTML = html;
  if (overlay) overlay.classList.add("open");
};

RW.closeModal = function () {
  var overlay = RW.qs("#modal-overlay");
  if (overlay) overlay.classList.remove("open");
};

/* --------------------------------------------------------------------------
   Product card
   -------------------------------------------------------------------------- */
RW.productCardHtml = function (p, opts) {
  opts = opts || {};
  var status = "";
  if (p.status === "RESTRICTED") status = '<div class="card-status restricted">RESTRICTED</div>';
  else if (p.status === "LOW STOCK") status = '<div class="card-status">LOW STOCK</div>';

  return (
    '<a class="card" href="#/product/' + p.id + '">' +
      '<div class="card-media">' + RW.mediaHtml(p, 0) + status + '</div>' +
      '<div class="card-body">' +
        '<div class="card-name">' + RW.escapeHtml(p.name) + '</div>' +
        (opts.hideMarker ? "" : '<div class="marker">' + RW.escapeHtml(p.stream) + '</div>') +
        '<div class="card-price">' + RW.money(p.price) + '</div>' +
      '</div>' +
    '</a>'
  );
};

/* --------------------------------------------------------------------------
   Router
   -------------------------------------------------------------------------- */
RW.parseHash = function () {
  var hash = window.location.hash || "";
  hash = hash.replace(/^#\/?/, "");
  return hash.split("/").filter(function (part) { return part.length > 0; }).map(function (p) {
    try { return decodeURIComponent(p); } catch (e) { return p; }
  });
};

RW.setActiveNav = function (page) {
  RW.qsa(".nav-link, .mobile-nav-link, .nav-text-btn[data-page]").forEach(function (a) {
    a.classList.toggle("active", a.getAttribute("data-page") === page);
  });
};

/* After navigation: home returns to the masthead; other pages open directly
   under the sticky navigation if the visitor had already scrolled past it. */
RW.scrollAfterRoute = function (page) {
  var masthead = RW.qs(".masthead");
  var mh = masthead ? masthead.offsetHeight : 0;
  if (page === "home" || window.pageYOffset < mh) { window.scrollTo(0, 0); return; }
  window.scrollTo(0, mh);
};

RW.route = function () {
  var parts = RW.parseHash();
  var page = parts[0] || RW.DEFAULT_ROUTE;
  var param = parts[1];

  RW.closeModal();
  RW.closeCart();
  RW.zoomClose();
  if (page !== "product") RW.soundStop(); /* product sound belongs to the product page only */

  var nav = page;
  if (page === "product") nav = "shop";
  if (page === "red-card") nav = "authorization";
  RW.setActiveNav(nav);

  if (page === "shop") {
    RW.setShopCategoryFromSlug(param);
    RW.renderShop();
  } else if (page === "mainstream") {
    RW.renderCollection("MAINSTREAM");
  } else if (page === "secondstream") {
    RW.renderCollection("SECONDSTREAM");
  } else if (page === "product") {
    RW.renderProduct(param);
  } else if (page === "authorization" || page === "red-card") {
    RW.renderAuthorization();
  } else if (page === "account") {
    RW.renderAccount();
  } else if (page === "archive") {
    RW.renderArchive();
  } else if (page === "checkout") {
    RW.renderCheckout();
  } else if (page === "contact") {
    /* V1.1 had a Contact page; contact details now live in the footer. */
    RW.renderHome();
    RW.setActiveNav(null);
    var foot = RW.qs("#site-footer");
    if (foot) setTimeout(function () { foot.scrollIntoView(); }, 0);
    return;
  } else {
    RW.setActiveNav(null);
    RW.renderHome();
    page = "home";
  }

  RW.scrollAfterRoute(page);
  if (page === "account" || page === "authorization" || page === "red-card") RW.backend.refresh();
};

/* --------------------------------------------------------------------------
   Home — editorial
   -------------------------------------------------------------------------- */
RW.renderHome = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var featured = RW.PRODUCTS.filter(function (p) { return p.featured; }).slice(0, 3);

  var direction = function (d, img) {
    return (
      '<a class="direction" href="#/' + d.slug + '">' +
        '<div class="direction-media">' + RW.editorialHtml(img) + '</div>' +
        '<div class="direction-body">' +
          '<h3 class="display">' + RW.escapeHtml(d.key) + '</h3>' +
          '<p>' + RW.escapeHtml(d.desc) + '</p>' +
          '<span class="text-link">VIEW COLLECTION</span>' +
        '</div>' +
      '</a>'
    );
  };

  app.innerHTML =
    '<div class="page home-page">' +
      '<div class="wrap">' +
        '<section class="home-lead-wrap">' +
          '<a class="home-lead" href="#/shop" style="display:block;">' + RW.editorialHtml(RW.EDITORIAL.lead) + '</a>' +
          '<div class="home-lead-caption">' +
            '<div class="mono dim">CURRENT PIECES</div>' +
            '<a class="text-link" href="#/shop">VIEW ALL</a>' +
          '</div>' +
        '</section>' +

        '<section class="section">' +
          '<div class="home-directions">' +
            direction(RW.DIRECTIONS.MAINSTREAM, RW.EDITORIAL.mainstream) +
            direction(RW.DIRECTIONS.SECONDSTREAM, RW.EDITORIAL.secondstream) +
          '</div>' +
        '</section>' +

        '<section class="section" style="padding-top:20px;">' +
          '<div class="section-head">' +
            '<div class="mono dim">SELECTED</div>' +
          '</div>' +
          '<div class="grid">' + featured.map(function (p) { return RW.productCardHtml(p); }).join("") + '</div>' +
        '</section>' +

        '<section class="home-note">' +
          '<div class="mono dim">RED CARD</div>' +
          '<p>Browsing is open. Purchasing requires an account. Selected pieces require an active Red Card.</p>' +
          '<a class="text-link" href="#/authorization">LEARN MORE</a>' +
        '</section>' +
      '</div>' +
    '</div>';
};

/* --------------------------------------------------------------------------
   Shop / catalog
   -------------------------------------------------------------------------- */
RW.setShopCategoryFromSlug = function (slug) {
  var cat = slug ? RW.categoryBySlug(slug) : null;
  RW.state.filters.category = cat ? cat.key : "ALL";
  RW.state.filters.sub = "ALL";
};

RW.syncShopHash = function () {
  var cat = RW.categoryByKey(RW.state.filters.category);
  var target = "#/shop" + (cat ? "/" + cat.slug : "");
  if (window.location.hash !== target && window.history && window.history.replaceState) {
    window.history.replaceState(null, "", target);
  }
};

RW.gendersList = function () {
  var set = {};
  RW.PRODUCTS.forEach(function (p) { if (p.gender !== "ALL") set[p.gender] = true; });
  return Object.keys(set);
};

RW.filteredProducts = function () {
  var f = RW.state.filters;
  var list = RW.PRODUCTS.slice();

  if (f.gender !== "ALL") list = list.filter(function (p) { return p.gender === f.gender || p.gender === "ALL"; });
  if (f.category !== "ALL") list = list.filter(function (p) { return p.category === f.category; });
  if (f.sub !== "ALL") list = list.filter(function (p) { return p.sub === f.sub; });
  if (f.stream !== "ALL") list = list.filter(function (p) { return p.stream === f.stream; });

  var q = String(f.query || "").trim().toLowerCase();
  if (q) {
    list = list.filter(function (p) {
      return [p.name, p.category, p.sub || "", p.stream, p.desc].join(" ").toLowerCase().indexOf(q) !== -1;
    });
  }

  if (f.sort === "PRICE_LOW") list.sort(function (a, b) { return a.price - b.price; });
  else if (f.sort === "PRICE_HIGH") list.sort(function (a, b) { return b.price - a.price; });
  else if (f.sort === "NAME") list.sort(function (a, b) { return a.name.localeCompare(b.name); });

  return list;
};

RW.chip = function (filter, value, label, active, extraClass) {
  return '<button type="button" class="filter-chip' + (active ? " active" : "") + (extraClass ? " " + extraClass : "") +
    '" data-filter="' + filter + '" data-value="' + RW.escapeHtml(value) + '">' + label + '</button>';
};

RW.renderShop = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var f = RW.state.filters;
  var catInfo = RW.categoryByKey(f.category);

  var catChips = [RW.chip("category", "ALL", "ALL", f.category === "ALL")]
    .concat(RW.CATEGORIES.map(function (c) { return RW.chip("category", c.key, RW.escapeHtml(c.key), f.category === c.key); }))
    .concat(RW.FUTURE_CATEGORIES.map(function (c) {
      return RW.chip("category", c.key, RW.escapeHtml(c.key) + '<span class="dev">STILL DEVELOPING</span>', f.category === c.key, "future");
    })).join("");

  var subRow = "";
  if (catInfo && catInfo.subs && catInfo.subs.length) {
    subRow = '<div class="filters-group sub">' +
      [RW.chip("sub", "ALL", "ALL " + RW.escapeHtml(catInfo.key), f.sub === "ALL")]
        .concat(catInfo.subs.map(function (s) { return RW.chip("sub", s, RW.escapeHtml(s), f.sub === s); })).join("") +
      '</div>';
  }

  var streamChips = ["ALL", "MAINSTREAM", "SECONDSTREAM"].map(function (s) {
    return RW.chip("stream", s, s === "ALL" ? "ALL DIRECTIONS" : s, f.stream === s);
  }).join("");

  var genders = RW.gendersList();
  if (!genders.length) RW.state.filters.gender = "ALL";
  var genderChips = genders.length
    ? '<span class="sep" aria-hidden="true"></span>' + ["ALL"].concat(genders).map(function (g) {
        return RW.chip("gender", g, g === "ALL" ? "EVERYONE" : g, f.gender === g);
      }).join("")
    : "";

  var title = f.category === "ALL" ? "SHOP" : f.category;

  app.innerHTML =
    '<div class="page shop-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker catalog-count" id="catalog-count">&nbsp;</div>' +
          '<h1 class="display">' + RW.escapeHtml(title) + '</h1>' +
        '</div>' +
        '<div class="filters">' +
          '<div class="filters-group">' + catChips + '</div>' +
          subRow +
          '<div class="filters-meta">' +
            streamChips +
            genderChips +
            '<span class="sep" aria-hidden="true"></span>' +
            '<select id="sort-select" class="select" aria-label="Sort">' +
              '<option value="DEFAULT">SORT</option>' +
              '<option value="PRICE_LOW">PRICE: LOW TO HIGH</option>' +
              '<option value="PRICE_HIGH">PRICE: HIGH TO LOW</option>' +
              '<option value="NAME">NAME: A&ndash;Z</option>' +
            '</select>' +
            '<input id="shop-search" class="search-input" type="search" placeholder="SEARCH" aria-label="Search pieces" value="' + RW.escapeHtml(f.query) + '">' +
          '</div>' +
        '</div>' +
        '<div class="grid" id="shop-grid"></div>' +
      '</div>' +
    '</div>';

  RW.renderShopResults();

  var sortSelect = RW.qs("#sort-select");
  if (sortSelect) {
    sortSelect.value = f.sort;
    sortSelect.addEventListener("change", function (e) {
      RW.state.filters.sort = e.target.value;
      RW.renderShopResults();
    });
  }

  var search = RW.qs("#shop-search");
  if (search) {
    search.addEventListener("input", function (e) {
      RW.state.filters.query = e.target.value;
      RW.renderShopResults();
    });
  }

  RW.qsa("[data-filter]", app).forEach(function (btn) {
    btn.addEventListener("click", function () {
      var key = btn.getAttribute("data-filter");
      var val = btn.getAttribute("data-value");
      RW.state.filters[key] = val;
      if (key === "category") RW.state.filters.sub = "ALL";
      RW.renderShop();
      if (key === "category") RW.syncShopHash();
    });
  });
};

RW.renderShopResults = function () {
  var gridEl = RW.qs("#shop-grid");
  var countEl = RW.qs("#catalog-count");
  if (!gridEl) return;
  var f = RW.state.filters;

  if (RW.isFutureCategory(f.category)) {
    gridEl.innerHTML =
      '<div class="empty-note">' +
        '<div class="display">' + RW.escapeHtml(f.category) + '</div>' +
        '<div class="mono">STILL DEVELOPING</div>' +
        '<p style="margin:6px 0 0;max-width:40ch;">No pieces have been released in this category.</p>' +
      '</div>';
    if (countEl) countEl.textContent = "SHOP";
    return;
  }

  if (f.category !== "ALL" && !RW.PRODUCTS.some(function (p) { return p.category === f.category; })) {
    gridEl.innerHTML =
      '<div class="empty-note">' +
        '<div class="display">' + RW.escapeHtml(f.category) + '</div>' +
        '<p style="margin:6px 0 0;max-width:40ch;">No pieces have been released in this category yet.</p>' +
      '</div>';
    if (countEl) countEl.textContent = "0 PIECES";
    return;
  }

  var list = RW.filteredProducts();
  if (countEl) countEl.textContent = list.length + (list.length === 1 ? " PIECE" : " PIECES");
  gridEl.innerHTML = list.length
    ? list.map(function (p) { return RW.productCardHtml(p); }).join("")
    : '<div class="empty-note"><div class="mono">NO PIECES MATCH THE CURRENT SELECTION.</div>' +
      '<button type="button" class="text-link" id="reset-filters">RESET</button></div>';

  var reset = RW.qs("#reset-filters");
  if (reset) {
    reset.addEventListener("click", function () {
      RW.state.filters = { gender: "ALL", category: "ALL", sub: "ALL", stream: "ALL", sort: "DEFAULT", query: "" };
      RW.renderShop();
      RW.syncShopHash();
    });
  }
};

/* --------------------------------------------------------------------------
   Collection pages — Mainstream / Secondstream
   -------------------------------------------------------------------------- */
RW.renderCollection = function (stream) {
  var app = RW.qs("#app");
  if (!app) return;
  var d = RW.DIRECTIONS[stream];
  var other = stream === "MAINSTREAM" ? RW.DIRECTIONS.SECONDSTREAM : RW.DIRECTIONS.MAINSTREAM;
  var active = RW.state.collectionCat[stream] || "ALL";

  var all = RW.PRODUCTS.filter(function (p) { return p.stream === stream; });
  var cats = RW.CATEGORIES.filter(function (c) { return all.some(function (p) { return p.category === c.key; }); });
  if (active !== "ALL" && !cats.some(function (c) { return c.key === active; })) active = "ALL";
  var list = active === "ALL" ? all : all.filter(function (p) { return p.category === active; });

  var chips = [RW.chip("ccat", "ALL", "ALL", active === "ALL")]
    .concat(cats.map(function (c) { return RW.chip("ccat", c.key, RW.escapeHtml(c.key), active === c.key); })).join("");

  app.innerHTML =
    '<div class="page collection-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker">COLLECTION</div>' +
          '<h1 class="display">' + RW.escapeHtml(stream) + '</h1>' +
          '<p class="lede">' + RW.escapeHtml(d.desc) + '</p>' +
        '</div>' +
        '<div class="filters"><div class="filters-group">' + chips + '</div></div>' +
        '<div class="grid">' +
          (list.length
            ? list.map(function (p) { return RW.productCardHtml(p, { hideMarker: true }); }).join("")
            : '<div class="empty-note"><div class="mono">NO PIECES IN THIS COLLECTION YET.</div></div>') +
        '</div>' +
        '<div class="home-note" style="padding-bottom:0;">' +
          '<a class="text-link" href="#/' + other.slug + '">' + RW.escapeHtml(other.key) + '</a>' +
        '</div>' +
      '</div>' +
    '</div>';

  RW.qsa('[data-filter="ccat"]', app).forEach(function (btn) {
    btn.addEventListener("click", function () {
      RW.state.collectionCat[stream] = btn.getAttribute("data-value");
      RW.renderCollection(stream);
    });
  });
};

/* --------------------------------------------------------------------------
   Product sound
   A product may carry `audio: { src, volume, loop }` (see data.js). The track
   plays while that product's page is open and fades out when the visitor
   leaves. Browsers block sound until the visitor has interacted with the page,
   so playback starts from the click on the product link where possible, and
   otherwise from the first click, key press or tap on the page. SOUND on the
   product page switches it off; the choice is remembered.
   -------------------------------------------------------------------------- */
RW.SOUND_KEY = "rw_sound_v1";                /* "off" when the visitor has muted product sound */
RW.sound = { el: null, src: null, id: null, fade: null, arm: null };

RW.soundEnabled = function () { return RW.store.get(RW.SOUND_KEY, "on") !== "off"; };

RW.soundFade = function (el, target, ms, done) {
  var s = RW.sound;
  clearInterval(s.fade);
  s.fade = null;
  var from = el.volume;
  if (from === target) { if (done) done(); return; }
  var steps = Math.max(1, Math.round(ms / 50)), n = 0;
  s.fade = setInterval(function () {
    n++;
    el.volume = Math.max(0, Math.min(1, from + (target - from) * (n / steps)));
    if (n >= steps) {
      clearInterval(s.fade);
      s.fade = null;
      if (done) done();
    }
  }, 50);
};

RW.soundDisarm = function () {
  var s = RW.sound;
  if (!s.arm) return;
  ["click", "keydown", "touchend"].forEach(function (t) { document.removeEventListener(t, s.arm, true); });
  s.arm = null;
};

/* Blocked by the browser: try again on the visitor's next interaction. */
RW.soundArm = function (p) {
  var s = RW.sound;
  RW.soundDisarm();
  s.arm = function () {
    RW.soundDisarm();
    if (s.id === p.id) RW.soundStart(p);
  };
  ["click", "keydown", "touchend"].forEach(function (t) { document.addEventListener(t, s.arm, true); });
};

RW.soundStart = function (p) {
  var s = RW.sound;
  var a = p && p.audio;
  if (!a || !a.src || !RW.soundEnabled()) { RW.soundStop(); return; }
  /* already playing (or finished) for this product, e.g. started from the link click */
  if (s.id === p.id && s.el && (!s.el.paused || s.el.ended)) return;

  clearInterval(s.fade);
  s.fade = null;
  RW.soundDisarm();

  if (!s.el) { s.el = new Audio(); s.el.preload = "auto"; }
  var el = s.el;
  if (s.src !== a.src) { el.src = a.src; s.src = a.src; }
  el.loop = !!a.loop;
  try { el.currentTime = 0; } catch (e) {}
  var target = typeof a.volume === "number" ? Math.max(0, Math.min(1, a.volume)) : 0.5;
  el.volume = 0;
  s.id = p.id;

  var started = el.play();
  var fadeIn = function () { if (s.id === p.id) RW.soundFade(el, target, 1400); };
  if (started && started.then) {
    started.then(fadeIn, function (err) {
      if (s.id === p.id && err && err.name === "NotAllowedError") RW.soundArm(p);
    });
  } else {
    fadeIn();
  }
};

RW.soundStop = function () {
  var s = RW.sound;
  RW.soundDisarm();
  s.id = null;
  var el = s.el;
  if (!el || el.paused) { clearInterval(s.fade); s.fade = null; return; }
  RW.soundFade(el, 0, 700, function () { el.pause(); });
};

/* Start from the click itself: this is the gesture that lets every browser play. */
RW.soundPrime = function (e) {
  if (e.defaultPrevented || e.button > 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
  var a = e.target && e.target.closest ? e.target.closest('a[href^="#/product/"]') : null;
  if (!a) return;
  var id = a.getAttribute("href").replace("#/product/", "");
  try { id = decodeURIComponent(id); } catch (err) {}
  var p = RW.findProduct(RW.LEGACY_IDS[id] || id);
  if (p && p.audio) RW.soundStart(p);
};

/* --------------------------------------------------------------------------
   Product details (full page — V1.1 detail markup and bindings, enlarged)
   -------------------------------------------------------------------------- */
RW.productDetailHtml = function (p) {
  var sizes = RW.availableSizes(p);
  var restricted = RW.isRestricted(p);
  var user = RW.currentUser();
  var cat = RW.categoryByKey(p.category);
  var dir = RW.DIRECTIONS[p.stream];

  var specs = {};
  Object.keys(p.specs || {}).forEach(function (k) { specs[k] = p.specs[k]; });
  specs.CATEGORY = p.category + (p.sub ? " / " + p.sub : "");
  specs.DIRECTION = p.stream;
  var variants = RW.variantsOf(p);
  var vSel = RW.variantOf(p, RW.state.pd.variant);
  if (vSel.label) specs.COLOUR = variants.map(function (v) { return v.label; }).join(" / ");

  var specsRows = Object.keys(specs).map(function (k) {
    return '<div class="row"><span class="k">' + RW.escapeHtml(k) + '</span><span>' + RW.escapeHtml(specs[k]) + '</span></div>';
  }).join("");

  var sizeButtons = sizes.map(function (s) {
    var avail = p.sizes[s];
    return '<button type="button" class="size-btn" data-size="' + s + '"' + (avail ? "" : " disabled") + '>' + s + '</button>';
  }).join("");

  var imgCount = RW.imagesOf(p, vSel.key).length;
  var thumbCount = imgCount || 3;
  var labels = p.imageLabels || [];
  var thumbs = "";
  for (var i = 0; i < thumbCount; i++) {
    var tl = labels[i] || "";
    thumbs += '<button type="button" class="pd-thumb' + (i === 0 ? " active" : "") + '" data-thumb="' + i + '" aria-label="' + (tl ? "View " + tl.toLowerCase() : "View image " + (i + 1)) + '">' +
      RW.mediaHtml(p, i, null, vSel.key) + (tl ? '<span class="pd-thumb-label">' + RW.escapeHtml(tl) + '</span>' : "") + '</button>';
  }

  var colourRow = "";
  if (variants.length > 1) {
    colourRow =
      '<div class="size-row">' +
        '<div class="size-label"><span class="mono">COLOUR</span><span class="mono dim" id="pd-colour-hint">' + RW.escapeHtml(vSel.label || "") + '</span></div>' +
        '<div class="sizes">' + variants.map(function (v) {
          return '<button type="button" class="colour-btn' + (v.key === vSel.key ? " active" : "") + '" data-variant="' + RW.escapeHtml(v.key) + '">' + RW.escapeHtml(v.label || v.key) + '</button>';
        }).join("") + '</div>' +
      '</div>';
  } else if (vSel.label) {
    colourRow = '<div class="size-row"><div class="size-label" style="margin-bottom:0;"><span class="mono">COLOUR</span><span class="colour-static">' + RW.escapeHtml(vSel.label) + '</span></div></div>';
  }

  var authNotice = "";
  if (restricted) {
    authNotice = '<div class="pd-auth-notice"><span class="t1">RED CARD REQUIRED</span><span class="t2">' +
      (RW.hasRedCard(user) ? "YOUR RED CARD IS ACTIVE" : "PURCHASE REQUIRES AN ACTIVE RED CARD") + '</span></div>';
  }

  var hint = "";
  if (!user) hint = "PURCHASING REQUIRES AN ACCOUNT.";
  else if (restricted && !RW.hasRedCard(user)) hint = "THIS PIECE REQUIRES AN ACTIVE RED CARD.";

  var crumbs =
    '<nav class="crumbs mono" aria-label="Breadcrumb">' +
      '<a href="#/shop">SHOP</a><span>/</span>' +
      '<a href="#/shop/' + (cat ? cat.slug : "") + '">' + RW.escapeHtml(p.category) + '</a>' +
      (p.sub ? '<span>/</span><span>' + RW.escapeHtml(p.sub) + '</span>' : "") +
    '</nav>';

  return (
    crumbs +
    '<div class="pd">' +
      '<div>' +
        '<div class="pd-gallery-main' + (imgCount ? " zoomable" : "") + '"><div id="pd-main" class="pd-main-media">' + RW.mediaHtml(p, 0, null, vSel.key) + '</div>' +
          (labels[0] ? '<span class="pd-view-label" id="pd-view-label">' + RW.escapeHtml(labels[0]) + '</span>' : "") +
          (imgCount ? '<button type="button" class="pd-zoom-btn" id="pd-zoom-btn" aria-label="Enlarge image">ZOOM</button>' : "") + '</div>' +
        '<div class="pd-thumbs">' + thumbs + '</div>' +
      '</div>' +
      '<div class="pd-info">' +
        '<div class="code-line mono">' +
          '<a href="#/' + dir.slug + '">' + RW.escapeHtml(p.stream) + '</a>' +
          '<span>' + RW.escapeHtml(p.category) + '</span>' +
          (restricted ? '<span class="restricted">RESTRICTED</span>' : (p.status === "LOW STOCK" ? "<span>LOW STOCK</span>" : "")) +
          (p.audio ? '<button type="button" class="sound-toggle" id="pd-sound" aria-pressed="' + (RW.soundEnabled() ? "true" : "false") + '">' + (RW.soundEnabled() ? "SOUND ON" : "SOUND OFF") + '</button>' : "") +
        '</div>' +
        '<h1 class="display">' + RW.escapeHtml(p.name) + '</h1>' +
        '<div class="pd-price">' + RW.money(p.price) + '</div>' +
        '<p class="pd-desc">' + RW.escapeHtml(p.desc) + '</p>' +
        authNotice +
        colourRow +
        '<div class="size-row">' +
          '<div class="size-label"><span class="mono">SIZE</span><span class="mono dim" id="pd-size-hint">SELECT</span></div>' +
          '<div class="sizes">' + sizeButtons + '</div>' +
        '</div>' +
        '<div class="qty-row">' +
          '<div class="qty-control">' +
            '<button type="button" id="pd-qty-minus" aria-label="Decrease quantity">&minus;</button>' +
            '<span id="pd-qty-value">1</span>' +
            '<button type="button" id="pd-qty-plus" aria-label="Increase quantity">+</button>' +
          '</div>' +
          '<span class="qty-limit" id="pd-qty-limit" aria-live="polite"></span>' +
        '</div>' +
        '<div class="pd-add-row">' +
          '<button type="button" class="btn primary block" id="pd-buy-btn">PURCHASE</button>' +
          '<button type="button" class="btn block" id="pd-add-btn">ADD TO BAG</button>' +
          (hint ? '<div class="mono pd-hint">' + hint + '</div>' : "") +
        '</div>' +
        '<div class="specs">' + specsRows + '</div>' +
      '</div>' +
    '</div>'
  );
};

/* --------------------------------------------------------------------------
   Quantity limits on the product page.
   Max 99. Reaching 67 the first time: a full-screen "67" blinks for 2 s,
   then "Are you stupid?" fades out over 2 s and the quantity returns to 1.
   It happens once per browser (remembered in localStorage).
   -------------------------------------------------------------------------- */
RW.QTY_MAX = 99;
RW.QTY67_KEY = "rw_qty67_seen_v1";

RW.qty67 = function (done) {
  var el = document.createElement("div");
  el.className = "qty67";
  el.setAttribute("aria-live", "assertive");
  el.innerHTML = '<span class="qty67-num">67</span>';
  document.body.appendChild(el);

  setTimeout(function () {
    el.innerHTML = '<span class="qty67-msg">Are you stupid?</span>';
    el.classList.add("fading");
    if (done) done();
    setTimeout(function () { if (el.parentNode) el.parentNode.removeChild(el); }, 2000);
  }, 2000);
};

RW.bindProductDetail = function (p) {
  var root = RW.qs(".product-page");
  if (!root) return;

  var soundBtn = RW.qs("#pd-sound", root);
  if (soundBtn) {
    soundBtn.addEventListener("click", function () {
      var on = !RW.soundEnabled();
      RW.store.set(RW.SOUND_KEY, on ? "on" : "off");
      soundBtn.textContent = on ? "SOUND ON" : "SOUND OFF";
      soundBtn.setAttribute("aria-pressed", on ? "true" : "false");
      if (on) RW.soundStart(p); else RW.soundStop();
    });
  }

  var main = RW.qs("#pd-main", root);
  RW.qsa(".pd-thumb", root).forEach(function (t) {
    t.addEventListener("click", function () {
      RW.qsa(".pd-thumb", root).forEach(function (o) { o.classList.remove("active"); });
      t.classList.add("active");
      RW.state.pd.thumb = parseInt(t.getAttribute("data-thumb"), 10);
      if (main) main.innerHTML = RW.mediaHtml(p, RW.state.pd.thumb, null, RW.state.pd.variant);
      var vl = RW.qs("#pd-view-label", root);
      if (vl && p.imageLabels) vl.textContent = p.imageLabels[RW.state.pd.thumb] || "";
    });
  });

  RW.qsa(".colour-btn", root).forEach(function (btn) {
    btn.addEventListener("click", function () {
      RW.qsa(".colour-btn", root).forEach(function (o) { o.classList.remove("active"); });
      btn.classList.add("active");
      RW.state.pd.variant = btn.getAttribute("data-variant");
      var v = RW.variantOf(p, RW.state.pd.variant);
      var ch = RW.qs("#pd-colour-hint", root);
      if (ch) ch.textContent = v.label || "";
      if (main) main.innerHTML = RW.mediaHtml(p, RW.state.pd.thumb, null, v.key);
      RW.qsa(".pd-thumb", root).forEach(function (t) {
        var i = parseInt(t.getAttribute("data-thumb"), 10);
        var lab = RW.qs(".pd-thumb-label", t);
        t.innerHTML = RW.mediaHtml(p, i, null, v.key);
        if (lab) t.appendChild(lab);
      });
    });
  });

  RW.qsa(".size-btn", root).forEach(function (btn) {
    if (btn.disabled) return;
    btn.addEventListener("click", function () {
      RW.qsa(".size-btn", root).forEach(function (o) { o.classList.remove("active"); });
      btn.classList.add("active");
      RW.state.pd.size = btn.getAttribute("data-size");
      var hint = RW.qs("#pd-size-hint", root);
      if (hint) hint.textContent = RW.state.pd.size;
    });
  });

  var qtyVal = RW.qs("#pd-qty-value", root);
  var qtyLimit = RW.qs("#pd-qty-limit", root);
  var minusBtn = RW.qs("#pd-qty-minus", root);
  var plusBtn = RW.qs("#pd-qty-plus", root);
  var setQty = function (n) {
    RW.state.pd.qty = n;
    if (qtyVal) qtyVal.textContent = n;
    if (qtyLimit && n < RW.QTY_MAX) qtyLimit.textContent = "";
  };
  if (minusBtn) {
    minusBtn.addEventListener("click", function () {
      if (RW.state.pd.qty > 1) setQty(RW.state.pd.qty - 1);
    });
  }
  if (plusBtn) {
    plusBtn.addEventListener("click", function () {
      if (RW.state.pd.qty >= RW.QTY_MAX) {
        if (qtyLimit) qtyLimit.textContent = "fucking chill, you dont have so much money";
        return;
      }
      setQty(RW.state.pd.qty + 1);
      if (RW.state.pd.qty === 67 && !RW.store.get(RW.QTY67_KEY, false)) {
        RW.store.set(RW.QTY67_KEY, true);
        RW.qty67(function () { setQty(1); });
      }
    });
  }

  var needSize = function () {
    if (RW.availableSizes(p).length && !RW.state.pd.size) { RW.toast("SELECT A SIZE"); return true; }
    return false;
  };

  /* ADD TO BAG — open for regular pieces; restricted pieces need a Red Card (as in V1.1). */
  var addBtn = RW.qs("#pd-add-btn", root);
  if (addBtn) {
    addBtn.addEventListener("click", function () {
      if (needSize()) return;
      if (RW.isRestricted(p)) {
        var blocker = RW.purchaseBlocker([{ id: p.id }]);
        if (blocker) { RW.redirectForAuthorization(blocker, "product/" + p.id); return; }
      }
      RW.addToCart(p.id, RW.state.pd.size, RW.state.pd.qty, RW.variantOf(p, RW.state.pd.variant).key);
      RW.toast("ADDED TO BAG");
    });
  }

  /* PURCHASE — Details → Purchase → Authorization / Red Card → Checkout */
  var buyBtn = RW.qs("#pd-buy-btn", root);
  if (buyBtn) {
    buyBtn.addEventListener("click", function () {
      if (needSize()) return;
      var blocker = RW.purchaseBlocker([{ id: p.id }]);
      if (blocker) { RW.redirectForAuthorization(blocker, "product/" + p.id); return; }
      RW.addToCart(p.id, RW.state.pd.size, RW.state.pd.qty, RW.variantOf(p, RW.state.pd.variant).key);
      RW.go("checkout");
    });
  }
};

RW.renderProduct = function (id) {
  var app = RW.qs("#app");
  if (!app) return;
  var p = RW.findProduct(RW.LEGACY_IDS[id] || id);
  if (!p) {
    RW.soundStop();
    app.innerHTML =
      '<div class="page"><div class="wrap"><div class="page-head">' +
        '<div class="mono kicker">SHOP</div><h1 class="display">NOT FOUND</h1>' +
        '<p class="lede">This piece is not available.</p>' +
        '<div style="margin-top:30px;"><a class="text-link" href="#/shop">RETURN TO SHOP</a></div>' +
      '</div></div></div>';
    return;
  }
  RW.state.pd = { id: p.id, size: null, qty: 1, thumb: 0, variant: RW.variantsOf(p)[0].key };
  app.innerHTML = '<div class="page product-page"><div class="wrap">' + RW.productDetailHtml(p) + '</div></div>';
  RW.bindProductDetail(p);
  RW.bindGalleryZoom(p);
  RW.soundStart(p); /* stops any other product's track; plays this one if it has audio */
};

/* --------------------------------------------------------------------------
   Image zoom
   Product page: with a mouse, the main image is magnified under the cursor.
   A click on the image (or ZOOM) opens the viewer: full screen on the photo
   background. Click or tap zooms in at that point; wheel, pinch or +/-
   adjust; drag moves the image. Arrow keys / swipe change the view.
   A variant may carry `zoom: [front, back]` — larger files used only in the
   viewer. Without it the regular product images are used.
   -------------------------------------------------------------------------- */
RW.ZOOM = { TAP: 2.5, MAX: 4, STEP: 1.6 };
RW.zoom = { el: null, open: false, p: null, variant: "", index: 0, s: 1, x: 0, y: 0, w: 0, h: 0, sw: 0, sh: 0, pointers: {}, g: null, opener: null };

RW.zoomFine = function () {
  return !!(window.matchMedia && window.matchMedia("(hover: hover) and (pointer: fine)").matches);
};

RW.zoomSources = function (p, key) {
  var v = RW.variantOf(p, key);
  return (v.images || []).map(function (src, i) { return (v.zoom && v.zoom[i]) || src; });
};

/* Product page gallery: hover magnification + open the viewer. */
RW.bindGalleryZoom = function (p) {
  var gal = RW.qs(".product-page .pd-gallery-main.zoomable");
  if (!gal) return;

  if (RW.zoomFine()) {
    var aim = function (e) {
      var img = RW.qs("#pd-main img.ph-photo", gal);
      if (!img) return;
      var r = gal.getBoundingClientRect();
      img.style.transformOrigin = ((e.clientX - r.left) / r.width * 100) + "% " + ((e.clientY - r.top) / r.height * 100) + "%";
    };
    gal.addEventListener("mouseenter", function (e) { aim(e); gal.classList.add("is-magnified"); });
    gal.addEventListener("mousemove", aim);
    gal.addEventListener("mouseleave", function () { gal.classList.remove("is-magnified"); });
  }

  gal.addEventListener("click", function () {
    gal.classList.remove("is-magnified");
    RW.zoomOpen(p, RW.state.pd.variant, RW.state.pd.thumb || 0);
  });
};

RW.zoomEnsure = function () {
  var z = RW.zoom;
  if (z.el) return z.el;

  var el = document.createElement("div");
  el.id = "zoom-viewer";
  el.className = "zoom-viewer";
  el.setAttribute("role", "dialog");
  el.setAttribute("aria-modal", "true");
  el.setAttribute("aria-label", "Image viewer");
  el.innerHTML =
    '<div class="zv-bar">' +
      '<span class="zv-title" id="zv-title"></span>' +
      '<button type="button" class="zv-btn" id="zv-close">CLOSE</button>' +
    '</div>' +
    '<div class="zv-stage" id="zv-stage"><img class="zv-img" id="zv-img" alt="" draggable="false"></div>' +
    '<div class="zv-bar zv-foot">' +
      '<div class="zv-nav" id="zv-nav">' +
        '<button type="button" class="zv-btn" id="zv-prev">PREVIOUS</button>' +
        '<span class="zv-count" id="zv-count"></span>' +
        '<button type="button" class="zv-btn" id="zv-next">NEXT</button>' +
      '</div>' +
      '<span class="zv-hint" id="zv-hint"></span>' +
      '<div class="zv-levels">' +
        '<button type="button" class="zv-btn zv-step" id="zv-out" aria-label="Zoom out">&minus;</button>' +
        '<span class="zv-count zv-level" id="zv-level">100%</span>' +
        '<button type="button" class="zv-btn zv-step" id="zv-in" aria-label="Zoom in">+</button>' +
      '</div>' +
    '</div>';
  document.body.appendChild(el);
  z.el = el;

  var stage = RW.qs("#zv-stage", el);
  var local = function (e) {
    var r = stage.getBoundingClientRect();
    return { x: e.clientX - r.left, y: e.clientY - r.top };
  };
  var dist = function (a, b) { return Math.sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)) || 1; };
  var mid = function (a, b) { return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 }; };
  var pts = function () { return Object.keys(z.pointers).map(function (k) { return z.pointers[k]; }); };

  RW.qs("#zv-close", el).addEventListener("click", function () { RW.zoomClose(); });
  RW.qs("#zv-prev", el).addEventListener("click", function () { RW.zoomShow(z.index - 1); });
  RW.qs("#zv-next", el).addEventListener("click", function () { RW.zoomShow(z.index + 1); });
  RW.qs("#zv-in", el).addEventListener("click", function () { RW.zoomAt(z.sw / 2, z.sh / 2, z.s * RW.ZOOM.STEP, true); });
  RW.qs("#zv-out", el).addEventListener("click", function () { RW.zoomAt(z.sw / 2, z.sh / 2, z.s / RW.ZOOM.STEP, true); });

  stage.addEventListener("wheel", function (e) {
    e.preventDefault();
    var q = local(e);
    var k = e.deltaMode === 1 ? 0.05 : 0.0022;
    RW.zoomAt(q.x, q.y, z.s * Math.exp(-e.deltaY * k), false);
  }, { passive: false });

  stage.addEventListener("pointerdown", function (e) {
    if (e.pointerType === "mouse" && e.button !== 0) return;
    try { stage.setPointerCapture(e.pointerId); } catch (err) {}
    z.pointers[e.pointerId] = local(e);
    var list = pts();
    if (list.length === 1) {
      z.g = { type: "pan", sx: list[0].x, sy: list[0].y, ox: z.x, oy: z.y, moved: false };
    } else if (list.length === 2) {
      z.g = { type: "pinch", d: dist(list[0], list[1]), m: mid(list[0], list[1]), moved: true };
    }
  });

  stage.addEventListener("pointermove", function (e) {
    if (!z.pointers[e.pointerId] || !z.g) return;
    z.pointers[e.pointerId] = local(e);
    var list = pts();
    if (z.g.type === "pan" && list.length === 1) {
      var dx = list[0].x - z.g.sx, dy = list[0].y - z.g.sy;
      if (!z.g.moved && Math.abs(dx) + Math.abs(dy) > 6) z.g.moved = true;
      if (z.g.moved && z.s > 1) {
        stage.classList.add("is-dragging");
        RW.zoomSet(z.s, z.g.ox + dx, z.g.oy + dy, false);
      }
    } else if (z.g.type === "pinch" && list.length >= 2) {
      var d = dist(list[0], list[1]), m = mid(list[0], list[1]);
      RW.zoomAt(m.x, m.y, z.s * d / z.g.d, false);
      RW.zoomSet(z.s, z.x + (m.x - z.g.m.x), z.y + (m.y - z.g.m.y), false);
      z.g.d = d;
      z.g.m = m;
    }
  });

  var end = function (e) {
    if (!z.pointers[e.pointerId]) return;
    var q = z.pointers[e.pointerId];
    delete z.pointers[e.pointerId];
    stage.classList.remove("is-dragging");
    var g = z.g;
    var list = pts();

    if (list.length === 1) {
      /* one finger left after a pinch: continue as a drag, never as a tap */
      z.g = { type: "pan", sx: list[0].x, sy: list[0].y, ox: z.x, oy: z.y, moved: true };
      return;
    }
    z.g = null;
    if (!g || g.type !== "pan" || e.type !== "pointerup") return;

    if (!g.moved) {
      if (z.s > 1.01) RW.zoomSet(1, 0, 0, true);
      else RW.zoomAt(q.x, q.y, RW.ZOOM.TAP, true);
    } else if (z.s <= 1.01 && e.pointerType !== "mouse") {
      /* swipe at fit scale changes the view */
      var dx = q.x - g.sx, dy = q.y - g.sy;
      if (Math.abs(dx) > 60 && Math.abs(dx) > Math.abs(dy) * 1.5) RW.zoomShow(z.index + (dx < 0 ? 1 : -1));
    }
  };
  stage.addEventListener("pointerup", end);
  stage.addEventListener("pointercancel", end);

  document.addEventListener("keydown", function (e) {
    if (!z.open) return;
    if (e.key === "Escape") { e.stopPropagation(); RW.zoomClose(); }
    else if (e.key === "ArrowRight") RW.zoomShow(z.index + 1);
    else if (e.key === "ArrowLeft") RW.zoomShow(z.index - 1);
    else if (e.key === "+" || e.key === "=") RW.zoomAt(z.sw / 2, z.sh / 2, z.s * RW.ZOOM.STEP, true);
    else if (e.key === "-" || e.key === "_") RW.zoomAt(z.sw / 2, z.sh / 2, z.s / RW.ZOOM.STEP, true);
    else if (e.key === "0") RW.zoomSet(1, 0, 0, true);
  }, true);

  window.addEventListener("resize", function () { if (z.open) RW.zoomLayout(); });
  return el;
};

/* Fit the image to the stage at scale 1. */
RW.zoomLayout = function () {
  var z = RW.zoom;
  var img = RW.qs("#zv-img"), stage = RW.qs("#zv-stage");
  if (!img || !stage || !img.naturalWidth) return;
  z.sw = stage.clientWidth;
  z.sh = stage.clientHeight;
  var fit = Math.min(z.sw / img.naturalWidth, z.sh / img.naturalHeight);
  z.w = img.naturalWidth * fit;
  z.h = img.naturalHeight * fit;
  img.style.width = z.w + "px";
  img.style.height = z.h + "px";
  img.style.left = (z.sw - z.w) / 2 + "px";
  img.style.top = (z.sh - z.h) / 2 + "px";
  RW.zoomSet(1, 0, 0, false);
};

RW.zoomSet = function (s, x, y, animate) {
  var z = RW.zoom;
  var img = RW.qs("#zv-img"), stage = RW.qs("#zv-stage");
  if (!img || !stage) return;
  s = Math.max(1, Math.min(RW.ZOOM.MAX, s));
  var mx = Math.max(0, (z.w * s - z.sw) / 2), my = Math.max(0, (z.h * s - z.sh) / 2);
  z.s = s;
  z.x = Math.max(-mx, Math.min(mx, x));
  z.y = Math.max(-my, Math.min(my, y));
  img.style.transition = animate ? "" : "opacity .3s ease";
  img.style.transform = "translate3d(" + z.x + "px," + z.y + "px,0) scale(" + s + ")";

  var zoomed = s > 1.01;
  stage.classList.toggle("is-zoomed", zoomed);
  var lv = RW.qs("#zv-level");
  if (lv) lv.textContent = Math.round(s * 100) + "%";
  var hint = RW.qs("#zv-hint");
  if (hint) {
    var fine = RW.zoomFine();
    hint.textContent = zoomed
      ? (fine ? "DRAG TO MOVE · CLICK TO RESET" : "DRAG TO MOVE · TAP TO RESET")
      : (fine ? "CLICK OR SCROLL TO ZOOM" : "TAP OR PINCH TO ZOOM");
  }
  var out = RW.qs("#zv-out"), inn = RW.qs("#zv-in");
  if (out) out.disabled = !zoomed;
  if (inn) inn.disabled = s >= RW.ZOOM.MAX - 0.001;
};

/* Zoom to scale s while keeping the point (px, py) of the stage in place. */
RW.zoomAt = function (px, py, s, animate) {
  var z = RW.zoom;
  s = Math.max(1, Math.min(RW.ZOOM.MAX, s));
  var r = s / z.s;
  var cx = z.sw / 2 + z.x, cy = z.sh / 2 + z.y;
  RW.zoomSet(s, px - (px - cx) * r - z.sw / 2, py - (py - cy) * r - z.sh / 2, animate);
};

RW.zoomShow = function (index) {
  var z = RW.zoom;
  var list = RW.zoomSources(z.p, z.variant);
  var n = list.length;
  if (!n) return;
  z.index = ((index % n) + n) % n;

  var img = RW.qs("#zv-img");
  var label = z.p.imageLabels && z.p.imageLabels[z.index];
  var vLabel = RW.variantOf(z.p, z.variant).label;
  RW.qs("#zv-title").textContent = z.p.name + (vLabel ? " · " + vLabel : "") + (label ? " · " + label : "");
  RW.qs("#zv-count").textContent = (z.index + 1) + " / " + n;
  RW.qs("#zv-nav").style.visibility = n > 1 ? "" : "hidden";
  img.alt = z.p.name + (label ? " — " + label.toLowerCase() : "");

  var src = list[z.index];
  img.onload = function () { RW.zoomLayout(); img.classList.remove("loading"); };
  if (img.getAttribute("src") === src && img.complete && img.naturalWidth) {
    RW.zoomLayout();
  } else {
    img.classList.add("loading");
    img.setAttribute("src", src);
  }
};

RW.zoomOpen = function (p, variant, index) {
  var z = RW.zoom;
  if (!p || !RW.zoomSources(p, variant).length) return;
  var el = RW.zoomEnsure();
  z.p = p;
  z.variant = variant || "";
  z.open = true;
  z.pointers = {};
  z.g = null;
  z.opener = document.activeElement;
  document.documentElement.classList.add("zv-lock");
  el.classList.add("open");
  RW.zoomShow(index || 0);
  var c = RW.qs("#zv-close", el);
  if (c) c.focus({ preventScroll: true });
};

RW.zoomClose = function () {
  var z = RW.zoom;
  if (!z.open) return;
  z.open = false;
  z.pointers = {};
  z.g = null;
  if (z.el) z.el.classList.remove("open");
  document.documentElement.classList.remove("zv-lock");

  /* keep the product page on the view that was last looked at */
  if (z.index !== (RW.state.pd.thumb || 0) && RW.state.pd.id === (z.p && z.p.id)) {
    var t = RW.qs('.product-page .pd-thumb[data-thumb="' + z.index + '"]');
    if (t) t.click();
  }
  if (z.opener && z.opener.focus && document.body.contains(z.opener)) z.opener.focus({ preventScroll: true });
  z.opener = null;
};

/* --------------------------------------------------------------------------
   Red Card document
   -------------------------------------------------------------------------- */
RW.redCardHtml = function (user, small) {
  var status = RW.redCardStatus(user);
  var rc = status !== "NOT ISSUED" ? user.redCard : null;   /* a revoked card keeps its identifier and date */
  return (
    '<div class="rc-doc' + (status === "ACTIVE" ? "" : " unissued") + (small ? " small" : "") + '">' +
      '<div class="rc-top">' +
        '<span class="rc-title">RED CARD</span>' +
        '<img class="rc-mark" src="img/rw-mark.png" alt="">' +
      '</div>' +
      '<div class="rc-fields">' +
        '<div style="grid-column:1 / -1;"><div class="f-label">HOLDER</div><div class="f-value">' + (rc ? RW.escapeHtml(user.name) : "&mdash;") + '</div></div>' +
        '<div><div class="f-label">ISSUED</div><div class="f-value">' + (rc ? RW.formatDate(rc.issued) : "&mdash;") + '</div></div>' +
        '<div><div class="f-label">STATUS</div><div class="f-value">' + status + '</div></div>' +
      '</div>' +
      '<div class="rc-id">' +
        '<div class="f-label">IDENTIFIER</div>' +
        '<div class="rc-number" id="rc-number">' + (rc ? RW.escapeHtml(rc.id) : "&mdash;") + '</div>' +
      '</div>' +
    '</div>'
  );
};

/* --------------------------------------------------------------------------
   Red Card application (requires an account)
   Date of birth and note are not stored. Country is saved to the profile.
   -------------------------------------------------------------------------- */
RW.openAuthModal = function () {
  var user = RW.currentUser();
  if (!user) { RW.redirectForAuthorization("ACCOUNT", "authorization"); return; }

  var html =
    '<div class="modal-close-row">' +
      '<button type="button" class="cart-close" id="auth-modal-close">CLOSE</button>' +
    '</div>' +
    '<h3>RED CARD APPLICATION</h3>' +
    '<p class="mono dim" style="text-align:center;line-height:1.8;margin:0 0 26px;">THE RED CARD IS ISSUED TO YOUR ACCOUNT.</p>' +
    '<form id="auth-form" novalidate>' +
      '<div class="field"><label for="auth-name">FULL NAME</label><input type="text" id="auth-name" required value="' + RW.escapeHtml(user.name) + '"></div>' +
      '<div class="field"><label for="auth-email">EMAIL</label><input type="email" id="auth-email" readonly value="' + RW.escapeHtml(user.email) + '"></div>' +
      '<div class="field-row">' +
        '<div class="field"><label for="auth-dob">DATE OF BIRTH</label><input type="date" id="auth-dob" required></div>' +
        '<div class="field"><label for="auth-country">COUNTRY</label><input type="text" id="auth-country" required value="' + RW.escapeHtml(user.profile && user.profile.country) + '"></div>' +
      '</div>' +
      '<div class="field"><label for="auth-note">NOTE (OPTIONAL)</label><textarea id="auth-note"></textarea></div>' +
      '<button type="submit" class="btn primary block" id="auth-submit-btn">SUBMIT</button>' +
      '<p class="mono dim form-note">PROTOTYPE. DATE OF BIRTH AND NOTE ARE NOT STORED.</p>' +
    '</form>';

  RW.openModal(html);

  var closeBtn = RW.qs("#auth-modal-close");
  if (closeBtn) closeBtn.addEventListener("click", RW.closeModal);

  var form = RW.qs("#auth-form");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();
      if (!form.checkValidity()) { form.reportValidity(); return; }
      var done = RW.busy(RW.qs("#auth-submit-btn"), "REVIEWING...");

      var name = RW.qs("#auth-name").value.trim();
      var country = RW.qs("#auth-country").value.trim();
      var pause = new Promise(function (resolve) { setTimeout(resolve, 1300); });

      Promise.all([RW.account.issueRedCard(name, country), pause]).then(function (out) {
        var res = out[0];
        if (res.error) { done(); RW.toast(res.error); return; }
        RW.closeModal();
        RW.toast("RED CARD ISSUED");
        if (RW.state.next) { RW.renderCart(); RW.continueAfterAuth(); return; }
        RW.refreshAccountViews();
      });
    });
  }
};

/* --------------------------------------------------------------------------
   Red Card page
   -------------------------------------------------------------------------- */
RW.renderAuthorization = function () {
  var app = RW.qs("#app");
  if (!app) return;
  var user = RW.currentUser();
  var active = RW.hasRedCard(user);
  var status = RW.redCardStatus(user);

  var actions;
  if (!user) {
    actions =
      '<a href="#/account" class="btn primary" id="auth-signin-btn">SIGN IN</a>' +
      '<a href="#/account" class="btn ghost" id="auth-create-btn">CREATE ACCOUNT</a>';
  } else if (status === "REVOKED") {
    actions = '<button type="button" class="btn primary" id="auth-reactivate-btn">REACTIVATE RED CARD</button>';
  } else if (!active) {
    actions = '<button type="button" class="btn primary" id="auth-apply-btn">APPLY FOR A RED CARD</button>';
  } else {
    actions =
      (RW.state.next ? '<button type="button" class="btn primary" id="auth-continue-btn">CONTINUE</button>' : '<a href="#/shop" class="btn primary">SHOP</a>') +
      '<button type="button" class="btn ghost" id="auth-revoke-btn">REVOKE RED CARD</button>';
  }

  app.innerHTML =
    '<div class="page auth-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker">AUTHORIZATION</div>' +
          '<h1 class="display">RED CARD</h1>' +
        '</div>' +
        '<div class="auth-grid">' +
          '<div class="auth-copy">' +
            '<p>The Red Card is a digital authorization document connected to your account. It carries a unique 94-digit identifier. The identifier and the date of issue are permanent.</p>' +
            '<p>Browsing is open to everyone. Purchasing requires an account. Pieces marked restricted require an active Red Card.</p>' +
            '<div class="auth-fields">' +
              '<div class="row"><span class="k">ACCOUNT</span><span class="v' + (user ? "" : " on") + '">' + (user ? "SIGNED IN" : "NOT SIGNED IN") + '</span></div>' +
              '<div class="row"><span class="k">RED CARD</span><span class="v' + (active ? "" : " on") + '">' + status + '</span></div>' +
              '<div class="row"><span class="k">RESTRICTED PURCHASE</span><span class="v' + (active ? "" : " on") + '">' + (active ? "PERMITTED" : "NOT PERMITTED") + '</span></div>' +
            '</div>' +
            '<div class="auth-actions">' + actions + '</div>' +
            '<p class="auth-contact">Red Card enquiries: <a href="mailto:' + RW.escapeHtml(RW.BRAND.redCardEmail) + '">' + RW.escapeHtml(RW.BRAND.redCardEmail) + '</a></p>' +
          '</div>' +
          '<div class="auth-doc-col">' + RW.redCardHtml(user) + '</div>' +
        '</div>' +
      '</div>' +
    '</div>';

  var createBtn = RW.qs("#auth-create-btn");
  if (createBtn) createBtn.addEventListener("click", function () { RW.state.accountTab = "create"; if (!RW.state.next) RW.state.next = "authorization"; });
  var signinBtn = RW.qs("#auth-signin-btn");
  if (signinBtn) signinBtn.addEventListener("click", function () { RW.state.accountTab = "signin"; if (!RW.state.next) RW.state.next = "authorization"; });

  var applyBtn = RW.qs("#auth-apply-btn");
  if (applyBtn) applyBtn.addEventListener("click", RW.openAuthModal);

  var contBtn = RW.qs("#auth-continue-btn");
  if (contBtn) contBtn.addEventListener("click", function () { RW.continueAfterAuth("shop"); });

  var reactivateBtn = RW.qs("#auth-reactivate-btn");
  if (reactivateBtn) {
    reactivateBtn.addEventListener("click", function () {
      var u = RW.currentUser();
      if (!u) return;
      var done = RW.busy(reactivateBtn, "REACTIVATING...");
      RW.account.issueRedCard(u.name).then(function (res) {
        if (res.error) { done(); RW.toast(res.error); return; }
        RW.toast("RED CARD REACTIVATED");
        if (RW.state.next) { RW.renderCart(); RW.continueAfterAuth(); return; }
        RW.refreshAccountViews();
      });
    });
  }

  var revokeBtn = RW.qs("#auth-revoke-btn");
  if (revokeBtn) {
    revokeBtn.addEventListener("click", function () {
      if (!RW.currentUser()) return;
      var done = RW.busy(revokeBtn);
      RW.account.revokeRedCard().then(function (res) {
        if (res.error) { done(); RW.toast(res.error); return; }
        RW.toast("RED CARD REVOKED");
        RW.refreshAccountViews();
      });
    });
  }
};

/* --------------------------------------------------------------------------
   Account
   -------------------------------------------------------------------------- */
RW.renderAccount = function () {
  var app = RW.qs("#app");
  if (!app) return;
  var user = RW.currentUser();
  if (!user) { RW.renderAccountAuth(); return; }

  var pr = user.profile || {};
  var orders = (user.orders || []).slice().reverse();
  var ordersHtml = orders.length
    ? orders.map(function (o) {
        return (
          '<div class="order">' +
            '<span class="mono">ORDER ' + RW.escapeHtml(o.ref) + '</span>' +
            '<span class="num">' + RW.money(o.total) + '</span>' +
            '<span class="mono dim">' + RW.formatDate(o.date) + '</span><span></span>' +
            '<div class="o-items">' + o.items.map(function (i) {
              return RW.escapeHtml(i.name) + (i.colour ? ' &middot; ' + RW.escapeHtml(i.colour) : "") + ' &middot; ' + RW.escapeHtml(i.size || "-") + ' &times; ' + i.qty;
            }).join("<br>") + '</div>' +
          '</div>'
        );
      }).join("")
    : '<p class="muted-empty">No orders yet.</p>';

  var field = function (id, label, value, type, extra) {
    return '<div class="field"><label for="' + id + '">' + label + '</label><input type="' + (type || "text") + '" id="' + id + '" value="' + RW.escapeHtml(value || "") + '"' + (extra || "") + '></div>';
  };

  app.innerHTML =
    '<div class="page account-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker">ACCOUNT</div>' +
          '<h1 class="display">' + RW.escapeHtml(user.name || "ACCOUNT") + '</h1>' +
          '<p class="lede">' + RW.escapeHtml(user.email) + '</p>' +
        '</div>' +
        '<div class="account-grid">' +
          '<div class="account-col">' +
            '<h2 class="block-title">PERSONAL INFORMATION</h2>' +
            '<form id="profile-form">' +
              field("pf-name", "FULL NAME", user.name, "text", " required") +
              field("pf-email", "EMAIL", user.email, "email", " readonly") +
              field("pf-phone", "PHONE", pr.phone, "tel") +
              '<div class="form-section">SAVED ADDRESS</div>' +
              '<div class="field-row">' + field("pf-country", "COUNTRY", pr.country) + field("pf-city", "CITY", pr.city) + '</div>' +
              field("pf-address", "ADDRESS", pr.address) +
              field("pf-postal", "POSTAL CODE", pr.postal) +
              '<button type="submit" class="btn primary block">SAVE</button>' +
            '</form>' +
          '</div>' +
          '<div class="account-side">' +
            '<div class="block">' +
              '<h2 class="block-title">RED CARD</h2>' +
              RW.redCardHtml(user, true) +
              '<div style="margin-top:20px;">' +
                (RW.hasRedCard(user)
                  ? '<a class="text-link" href="#/authorization">VIEW RED CARD</a>'
                  : RW.redCardStatus(user) === "REVOKED"
                    ? '<a class="text-link" href="#/authorization">REACTIVATE RED CARD</a>'
                    : '<button type="button" class="text-link" id="acc-apply">APPLY FOR A RED CARD</button>') +
              '</div>' +
            '</div>' +
            '<div class="block">' +
              '<h2 class="block-title">BAG</h2>' +
              '<p class="muted-empty">' + (RW.cartCount() ? RW.cartCount() + (RW.cartCount() === 1 ? " piece" : " pieces") + " &middot; " + RW.money(RW.cartSubtotal()) : "Your bag is empty.") + '</p>' +
              (RW.cartCount() ? '<button type="button" class="text-link" id="acc-open-bag">OPEN BAG</button>' : "") +
            '</div>' +
            '<div class="block orders">' +
              '<h2 class="block-title">ORDER HISTORY</h2>' +
              ordersHtml +
            '</div>' +
            '<div class="block">' +
              '<button type="button" class="btn ghost" id="acc-signout">SIGN OUT</button>' +
            '</div>' +
          '</div>' +
        '</div>' +
      '</div>' +
    '</div>';

  var form = RW.qs("#profile-form");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();
      if (!RW.currentUser()) return;
      var name = RW.qs("#pf-name").value.trim();
      if (!name) { RW.toast("NAME IS REQUIRED"); return; }
      var done = RW.busy(RW.qs('button[type="submit"]', form), "SAVING...");
      RW.account.saveProfile(name, {
        phone: RW.qs("#pf-phone").value.trim(),
        country: RW.qs("#pf-country").value.trim(),
        city: RW.qs("#pf-city").value.trim(),
        address: RW.qs("#pf-address").value.trim(),
        postal: RW.qs("#pf-postal").value.trim()
      }).then(function (res) {
        if (res.error) { done(); RW.toast(res.error); return; }
        RW.toast("SAVED");
        RW.renderAccount();
      });
    });
  }

  var apply = RW.qs("#acc-apply");
  if (apply) apply.addEventListener("click", RW.openAuthModal);
  var openBag = RW.qs("#acc-open-bag");
  if (openBag) openBag.addEventListener("click", function () { RW.renderCart(); RW.openCart(); });
  var out = RW.qs("#acc-signout");
  if (out) {
    out.addEventListener("click", function () {
      RW.busy(out);
      RW.account.signOut().then(function () {
        RW.toast("SIGNED OUT");
        RW.renderCart();
        RW.renderAccount();
      });
    });
  }
};

RW.renderAccountAuth = function () {
  var app = RW.qs("#app");
  var tab = RW.state.accountTab === "create" ? "create" : "signin";

  var form = tab === "signin"
    ? '<form id="signin-form">' +
        '<div class="field"><label for="si-email">EMAIL</label><input type="email" id="si-email" required autocomplete="email"></div>' +
        '<div class="field"><label for="si-pass">PASSWORD</label><input type="password" id="si-pass" required autocomplete="current-password"></div>' +
        '<button type="submit" class="btn primary block">SIGN IN</button>' +
      '</form>'
    : '<form id="create-form">' +
        '<div class="field"><label for="ca-name">FULL NAME</label><input type="text" id="ca-name" required autocomplete="name"></div>' +
        '<div class="field"><label for="ca-email">EMAIL</label><input type="email" id="ca-email" required autocomplete="email"></div>' +
        '<div class="field"><label for="ca-pass">PASSWORD</label><input type="password" id="ca-pass" required minlength="8" autocomplete="new-password"></div>' +
        '<button type="submit" class="btn primary block">CREATE ACCOUNT</button>' +
      '</form>';

  app.innerHTML =
    '<div class="page account-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker">ACCOUNT</div>' +
          '<h1 class="display">' + (tab === "signin" ? "SIGN IN" : "CREATE ACCOUNT") + '</h1>' +
          (RW.state.next ? '<p class="lede">Sign in or create an account to continue.</p>' : "") +
        '</div>' +
        '<div class="account-auth">' +
          '<div class="tabs" role="tablist">' +
            '<button type="button" class="tab' + (tab === "signin" ? " active" : "") + '" data-tab="signin" role="tab">SIGN IN</button>' +
            '<button type="button" class="tab' + (tab === "create" ? " active" : "") + '" data-tab="create" role="tab">CREATE ACCOUNT</button>' +
          '</div>' +
          form +
          (RW.backend.mode === "server"
            ? '<p class="mono form-note">YOUR ACCOUNT AND RED CARD ARE AVAILABLE ON ANY DEVICE.<br>DO NOT REUSE A PASSWORD FROM ANOTHER SERVICE.</p>'
            : '<p class="mono form-note">PROTOTYPE. ACCOUNTS ARE KEPT IN THIS BROWSER ONLY.<br>DO NOT USE A REAL PASSWORD.</p>') +
        '</div>' +
      '</div>' +
    '</div>';

  RW.qsa(".tab", app).forEach(function (t) {
    t.addEventListener("click", function () {
      RW.state.accountTab = t.getAttribute("data-tab");
      RW.renderAccountAuth();
    });
  });

  var after = function (msg) {
    RW.toast(msg);
    RW.renderCart();
    if (RW.state.next) RW.continueAfterAuth();
    else RW.renderAccount();
  };

  var si = RW.qs("#signin-form");
  if (si) {
    si.addEventListener("submit", function (e) {
      e.preventDefault();
      var done = RW.busy(RW.qs('button[type="submit"]', si), "SIGNING IN...");
      RW.account.signIn(RW.qs("#si-email").value, RW.qs("#si-pass").value).then(function (res) {
        if (res.error) { done(); RW.toast(res.error); return; }
        after("SIGNED IN");
      });
    });
  }

  var ca = RW.qs("#create-form");
  if (ca) {
    ca.addEventListener("submit", function (e) {
      e.preventDefault();
      var pass = RW.qs("#ca-pass").value;
      if (pass.length < 8) { RW.toast("PASSWORD MUST BE AT LEAST 8 CHARACTERS"); return; }
      var done = RW.busy(RW.qs('button[type="submit"]', ca), "CREATING...");
      RW.account.createAccount(RW.qs("#ca-name").value, RW.qs("#ca-email").value, pass).then(function (res) {
        if (res.error) { done(); RW.toast(res.error); return; }
        after("ACCOUNT CREATED");
      });
    });
  }
};

/* --------------------------------------------------------------------------
   Archive
   -------------------------------------------------------------------------- */
RW.renderArchive = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var entries = RW.ARCHIVE.map(function (e) {
    return (
      '<div class="archive-entry">' +
        '<div class="a-date mono">' + RW.escapeHtml(e.label) + '</div>' +
        '<div>' +
          '<h3>' + RW.escapeHtml(e.title) + '</h3>' +
          '<p>' + RW.escapeHtml(e.body) + '</p>' +
        '</div>' +
      '</div>'
    );
  }).join("");

  app.innerHTML =
    '<div class="page archive-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker">NOTES</div>' +
          '<h1 class="display">ARCHIVE</h1>' +
        '</div>' +
        '<div class="archive-list">' + entries + '</div>' +
      '</div>' +
    '</div>';
};

/* --------------------------------------------------------------------------
   Checkout — visual prototype only. No payment is processed.
   Payment fields are never read by this script and never stored.
   The authorization check runs again here and on submit, so the flow
   cannot be skipped by opening #/checkout directly.
   -------------------------------------------------------------------------- */
RW.renderCheckout = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var items = RW.state.cart;

  if (!items.length) {
    app.innerHTML =
      '<div class="page"><div class="wrap"><div class="page-head">' +
        '<div class="mono kicker">CHECKOUT</div>' +
        '<h1 class="display">YOUR BAG IS EMPTY</h1>' +
        '<div style="margin-top:34px;"><a href="#/shop" class="btn primary">RETURN TO SHOP</a></div>' +
      '</div></div></div>';
    return;
  }

  var blocker = RW.purchaseBlocker(items);
  if (blocker) {
    RW.state.next = "checkout";
    app.innerHTML =
      '<div class="page"><div class="wrap"><div class="page-head">' +
        '<div class="mono kicker">CHECKOUT</div>' +
        '<h1 class="display">AUTHORIZATION REQUIRED</h1>' +
        '<p class="lede">' + (blocker === "ACCOUNT"
          ? "Purchasing requires an account."
          : "Your bag contains restricted pieces. An active Red Card is required.") + '</p>' +
        '<div style="margin-top:34px;display:flex;gap:12px;justify-content:center;flex-wrap:wrap;">' +
          (blocker === "ACCOUNT"
            ? '<a href="#/account" class="btn primary">SIGN IN</a>'
            : '<a href="#/authorization" class="btn primary">RED CARD</a>') +
          '<a href="#/shop" class="btn ghost">RETURN TO SHOP</a>' +
        '</div>' +
      '</div></div></div>';
    return;
  }

  var user = RW.currentUser();
  var pr = user.profile || {};

  var rows = items.map(function (i) {
    var p = RW.findProduct(i.id);
    if (!p) return "";
    return (
      '<div class="cart-item">' +
        '<div class="ci-thumb">' + RW.mediaHtml(p, 0, null, i.variant) + '</div>' +
        '<div class="ci-info">' +
          '<div class="ci-name">' + RW.escapeHtml(p.name) + '</div>' +
          '<div class="ci-meta">' + (RW.variantOf(p, i.variant).label ? RW.escapeHtml(RW.variantOf(p, i.variant).label) + ' &middot; ' : "") + 'SIZE ' + RW.escapeHtml(i.size || "-") + ' &middot; QTY ' + i.qty + ' &middot; ' + RW.escapeHtml(p.stream) + '</div>' +
          '<div class="ci-bottom"><span></span><span>' + RW.money(p.price * i.qty) + '</span></div>' +
        '</div>' +
      '</div>'
    );
  }).join("");

  var subtotal = RW.cartSubtotal();
  var v = function (x) { return RW.escapeHtml(x || ""); };

  app.innerHTML =
    '<div class="page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono kicker">VISUAL PROTOTYPE &mdash; NO PAYMENT IS PROCESSED</div>' +
          '<h1 class="display">CHECKOUT</h1>' +
        '</div>' +
        '<div class="contact-grid">' +
          '<div>' +
            '<form id="checkout-form">' +
              '<div class="form-section">CONTACT</div>' +
              '<div class="field"><label for="co-email">EMAIL</label><input type="email" id="co-email" required value="' + v(user.email) + '"></div>' +

              '<div class="form-section">SHIPPING</div>' +
              '<div class="field"><label for="co-name">FULL NAME</label><input type="text" id="co-name" required value="' + v(user.name) + '"></div>' +
              '<div class="field-row">' +
                '<div class="field"><label for="co-country">COUNTRY</label><input type="text" id="co-country" required value="' + v(pr.country) + '"></div>' +
                '<div class="field"><label for="co-city">CITY</label><input type="text" id="co-city" required value="' + v(pr.city) + '"></div>' +
              '</div>' +
              '<div class="field"><label for="co-address">ADDRESS</label><input type="text" id="co-address" required value="' + v(pr.address) + '"></div>' +
              '<div class="field"><label for="co-postal">POSTAL CODE</label><input type="text" id="co-postal" required value="' + v(pr.postal) + '"></div>' +
              '<label class="check"><input type="checkbox" id="co-save" checked> Save shipping details to my account</label>' +

              '<div class="form-section">PAYMENT</div>' +
              '<div class="field"><label for="co-card-name">CARDHOLDER NAME</label><input type="text" id="co-card-name" required autocomplete="off"></div>' +
              '<div class="field"><label for="co-card-number">CARD NUMBER</label><input type="text" id="co-card-number" inputmode="numeric" maxlength="19" placeholder="0000 0000 0000 0000" required autocomplete="off"></div>' +
              '<div class="field-row">' +
                '<div class="field"><label for="co-expiry">EXPIRY DATE</label><input type="text" id="co-expiry" placeholder="MM/YY" maxlength="5" required autocomplete="off"></div>' +
                '<div class="field"><label for="co-cvv">CVV</label><input type="text" id="co-cvv" inputmode="numeric" maxlength="4" required autocomplete="off"></div>' +
              '</div>' +

              '<p class="mono proto-note">VISUAL PROTOTYPE. PAYMENT DETAILS ARE NEVER TRANSMITTED, PROCESSED OR STORED. DO NOT ENTER REAL CARD DETAILS.</p>' +
              '<button type="submit" class="btn primary block" id="place-order-btn">PLACE ORDER</button>' +
            '</form>' +
          '</div>' +
          '<div>' +
            '<div class="form-section">ORDER SUMMARY</div>' +
            '<div class="cart-items" style="padding:0;border-top:1px solid var(--line-soft);">' + rows + '</div>' +
            '<div class="cart-foot" style="padding:24px 0;border-top:0;">' +
              '<div class="cart-summary-line"><span class="mono dim">SUBTOTAL</span><span class="num">' + RW.money(subtotal) + '</span></div>' +
              '<div class="cart-summary-line"><span class="mono dim">SHIPPING</span><span class="mono dim">COMPLIMENTARY</span></div>' +
              '<div class="cart-total"><span>TOTAL</span><span>' + RW.money(subtotal) + '</span></div>' +
            '</div>' +
          '</div>' +
        '</div>' +
      '</div>' +
    '</div>';

  var form = RW.qs("#checkout-form");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();

      /* Re-check at the moment of purchase. */
      var b = RW.purchaseBlocker(RW.state.cart);
      if (b) { RW.redirectForAuthorization(b, "checkout"); return; }
      if (!RW.state.cart.length) { RW.go("shop"); return; }

      var profile = null;
      if (RW.qs("#co-save") && RW.qs("#co-save").checked) {
        profile = {
          country: RW.qs("#co-country").value.trim(),
          city: RW.qs("#co-city").value.trim(),
          address: RW.qs("#co-address").value.trim(),
          postal: RW.qs("#co-postal").value.trim()
        };
      }

      /* Only product lines and totals are recorded — never payment data. */
      var items = RW.state.cart.map(function (i) {
        var p = RW.findProduct(i.id);
        return { id: i.id, name: p ? p.name : i.id, size: i.size, qty: i.qty, price: p ? p.price : 0,
                 colour: p ? (RW.variantOf(p, i.variant).label || "") : "" };
      });

      var done = RW.busy(RW.qs("#place-order-btn"), "PLACING ORDER...");
      RW.account.placeOrder(items, profile).then(function (res) {
        if (res.error) { done(); RW.toast(res.error); return; }
        var order = res.order;

        RW.state.cart = [];
        RW.saveCart();
        RW.updateBagCount();
        RW.renderCart();

        RW.openModal(
          '<h3>ORDER RECEIVED</h3>' +
          '<p class="mono dim" style="text-align:center;line-height:1.8;margin:0;">REFERENCE ' + RW.escapeHtml(order.ref) + '</p>' +
          '<p class="mono dim" style="text-align:center;line-height:1.8;margin:8px 0 0;">VISUAL PROTOTYPE &mdash; NO PAYMENT WAS PROCESSED.</p>' +
          '<div style="text-align:center;margin-top:26px;display:flex;gap:10px;justify-content:center;flex-wrap:wrap;">' +
            '<button type="button" class="btn primary" id="order-confirm-close">RETURN TO SHOP</button>' +
            '<button type="button" class="btn ghost" id="order-view-account">VIEW ORDERS</button>' +
          '</div>'
        );

        var closeBtn = RW.qs("#order-confirm-close");
        if (closeBtn) closeBtn.addEventListener("click", function () { RW.closeModal(); RW.go("shop"); });
        var accBtn = RW.qs("#order-view-account");
        if (accBtn) accBtn.addEventListener("click", function () { RW.closeModal(); RW.go("account"); });
      });
    });
  }
};

/* --------------------------------------------------------------------------
   Global bindings
   -------------------------------------------------------------------------- */
RW.bindGlobal = function () {
  var openCartBtn = RW.qs("#open-cart");
  if (openCartBtn) {
    openCartBtn.addEventListener("click", function () {
      RW.renderCart();
      RW.openCart();
    });
  }
  var closeCartBtn = RW.qs("#close-cart");
  if (closeCartBtn) closeCartBtn.addEventListener("click", RW.closeCart);

  var cartOverlay = RW.qs("#cart-overlay");
  if (cartOverlay) {
    cartOverlay.addEventListener("click", function (e) {
      if (e.target.id === "cart-overlay") RW.closeCart();
    });
  }

  var menu = RW.qs("#mobile-menu");
  var openMenuBtn = RW.qs("#open-mobile-menu");
  if (openMenuBtn) openMenuBtn.addEventListener("click", function () { if (menu) menu.classList.add("open"); });
  var closeMenuBtn = RW.qs("#close-mobile-menu");
  if (closeMenuBtn) closeMenuBtn.addEventListener("click", function () { if (menu) menu.classList.remove("open"); });
  RW.qsa(".mobile-nav-link").forEach(function (a) {
    a.addEventListener("click", function () { if (menu) menu.classList.remove("open"); });
  });

  var themeBtn = RW.qs("#theme-toggle");
  if (themeBtn) themeBtn.addEventListener("click", RW.toggleTheme);

  var modalOverlay = RW.qs("#modal-overlay");
  if (modalOverlay) {
    modalOverlay.addEventListener("click", function (e) {
      if (e.target.id === "modal-overlay") RW.closeModal();
    });
  }

  RW.bindCheckoutButton();

  document.addEventListener("keydown", function (e) {
    if (e.key === "Escape") {
      RW.closeCart();
      RW.closeModal();
      if (menu) menu.classList.remove("open");
    }
  });

  window.addEventListener("hashchange", RW.route);
  document.addEventListener("click", RW.soundPrime, true);
  document.addEventListener("visibilitychange", function () {
    if (document.visibilityState === "visible") RW.backend.refresh();
  });

  /* Keep the bag / Red Card state in step with other tabs. */
  window.addEventListener("storage", function (e) {
    if (e.key === RW.CART_KEY) { RW.loadCart(); RW.updateBagCount(); RW.renderCart(); }
    if (e.key === RW.TOKEN_KEY || e.key === RW.SESSION_KEY || e.key === RW.ACCOUNTS_KEY) {
      RW.backend.init().then(RW.refreshAccountViews);
    }
  });
};

/* --------------------------------------------------------------------------
   Loader (V1.1 motion: logo, 1px progress line, fade out)
   -------------------------------------------------------------------------- */
/* The loader stays until the account has been restored (normally well
   within its own 1.6 s). */
RW.runLoader = function (ready) {
  var barEl = RW.qs("#loader-progress");
  var loaderEl = RW.qs("#loader");
  var minimum = new Promise(function (resolve) { setTimeout(resolve, 1600); });
  setTimeout(function () { if (barEl) barEl.style.width = "100%"; }, 60);
  Promise.all([minimum, ready || Promise.resolve()]).then(function () {
    if (loaderEl) loaderEl.classList.add("hide");
  });
};

/* --------------------------------------------------------------------------
   Init
   -------------------------------------------------------------------------- */
RW.init = function () {
  RW.LEGACY_KEYS.forEach(RW.store.remove); /* V1.1 global authorization flag / old theme key */
  RW.loadCart();
  RW.loadTheme();
  RW.updateBagCount();
  RW.renderCart();
  RW.bindGlobal();
  var ready = RW.backend.init().catch(function () { RW.backend.mode = "local"; }).then(function () {
    RW.renderCart();
    RW.route();
  });
  RW.runLoader(ready);
};

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", RW.init);
} else {
  RW.init();
}
