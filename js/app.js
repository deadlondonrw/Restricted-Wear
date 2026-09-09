/* ==========================================================================
   RESTRICTED WEAR — application
   Vanilla JS, hash-routed, no build step. Organized as:
     1. State + cart persistence
     2. Small helpers
     3. Components  (Navbar is static in index.html; these are the dynamic ones)
     4. Pages
     5. Cart drawer
     6. Router
     7. Event wiring + init
   ========================================================================== */

var RW = window.RW || {};

/* --------------------------------------------------------------------------
   1. STATE
   -------------------------------------------------------------------------- */
RW.state = {
  cart: [],              // [{ id, size, qty }]
  filter: "ALL",
  pd: { id: null, size: null, qty: 1, thumb: 0 }
};

RW.CART_KEY = "rw_cart_v1";

RW.loadCart = function () {
  try {
    var raw = localStorage.getItem(RW.CART_KEY);
    RW.state.cart = raw ? JSON.parse(raw) : [];
  } catch (e) {
    RW.state.cart = [];
  }
};

RW.saveCart = function () {
  try {
    localStorage.setItem(RW.CART_KEY, JSON.stringify(RW.state.cart));
  } catch (e) { /* ignore */ }
};

RW.findProduct = function (id) {
  return RW.PRODUCTS.filter(function (p) { return p.id === id; })[0];
};

RW.addToCart = function (id, size, qty) {
  var existing = RW.state.cart.filter(function (i) { return i.id === id && i.size === size; })[0];
  if (existing) {
    existing.qty += qty;
  } else {
    RW.state.cart.push({ id: id, size: size, qty: qty });
  }
  RW.saveCart();
  RW.updateBagCount();
  RW.renderCart();
};

RW.removeFromCart = function (id, size) {
  RW.state.cart = RW.state.cart.filter(function (i) { return !(i.id === id && i.size === size); });
  RW.saveCart();
  RW.updateBagCount();
  RW.renderCart();
};

RW.changeCartQty = function (id, size, delta) {
  var item = RW.state.cart.filter(function (i) { return i.id === id && i.size === size; })[0];
  if (!item) return;
  item.qty += delta;
  if (item.qty < 1) {
    RW.removeFromCart(id, size);
    return;
  }
  RW.saveCart();
  RW.renderCart();
};

RW.cartCount = function () {
  return RW.state.cart.reduce(function (sum, i) { return sum + i.qty; }, 0);
};

RW.cartTotal = function () {
  return RW.state.cart.reduce(function (sum, i) {
    var p = RW.findProduct(i.id);
    return sum + (p ? p.price * i.qty : 0);
  }, 0);
};

/* --------------------------------------------------------------------------
   2. HELPERS
   -------------------------------------------------------------------------- */
RW.money = function (n) { return "$" + n.toLocaleString("en-US"); };

RW.qs = function (sel, ctx) { return (ctx || document).querySelector(sel); };
RW.qsa = function (sel, ctx) { return Array.prototype.slice.call((ctx || document).querySelectorAll(sel)); };

RW.availableSizes = function (p) {
  return Object.keys(p.sizes).filter(function (s) { return p.sizes[s]; });
};

RW.toast = function (msg) {
  var el = RW.qs("#toast");
  el.textContent = msg;
  el.classList.add("show");
  clearTimeout(RW._toastTimer);
  RW._toastTimer = setTimeout(function () { el.classList.remove("show"); }, 2200);
};

/* --------------------------------------------------------------------------
   3. COMPONENTS
   -------------------------------------------------------------------------- */
RW.placeholderArt = function (code, label) {
  return '<div class="placeholder-art"><span class="placeholder-code">' + code +
    (label ? " &mdash; " + label : "") + "</span></div>";
};

RW.productCard = function (p) {
  var statusClass = p.status === "RESTRICTED" ? " restricted" : "";
  var sizes = RW.availableSizes(p).join(" / ");
  return (
    '<div class="card">' +
      '<a href="#/product/' + p.id + '">' +
        '<div class="card-media">' +
          RW.placeholderArt(p.code) +
          '<span class="card-status-tag' + statusClass + '">' + p.status + "</span>" +
          '<div class="card-meta-reveal">ACCESS LEVEL ' + p.accessLevel + " &nbsp;/&nbsp; SIZES " + sizes + "</div>" +
        "</div>" +
      "</a>" +
      '<div class="card-body">' +
        '<a href="#/product/' + p.id + '"><div class="card-name">' + p.name + "</div></a>" +
        '<div class="card-code mono">' + p.code + "</div>" +
        '<div class="card-bottom"><span class="card-price">' + RW.money(p.price) + '</span><span class="mono dim">' + p.category + "</span></div>" +
      "</div>" +
    "</div>"
  );
};

