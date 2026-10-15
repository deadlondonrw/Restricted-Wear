/* ==========================================================================
   RESTRICTED WEAR — application
   ========================================================================== */
var RW = window.RW || {};

/* --------------------------------------------------------------------------
   Constants
   -------------------------------------------------------------------------- */
RW.CART_KEY = "rw_cart_v2";
RW.THEME_KEY = "rw_theme_v1";
RW.AUTH_KEY = "rw_redcard_auth_v1";
RW.DEFAULT_ROUTE = "shop";

/* --------------------------------------------------------------------------
   State
   -------------------------------------------------------------------------- */
RW.state = {
  cart: [],
  authorized: false,
  theme: "dark",
  filters: { gender: "ALL", category: "ALL", stream: "ALL", sort: "DEFAULT" },
  pd: { id: null, size: null, qty: 1, thumb: 0 }
};

/* --------------------------------------------------------------------------
   Generic helpers
   -------------------------------------------------------------------------- */
RW.qs = function (sel, ctx) { return (ctx || document).querySelector(sel); };
RW.qsa = function (sel, ctx) { return Array.prototype.slice.call((ctx || document).querySelectorAll(sel)); };
RW.money = function (n) { return "$" + Number(n || 0).toLocaleString("en-US"); };
RW.findProduct = function (id) { return RW.PRODUCTS.filter(function (p) { return p.id === id; })[0]; };
RW.availableSizes = function (p) { return Object.keys(p.sizes); };

RW.escapeHtml = function (str) {
  return String(str === null || str === undefined ? "" : str).replace(/[&<>"']/g, function (c) {
    var map = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" };
    return map[c];
  });
};

