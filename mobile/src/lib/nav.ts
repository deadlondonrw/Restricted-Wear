import { router, type Href } from 'expo-router';

import { categoryBySlug } from '@/data/products';
import type { Blocker } from '@/store/account';
import { toast, useUI } from '@/store/ui';

/* ==========================================================================
   Navigation — the website's hash routes (#/shop, #/product/<id> …).
   Sections are sibling screens; a product or the checkout opens above them.
   Every navigation closes the menu, bag, modal and zoom viewer, as RW.route
   does.
   ========================================================================== */

export type Section = 'home' | 'shop' | 'mainstream' | 'secondstream' | 'authorization' | 'archive' | 'account';

const SECTION_HREF: Record<Section, Href> = {
  home: '/',
  shop: '/shop',
  mainstream: '/mainstream',
  secondstream: '/secondstream',
  authorization: '/red-card',
  archive: '/archive',
  account: '/account',
};

/** RW.go for the top-level pages. */
export function goSection(section: Section) {
  const ui = useUI.getState();
  ui.closeOverlays();
  ui.bumpNav();
  if (router.canDismiss()) router.dismissAll();
  router.navigate(SECTION_HREF[section]);
}

/** #/shop/<slug> — the shop with a category preselected ("" = all pieces). */
export function goShop(slug?: string) {
  const category = slug ? categoryBySlug(slug) : undefined;
  useUI.getState().setFilters({ category: category ? category.key : 'ALL', sub: 'ALL' });
  goSection('shop');
}

/** A screen that sits above the sections; only one is kept open at a time. */
function openAbove(href: Href) {
  const ui = useUI.getState();
  ui.closeOverlays();
  ui.bumpNav();
  if (router.canDismiss()) router.replace(href);
  else router.push(href);
}

export function goProduct(id: string) {
  openAbove({ pathname: '/product/[id]', params: { id } });
}

export function goCheckout() {
  openAbove('/checkout');
}

/** RW.go(route) for a stored route string: "checkout", "authorization", "product/<id>" … */
export function goRoute(route: string) {
  if (route === 'checkout') goCheckout();
  else if (route.startsWith('product/')) goProduct(route.slice('product/'.length));
  else if (route in SECTION_HREF) goSection(route as Section);
  else goSection('home');
}

/** RW.redirectForAuthorization */
export function redirectForAuthorization(blocker: Exclude<Blocker, null>, next: string | null) {
  const ui = useUI.getState();
  ui.setNext(next);
  if (blocker === 'ACCOUNT') {
    toast('SIGN IN TO CONTINUE');
    ui.setAccountTab('signin');
    goSection('account');
  } else {
    toast('AN ACTIVE RED CARD IS REQUIRED');
    goSection('authorization');
  }
}

/** RW.continueAfterAuth */
export function continueAfterAuth(fallback?: Section) {
  const ui = useUI.getState();
  const next = ui.next;
  ui.setNext(null);
  if (next) goRoute(next);
  else if (fallback) goSection(fallback);
}
