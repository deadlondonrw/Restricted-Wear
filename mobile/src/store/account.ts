/* ==========================================================================
   Accounts — the website's RW.account / RW.backend / RW.local.

   Two storage modes, chosen automatically at start-up:
   • server — the website's API answers (Cloudflare Pages Functions + KV).
     Accounts, Red Cards and orders live on the server, so they are the same
     in this app, on the website and on every device.
   • local  — the API cannot be reached. Everything stays on this device,
     exactly like the website's in-browser prototype mode.

   In both modes a Red Card is issued once per account and kept permanently:
   its identifier and issue date never change. Revoking only switches the
   status; reactivating restores the same document.
   ========================================================================== */
import { create } from 'zustand';
import { persist } from 'zustand/middleware';

import { findProduct, isRestricted, REDCARD } from '@/data/products';
import { randomDigits, randomUnit } from '@/lib/random';
import { api, type ApiResponse } from '@/services/api';

import { deviceStorage, STORAGE_KEYS } from './storage';

export type Profile = {
  phone?: string;
  country?: string;
  city?: string;
  address?: string;
  postal?: string;
};

export type RedCard = {
  id: string;
  holder?: string;
  issued: string;
  status: 'ACTIVE' | 'REVOKED';
};

export type OrderItem = { id: string; name: string; size: string; qty: number; price: number; colour: string };
export type Order = { ref: string; date: string; total: number; items: OrderItem[] };

/** The account as the app sees it — never a password. */
export type User = {
  name: string;
  email: string;
  created?: string;
  profile: Profile;
  redCard: RedCard | null;
  orders: Order[];
};

type LocalUser = User & { pass: string };

export type Outcome<T = unknown> = { error: string } | ({ error?: undefined } & T);

type ServerPayload = { user?: User; token?: string; order?: Order };

type AccountState = {
  mode: 'server' | 'local';
  /** true once the storage mode is decided and the session restored. */
  ready: boolean;
  /** Server mode: the signed-in account, re-read from the server at start-up. */
  serverUser: User | null;
  token: string | null;
  /** Local mode: accounts kept on this device (rw_accounts_v1). */
  accounts: Record<string, LocalUser>;
  /** Local mode: email of the signed-in account (rw_session_v1). */
  session: string | null;
  /** Local mode: every identifier issued on this device (rw_redcard_ids_v1). */
  registry: string[];

  init: () => Promise<void>;
  refresh: () => Promise<void>;
  createAccount: (name: string, email: string, password: string) => Promise<Outcome<{ user: User }>>;
  signIn: (email: string, password: string) => Promise<Outcome<{ user: User }>>;
  signOut: () => Promise<void>;
  saveProfile: (name: string, profile: Profile) => Promise<Outcome<{ user: User }>>;
  issueRedCard: (name?: string, country?: string) => Promise<Outcome<{ user: User }>>;
  revokeRedCard: () => Promise<Outcome<{ user: User }>>;
  placeOrder: (items: OrderItem[], profile: Profile | null) => Promise<Outcome<{ order: Order }>>;
};

/* --------------------------------------------------------------------------
   Helpers
   -------------------------------------------------------------------------- */
const normEmail = (e: string) => String(e || '').trim().toLowerCase();

/**
 * Local mode only: passwords are hashed with a non-cryptographic hash purely
 * so they are not kept as plain text. This is not a security measure.
 */
function demoHash(str: string): string {
  let h1 = 0xdeadbeef;
  let h2 = 0x41c6ce57;
  for (let i = 0; i < str.length; i += 1) {
    const ch = str.charCodeAt(i);
    h1 = Math.imul(h1 ^ ch, 2654435761);
    h2 = Math.imul(h2 ^ ch, 1597334677);
  }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507);
  h1 ^= Math.imul(h2 ^ (h2 >>> 13), 3266489909);
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507);
  h2 ^= Math.imul(h1 ^ (h1 >>> 13), 3266489909);
  return (4294967296 * (2097151 & h2) + (h1 >>> 0)).toString(16);
}

export function isValidRedCardId(id: unknown): id is string {
  return typeof id === 'string' && id.length === REDCARD.LENGTH && /^[0-9]+$/.test(id);
}

function redCardPrefix(): string {
  if (randomUnit() < REDCARD.RANDOM_PREFIX_CHANCE) return randomDigits(4);
  return REDCARD.PREFIXES[Math.floor(randomUnit() * REDCARD.PREFIXES.length)];
}

const publicUser = ({ pass: _pass, ...user }: LocalUser): User => user;

const SIGN_IN = 'SIGN IN TO CONTINUE';
const failed = (r: ApiResponse<ServerPayload>) => r.data.error || 'SOMETHING WENT WRONG. TRY AGAIN.';

