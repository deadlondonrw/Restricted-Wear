import { useMemo } from 'react';
import { create } from 'zustand';
import { persist } from 'zustand/middleware';

import { findProduct, isRestricted, variantOf } from '@/data/products';
import type { Product, ProductVariant } from '@/data/types';

import { deviceStorage, STORAGE_KEYS } from './storage';

/** Same shape as the website's cart: [{ id, size, qty, variant }] */
export type CartLine = { id: string; size: string; qty: number; variant: string };

export type ResolvedLine = CartLine & { product: Product; colour: ProductVariant; lineTotal: number };

/** Most the website's product page allows, and the most the order API accepts per line. */
export const QTY_MAX = 99;

type CartState = {
  lines: CartLine[];
  add: (id: string, size: string, qty: number, variant: string) => void;
  remove: (id: string, size: string, variant: string) => void;
  changeQty: (id: string, size: string, variant: string, delta: number) => void;
  clear: () => void;
};

/** RW.sameLine */
const same = (l: CartLine, id: string, size: string, variant: string) =>
  l.id === id && l.size === size && (l.variant || '') === (variant || '');

/** RW.loadCart — legacy ids mapped, unknown products dropped, colourway normalised. */
function sanitize(lines: unknown): CartLine[] {
  if (!Array.isArray(lines)) return [];
  const out: CartLine[] = [];
  lines.forEach((l) => {
    const product = typeof l?.id === 'string' ? findProduct(l.id) : undefined;
    const qty = Math.floor(Number(l?.qty));
    if (!product || !(qty > 0)) return;
    const size = String(l.size ?? '');
    const variant = variantOf(product, l.variant).key;
    const existing = out.find((x) => same(x, product.id, size, variant));
    if (existing) existing.qty = Math.min(QTY_MAX, existing.qty + qty);
    else out.push({ id: product.id, size, qty: Math.min(QTY_MAX, qty), variant });
  });
  return out;
}

export const useCart = create<CartState>()(
  persist(
    (set) => ({
      lines: [],

      add: (id, size, qty, variant) =>
        set((s) => {
          const existing = s.lines.find((l) => same(l, id, size, variant));
          if (existing) {
            return {
              lines: s.lines.map((l) =>
                same(l, id, size, variant) ? { ...l, qty: Math.min(QTY_MAX, l.qty + qty) } : l,
              ),
            };
          }
          return { lines: [...s.lines, { id, size, qty: Math.min(QTY_MAX, qty), variant: variant || '' }] };
        }),

      remove: (id, size, variant) =>
        set((s) => ({ lines: s.lines.filter((l) => !same(l, id, size, variant)) })),

      changeQty: (id, size, variant, delta) =>
        set((s) => {
          const line = s.lines.find((l) => same(l, id, size, variant));
          if (!line) return s;
          const next = line.qty + delta;
          if (next < 1) return { lines: s.lines.filter((l) => !same(l, id, size, variant)) };
          return {
            lines: s.lines.map((l) =>
              same(l, id, size, variant) ? { ...l, qty: Math.min(QTY_MAX, next) } : l,
            ),
          };
        }),

      clear: () => set({ lines: [] }),
    }),
    {
      name: STORAGE_KEYS.cart,
      storage: deviceStorage,
      partialize: ({ lines }) => ({ lines }),
      merge: (persisted, current) => ({
        ...current,
        lines: sanitize((persisted as Partial<CartState> | undefined)?.lines),
      }),
    },
  ),
);

/* --------------------------------------------------------------------------
   Derived values (RW.cartCount / RW.cartSubtotal)
   -------------------------------------------------------------------------- */
export function useCartCount(): number {
  return useCart((s) => s.lines.reduce((sum, l) => sum + l.qty, 0));
}

export function resolveLines(lines: CartLine[]): ResolvedLine[] {
  return lines.flatMap((l) => {
    const product = findProduct(l.id);
    if (!product) return [];
    return [{ ...l, product, colour: variantOf(product, l.variant), lineTotal: product.price * l.qty }];
  });
}

export function useCartSummary() {
  const lines = useCart((s) => s.lines);
  return useMemo(() => {
    const resolved = resolveLines(lines);
    return {
      lines: resolved,
      count: resolved.reduce((sum, l) => sum + l.qty, 0),
      subtotal: resolved.reduce((sum, l) => sum + l.lineTotal, 0),
      needsRedCard: resolved.some((l) => isRestricted(l.product)),
    };
  }, [lines]);
}
