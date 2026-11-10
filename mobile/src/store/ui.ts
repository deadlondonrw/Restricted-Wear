import { create } from 'zustand';

import type { Stream } from '@/data/types';
import { DEFAULT_SHOP_FILTER, type ShopFilter } from '@/data/products';

/* ==========================================================================
   Interface state that lives for the session only — the website's RW.state
   plus what its DOM holds (open drawer, open modal, toast).
   ========================================================================== */

/** #modal-overlay content. */
export type ModalContent =
  | { kind: 'redcard-application' }
  | { kind: 'order-received'; reference: string }
  | { kind: 'sort' };

/** .zoom-viewer — which product, colourway and view is open. */
export type ZoomTarget = { productId: string; variant: string; index: number };

type UIState = {
  menuOpen: boolean;
  bagOpen: boolean;
  modal: ModalContent | null;
  zoom: ZoomTarget | null;
  /** The view the zoom viewer was last on, so the product page follows it. */
  zoomLeft: ZoomTarget | null;
  toast: { message: string; seq: number } | null;
  /** RW.state.next — where to continue after signing in / getting a Red Card. */
  next: string | null;
  /** RW.state.accountTab */
  accountTab: 'signin' | 'create';
  /** RW.state.filters */
  filters: ShopFilter;
  /** RW.state.collectionCat */
  collectionCat: Record<Stream, string>;
  /** The quantity-67 overlay is showing. */
  qty67: boolean;
  /** It happens once per launch. */
  qty67Seen: boolean;
  /** Bumped on every navigation, so pages return to the top like a page load. */
  navSeq: number;

  setMenu: (open: boolean) => void;
  setBag: (open: boolean) => void;
  openModal: (modal: ModalContent) => void;
  closeModal: () => void;
  openZoom: (target: ZoomTarget) => void;
  setZoomIndex: (index: number) => void;
  closeZoom: () => void;
  showToast: (message: string) => void;
  setNext: (next: string | null) => void;
  setAccountTab: (tab: 'signin' | 'create') => void;
  setFilters: (patch: Partial<ShopFilter>) => void;
  resetFilters: () => void;
  setCollectionCat: (stream: Stream, category: string) => void;
  setQty67: (showing: boolean) => void;
  /** RW.route — closes everything that floats above the page. */
  closeOverlays: () => void;
  bumpNav: () => void;
};

export const useUI = create<UIState>()((set) => ({
  menuOpen: false,
  bagOpen: false,
  modal: null,
  zoom: null,
  zoomLeft: null,
  toast: null,
  next: null,
  accountTab: 'signin',
  filters: DEFAULT_SHOP_FILTER,
  collectionCat: { MAINSTREAM: 'ALL', SECONDSTREAM: 'ALL' },
  qty67: false,
  qty67Seen: false,
  navSeq: 0,

  setMenu: (menuOpen) => set({ menuOpen }),
  setBag: (bagOpen) => set({ bagOpen }),
  openModal: (modal) => set({ modal }),
  closeModal: () => set({ modal: null }),
  openZoom: (zoom) => set({ zoom, zoomLeft: null }),
  setZoomIndex: (index) => set((s) => (s.zoom ? { zoom: { ...s.zoom, index } } : s)),
  closeZoom: () => set((s) => ({ zoom: null, zoomLeft: s.zoom })),
  showToast: (message) => set((s) => ({ toast: { message, seq: (s.toast?.seq ?? 0) + 1 } })),
  setNext: (next) => set({ next }),
  setAccountTab: (accountTab) => set({ accountTab }),
  setFilters: (patch) => set((s) => ({ filters: { ...s.filters, ...patch } })),
  resetFilters: () => set({ filters: DEFAULT_SHOP_FILTER }),
  setCollectionCat: (stream, category) =>
    set((s) => ({ collectionCat: { ...s.collectionCat, [stream]: category } })),
  setQty67: (showing) => set(showing ? { qty67: true, qty67Seen: true } : { qty67: false }),
  closeOverlays: () => set({ menuOpen: false, bagOpen: false, modal: null, zoom: null }),
  bumpNav: () => set((s) => ({ navSeq: s.navSeq + 1 })),
}));

/** RW.toast */
export const toast = (message: string) => useUI.getState().showToast(message);