/* --------------------------------------------------------------------------
   Store
   -------------------------------------------------------------------------- */
export const useAccount = create<AccountState>()(
  persist(
    (set, get) => {
      /** RW.apiResult — keeps the cached account and token in step with an API answer. */
      const accept = (r: ApiResponse<ServerPayload>): string | null => {
        if (!r.ok) {
          if (r.status === 401 && get().serverUser) set({ serverUser: null, token: null });
          return failed(r);
        }
        if (r.data.token) set({ token: r.data.token });
        if (r.data.user) set({ serverUser: r.data.user });
        return null;
      };

      const call = (method: 'GET' | 'POST' | 'PUT' | 'DELETE', path: string, body?: unknown, timeoutMs?: number) =>
        api<ServerPayload>(method, path, { body, token: get().token, timeoutMs });

      /** Local mode: the signed-in account and a way to store changes to it. */
      const localUser = (): LocalUser | null => {
        const { session, accounts } = get();
        return (session && accounts[session]) || null;
      };
      const saveLocal = (user: LocalUser) => set((s) => ({ accounts: { ...s.accounts, [user.email]: user } }));

      /**
       * A Red Card made earlier on this device, for the same email and password.
       * It is carried over to the server unchanged (same identifier, same date).
       */
      const legacyRedCardFor = (email: string, password: string) => {
        const u = get().accounts[normEmail(email)];
        if (!u || u.pass !== demoHash(`${normEmail(email)}::${password}`)) return null;
        if (!u.redCard || !isValidRedCardId(u.redCard.id)) return null;
        return { id: u.redCard.id, issued: u.redCard.issued, status: u.redCard.status };
      };

      /** Local mode: a new identifier, unique among those issued on this device. */
      const generateRedCardId = (): string => {
        const registry = [...get().registry];
        Object.values(get().accounts).forEach((u) => {
          if (u.redCard?.id && !registry.includes(u.redCard.id)) registry.push(u.redCard.id);
        });
        let id: string;
        do {
          id = redCardPrefix() + randomDigits(REDCARD.LENGTH - 4);
        } while (!isValidRedCardId(id) || registry.includes(id));
        registry.push(id);
        set({ registry });
        return id;
      };

      return {
        mode: 'local',
        ready: false,
        serverUser: null,
        token: null,
        accounts: {},
        session: null,
        registry: [],

        /* RW.backend.init — decides the storage mode and restores the session. */
        init: async () => {
          const health = await api<{ storage?: boolean }>('GET', 'health', { timeoutMs: 4000 });
          const mode = health.ok && health.data.storage ? 'server' : 'local';
          set({ mode, serverUser: null });
          if (mode === 'server' && get().token) {
            const me = await call('GET', 'me', undefined, 6000);
            if (me.ok && me.data.user) set({ serverUser: me.data.user });
            else if (me.status === 401) set({ token: null });
          }
          set({ ready: true });
        },

        /* RW.backend.refresh — quietly re-reads the account (changes made elsewhere). */
        refresh: async () => {
          if (get().mode !== 'server' || !get().serverUser) return;
          const r = await call('GET', 'me', undefined, 6000);
          if (r.ok && r.data.user) set({ serverUser: r.data.user });
          else if (r.status === 401) set({ serverUser: null, token: null });
        },

        createAccount: async (name, emailRaw, password) => {
          const email = normEmail(emailRaw);
          if (get().mode === 'server') {
            const r = await call('POST', 'register', {
              name,
              email,
              password,
              legacyRedCard: legacyRedCardFor(email, password),
            });
            const error = accept(r);
            return error ? { error } : { user: r.data.user as User };
          }
          if (get().accounts[email]) return { error: 'AN ACCOUNT WITH THIS EMAIL ALREADY EXISTS' };
          const user: LocalUser = {
            name: String(name || '').trim(),
            email,
            pass: demoHash(`${email}::${password}`),
            created: new Date().toISOString(),
            profile: { phone: '', country: '', city: '', address: '', postal: '' },
            redCard: null,
            orders: [],
          };
          set((s) => ({ accounts: { ...s.accounts, [email]: user }, session: email }));
          return { user: publicUser(user) };
        },

        signIn: async (emailRaw, password) => {
          const email = normEmail(emailRaw);
          if (get().mode === 'server') {
            const r = await call('POST', 'login', {
              email,
              password,
              legacyRedCard: legacyRedCardFor(email, password),
            });
            const error = accept(r);
            return error ? { error } : { user: r.data.user as User };
          }
          const user = get().accounts[email];
          if (!user || user.pass !== demoHash(`${email}::${password}`)) {
            return { error: 'EMAIL OR PASSWORD IS INCORRECT' };
          }
          set({ session: email });
          return { user: publicUser(user) };
        },

        signOut: async () => {
          if (get().mode === 'server') {
            await call('POST', 'logout', {});
            set({ serverUser: null, token: null });
            return;
          }
          set({ session: null });
        },

        saveProfile: async (name, profile) => {
          if (get().mode === 'server') {
            const r = await call('PUT', 'profile', { name, profile });
            const error = accept(r);
            return error ? { error } : { user: r.data.user as User };
          }
          const u = localUser();
          if (!u) return { error: SIGN_IN };
          const next = { ...u, name, profile: { ...u.profile, ...profile } };
          saveLocal(next);
          return { user: publicUser(next) };
        },

        issueRedCard: async (name, country) => {
          if (get().mode === 'server') {
            const r = await call('POST', 'redcard', { name, country });
            const error = accept(r);
            return error ? { error } : { user: r.data.user as User };
          }
          const u = localUser();
          if (!u) return { error: SIGN_IN };
          const next: LocalUser = { ...u, profile: { ...u.profile } };
          if (name) next.name = name;
          if (country !== undefined) next.profile.country = country;
          if (next.redCard && isValidRedCardId(next.redCard.id)) {
            next.redCard = { ...next.redCard, status: 'ACTIVE' }; /* same identifier, same issue date */
          } else {
            next.redCard = { id: generateRedCardId(), holder: next.name, issued: new Date().toISOString(), status: 'ACTIVE' };
          }
          saveLocal(next);
          return { user: publicUser(next) };
        },

        revokeRedCard: async () => {
          if (get().mode === 'server') {
            const r = await call('DELETE', 'redcard');
            const error = accept(r);
            return error ? { error } : { user: r.data.user as User };
          }
          const u = localUser();
          if (!u) return { error: SIGN_IN };
          const next = { ...u, redCard: u.redCard ? { ...u.redCard, status: 'REVOKED' as const } : null }; /* the document is kept */
          saveLocal(next);
          return { user: publicUser(next) };
        },

        placeOrder: async (items, profile) => {
          if (get().mode === 'server') {
            const r = await call('POST', 'orders', { items, profile });
            const error = accept(r);
            return error || !r.data.order ? { error: error || 'SOMETHING WENT WRONG. TRY AGAIN.' } : { order: r.data.order };
          }
          const u = localUser();
          if (!u) return { error: SIGN_IN };
          const order: Order = {
            ref: randomDigits(10),
            date: new Date().toISOString(),
            total: Math.round(items.reduce((s, i) => s + i.price * i.qty, 0) * 100) / 100,
            items,
          };
          saveLocal({ ...u, profile: profile ? { ...u.profile, ...profile } : u.profile, orders: [...(u.orders || []), order] });
          return { order };
        },
      };
    },
    {
      name: STORAGE_KEYS.account,
      storage: deviceStorage,
      /* The server account itself is not cached: it is re-read at start-up. */
      partialize: ({ token, accounts, session, registry }) => ({ token, accounts, session, registry }),
    },
  ),
);