RW.issueDate = function () {
  var d = new Date();
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

/* --------------------------------------------------------------------------
   Persistence — cart / authorization / theme
   -------------------------------------------------------------------------- */
RW.loadCart = function () {
  try {
    var raw = localStorage.getItem(RW.CART_KEY);
    RW.state.cart = raw ? JSON.parse(raw) : [];
  } catch (e) { RW.state.cart = []; }
};
RW.saveCart = function () {
  try { localStorage.setItem(RW.CART_KEY, JSON.stringify(RW.state.cart)); } catch (e) {}
};

RW.loadAuth = function () {
  try { RW.state.authorized = localStorage.getItem(RW.AUTH_KEY) === "1"; } catch (e) { RW.state.authorized = false; }
};
RW.saveAuth = function () {
  try { localStorage.setItem(RW.AUTH_KEY, RW.state.authorized ? "1" : "0"); } catch (e) {}
};

RW.loadTheme = function () {
  try { RW.state.theme = localStorage.getItem(RW.THEME_KEY) || "dark"; } catch (e) { RW.state.theme = "dark"; }
  RW.applyTheme();
};
RW.applyTheme = function () {
  document.body.classList.toggle("light-theme", RW.state.theme === "light");
};
RW.toggleTheme = function () {
  RW.state.theme = RW.state.theme === "light" ? "dark" : "light";
  try { localStorage.setItem(RW.THEME_KEY, RW.state.theme); } catch (e) {}
  RW.applyTheme();
};

/* --------------------------------------------------------------------------
   Cart logic
   -------------------------------------------------------------------------- */
RW.addToCart = function (id, size, qty) {
  qty = qty || 1;
  var existing = RW.state.cart.filter(function (i) { return i.id === id && i.size === size; })[0];
  if (existing) { existing.qty += qty; } else { RW.state.cart.push({ id: id, size: size, qty: qty }); }
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
  if (item.qty < 1) { RW.removeFromCart(id, size); return; }
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
    container.innerHTML = '<div class="cart-empty">YOUR BAG IS EMPTY.<br>BROWSE THE CATALOG TO ADD ITEMS.</div>';
  } else {
    container.innerHTML = RW.state.cart.map(function (item) {
      var p = RW.findProduct(item.id);
      if (!p) return "";
      var sizeAttr = RW.escapeHtml(item.size || "");
      return (
        '<div class="cart-item">' +
          '<div class="ci-thumb"></div>' +
          '<div class="ci-info">' +
            '<div class="ci-name">' + RW.escapeHtml(p.name) + '</div>' +
            '<div class="ci-meta">SIZE ' + RW.escapeHtml(item.size || "-") + ' &middot; ' + RW.escapeHtml(p.code) + '</div>' +
            '<div class="ci-bottom">' +
              '<div class="qty-control">' +
                '<button type="button" data-cart-qty="-1" data-id="' + p.id + '" data-size="' + sizeAttr + '">&minus;</button>' +
                '<span>' + item.qty + '</span>' +
                '<button type="button" data-cart-qty="1" data-id="' + p.id + '" data-size="' + sizeAttr + '">+</button>' +
              '</div>' +
              '<span class="mono">' + RW.money(p.price * item.qty) + '</span>' +
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

  var subtitle = RW.qs("#cart-status-subtitle");
  if (subtitle) subtitle.textContent = RW.state.cart.length ? "DISPATCH READY" : "BAG EMPTY";

  RW.qsa("[data-cart-qty]", container).forEach(function (btn) {
    btn.addEventListener("click", function () {
      RW.changeCartQty(btn.getAttribute("data-id"), btn.getAttribute("data-size"), parseInt(btn.getAttribute("data-cart-qty"), 10));
    });
  });
  RW.qsa("[data-cart-remove]", container).forEach(function (btn) {
    btn.addEventListener("click", function () {
      RW.removeFromCart(btn.getAttribute("data-id"), btn.getAttribute("data-size"));
    });
  });
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
    if (!RW.state.cart.length) { RW.toast("BAG IS EMPTY"); return; }
    var needsAuth = RW.state.cart.some(function (i) {
      var p = RW.findProduct(i.id);
      return p && p.status === "RESTRICTED";
    });
    if (needsAuth && !RW.state.authorized) {
      RW.toast("RED CARD AUTHORIZATION REQUIRED");
      RW.closeCart();
      window.location.hash = "#/authorization";
      return;
    }
    RW.closeCart();
    window.location.hash = "#/checkout";
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
  if (window.location.hash.indexOf("#/product/") === 0) {
    window.location.hash = "#/shop";
  }
};

/* --------------------------------------------------------------------------
   Product detail modal
   -------------------------------------------------------------------------- */
RW.productModalHtml = function (p) {
  var sizes = RW.availableSizes(p);
  var restricted = p.status === "RESTRICTED";

  var specsRows = Object.keys(p.specs).map(function (k) {
    return '<div class="row"><span class="k">' + RW.escapeHtml(k) + '</span><span>' + RW.escapeHtml(p.specs[k]) + '</span></div>';
  }).join("");

  var sizeButtons = sizes.map(function (s) {
    var avail = p.sizes[s];
    return '<button type="button" class="size-btn" data-size="' + s + '"' + (avail ? "" : " disabled") + '>' + s + '</button>';
  }).join("");

  var thumbs = [0, 1, 2].map(function (i) {
    return '<div class="pd-thumb' + (i === 0 ? " active" : "") + '" data-thumb="' + i + '"><span class="placeholder-code">' + (i + 1) + '</span></div>';
  }).join("");

  var authNotice = restricted
    ? '<div class="pd-auth-notice"><span class="t1">RED CARD REQUIRED</span><span class="t2">AUTHORIZATION NEEDED TO PURCHASE</span></div>'
    : "";

  return (
    '<div style="display:flex;justify-content:flex-end;margin-bottom:10px;">' +
      '<button type="button" class="cart-close" id="modal-close-btn">[CLOSE]</button>' +
    '</div>' +
    '<div class="pd">' +
      '<div>' +
        '<div class="pd-gallery-main"><span class="placeholder-code">' + RW.escapeHtml(p.code) + '</span></div>' +
        '<div class="pd-thumbs">' + thumbs + '</div>' +
      '</div>' +
      '<div class="pd-info">' +
        '<div class="code-line"><span class="mono">ITEM ' + RW.escapeHtml(p.code) + '</span><span class="mono">' + RW.escapeHtml(p.status) + '</span></div>' +
        '<h1 class="display">' + RW.escapeHtml(p.name) + '</h1>' +
        '<div class="pd-price">' + RW.money(p.price) + '</div>' +
        '<p class="pd-desc">' + RW.escapeHtml(p.desc) + '</p>' +
        authNotice +
        '<div class="size-row">' +
          '<div class="size-label"><span class="mono">SELECT SIZE</span><span class="mono dim" id="pd-size-hint">REQUIRED</span></div>' +
          '<div class="sizes">' + sizeButtons + '</div>' +
        '</div>' +
        '<div class="qty-row">' +
          '<div class="qty-control">' +
            '<button type="button" id="pd-qty-minus">&minus;</button>' +
            '<span id="pd-qty-value">1</span>' +
            '<button type="button" id="pd-qty-plus">+</button>' +
          '</div>' +
        '</div>' +
        '<div class="pd-add-row">' +
          '<button type="button" class="btn primary block" id="pd-add-btn">ADD TO BAG</button>' +
        '</div>' +
        '<div class="specs">' + specsRows + '</div>' +
      '</div>' +
    '</div>'
  );
};

RW.bindProductModal = function (p) {
  var box = RW.qs("#modal-box-content");
  if (!box) return;

  var closeBtn = RW.qs("#modal-close-btn", box);
  if (closeBtn) closeBtn.addEventListener("click", RW.closeModal);

  RW.qsa(".pd-thumb", box).forEach(function (t) {
    t.addEventListener("click", function () {
      RW.qsa(".pd-thumb", box).forEach(function (o) { o.classList.remove("active"); });
      t.classList.add("active");
      RW.state.pd.thumb = parseInt(t.getAttribute("data-thumb"), 10);
    });
  });

  RW.qsa(".size-btn", box).forEach(function (btn) {
    if (btn.disabled) return;
    btn.addEventListener("click", function () {
      RW.qsa(".size-btn", box).forEach(function (o) { o.classList.remove("active"); });
      btn.classList.add("active");
      RW.state.pd.size = btn.getAttribute("data-size");
      var hint = RW.qs("#pd-size-hint", box);
      if (hint) hint.textContent = RW.state.pd.size;
    });
  });

  var qtyVal = RW.qs("#pd-qty-value", box);
  var minusBtn = RW.qs("#pd-qty-minus", box);
  var plusBtn = RW.qs("#pd-qty-plus", box);
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

  var addBtn = RW.qs("#pd-add-btn", box);
  if (addBtn) {
    addBtn.addEventListener("click", function () {
      var sizes = RW.availableSizes(p);
      if (sizes.length && !RW.state.pd.size) { RW.toast("SELECT A SIZE"); return; }
      if (p.status === "RESTRICTED" && !RW.state.authorized) {
        RW.toast("RED CARD AUTHORIZATION REQUIRED");
        return;
      }
      RW.addToCart(p.id, RW.state.pd.size, RW.state.pd.qty);
      RW.toast("ADDED TO BAG");
      RW.closeModal();
    });
  }
};

RW.openProductModal = function (id) {
  var p = RW.findProduct(id);
  if (!p) { RW.toast("ITEM NOT FOUND"); return; }
  RW.state.pd = { id: p.id, size: null, qty: 1, thumb: 0 };
  RW.openModal(RW.productModalHtml(p));
  RW.bindProductModal(p);
};

/* --------------------------------------------------------------------------
   Red Card authorization application (frontend-only registration flow)
   No password is ever created. No personal information submitted here is
   persisted — only the resulting AUTHORIZED / UNAUTHORIZED boolean is saved.
   -------------------------------------------------------------------------- */
RW.openAuthModal = function () {
  var html =
    '<div style="display:flex;justify-content:flex-end;margin-bottom:10px;">' +
      '<button type="button" class="cart-close" id="auth-modal-close">[CLOSE]</button>' +
    '</div>' +
    '<h3>RED CARD APPLICATION</h3>' +
    '<p class="mono dim" style="text-align:center;line-height:1.7;margin-bottom:22px;">SUBMIT YOUR DETAILS FOR AUTHORIZATION REVIEW. NO PASSWORD IS CREATED OR STORED.</p>' +
    '<form id="auth-form">' +
      '<div class="field"><label for="auth-name">FULL NAME</label><input type="text" id="auth-name" required></div>' +
      '<div class="field"><label for="auth-email">EMAIL</label><input type="email" id="auth-email" required></div>' +
      '<div class="field"><label for="auth-dob">DATE OF BIRTH</label><input type="date" id="auth-dob" required></div>' +
      '<div class="field"><label for="auth-country">COUNTRY</label><input type="text" id="auth-country" required></div>' +
      '<div class="field"><label for="auth-note">APPLICATION NOTE (OPTIONAL)</label><textarea id="auth-note"></textarea></div>' +
      '<button type="submit" class="btn primary block" id="auth-submit-btn">SUBMIT APPLICATION</button>' +
    '</form>';

  RW.openModal(html);

  var closeBtn = RW.qs("#auth-modal-close");
  if (closeBtn) closeBtn.addEventListener("click", RW.closeModal);

  var form = RW.qs("#auth-form");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();
      var submitBtn = RW.qs("#auth-submit-btn");
      if (submitBtn) {
        submitBtn.disabled = true;
        submitBtn.textContent = "VERIFYING...";
      }
      /* Personal information entered above is never read into RW.state or
         localStorage — it exists only in these form fields, which are
         discarded the moment the modal content is replaced. */
      setTimeout(function () {
        RW.state.authorized = true;
        RW.saveAuth();
        RW.closeModal();
        RW.toast("RED CARD AUTHORIZED");
        if (RW.qs(".auth-page")) RW.renderAuthorization();
      }, 1300);
    });
  }
};

/* --------------------------------------------------------------------------
   Router
   -------------------------------------------------------------------------- */
RW.parseHash = function () {
  var hash = window.location.hash || "";
  hash = hash.replace(/^#\/?/, "");
  return hash.split("/").filter(function (part) { return part.length > 0; });
};

RW.setActiveNav = function (page) {
  RW.qsa(".nav-link").forEach(function (a) {
    a.classList.toggle("active", a.getAttribute("data-page") === page);
  });
  RW.qsa(".mobile-nav-link").forEach(function (a) {
    a.classList.toggle("active", a.getAttribute("data-page") === page);
  });
};

RW.route = function () {
  var parts = RW.parseHash();
  var page = parts[0] || RW.DEFAULT_ROUTE;
  var param = parts[1];

  RW.setActiveNav(page === "product" ? "shop" : page);

  if (page === "authorization") {
    RW.renderAuthorization();
  } else if (page === "archive") {
    RW.renderArchive();
  } else if (page === "contact") {
    RW.renderContact();
  } else if (page === "checkout") {
    RW.renderCheckout();
  } else if (page === "product") {
    RW.renderShop();
    if (param) RW.openProductModal(param);
  } else {
    RW.renderShop();
  }

  window.scrollTo(0, 0);
};

/* --------------------------------------------------------------------------
   Shop / catalog page
   -------------------------------------------------------------------------- */
RW.categoriesList = function () {
  var set = {};
  RW.PRODUCTS.forEach(function (p) { set[p.category] = true; });
  return Object.keys(set);
};
RW.gendersList = function () {
  var set = {};
  RW.PRODUCTS.forEach(function (p) { if (p.gender !== "ALL") set[p.gender] = true; });
  return Object.keys(set);
};

RW.filteredProducts = function () {
  var f = RW.state.filters;
  var list = RW.PRODUCTS.slice();

  if (f.gender !== "ALL") {
    list = list.filter(function (p) { return p.gender === f.gender || p.gender === "ALL"; });
  }
  if (f.category !== "ALL") {
    list = list.filter(function (p) { return p.category === f.category; });
  }
  if (f.stream !== "ALL") {
    list = list.filter(function (p) { return p.stream === f.stream; });
  }

  if (f.sort === "PRICE_LOW") {
    list.sort(function (a, b) { return a.price - b.price; });
  } else if (f.sort === "PRICE_HIGH") {
    list.sort(function (a, b) { return b.price - a.price; });
  } else if (f.sort === "NAME") {
    list.sort(function (a, b) { return a.name.localeCompare(b.name); });
  }

  return list;
};

RW.productCardHtml = function (p) {
  return (
    '<a class="card" href="#/product/' + p.id + '">' +
      '<div class="card-media">' +
        '<div class="placeholder-art"><span class="placeholder-code">' + RW.escapeHtml(p.code) + '</span></div>' +
        '<div class="card-status-tag' + (p.status === "RESTRICTED" ? " restricted" : "") + '">' + RW.escapeHtml(p.status) + '</div>' +
      '</div>' +
      '<div class="card-body">' +
        '<div class="card-name">' + RW.escapeHtml(p.name) + '</div>' +
        '<div class="card-code">' + RW.escapeHtml(p.code) + ' &middot; ' + RW.escapeHtml(p.category) + '</div>' +
        '<div class="card-bottom">' +
          '<span class="card-price">' + RW.money(p.price) + '</span>' +
        '</div>' +
      '</div>' +
    '</a>'
  );
};

RW.renderShop = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var f = RW.state.filters;
  var cats = RW.categoriesList();
  var genders = RW.gendersList();

  var genderChips = ["ALL"].concat(genders).map(function (g) {
    return '<button type="button" class="filter-chip' + (f.gender === g ? " active" : "") + '" data-filter="gender" data-value="' + g + '">' + g + '</button>';
  }).join("");

  var catChips = ["ALL"].concat(cats).map(function (c) {
    return '<button type="button" class="filter-chip' + (f.category === c ? " active" : "") + '" data-filter="category" data-value="' + c + '">' + c + '</button>';
  }).join("");

  var streamChips = ["ALL", "MAINSTREAM", "SECONDSTREAM"].map(function (s) {
    return '<button type="button" class="filter-chip' + (f.stream === s ? " active" : "") + '" data-filter="stream" data-value="' + s + '">' + s + '</button>';
  }).join("");

  var list = RW.filteredProducts();
  var grid = list.length
    ? list.map(RW.productCardHtml).join("")
    : '<div class="mono dim" style="padding:40px;">NO ITEMS MATCH THE SELECTED FILTERS.</div>';

  app.innerHTML =
    '<div class="page">' +
      '<div class="wrap">' +
        '<section class="hero">' +
          '<div>' +
            '<div class="mono dim" style="margin-bottom:14px;">' + RW.escapeHtml(RW.BRAND.code) + ' // ' + RW.escapeHtml(RW.BRAND.tagline) + '</div>' +
            '<div class="hero-word display">' + RW.escapeHtml(RW.BRAND.name) + '</div>' +
          '</div>' +
          '<div>' +
            '<p class="hero-sub">High-utility apparel systems. Certain garments require RED CARD authorization prior to dispatch.</p>' +
            '<div class="hero-actions">' +
              '<a href="#/authorization" class="btn primary">REQUEST AUTHORIZATION</a>' +
              '<a href="#/archive" class="btn ghost">VIEW ARCHIVE</a>' +
            '</div>' +
          '</div>' +
        '</section>' +

        '<section class="section">' +
          '<div class="section-head">' +
            '<h2>CATALOG</h2>' +
            '<span class="mono dim">' + list.length + ' ITEM' + (list.length === 1 ? "" : "S") + '</span>' +
          '</div>' +
          '<div class="filters">' +
            '<div class="filters-group">' + genderChips + '</div>' +
            '<div class="filters-group">' + catChips + '</div>' +
            '<div class="filters-group">' + streamChips + '</div>' +
            '<select id="sort-select" class="mono" style="background:var(--charcoal);color:var(--off-white);border:1px solid var(--line);padding:8px 12px;">' +
              '<option value="DEFAULT">SORT: DEFAULT</option>' +
              '<option value="PRICE_LOW">PRICE: LOW TO HIGH</option>' +
              '<option value="PRICE_HIGH">PRICE: HIGH TO LOW</option>' +
              '<option value="NAME">NAME: A-Z</option>' +
            '</select>' +
          '</div>' +
          '<div class="grid">' + grid + '</div>' +
        '</section>' +
      '</div>' +
    '</div>';

  var sortSelect = RW.qs("#sort-select");
  if (sortSelect) {
    sortSelect.value = f.sort;
    sortSelect.addEventListener("change", function (e) {
      RW.state.filters.sort = e.target.value;
      RW.renderShop();
    });
  }

  RW.qsa("[data-filter]").forEach(function (btn) {
    btn.addEventListener("click", function () {
      var key = btn.getAttribute("data-filter");
      var val = btn.getAttribute("data-value");
      RW.state.filters[key] = val;
      RW.renderShop();
    });
  });
};

/* --------------------------------------------------------------------------
   Authorization page
   -------------------------------------------------------------------------- */
RW.renderAuthorization = function () {
  var app = RW.qs("#app");
  if (!app) return;
  var authed = RW.state.authorized;

  app.innerHTML =
    '<div class="page auth-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono dim kicker">' + RW.escapeHtml(RW.BRAND.contactDept) + ' // AUTHORIZATION SYSTEM</div>' +
          '<h1 class="display">RED CARD</h1>' +
        '</div>' +
        '<div class="auth-grid">' +
          '<div class="auth-copy">' +
            '<p>The Red Card is the authorization document that governs access to restricted garments. It confirms nothing beyond the ability to purchase what has been marked as restricted.</p>' +
            '<div class="quote">Access is not guaranteed. It is assigned, garment by garment.</div>' +
            '<p>Unauthorized requests to purchase restricted items are declined automatically. Browsing the catalog remains open to everyone.</p>' +
            '<div class="auth-fields">' +
              '<div class="row"><span class="k">CATALOG ACCESS</span><span class="v on">GRANTED</span></div>' +
              '<div class="row"><span class="k">RESTRICTED PURCHASE</span><span class="v' + (authed ? "" : " on") + '">' + (authed ? "GRANTED" : "DENIED") + '</span></div>' +
              '<div class="row"><span class="k">RED CARD STATUS</span><span class="v' + (authed ? "" : " on") + '">' + (authed ? "AUTHORIZED" : "UNAUTHORIZED") + '</span></div>' +
            '</div>' +
            '<div style="margin-top:24px;">' +
              (authed
                ? '<button type="button" class="btn ghost" id="auth-toggle-btn">REVOKE AUTHORIZATION</button>'
                : '<button type="button" class="btn primary" id="auth-toggle-btn">REQUEST AUTHORIZATION</button>') +
            '</div>' +
          '</div>' +
          '<div class="id-card">' +
            '<div class="id-top">' +
              '<span class="id-brand">' + RW.escapeHtml(RW.BRAND.name) + '</span>' +
              '<span class="id-code">RED CARD<br>' + (authed ? "ISSUED" : "UNISSUED") + '</span>' +
            '</div>' +
            '<div class="id-photo-row">' +
              '<div class="id-photo"></div>' +
              '<div class="id-fields">' +
                '<div><div class="f-label">HOLDER</div><div class="f-value">' + (authed ? "VERIFIED GUEST" : "UNVERIFIED") + '</div></div>' +
                '<div><div class="f-label">DEPT</div><div class="f-value">' + RW.escapeHtml(RW.BRAND.contactDept) + '</div></div>' +
                '<div><div class="f-label">ISSUED</div><div class="f-value">' + (authed ? RW.issueDate() : "&mdash;") + '</div></div>' +
              '</div>' +
            '</div>' +
            '<div class="id-bottom">' +
              '<span class="id-status mono">' + (authed ? "ACTIVE" : "INACTIVE") + '</span>' +
              '<span class="mono dim">' + RW.escapeHtml(RW.BRAND.code) + '</span>' +
            '</div>' +
          '</div>' +
        '</div>' +
      '</div>' +
    '</div>';

  var btn = RW.qs("#auth-toggle-btn");
  if (btn) {
    btn.addEventListener("click", function () {
      if (RW.state.authorized) {
        RW.state.authorized = false;
        RW.saveAuth();
        RW.toast("RED CARD REVOKED");
        RW.renderAuthorization();
      } else {
        RW.openAuthModal();
      }
    });
  }
};

/* --------------------------------------------------------------------------
   Archive page
   -------------------------------------------------------------------------- */
RW.renderArchive = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var entries = RW.ARCHIVE.map(function (e) {
    return (
      '<div class="archive-entry">' +
        '<div class="a-date mono">' + RW.escapeHtml(e.date) + '</div>' +
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
          '<div class="mono dim kicker">RECORDS</div>' +
          '<h1 class="display">ARCHIVE</h1>' +
        '</div>' +
        entries +
      '</div>' +
    '</div>';
};

/* --------------------------------------------------------------------------
   Contact page
   -------------------------------------------------------------------------- */
RW.renderContact = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var secondaryEmail = "restrictedwearofficial@gmail.com";
  var phone1 = "+48 608 148 679";
  var phone1Href = "tel:+48608148679";
  var phone2 = "+380 66 401 73 88";
  var phone2Href = "tel:+380664017388";

  app.innerHTML =
    '<div class="page contact-page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono dim kicker">GET IN TOUCH</div>' +
          '<h1 class="display">CONTACT</h1>' +
        '</div>' +
        '<div class="contact-grid">' +
          '<div class="contact-block">' +
            '<div class="dept">' + RW.escapeHtml(RW.BRAND.contactDept) + ' DEPARTMENT</div>' +
            '<div class="dept-name">' + RW.escapeHtml(RW.BRAND.name) + '</div>' +
            '<p class="dim">' + RW.escapeHtml(RW.BRAND.tagline) + ' For authorization inquiries, dispatch questions, or general correspondence, use the details below.</p>' +
            '<div style="margin-top:22px;"><a class="email" href="mailto:' + RW.escapeHtml(RW.BRAND.email) + '">' + RW.escapeHtml(RW.BRAND.email) + '</a></div>' +
            '<div style="margin-top:14px;"><a class="email" href="mailto:' + RW.escapeHtml(secondaryEmail) + '">' + RW.escapeHtml(secondaryEmail) + '</a></div>' +
            '<div style="margin-top:14px;"><a class="email" href="' + phone1Href + '">' + phone1 + '</a></div>' +
            '<div style="margin-top:14px;"><a class="email" href="' + phone2Href + '">' + phone2 + '</a></div>' +
          '</div>' +
          '<div>' +
            '<form id="contact-form">' +
              '<div class="field"><label for="cf-name">NAME</label><input type="text" id="cf-name" required></div>' +
              '<div class="field"><label for="cf-email">EMAIL</label><input type="email" id="cf-email" required></div>' +
              '<div class="field"><label for="cf-message">MESSAGE</label><textarea id="cf-message" required></textarea></div>' +
              '<button type="submit" class="btn primary block">PREPARE MESSAGE</button>' +
              '<p class="mono dim" style="margin-top:14px;line-height:1.7;">THIS FORM IS A FRONTEND PROTOTYPE WITH NO CONNECTED BACKEND. PLEASE REACH US DIRECTLY VIA EMAIL OR PHONE ABOVE.</p>' +
            '</form>' +
          '</div>' +
        '</div>' +
      '</div>' +
    '</div>';

  var form = RW.qs("#contact-form");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();
      RW.toast("NO BACKEND CONNECTED — PLEASE EMAIL OR CALL DIRECTLY");
      form.reset();
    });
  }
};

