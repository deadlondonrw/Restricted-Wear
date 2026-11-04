/* ==========================================================================
   RESTRICTED WEAR — account & Red Card API (Cloudflare Pages Functions)

   Storage: one Workers KV namespace bound to this Pages project as  RW_DB
   (Cloudflare dashboard → Pages project → Settings → Bindings → KV namespace).
   Without the binding the API answers  { storage: false }  and the site keeps
   working in its in-browser prototype mode.

   KV keys
     user:<email>      account record (password hash, profile, Red Card, orders)
     session:<token>   email of the signed-in account (expires automatically)
     redcard:<id>      email that owns the identifier — the uniqueness registry

   Red Card rule: one card per account, issued once. The identifier and the
   issue date never change. Revoking only switches the status; reactivating
   restores the same document.

   No payment data is ever received or stored by this API.
   ========================================================================== */

const REDCARD = {
  LENGTH: 94,
  PREFIXES: ["2094", "8888", "6666", "4546", "9999"],
  RANDOM_PREFIX_CHANCE: 0.08
};
const SESSION_TTL = 60 * 60 * 24 * 60;      /* 60 days */
const PBKDF2_ITERATIONS = 100000;           /* Workers maximum */
const MAX_ORDERS = 200;

/* --------------------------------------------------------------------------
   Helpers
   -------------------------------------------------------------------------- */
const json = (data, status = 200) =>
  new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" }
  });
const fail = (error, status = 400) => json({ error }, status);

const normEmail = (e) => String(e || "").trim().toLowerCase();
const isEmail = (e) => /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(e) && e.length <= 200;
const clip = (s, n) => String(s === null || s === undefined ? "" : s).trim().slice(0, n);

const hex = (buf) => Array.from(new Uint8Array(buf), (b) => b.toString(16).padStart(2, "0")).join("");
const randomHex = (bytes) => hex(crypto.getRandomValues(new Uint8Array(bytes)));

function randomDigits(n) {
  let out = "";
  while (out.length < n) {
    const buf = crypto.getRandomValues(new Uint8Array(n - out.length + 16));
    for (let i = 0; i < buf.length && out.length < n; i++) {
      if (buf[i] < 250) out += String(buf[i] % 10); /* rejection sampling: uniform 0–9 */
    }
  }
  return out;
}
const randomUnit = () => crypto.getRandomValues(new Uint32Array(1))[0] / 4294967296;

const isValidRedCardId = (id) => typeof id === "string" && id.length === REDCARD.LENGTH && /^[0-9]+$/.test(id);

function redCardPrefix() {
  if (randomUnit() < REDCARD.RANDOM_PREFIX_CHANCE) return randomDigits(4);
  return REDCARD.PREFIXES[Math.floor(randomUnit() * REDCARD.PREFIXES.length)];
}

async function hashPassword(password, saltHex) {
  const enc = new TextEncoder();
  const key = await crypto.subtle.importKey("raw", enc.encode(password), "PBKDF2", false, ["deriveBits"]);
  const salt = new Uint8Array(saltHex.match(/../g).map((h) => parseInt(h, 16)));
  const bits = await crypto.subtle.deriveBits({ name: "PBKDF2", hash: "SHA-256", salt, iterations: PBKDF2_ITERATIONS }, key, 256);
  return hex(bits);
}