RW.archiveEntryHtml = function (e) {
  return (
    '<div class="archive-entry">' +
      '<div>' +
        '<div class="a-date mono">' + e.date + "</div>" +
        '<span class="a-tag mono">' + e.tag + "</span>" +
      "</div>" +
      "<div>" +
        "<h3>" + e.title + "</h3>" +
        "<p>" + e.body + "</p>" +
      "</div>" +
    "</div>"
  );
};

/* --------------------------------------------------------------------------
   4. PAGES
   -------------------------------------------------------------------------- */
RW.heroHtml = function () {
  return (
    '<div class="wrap hero">' +
      "<div>" +
        '<h1 class="display hero-word">RESTRICTED<br>WEAR</h1>' +
        '<p class="hero-sub">' + RW.BRAND.tagline + "</p>" +
        '<div class="hero-actions">' +
          '<a href="#/collection" class="btn primary">Enter Collection</a>' +
          '<a href="#/authorization" class="btn ghost">Authorization</a>' +
        "</div>" +
      "</div>" +
      '<div class="hero-meta mono">' +
        '<div class="row"><span>Ref.</span><b>' + RW.BRAND.code + "</b></div>" +
        '<div class="row"><span>Status</span><b>Restricted</b></div>' +
        '<div class="row"><span>Access Level</span><b>01</b></div>' +
        '<div class="row"><span>Lot</span><b>004 / 2094</b></div>' +
      "</div>" +
    "</div>"
  );
};

RW.marqueeHtml = function () {
  var items = ["RW-2094", "STATUS: RESTRICTED", "AUTHORIZATION REQUIRED", "RED CARD DOCSED", "ACCESS LEVEL 01&ndash;03", "PROTOCOL 2094"];
  var track = items.concat(items).map(function (t) { return "<span>" + t + "</span>"; }).join("&bull;");
  return (
    '<div class="marquee"><div class="wrap"><div class="marquee-track mono">' + track + "</div></div></div>"
  );
};

RW.homePage = function () {
  var featured = RW.PRODUCTS.slice(0, 3).map(RW.productCard).join("");
  var archiveBits = RW.ARCHIVE.slice(0, 1).map(RW.archiveEntryHtml).join("");
  return (
    RW.heroHtml() +
    RW.marqueeHtml() +
    '<div class="wrap section">' +
      '<div class="section-head"><h2 class="display">Current Lot</h2><a href="#/collection" class="see-all mono">View Full Collection</a></div>' +
      '<div class="grid">' + featured + "</div>" +
    "</div>" +
    '<div class="rule wrap" style="max-width:var(--max-w)"></div>' +
    '<div class="wrap section auth-teaser">' +
      '<div class="auth-grid" style="margin-top:0">' +
        "<div>" +
          '<span class="mono dim">RW-2094 / AUTHORIZATION</span>' +
          '<h2 class="display" style="font-size:clamp(26px,4vw,42px);margin:14px 0 16px;">Not everything here is for sale to everyone.</h2>' +
          '<p class="dim" style="max-width:46ch;">Certain items require an Authoric Document before purchase is completed. It is not a paper form. It is not optional. It is checked at the point of authorization, not before.</p>' +
          '<div style="margin-top:22px;"><a href="#/authorization" class="btn ghost">Review Authorization</a></div>' +
        "</div>" +
        '<div class="hero-meta mono" style="border-left:1px solid var(--line);padding-left:var(--gutter);">' +
          '<div class="row"><span>System</span><b>Red Card Docsed</b></div>' +
          '<div class="row"><span>Protocol</span><b>2094</b></div>' +
          '<div class="row"><span>Clearance</span><b>Levels 01&ndash;03</b></div>' +
          '<div class="row"><span>Status</span><b>Active</b></div>' +
        "</div>" +
      "</div>" +
    "</div>" +
    '<div class="wrap section">' +
      '<div class="section-head"><h2 class="display">From the Archive</h2><a href="#/archive" class="see-all mono">View Archive</a></div>' +
      '<div class="archive-list">' + archiveBits + "</div>" +
    "</div>"
  );
};