/* --------------------------------------------------------------------------
   Checkout page (visual prototype only — no real payment processing)
   Card number, CVV, expiry, and all shipping/payment fields are read only
   long enough to render the confirmation, then discarded. Nothing from
   this form is ever written to localStorage.
   -------------------------------------------------------------------------- */
RW.renderCheckout = function () {
  var app = RW.qs("#app");
  if (!app) return;

  var items = RW.state.cart;

  if (!items.length) {
    app.innerHTML =
      '<div class="page">' +
        '<div class="wrap">' +
          '<div class="page-head">' +
            '<div class="mono dim kicker">CHECKOUT</div>' +
            '<h1 class="display">BAG IS EMPTY</h1>' +
          '</div>' +
          '<a href="#/shop" class="btn primary">RETURN TO CATALOG</a>' +
        '</div>' +
      '</div>';
    return;
  }

  var rows = items.map(function (i) {
    var p = RW.findProduct(i.id);
    if (!p) return "";
    return (
      '<div class="cart-item">' +
        '<div class="ci-thumb"></div>' +
        '<div class="ci-info">' +
          '<div class="ci-name">' + RW.escapeHtml(p.name) + '</div>' +
          '<div class="ci-meta">SIZE ' + RW.escapeHtml(i.size || "-") + ' &middot; QTY ' + i.qty + '</div>' +
          '<div class="ci-bottom"><span></span><span class="mono">' + RW.money(p.price * i.qty) + '</span></div>' +
        '</div>' +
      '</div>'
    );
  }).join("");

  var subtotal = RW.cartSubtotal();

  app.innerHTML =
    '<div class="page">' +
      '<div class="wrap">' +
        '<div class="page-head">' +
          '<div class="mono dim kicker">VISUAL PROTOTYPE // NO PAYMENT PROCESSED</div>' +
          '<h1 class="display">CHECKOUT</h1>' +
        '</div>' +
        '<div class="contact-grid">' +
          '<div>' +
            '<form id="checkout-form">' +
              '<div class="mono dim" style="margin:0 0 14px;letter-spacing:.15em;">CONTACT</div>' +
              '<div class="field"><label for="co-email">EMAIL</label><input type="email" id="co-email" required></div>' +

              '<div class="mono dim" style="margin:26px 0 14px;letter-spacing:.15em;">SHIPPING</div>' +
              '<div class="field"><label for="co-name">FULL NAME</label><input type="text" id="co-name" required></div>' +
              '<div class="field"><label for="co-country">COUNTRY</label><input type="text" id="co-country" required></div>' +
              '<div class="field"><label for="co-city">CITY</label><input type="text" id="co-city" required></div>' +
              '<div class="field"><label for="co-address">ADDRESS</label><input type="text" id="co-address" required></div>' +
              '<div class="field"><label for="co-postal">POSTAL CODE</label><input type="text" id="co-postal" required></div>' +

              '<div class="mono dim" style="margin:26px 0 14px;letter-spacing:.15em;">PAYMENT</div>' +
              '<div class="field"><label for="co-card-name">CARDHOLDER NAME</label><input type="text" id="co-card-name" required></div>' +
              '<div class="field"><label for="co-card-number">CARD NUMBER</label><input type="text" id="co-card-number" inputmode="numeric" maxlength="19" placeholder="XXXX XXXX XXXX XXXX" required></div>' +
              '<div style="display:flex;gap:14px;">' +
                '<div class="field" style="flex:1;"><label for="co-expiry">EXPIRY DATE</label><input type="text" id="co-expiry" placeholder="MM/YY" maxlength="5" required></div>' +
                '<div class="field" style="flex:1;"><label for="co-cvv">CVV</label><input type="text" id="co-cvv" inputmode="numeric" maxlength="4" required></div>' +
              '</div>' +

              '<p class="mono dim" style="line-height:1.7;margin:18px 0 22px;">VISUAL PROTOTYPE ONLY. PAYMENT DETAILS ARE NEVER TRANSMITTED, PROCESSED, OR STORED.</p>' +
              '<button type="submit" class="btn primary block" id="place-order-btn">PLACE ORDER</button>' +
            '</form>' +
          '</div>' +
          '<div>' +
            '<div class="mono dim" style="margin:0 0 14px;letter-spacing:.15em;">ORDER SUMMARY</div>' +
            '<div class="cart-items" style="border-top:1px solid var(--line-soft);">' + rows + '</div>' +
            '<div class="cart-foot" style="padding:24px 0;">' +
              '<div class="cart-summary-line"><span class="mono">SUBTOTAL</span><span class="mono">' + RW.money(subtotal) + '</span></div>' +
              '<div class="cart-total"><span>ESTIMATED TOTAL</span><span>' + RW.money(subtotal) + '</span></div>' +
            '</div>' +
          '</div>' +
        '</div>' +
      '</div>' +
    '</div>';

  var form = RW.qs("#checkout-form");
  if (form) {
    form.addEventListener("submit", function (e) {
      e.preventDefault();

      /* This is a visual prototype only. Shipping and payment fields above
         are read implicitly by the browser's native form validation and
         are never inspected, transmitted, or persisted by this script. */

      RW.openModal(
        '<h3>ORDER TRANSMITTED</h3>' +
        '<p class="mono dim" style="text-align:center;line-height:1.7;">VISUAL PROTOTYPE &mdash; NO PAYMENT PROCESSED.</p>' +
        '<div style="text-align:center;margin-top:20px;">' +
          '<button type="button" class="btn primary" id="order-confirm-close">RETURN TO CATALOG</button>' +
        '</div>'
      );

      RW.state.cart = [];
      RW.saveCart();
      RW.updateBagCount();
      RW.renderCart();

      var closeBtn = RW.qs("#order-confirm-close");
      if (closeBtn) {
        closeBtn.addEventListener("click", function () {
          var overlay = RW.qs("#modal-overlay");
          if (overlay) overlay.classList.remove("open");
          window.location.hash = "#/shop";
        });
      }
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

  var openMenuBtn = RW.qs("#open-mobile-menu");
  if (openMenuBtn) {
    openMenuBtn.addEventListener("click", function () {
      var menu = RW.qs("#mobile-menu");
      if (menu) menu.classList.add("open");
    });
  }
  var closeMenuBtn = RW.qs("#close-mobile-menu");
  if (closeMenuBtn) {
    closeMenuBtn.addEventListener("click", function () {
      var menu = RW.qs("#mobile-menu");
      if (menu) menu.classList.remove("open");
    });
  }
  RW.qsa(".mobile-nav-link").forEach(function (a) {
    a.addEventListener("click", function () {
      var menu = RW.qs("#mobile-menu");
      if (menu) menu.classList.remove("open");
    });
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
      var menu = RW.qs("#mobile-menu");
      if (menu) menu.classList.remove("open");
    }
  });

  window.addEventListener("hashchange", RW.route);
};

/* --------------------------------------------------------------------------
   Legacy access-level cleanup
   The static markup previously shipped with a "LVL-01" nav badge and
   "CLEARANCE: LEVEL 01" / "HIGH-TIER" footer and mobile-menu copy from an
   earlier concept. Authorization is now simply AUTHORIZED / UNAUTHORIZED
   with no level hierarchy, so these leftover fragments are stripped from
   the DOM at runtime without touching the underlying markup file.
   -------------------------------------------------------------------------- */
RW.stripLegacyBadges = function () {
  var badge = RW.qs(".mark-badge");
  if (badge && badge.parentNode) badge.parentNode.removeChild(badge);

  RW.qsa(".foot-col span").forEach(function (el) {
    if (/CLEARANCE|LEVEL|HIGH-TIER/i.test(el.textContent)) {
      if (el.parentNode) el.parentNode.removeChild(el);
    }
  });

  RW.qsa(".mm-meta .mono").forEach(function (el) {
    if (/CLEARANCE|LEVEL|HIGH-TIER/i.test(el.textContent)) {
      if (el.parentNode) el.parentNode.removeChild(el);
    }
  });
};

/* --------------------------------------------------------------------------
   System initialization loader
   -------------------------------------------------------------------------- */
RW.runLoader = function () {
  var statusEl = RW.qs("#loader-status");
  var barEl = RW.qs("#loader-progress");
  var loaderEl = RW.qs("#loader");

  var steps = [
    { pct: "30%", msg: "VERIFYING ACCESS...", delay: 0, granted: false },
    { pct: "62%", msg: "CHECKING RED CARD STATUS...", delay: 850, granted: false },
    { pct: "88%", msg: "LOADING CATALOG...", delay: 1700, granted: false },
    { pct: "100%", msg: "ACCESS GRANTED", delay: 2500, granted: true }
  ];

  steps.forEach(function (step) {
    setTimeout(function () {
      if (barEl) barEl.style.width = step.pct;
      if (statusEl) {
        statusEl.textContent = step.msg;
        statusEl.classList.toggle("granted", step.granted);
      }
    }, step.delay);
  });

  setTimeout(function () {
    if (loaderEl) loaderEl.classList.add("hide");
  }, 3300);
};

/* --------------------------------------------------------------------------
   Init
   -------------------------------------------------------------------------- */
RW.init = function () {
  RW.loadCart();
  RW.loadAuth();
  RW.loadTheme();
  RW.stripLegacyBadges();
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