/* --------------------------------------------------------------------------
   Selectors — RW.currentUser / RW.hasRedCard / RW.redCardStatus /
   RW.purchaseBlocker
   -------------------------------------------------------------------------- */
type UserSource = Pick<AccountState, 'mode' | 'serverUser' | 'accounts' | 'session'>;

export function currentUser(s: UserSource): User | null {
  if (s.mode === 'server') return s.serverUser;
  return (s.session && s.accounts[s.session]) || null;
}

export function useUser(): User | null {
  return useAccount(currentUser);
}

export function hasRedCard(user: User | null): boolean {
  return !!(user?.redCard && user.redCard.status === 'ACTIVE' && isValidRedCardId(user.redCard.id));
}

export type RedCardStatus = 'ACTIVE' | 'REVOKED' | 'NOT ISSUED';

export function redCardStatus(user: User | null): RedCardStatus {
  const rc = user?.redCard;
  if (!rc || !isValidRedCardId(rc.id)) return 'NOT ISSUED';
  return rc.status === 'ACTIVE' ? 'ACTIVE' : 'REVOKED';
}

export type Blocker = 'ACCOUNT' | 'REDCARD' | null;

/** Returns null when the items may be purchased, otherwise what is missing. */
export function purchaseBlocker(user: User | null, items: { id: string }[]): Blocker {
  if (!user) return 'ACCOUNT';
  const needsCard = items.some((i) => isRestricted(findProduct(i.id)));
  if (needsCard && !hasRedCard(user)) return 'REDCARD';
  return null;
}