function safeEqual(a, b) {
  if (typeof a !== "string" || typeof b !== "string" || a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

/* The account as the browser sees it — never the password hash. */
function publicUser(u) {
  return {
    name: u.name,
    email: u.email,
    created: u.created,
    profile: u.profile || {},
    redCard: u.redCard || null,
    orders: u.orders || []
  };
}

const getUser = async (db, email) => (email ? db.get("user:" + email, "json") : null);
const putUser = (db, u) => db.put("user:" + u.email, JSON.stringify(u));

async function newSession(db, email) {
  const token = randomHex(32);
  await db.put("session:" + token, email, { expirationTtl: SESSION_TTL });
  return token;
}

function bearer(request) {
  const h = request.headers.get("authorization") || "";
  const m = h.match(/^Bearer ([a-f0-9]{64})$/i);
  return m ? m[1].toLowerCase() : null;
}

async function sessionUser(db, request) {
  const token = bearer(request);
  if (!token) return { token: null, user: null };
  const email = await db.get("session:" + token);
  return { token, user: await getUser(db, email) };
}

async function readBody(request) {
  try {
    const text = await request.text();
    if (text.length > 64 * 1024) return null;
    return text ? JSON.parse(text) : {};
  } catch (e) {
    return null;
  }
}

/* A new identifier that has never been issued to anyone. */
async function uniqueRedCardId(db) {
  for (let attempt = 0; attempt < 20; attempt++) {
    const id = redCardPrefix() + randomDigits(REDCARD.LENGTH - 4);
    if (isValidRedCardId(id) && !(await db.get("redcard:" + id))) return id;
  }
  throw new Error("identifier space exhausted");
}

/* A Red Card issued earlier by the in-browser prototype is carried over
   unchanged, so its identifier and issue date stay the same. */
async function importLegacyCard(db, user, legacy) {
  if (user.redCard || !legacy || !isValidRedCardId(legacy.id)) return false;
  const issued = new Date(legacy.issued);
  if (isNaN(issued.getTime()) || issued.getTime() > Date.now() + 60000 || issued.getFullYear() < 2025) return false;
  const owner = await db.get("redcard:" + legacy.id);
  if (owner && owner !== user.email) return false;
  user.redCard = {
    id: legacy.id,
    holder: user.name,
    issued: issued.toISOString(),
    status: legacy.status === "REVOKED" ? "REVOKED" : "ACTIVE"
  };
  await db.put("redcard:" + legacy.id, user.email);
  return true;
}

/* --------------------------------------------------------------------------
   Routes
   -------------------------------------------------------------------------- */
async function handle(request, db, path) {
  const method = request.method;

  if (path === "health" && method === "GET") return json({ ok: true, storage: true });

  /* Create account */
  if (path === "register" && method === "POST") {
    const body = await readBody(request);
    if (!body) return fail("INVALID REQUEST");
    const email = normEmail(body.email);
    const name = clip(body.name, 120);
    const password = String(body.password || "");
    if (!name) return fail("NAME IS REQUIRED");
    if (!isEmail(email)) return fail("ENTER A VALID EMAIL");
    if (password.length < 8 || password.length > 200) return fail("PASSWORD MUST BE AT LEAST 8 CHARACTERS");
    if (await getUser(db, email)) return fail("AN ACCOUNT WITH THIS EMAIL ALREADY EXISTS", 409);

    const salt = randomHex(16);
    const user = {
      name,
      email,
      salt,
      pass: await hashPassword(password, salt),
      created: new Date().toISOString(),
      profile: { phone: "", country: "", city: "", address: "", postal: "" },
      redCard: null,
      orders: []
    };
    await importLegacyCard(db, user, body.legacyRedCard);
    await putUser(db, user);
    return json({ token: await newSession(db, email), user: publicUser(user) });
  }

  /* Sign in */
  if (path === "login" && method === "POST") {
    const body = await readBody(request);
    if (!body) return fail("INVALID REQUEST");
    const email = normEmail(body.email);
    const user = await getUser(db, email);
    const ok = user && safeEqual(await hashPassword(String(body.password || ""), user.salt), user.pass);
    if (!ok) return fail("EMAIL OR PASSWORD IS INCORRECT", 401);
    if (await importLegacyCard(db, user, body.legacyRedCard)) await putUser(db, user);
    return json({ token: await newSession(db, email), user: publicUser(user) });
  }

  /* Everything below requires a session */
  const { token, user } = await sessionUser(db, request);
  if (!user) return fail("SIGN IN TO CONTINUE", 401);

  if (path === "logout" && method === "POST") {
    await db.delete("session:" + token);
    return json({ ok: true });
  }

  if (path === "me" && method === "GET") return json({ user: publicUser(user) });

  /* Personal information */
  if (path === "profile" && method === "PUT") {
    const body = await readBody(request);
    if (!body) return fail("INVALID REQUEST");
    if (body.name !== undefined) {
      const name = clip(body.name, 120);
      if (!name) return fail("NAME IS REQUIRED");
      user.name = name;
    }
    if (body.profile && typeof body.profile === "object") {
      const p = user.profile || {};
      ["phone", "country", "city", "address", "postal"].forEach((k) => {
        if (body.profile[k] !== undefined) p[k] = clip(body.profile[k], 200);
      });
      user.profile = p;
    }
    await putUser(db, user);
    return json({ user: publicUser(user) });
  }

  /* Issue — or reactivate — the account's Red Card */
  if (path === "redcard" && method === "POST") {
    const body = (await readBody(request)) || {};
    const name = clip(body.name, 120);
    if (name) user.name = name;
    if (body.country !== undefined) {
      user.profile = user.profile || {};
      user.profile.country = clip(body.country, 200);
    }
    if (user.redCard && isValidRedCardId(user.redCard.id)) {
      user.redCard.status = "ACTIVE";            /* same identifier, same issue date */
    } else {
      const id = await uniqueRedCardId(db);
      user.redCard = { id, holder: user.name, issued: new Date().toISOString(), status: "ACTIVE" };
      await db.put("redcard:" + id, user.email);
    }
    await putUser(db, user);
    return json({ user: publicUser(user) });
  }

  /* Revoke — the document is kept and can be reactivated unchanged */
  if (path === "redcard" && method === "DELETE") {
    if (user.redCard) {
      user.redCard.status = "REVOKED";
      await putUser(db, user);
    }
    return json({ user: publicUser(user) });
  }

  /* Order history (prototype — no payment) */
  if (path === "orders" && method === "POST") {
    const body = await readBody(request);
    if (!body || !Array.isArray(body.items) || !body.items.length || body.items.length > 50) return fail("INVALID ORDER");
    const items = body.items.map((i) => ({
      id: clip(i.id, 80),
      name: clip(i.name, 120),
      size: clip(i.size, 12),
      colour: clip(i.colour, 40),
      qty: Math.max(1, Math.min(99, parseInt(i.qty, 10) || 1)),
      price: Math.max(0, Number(i.price) || 0)
    }));
    const order = {
      ref: randomDigits(10),
      date: new Date().toISOString(),
      total: Math.round(items.reduce((s, i) => s + i.price * i.qty, 0) * 100) / 100,
      items
    };
    if (body.profile && typeof body.profile === "object") {
      const p = user.profile || {};
      ["country", "city", "address", "postal"].forEach((k) => {
        if (body.profile[k] !== undefined) p[k] = clip(body.profile[k], 200);
      });
      user.profile = p;
    }
    user.orders = (user.orders || []).concat(order).slice(-MAX_ORDERS);
    await putUser(db, user);
    return json({ order, user: publicUser(user) });
  }

  return fail("NOT FOUND", 404);
}

export async function onRequest(context) {
  const { request, env, params } = context;
  const path = [].concat(params.path || []).join("/");
  const db = env.RW_DB;

  if (!db) {
    if (path === "health") return json({ ok: true, storage: false });
    return fail("STORAGE IS NOT CONFIGURED", 503);
  }

  try {
    return await handle(request, db, path);
  } catch (e) {
    return fail("SOMETHING WENT WRONG. TRY AGAIN.", 500);
  }
}