RW.collectionPage = function () {
  var cats = ["ALL", "TEE", "HOODIE", "LONGSLEEVE"];
  var list = RW.PRODUCTS.filter(function (p) {
    return RW.state.filter === "ALL" || p.category === RW.state.filter;
  });
  var chips = cats.map(function (c) {
    var active = RW.state.filter === c ? " active" : "";
    return '<button class="filter-chip' + active + '" data-action="filter" data-cat="' + c + '">' + c + "</button>";
  }).join("");
  var cards = list.map(RW.productCard).join("") ||
    '<p class="dim mono" style="padding:30px 0;">NO ITEMS MATCH ACCESS FILTER.</p>';
  return (
    '<div class="wrap page-head">' +
      '<span class="mono kicker dim">' + RW.BRAND.code + " / FULL CATALOG</span>" +
      "<h1 class=\"display\">Collection</h1>" +
    "</div>" +
    '<div class="wrap section" style="padding-top:0;">' +
      '<div class="filters">' + chips + "</div>" +
      '<div class="grid">' + cards + "</div>" +
    "</div>"
  );
};

RW.productPage = function (id) {
  var p = RW.findProduct(id);
  if (!p) return RW.notFoundPage();

  if (RW.state.pd.id !== id) {
    RW.state.pd = { id: id, size: null, qty: 1, thumb: 0 };
  }
  var pd = RW.state.pd;
  var views = ["FRONT", "BACK", "DETAIL", "FULL LENGTH"];

  var thumbs = views.map(function (v, i) {
    var active = i === pd.thumb ? " active" : "";
    return '<button class="pd-thumb' + active + '" data-action="thumb-select" data-idx="' + i + '"><span>' + v + "</span></button>";
  }).join("");

  var sizeBtns = Object.keys(p.sizes).map(function (s) {
    var ok = p.sizes[s];
    var active = pd.size === s ? " active" : "";
    return '<button class="size-btn' + active + '" ' + (ok ? "" : "disabled") +
      ' data-action="select-size" data-size="' + s + '">' + s + "</button>";
  }).join("");

  var specRows = Object.keys(p.specs).map(function (k) {
    return '<div class="row"><span class="k">' + k + '</span><span class="v">' + p.specs[k] + "</span></div>";
  }).join("");

  var addDisabled = pd.size ? "" : "disabled";

  return (
    '<div class="wrap pd">' +
      "<div>" +
        '<div class="pd-gallery-main ticks">' +
          '<span class="tick-tr"></span><span class="tick-br"></span>' +
          RW.placeholderArt(p.code, views[pd.thumb]) +
        "</div>" +
        '<div class="pd-thumbs">' + thumbs + "</div>" +
      "</div>" +
      '<div class="pd-info">' +
        '<div class="code-line mono"><span>' + p.code + "</span><span>" + p.category + "</span></div>" +
        "<h1>" + p.name + "</h1>" +
        '<div class="pd-price">' + RW.money(p.price) + "</div>" +
        '<p class="pd-desc">' + p.desc + "</p>" +
        (p.status === "RESTRICTED" ?
          '<div class="pd-auth-notice"><span class="t1">AUTHORIZATION REQUIRED</span><span class="t2">CLEARANCE LEVEL RW-' + p.accessLevel + "</span></div>" :
          '<div class="pd-auth-notice" style="border-color:var(--line);"><span class="t1" style="color:var(--dim);">OPEN ACCESS ITEM</span><span class="t2">NO CLEARANCE REQUIRED</span></div>'
        ) +
        '<div class="size-row">' +
          '<div class="size-label"><span class="mono dim">Size</span><span class="link mono">Size Guide</span></div>' +
          '<div class="sizes">' + sizeBtns + "</div>" +
        "</div>" +
        '<div class="qty-row">' +
          '<span class="mono dim">Qty</span>' +
          '<div class="qty-control">' +
            '<button data-action="qty-dec">&minus;</button><span>' + pd.qty + '</span><button data-action="qty-inc">+</button>' +
          "</div>" +
        "</div>" +
        '<div class="pd-add-row">' +
          '<button class="btn primary block" data-action="add-to-cart" data-id="' + p.id + '" ' + addDisabled + ">" +
            (addDisabled ? "Select a Size" : "Add to Bag") +
          "</button>" +
        "</div>" +
        '<div class="specs">' + specRows + "</div>" +
      "</div>" +
    "</div>"
  );
};

