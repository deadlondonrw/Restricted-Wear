# Restricted Wear — mobile app

React Native + Expo app that mirrors the Restricted Wear website: the same
pages, type, colours, products, product sound and Red Card accounts.

## Run

```
npm install
npx expo start
```

Open it in Expo Go (scan the QR code), or press `a` / `i` / `w` for an
Android emulator, the iOS simulator or the browser.

## What is in it

| Website | App |
| --- | --- |
| Home, Shop, Mainstream, Secondstream, Red Card, Archive, Account | `src/app/(main)/` |
| Product page (colours, sizes, quantity, zoom, sound) | `src/app/product/[id].tsx` |
| Checkout (visual prototype) | `src/app/checkout.tsx` |
| Masthead, navigation, menu, bag drawer, footer | `src/components/site/`, `src/components/overlays/` |
| `js/data.js` (products, archive, brand) | `src/data/catalog.generated.ts` |

- **Both colour modes** — the half-circle in the navigation inverts the colours, as on the website.
- **Accounts and Red Card** use the website's API, so an account is the same in
  the app and on the website. If the API cannot be reached the app keeps
  accounts on the device instead. The address is `expo.extra.apiBase` in `app.json`.
- **No payments.** Checkout is a visual prototype: payment fields are never sent or stored.

## Layout and text

`src/theme/responsive.ts` follows the website's breakpoints, so the app
adapts from 320 px phones to tablets. Labels, titles and buttons always stay
on one line: `src/components/T.tsx` shrinks them to fit instead of wrapping
or cutting them off. Only running copy (descriptions, paragraphs) wraps.

## Updating the catalog from the website

Products, images and tracks are copied from the website files in the
repository root:

```
npm run sync-site
```

This regenerates `assets/products`, `assets/audio`, the brand images and
`src/data/catalog.generated.ts` (requires ImageMagick's `convert`).

## Checks

```
npx tsc --noEmit
npx eslint src
npx expo export --platform web
```
