/* ==========================================================================
   RESTRICTED WEAR — application (Version 1.2)
   Frontend prototype. Accounts, the Red Card and checkout run entirely in the
   browser (localStorage) for demonstration. This is NOT secure
   authentication and no payment is ever processed.
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
   Uniqueness is enforced against every identifier issued in this browser.
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
   Accounts (demonstration only — stored in this browser)
   Passwords are hashed with a non-cryptographic hash purely so they are not
   kept as plain text. This is not a security measure.
   -------------------------------------------------------------------------- */
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
  var email = RW.store.get(RW.SESSION_KEY, null);
  if (!email) return null;
  return RW.accounts()[email] || null;
};

RW.saveUser = function (user) {
  var map = RW.accounts();
  map[user.email] = user;
  RW.saveAccounts(map);
};

RW.createAccount = function (name, email, password) {
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
};

RW.signIn = function (email, password) {
  email = RW.normEmail(email);
  var user = RW.accounts()[email];
  if (!user || user.pass !== RW.demoHash(email + "::" + password)) return { error: "EMAIL OR PASSWORD IS INCORRECT" };
  RW.store.set(RW.SESSION_KEY, email);
  return { user: user };
};

RW.signOut = function () { RW.store.remove(RW.SESSION_KEY); };

/* --------------------------------------------------------------------------
   Authorization
   Browsing is public. Any purchase requires a signed-in account.
   RESTRICTED pieces additionally require an active Red Card.
   -------------------------------------------------------------------------- */