RW.authorizationPage = function () {
  return (
    '<div class="wrap auth-page">' +
      '<div class="page-head" style="padding-top:0;">' +
        '<span class="mono kicker dim">' + RW.BRAND.code + " / AUTHORIZATION SYSTEM</span>" +
        '<h1 class="display">Authoric Document</h1>' +
      "</div>" +
      '<div class="auth-grid">' +
        '<div class="auth-copy">' +
          "<p>Restricted Wear is restricted for purchase by persons without an Authoric Document. It is checked, not signed &mdash; there is no form to fill in and no waiting period.</p>" +
          '<div class="quote">An Authoric Document is not a paper. It is not a regular document. It resembles an ID card, and it is shown before a deal is made &mdash; not after.</div>' +
          "<p>Clearance is assigned to garments, not to people. A single document may authorize one item and not the next. This is by design and is not explained further on this page.</p>" +
          '<div class="auth-fields">' +
            '<div class="row"><span class="k">Access Protocol</span><span class="v">2094</span></div>' +
            '<div class="row"><span class="k">System</span><span class="v">Red Card Docsed</span></div>' +
            '<div class="row"><span class="k">Status</span><span class="v on">Active</span></div>' +
            '<div class="row"><span class="k">Clearance Range</span><span class="v">Level 01 &ndash; 03</span></div>' +
            '<div class="row"><span class="k">Review Cycle</span><span class="v">Continuous</span></div>' +
          "</div>" +
        "</div>" +
        RW.idCardHtml() +
      "</div>" +
    "</div>"
  );
};

RW.idCardHtml = function () {
  return (
    '<div class="id-card ticks">' +
      '<span class="tick-tr"></span><span class="tick-br"></span>' +
      '<div class="id-top">' +
        '<span class="id-brand">Restricted Wear</span>' +
        '<span class="id-code">AUTHORIC DOCUMENT<br>RW-2094</span>' +
      "</div>" +
      '<div class="id-photo-row">' +
        '<div class="id-photo"></div>' +
        '<div class="id-fields">' +
          '<div><div class="f-label">HOLDER</div><div class="f-value">&mdash;&mdash;&mdash;&mdash;&mdash;&mdash;</div></div>' +
          '<div><div class="f-label">CLASS</div><div class="f-value">RESTRICTED ACCESS</div></div>' +
          '<div><div class="f-label">ISSUED</div><div class="f-value">LOT 002 / 2094</div></div>' +
        "</div>" +
      "</div>" +
      '<div class="id-barcode"></div>' +
      '<div class="id-bottom">' +
        '<span class="id-status mono">STATUS: ACTIVE</span>' +
        '<span class="id-year mono">2094</span>' +
      "</div>" +
    "</div>"
  );
};

RW.archivePage = function () {
  var entries = RW.ARCHIVE.map(RW.archiveEntryHtml).join("");
  return (
    '<div class="wrap archive-page">' +
      '<div class="page-head" style="padding-top:0;">' +
        '<span class="mono kicker dim">' + RW.BRAND.code + " / RECORD" + "</span>" +
        '<h1 class="display">Archive</h1>' +
        '<p class="dim" style="max-width:52ch;margin-top:16px;">Fragments only. Full records are not published.</p>' +
      "</div>" +
      '<div class="archive-list">' + entries + "</div>" +
    "</div>"
  );
};

RW.contactPage = function () {
  return (
    '<div class="wrap contact-page">' +
      '<div class="page-head" style="padding-top:0;">' +
        '<span class="mono kicker dim">' + RW.BRAND.code + " / CONTACT" + "</span>" +
        '<h1 class="display">Contact</h1>' +
      "</div>" +
      '<div class="contact-grid">' +
        '<div class="contact-block">' +
          '<div class="dept mono">DOCUMENTATION / AUTHORIZATION</div>' +
          '<div class="dept-name">' + RW.BRAND.contactDept + "</div>" +
          '<a class="email" href="mailto:' + RW.BRAND.email + '">' + RW.BRAND.email + "</a>" +
          '<p class="contact-note">This department handles authorization queries, order status, and press. Response times are not published.</p>' +
        "</div>" +
        '<form class="contact-block" id="contact-form">' +
          '<div class="field"><label class="mono">Name</label><input type="text" required></div>' +
          '<div class="field"><label class="mono">Email</label><input type="email" required></div>' +
          '<div class="field"><label class="mono">Request</label><textarea required></textarea></div>' +
          '<button type="submit" class="btn primary block">Send Request</button>' +
        "</form>" +
      "</div>" +
    "</div>"
  );
};

