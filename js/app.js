/* ==========================================================================
   RESTRICTED WEAR — application
   ========================================================================== */
var RW = window.RW || {};

RW.state = {
  cart: [],
  stream: "MAINSTREAM", // MAINSTREAM, SECONDSTREAM
  gender: "ALL",        // WOMEN, MEN, ALL
  cat: "ALL",           // T-SHIRTS, HOODIES, JEANS, SNEAKERS, etc.
  pd: { id: null, size: null, qty: 1, thumb: 0 }
};

RW.CART_KEY = "rw_cart_v2";

RW.loadCart = function () {
  try {
    var raw = localStorage.getItem(RW.CART_KEY);
    RW.state.cart = raw ? JSON.parse(raw) : [];
  } catch (e) { RW.state.cart = []; }
};

RW.saveCart = function () {
  try { localStorage.setItem(RW.CART_KEY, JSON.stringify(RW.state.cart)); } catch (e) {}
};

RW.findProduct = function (id) {
  return RW.PRODUCTS.filter(function (p) { return p.id === id; })[0];
};

RW.addToCart = function (id, size, qty) {
  var existing = RW.state.cart.filter(function (i) { return i.id === id && i.size === size; })[0];
  if (existing) { existing.qty += qty; } else { RW.state.cart.push({ id: id, size: size, qty: qty }); }
  RW.saveCart(); RW.updateBagCount(); RW.renderCart();
};

RW.removeFromCart = function (id, size) {
  RW.state.cart = RW.state.cart.filter(function (i) { return !(i.id === id && i.size === size); });
  RW.saveCart(); RW.updateBagCount(); RW.renderCart();
};

RW.changeCartQty = function (id, size, delta) {
  var item = RW.state.cart.filter(function (i) { return i.id === id && i.size === size; })[0];
  if (!item) return;
  item.qty += delta;
  if (item.qty < 1) { RW.removeFromCart(id, size); return; }
  RW.saveCart(); RW.renderCart();
};

RW.cartCount = function () { return RW.state.cart.reduce(function (sum, i) { return sum + i.qty; }, 0); };
RW.cartTotal = function () {
  return RW.state.cart.reduce(function (sum, i) {
    var p = RW.findProduct(i.id);
    return sum + (p ? p.price * i.qty : 0);
  }, 0);
};

RW.money = function (n) { return "$" + n.toLocaleString("en-US"); };
RW.qs = function (sel, ctx) { return (ctx || document).querySelector(sel); };
RW.qsa = function (sel, ctx) { return Array.prototype.slice.call((ctx || document).querySelectorAll(sel)); };
RW.availableSizes = function (p) { return Object.keys(p.sizes).filter(function (s) { return p.sizes[s]; }); };

RW.toast = function (msg) {
  var el = RW.qs("#toast");
  el.textContent = msg; el.classList.add("show");
  clearTimeout(RW._toastTimer);
  RW._toastTimer = setTimeout(function () { el.classList.remove("show"); }, 2200);
};

/* --------------------------------------------------------------------------
   Components
   -------------------------------------------------------------------------- */
RW.placeholderArt = function (code) {
  return '