RW.hasRedCard = function (user) {
  user = user || RW.currentUser();
  return !!(user && user.redCard && user.redCard.status === "ACTIVE" && RW.isValidRedCardId(user.redCard.id));
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

RW.issueRedCard = function (user) {
  user.redCard = {
    id: RW.generateRedCardId(),
    holder: user.name,
    issued: new Date().toISOString(),
    status: "ACTIVE"
  };
  RW.saveUser(user);
  return user.redCard;
};

RW.revokeRedCard = function (user) {
  user.redCard = null; /* the identifier stays in the registry and is never reissued */
  RW.saveUser(user);
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
        '<div class="pd-gallery-main"><div id="pd-main" class="pd-main-media">' + RW.mediaHtml(p, 0, null, vSel.key) + '</div>' +
          (labels[0] ? '<span class="pd-view-label" id="pd-view-label">' + RW.escapeHtml(labels[0]) + '</span>' : "") + '</div>' +
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
  var minusBtn = RW.qs("#pd-qty-minus", root);
  var plusBtn = RW.qs("#pd-qty-plus", root);
  if (minusBtn) {
    minusBtn.addEventListener("click", function () {
      if (RW.state.pd.qty > 1) {
        RW.state.pd.qty -= 1;
        if (qtyVal) qtyVal.textContent = RW.state.pd.qty;
      }
    });
  }
  if (plusBtn) {
    plusBtn.addEventListener("click", function () {
      RW.state.pd.qty += 1;
      if (qtyVal) qtyVal.textContent = RW.state.pd.qty;
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
  RW.soundStart(p); /* stops any other product's track; plays this one if it has audio */
};

/* --------------------------------------------------------------------------
   Red Card document
   -------------------------------------------------------------------------- */
RW.redCardHtml = function (user, small) {
  var rc = user && user.redCard && RW.hasRedCard(user) ? user.redCard : null;
  return (
    '<div class="rc-doc' + (rc ? "" : " unissued") + (small ? " small" : "") + '">' +
      '<div class="rc-top">' +
        '<span class="rc-title">RED CARD</span>' +
        '<img class="rc-mark" src="img/rw-mark.png" alt="">' +
      '</div>' +
      '<div class="rc-fields">' +
        '<div style="grid-column:1 / -1;"><div class="f-label">HOLDER</div><div class="f-value">' + (rc ? RW.escapeHtml(user.name) : "&mdash;") + '</div></div>' +
        '<div><div class="f-label">ISSUED</div><div class="f-value">' + (rc ? RW.formatDate(rc.issued) : "&mdash;") + '</div></div>' +
        '<div><div class="f-label">STATUS</div><div class="f-value">' + (rc ? "ACTIVE" : "NOT ISSUED") + '</div></div>' +
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
      var submitBtn = RW.qs("#auth-submit-btn");
      if (submitBtn) { submitBtn.disabled = true; submitBtn.textContent = "REVIEWING..."; }

      var name = RW.qs("#auth-name").value.trim();
      var country = RW.qs("#auth-country").value.trim();

      setTimeout(function () {
        var u = RW.currentUser();
        if (!u) { RW.closeModal(); return; }
        if (name) u.name = name;
        u.profile = u.profile || {};
        u.profile.country = country;
        RW.issueRedCard(u);
        RW.closeModal();
        RW.toast("RED CARD ISSUED");
        RW.renderCart();
        if (RW.state.next) { RW.continueAfterAuth(); return; }
        if (RW.qs(".auth-page")) RW.renderAuthorization();
        else if (RW.qs(".account-page")) RW.renderAccount();
      }, 1300);
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

  var actions;
  if (!user) {
    actions =
      '<a href="#/account" class="btn primary" id="auth-signin-btn">SIGN IN</a>' +
      '<a href="#/account" class="btn ghost" id="auth-create-btn">CREATE ACCOUNT</a>';
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
            '<p>The Red Card is a digital authorization document connected to your account. It carries a unique 94-digit identifier.</p>' +
            '<p>Browsing is open to everyone. Purchasing requires an account. Pieces marked restricted require an active Red Card.</p>' +
            '<div class="auth-fields">' +
              '<div class="row"><span class="k">ACCOUNT</span><span class="v' + (user ? "" : " on") + '">' + (user ? "SIGNED IN" : "NOT SIGNED IN") + '</span></div>' +
              '<div class="row"><span class="k">RED CARD</span><span class="v' + (active ? "" : " on") + '">' + (active ? "ACTIVE" : "NOT ISSUED") + '</span></div>' +
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

  var revokeBtn = RW.qs("#auth-revoke-btn");
  if (revokeBtn) {
    revokeBtn.addEventListener("click", function () {
      var u = RW.currentUser();
      if (!u) return;
      RW.revokeRedCard(u);
      RW.toast("RED CARD REVOKED");
      RW.renderCart();
      RW.renderAuthorization();
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
      var u = RW.currentUser();
      if (!u) return;
      var name = RW.qs("#pf-name").value.trim();
      if (!name) { RW.toast("NAME IS REQUIRED"); return; }
      u.name = name;
      u.profile = {
        phone: RW.qs("#pf-phone").value.trim(),
        country: RW.qs("#pf-country").value.trim(),
        city: RW.qs("#pf-city").value.trim(),
        address: RW.qs("#pf-address").value.trim(),
        postal: RW.qs("#pf-postal").value.trim()
      };
      RW.saveUser(u);
      RW.toast("SAVED");
      RW.renderAccount();
    });
  }

  var apply = RW.qs("#acc-apply");
  if (apply) apply.addEventListener("click", RW.openAuthModal);
  var openBag = RW.qs("#acc-open-bag");
  if (openBag) openBag.addEventListener("click", function () { RW.renderCart(); RW.openCart(); });
  var out = RW.qs("#acc-signout");
  if (out) {
    out.addEventListener("click", function () {
      RW.signOut();
      RW.toast("SIGNED OUT");
      RW.renderCart();
      RW.renderAccount();
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
          '<p class="mono form-note">PROTOTYPE. ACCOUNTS ARE KEPT IN THIS BROWSER ONLY.<br>DO NOT USE A REAL PASSWORD.</p>' +
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
      var res = RW.signIn(RW.qs("#si-email").value, RW.qs("#si-pass").value);
      if (res.error) { RW.toast(res.error); return; }
      after("SIGNED IN");
    });
  }

  var ca = RW.qs("#create-form");
  if (ca) {
    ca.addEventListener("submit", function (e) {
      e.preventDefault();
      var pass = RW.qs("#ca-pass").value;
      if (pass.length < 8) { RW.toast("PASSWORD MUST BE AT LEAST 8 CHARACTERS"); return; }
      var res = RW.createAccount(RW.qs("#ca-name").value, RW.qs("#ca-email").value, pass);
      if (res.error) { RW.toast(res.error); return; }
      after("ACCOUNT CREATED");
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

      var u = RW.currentUser();
      if (RW.qs("#co-save") && RW.qs("#co-save").checked) {
        u.profile = u.profile || {};
        u.profile.country = RW.qs("#co-country").value.trim();
        u.profile.city = RW.qs("#co-city").value.trim();
        u.profile.address = RW.qs("#co-address").value.trim();
        u.profile.postal = RW.qs("#co-postal").value.trim();
      }

      /* Only product lines and totals are recorded — never payment data. */
      var order = {
        ref: RW.randomDigits(10),
        date: new Date().toISOString(),
        total: RW.cartSubtotal(),
        items: RW.state.cart.map(function (i) {
          var p = RW.findProduct(i.id);
          return { id: i.id, name: p ? p.name : i.id, size: i.size, qty: i.qty, price: p ? p.price : 0,
                   colour: p ? (RW.variantOf(p, i.variant).label || "") : "" };
        })
      };
      u.orders = u.orders || [];
      u.orders.push(order);
      RW.saveUser(u);

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

  /* Keep the bag / Red Card state in step with other tabs. */
  window.addEventListener("storage", function (e) {
    if (e.key === RW.CART_KEY) { RW.loadCart(); RW.updateBagCount(); RW.renderCart(); }
  });
};

/* --------------------------------------------------------------------------
   Loader (V1.1 motion: logo, 1px progress line, fade out)
   -------------------------------------------------------------------------- */
RW.runLoader = function () {
  var barEl = RW.qs("#loader-progress");
  var loaderEl = RW.qs("#loader");
  setTimeout(function () { if (barEl) barEl.style.width = "100%"; }, 60);
  setTimeout(function () { if (loaderEl) loaderEl.classList.add("hide"); }, 1600);
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
  RW.route();
  RW.runLoader();
};

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", RW.init);
} else {
  RW.init();
}