RW.notFoundPage = function () {
  return (
    '<div class="wrap not-found">' +
      '<span class="mono dim">RW-2094 / ERROR</span>' +
      '<h1 class="display" style="margin:16px 0;">Access Not Found</h1>' +
      '<a href="#/" class="btn ghost">Return Home</a>' +
    "</div>"
  );
};

/* --------------------------------------------------------------------------
   5. CART DRAWER
   -------------------------------------------------------------------------- */
RW.updateBagCount = function () {
  RW.qsa(".bag-count").forEach(function (el) { el.textContent = RW.cartCount(); });
};

RW.renderCart = function () {
  var wrap = RW.qs("#cart-items");
  if (RW.state.cart.length === 0) {
    wrap.innerHTML = '<div class="cart-empty mono">BAG IS EMPTY. NO ITEMS AUTHORIZED YET.</div>';
  } else {
    wrap.innerHTML = RW.state.cart.map(function (item) {
      var p = RW.findProduct(item.id);
      if (!p) return "";
      return (
        '<div class="cart-item">' +
          '<div class="ci-thumb"></div>' +
          '<div class="ci-info">' +
            '<div class="ci-name">' + p.name + "</div>" +
            '<div class="ci-meta mono">SIZE ' + item.size + " &nbsp;&middot;&nbsp; " + p.code + "</div>" +
            '<div class="ci-bottom">' +
              '<div class="qty-control">' +
                '<button data-action="cart-qty-dec" data-id="' + item.id + '" data-size="' + item.size + '">&minus;</button>' +
                '<span>' + item.qty + '</span>' +
                '<button data-action="cart-qty-inc" data-id="' + item.id + '" data-size="' + item.size + '">+</button>' +
              "</div>" +
              '<span class="mono">' + RW.money(p.price * item.qty) + "</span>" +
            "</div>" +
            '<button class="ci-remove" data-action="remove-cart-item" data-id="' + item.id + '" data-size="' + item.size + '">Remove</button>' +
          "</div>" +
        "</div>"
      );
    }).join("");
  }
  RW.qs("#cart-total").textContent = RW.money(RW.cartTotal());
  RW.qs("#cart-checkout").disabled = RW.state.cart.length === 0;
};

/* --------------------------------------------------------------------------
   6. ROUTER
   -------------------------------------------------------------------------- */
RW.render = function () {
  var hash = location.hash.replace(/^#\/?/, "");
  var parts = hash.split("/").filter(Boolean);
  var page = parts[0] || "home";
  var param = parts[1];
  var html;

  switch (page) {
    case "home": html = RW.homePage(); break;
    case "collection": html = RW.collectionPage(); break;
    case "product": html = RW.productPage(param); break;
    case "authorization": html = RW.authorizationPage(); break;
    case "archive": html = RW.archivePage(); break;
    case "contact": html = RW.contactPage(); break;
    default: html = RW.notFoundPage(); page = "";
  }

  RW.qs("#app").innerHTML = '<div class="page">' + html + "</div>";
  RW.qsa("[data-route]").forEach(function (a) {
    a.classList.toggle("active", a.getAttribute("data-route") === page);
  });
  window.scrollTo(0, 0);
  RW.closeMobileMenu();
};

/* --------------------------------------------------------------------------
   7. EVENT WIRING + INIT
   -------------------------------------------------------------------------- */
RW.openCart = function () { RW.qs("#cart-overlay").classList.add("open"); };
RW.closeCartFn = function () { RW.qs("#cart-overlay").classList.remove("open"); };
RW.openMobileMenu = function () { RW.qs("#mobile-menu").classList.add("open"); };
RW.closeMobileMenu = function () { RW.qs("#mobile-menu").classList.remove("open"); };
RW.openModal = function (title, body) {
  RW.qs("#modal-title").textContent = title;
  RW.qs("#modal-body").textContent = body;
  RW.qs("#modal-overlay").classList.add("open");
};
RW.closeModalFn = function () { RW.qs("#modal-overlay").classList.remove("open"); };

RW.handleClick = function (e) {
  var el = e.target.closest("[data-action]");
  if (!el) return;
  var action = el.getAttribute("data-action");

  switch (action) {
    case "nav-toggle-mobile": RW.openMobileMenu(); break;
    case "mobile-close": RW.closeMobileMenu(); break;
    case "cart-open": e.preventDefault(); RW.openCart(); break;
    case "cart-close": RW.closeCartFn(); break;
    case "modal-close": RW.closeModalFn(); break;

    case "filter":
      RW.state.filter = el.getAttribute("data-cat");
      RW.render();
      break;

    case "select-size":
      if (el.disabled) return;
      RW.state.pd.size = el.getAttribute("data-size");
      RW.render();
      break;

    case "thumb-select":
      RW.state.pd.thumb = parseInt(el.getAttribute("data-idx"), 10);
      RW.render();
      break;

    case "qty-inc":
      RW.state.pd.qty = Math.min(9, RW.state.pd.qty + 1);
      RW.render();
      break;
    case "qty-dec":
      RW.state.pd.qty = Math.max(1, RW.state.pd.qty - 1);
      RW.render();
      break;

    case "add-to-cart": {
      var pid = el.getAttribute("data-id");
      if (!RW.state.pd.size) { RW.toast("SELECT A SIZE FIRST"); return; }
      RW.addToCart(pid, RW.state.pd.size, RW.state.pd.qty);
      RW.toast("ADDED TO BAG");
      break;
    }

    case "remove-cart-item":
      RW.removeFromCart(el.getAttribute("data-id"), el.getAttribute("data-size"));
      break;
    case "cart-qty-inc":
      RW.changeCartQty(el.getAttribute("data-id"), el.getAttribute("data-size"), 1);
      break;
    case "cart-qty-dec":
      RW.changeCartQty(el.getAttribute("data-id"), el.getAttribute("data-size"), -1);
      break;

    case "checkout":
      if (RW.state.cart.length === 0) return;
      RW.openModal(
        "Authorization Checkpoint",
        "This is a demonstration storefront. Checkout is simulated \u2014 no payment is processed and no order is created."
      );
      break;
  }
};

RW.handleSubmit = function (e) {
  if (e.target && e.target.id === "contact-form") {
    e.preventDefault();
    RW.toast("REQUEST PREPARED FOR " + RW.BRAND.email.toUpperCase());
    e.target.reset();
  }
};

RW.runLoader = function () {
  var loader = RW.qs("#loader");
  if (!loader) return;
  if (sessionStorage.getItem("rw_intro_shown")) {
    loader.style.display = "none";
    return;
  }
  var statusEl = RW.qs("#loader .l-status");
  var barSpan = RW.qs("#loader .l-bar span");
  setTimeout(function () { barSpan.style.width = "100%"; }, 120);
  setTimeout(function () {
    statusEl.textContent = "ACCESS GRANTED";
    statusEl.classList.add("granted");
  }, 1250);
  setTimeout(function () {
    loader.classList.add("hide");
    sessionStorage.setItem("rw_intro_shown", "1");
    // fully remove from the render tree once the fade finishes, rather
    // than leaving an inert full-viewport fixed element behind
    setTimeout(function () { loader.style.display = "none"; }, 550);
  }, 1750);
};

RW.init = function () {
  RW.loadCart();
  RW.updateBagCount();
  RW.renderCart();
  RW.render();
  RW.runLoader();

  document.addEventListener("click", RW.handleClick);
  document.addEventListener("submit", RW.handleSubmit);
  window.addEventListener("hashchange", RW.render);

  // backdrop click closes cart / modal
  RW.qs("#cart-overlay").addEventListener("click", function (e) {
    if (e.target.id === "cart-overlay") RW.closeCartFn();
  });
  RW.qs("#modal-overlay").addEventListener("click", function (e) {
    if (e.target.id === "modal-overlay") RW.closeModalFn();
  });

  // escape closes any open overlay
  document.addEventListener("keydown", function (e) {
    if (e.key !== "Escape") return;
    RW.closeCartFn();
    RW.closeModalFn();
    RW.closeMobileMenu();
  });
};

document.addEventListener("DOMContentLoaded", RW.init);
