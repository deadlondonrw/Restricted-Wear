import { useFocusEffect, useLocalSearchParams } from 'expo-router';
import { useCallback, useEffect, useState } from 'react';
import { Pressable, View } from 'react-native';

import { Button } from '@/components/Button';
import { PageHead } from '@/components/Heads';
import { OptionButtons } from '@/components/product/OptionButtons';
import { ProductMedia } from '@/components/product/ProductMedia';
import { QtyControl } from '@/components/product/QtyControl';
import { Rows } from '@/components/Rows';
import { Page } from '@/components/site/Page';
import { T } from '@/components/T';
import { TextLink } from '@/components/TextLink';
import { categoryByKey, findProduct, isRestricted, LEGACY_IDS, variantOf } from '@/data/products';
import type { Product } from '@/data/types';
import { money } from '@/lib/format';
import { goCheckout, goSection, goShop, redirectForAuthorization } from '@/lib/nav';
import { sound } from '@/services/sound';
import { hasRedCard, purchaseBlocker, useUser } from '@/store/account';
import { QTY_MAX, useCart } from '@/store/cart';
import { useSettings } from '@/store/settings';
import { toast, useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { photoInk } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

const QTY67_HOLD_MS = 3000;

/** RW.renderProduct */
export default function ProductScreen() {
  const { id } = useLocalSearchParams<{ id: string }>();
  const key = Array.isArray(id) ? id[0] : id;
  const product = findProduct((key && (LEGACY_IDS as Record<string, string>)[key]) || key);

  useFocusEffect(
    useCallback(() => {
      if (product) sound.start(product);
      else sound.stop();
      return () => sound.stop();
    }, [product]),
  );

  return (
    <Page active="shop">
      {product ? (
        <Detail key={product.id} product={product} />
      ) : (
        <PageHead kicker="SHOP" title="NOT FOUND" lede="This piece is not available.">
          <TextLink label="RETURN TO SHOP" onPress={() => goSection('shop')} style={{ marginTop: 30 }} />
        </PageHead>
      )}
    </Page>
  );
}

/** RW.productDetailHtml + RW.bindProductDetail */
function Detail({ product: p }: { product: Product }) {
  const s = useStyles();
  const { content, gutterLg, stacked, vw } = useLayout();
  const user = useUser();
  const soundOn = useSettings((st) => st.sound);
  const openZoom = useUI((u) => u.openZoom);
  const zoomLeft = useUI((u) => u.zoomLeft);
  const qty67Showing = useUI((u) => u.qty67);

  const [variant, setVariant] = useState(p.variants[0].key);
  const [thumb, setThumb] = useState(0);
  const [size, setSize] = useState<string | null>(null);
  const [qty, setQty] = useState(1);
  const [limit, setLimit] = useState(false);

  const colour = variantOf(p, variant);
  const restricted = isRestricted(p);
  const category = categoryByKey(p.category);
  const images = colour.images;
  const thumbCount = images.length || 3;

  const galleryWidth = stacked ? content : Math.floor(((content - gutterLg) * 1.2) / 2);
  const infoWidth = stacked ? content : content - gutterLg - galleryWidth;
  const thumbWidth = Math.floor((galleryWidth - 20) / 3);

  /* The gallery follows the view the zoom viewer was closed on. */
  useEffect(() => {
    if (zoomLeft && zoomLeft.productId === p.id) {
      setVariant(zoomLeft.variant);
      setThumb(zoomLeft.index);
    }
  }, [zoomLeft, p.id]);

  /* Holding the quantity at 67 for 3 s — any change restarts the wait. Once per launch. */
  useEffect(() => {
    if (qty !== 67 || useUI.getState().qty67Seen) return;
    const timer = setTimeout(() => {
      if (!useUI.getState().qty67Seen) useUI.getState().setQty67(true);
    }, QTY67_HOLD_MS);
    return () => clearTimeout(timer);
  }, [qty]);

  /* "Are you stupid?" — the quantity returns to 1. */
  useEffect(() => {
    if (qty67Showing) return;
    if (useUI.getState().qty67Seen) setQty((q) => (q === 67 ? 1 : q));
  }, [qty67Showing]);

  const changeQty = (delta: number) => {
    if (delta > 0 && qty >= QTY_MAX) return setLimit(true);
    const next = Math.max(1, qty + delta);
    setQty(next);
    if (next < QTY_MAX) setLimit(false);
  };

  const needSize = () => {
    if (p.sizeOrder.length && !size) {
      toast('SELECT A SIZE');
      return true;
    }
    return false;
  };

  const addToBag = () => {
    if (needSize()) return;
    if (restricted) {
      const blocker = purchaseBlocker(user, [{ id: p.id }]);
      if (blocker) return redirectForAuthorization(blocker, `product/${p.id}`);
    }
    useCart.getState().add(p.id, size ?? '', qty, colour.key);
    toast('ADDED TO BAG');
  };

  const purchase = () => {
    if (needSize()) return;
    const blocker = purchaseBlocker(user, [{ id: p.id }]);
    if (blocker) return redirectForAuthorization(blocker, `product/${p.id}`);
    useCart.getState().add(p.id, size ?? '', qty, colour.key);
    goCheckout();
  };

  const toggleSound = () => {
    const on = !soundOn;
    useSettings.getState().setSound(on);
    if (on) sound.start(p);
    else sound.stop();
  };

  const zoom = () => {
    if (images.length) openZoom({ productId: p.id, variant: colour.key, index: Math.min(thumb, images.length - 1) });
  };

  const specs = [
    ...Object.entries(p.specs).map(([k, v]) => ({ k, v })),
    { k: 'CATEGORY', v: p.category + (p.sub ? ` / ${p.sub}` : '') },
    { k: 'DIRECTION', v: p.stream },
    ...(colour.label ? [{ k: 'COLOUR', v: p.variants.map((v) => v.label).join(' / ') }] : []),
  ];

  const hint = !user
    ? 'PURCHASING REQUIRES AN ACCOUNT.'
    : restricted && !hasRedCard(user)
      ? 'THIS PIECE REQUIRES AN ACTIVE RED CARD.'
      : '';

  const viewLabel = images[thumb]?.view;

  return (
    <>
      {/* .crumbs */}
      <View style={s.crumbs} accessibilityLabel="Breadcrumb">
        <Crumb label="SHOP" onPress={() => goShop()} />
        <T variant="label" color="dim">
          /
        </T>
        <Crumb label={p.category} onPress={() => goShop(category?.slug)} />
        {p.sub ? (
          <>
            <T variant="label" color="dim">
              /
            </T>
            <T variant="label" color="dim">
              {p.sub}
            </T>
          </>
        ) : null}
      </View>

      <View style={[s.pd, stacked ? s.pdStacked : { flexDirection: 'row', gap: gutterLg }]}>
        {/* gallery */}
        <View style={{ width: galleryWidth }}>
          <Pressable
            onPress={zoom}
            disabled={!images.length}
            accessibilityRole="imagebutton"
            accessibilityLabel="Enlarge image"
            style={s.main}
          >
            <ProductMedia product={p} variant={colour.key} index={thumb} markWidth={Math.round(galleryWidth * 0.12)} />
            {viewLabel ? (
              <T variant="label" size={12} tracking={0.11} ink={photoInk.label} style={s.viewLabel}>
                {viewLabel}
              </T>
            ) : null}
            {images.length ? (
              <T variant="label" size={12} tracking={0.11} ink={photoInk.label} style={s.zoomLabel}>
                ZOOM
              </T>
            ) : null}
          </Pressable>
          <View style={s.thumbs}>
            {Array.from({ length: thumbCount }, (_, i) => {
              const label = images[i]?.view;
              return (
                <Pressable
                  key={i}
                  onPress={() => setThumb(i)}
                  accessibilityRole="button"
                  accessibilityLabel={label ? `View ${label.toLowerCase()}` : `View image ${i + 1}`}
                  accessibilityState={{ selected: thumb === i }}
                  style={[s.thumb, { width: thumbWidth, height: thumbWidth }, thumb === i && s.thumbActive]}
                >
                  <ProductMedia product={p} variant={colour.key} index={i} markWidth={Math.round(thumbWidth * 0.2)} />
                  {label ? (
                    <T variant="label" size={11} ink={photoInk.label} align="center" fit={thumbWidth - 8} style={s.thumbLabel}>
                      {label}
                    </T>
                  ) : null}
                </Pressable>
              );
            })}
          </View>
        </View>

        {/* .pd-info */}
        <View style={{ width: infoWidth }}>
          <View style={s.codeLine}>
            <Crumb label={p.stream} onPress={() => goSection(p.stream === 'MAINSTREAM' ? 'mainstream' : 'secondstream')} />
            <T variant="label" color="dim">
              {p.category}
            </T>
            {restricted ? (
              <T variant="label" color="accent">
                RESTRICTED
              </T>
            ) : p.status === 'LOW STOCK' ? (
              <T variant="label" color="dim">
                LOW STOCK
              </T>
            ) : null}
            {p.audio ? (
              <Pressable
                onPress={toggleSound}
                hitSlop={10}
                accessibilityRole="switch"
                accessibilityState={{ checked: soundOn }}
                accessibilityLabel="Product sound"
              >
                {({ pressed }) => (
                  <T variant="label" color={pressed ? 'fg' : 'dim'}>
                    {soundOn ? 'SOUND ON' : 'SOUND OFF'}
                  </T>
                )}
              </Pressable>
            ) : null}
          </View>

          <T variant="display" size={vw(30, 3.6, 46)} fit={infoWidth} accessibilityRole="header" style={s.name}>
            {p.name}
          </T>
          <T size={19} lineHeight={1.3} fit={infoWidth} style={s.price}>
            {money(p.price)}
          </T>
          <T color="dim" style={s.desc}>
            {p.desc}
          </T>

          {restricted ? (
            <View style={s.notice}>
              <T variant="label" tracking={0.11} color="accent">
                RED CARD REQUIRED
              </T>
              <T variant="label" color="dim">
                {hasRedCard(user) ? 'YOUR RED CARD IS ACTIVE' : 'PURCHASE REQUIRES AN ACTIVE RED CARD'}
              </T>
            </View>
          ) : null}

          {p.variants.length > 1 ? (
            <View style={s.row}>
              <View style={s.rowLabel}>
                <T variant="label">COLOUR</T>
                <T variant="label" color="dim" align="right" style={s.grow}>
                  {colour.label ?? ''}
                </T>
              </View>
              <OptionButtons
                kind="colour"
                options={p.variants.map((v) => ({ key: v.key, label: v.label ?? v.key }))}
                selected={colour.key}
                onSelect={setVariant}
              />
            </View>
          ) : colour.label ? (
            <View style={[s.row, s.rowLabel, s.rowStatic]}>
              <T variant="label">COLOUR</T>
              <T variant="label" size={13} tracking={0.11} align="right" style={s.grow}>
                {colour.label}
              </T>
            </View>
          ) : null}

          <View style={s.row}>
            <View style={s.rowLabel}>
              <T variant="label">SIZE</T>
              <T variant="label" color="dim" align="right" style={s.grow}>
                {size ?? 'SELECT'}
              </T>
            </View>
            <OptionButtons
              kind="size"
              options={p.sizeOrder.map((k) => ({ key: k, label: k, disabled: !p.sizes[k] }))}
              selected={size}
              onSelect={setSize}
            />
          </View>

          <View style={s.qtyRow}>
            <QtyControl value={qty} onDecrement={() => changeQty(-1)} onIncrement={() => changeQty(1)} />
            {limit ? (
              <T
                size={12.5}
                weight="medium"
                tracking={0.04}
                lineHeight={1.3}
                color="accent"
                fit
                accessibilityLiveRegion="polite"
                style={s.grow}
              >
                fucking chill, you dont have so much money
              </T>
            ) : null}
          </View>

          <View style={s.addRow}>
            <Button block variant="primary" label="PURCHASE" onPress={purchase} />
            <Button block label="ADD TO BAG" onPress={addToBag} />
            {hint ? (
              <T variant="label" color="dim" align="center" fit={infoWidth}>
                {hint}
              </T>
            ) : null}
          </View>

          <Rows rows={specs} />
        </View>
      </View>
    </>
  );
}

function Crumb({ label, onPress }: { label: string; onPress: () => void }) {
  return (
    <Pressable onPress={onPress} hitSlop={10} accessibilityRole="link" accessibilityLabel={label}>
      {({ pressed }) => (
        <T variant="label" color={pressed ? 'fg' : 'dim'}>
          {label}
        </T>
      )}
    </Pressable>
  );
}

const useStyles = makeStyles((c) => ({
  crumbs: { paddingTop: 34, flexDirection: 'row', flexWrap: 'wrap', gap: 10 },
  pd: { paddingTop: 34, paddingBottom: 100, alignItems: 'flex-start' },
  pdStacked: { flexDirection: 'column', gap: 36 },
  main: { aspectRatio: 4 / 5, overflow: 'hidden' },
  viewLabel: { position: 'absolute', top: 16, left: 16 },
  zoomLabel: { position: 'absolute', top: 16, right: 16 },
  thumbs: { flexDirection: 'row', gap: 10, marginTop: 10 },
  thumb: { overflow: 'hidden', borderWidth: 1, borderColor: 'transparent' },
  thumbActive: { borderColor: c.fg },
  thumbLabel: { position: 'absolute', left: 4, right: 4, bottom: 8 },
  codeLine: { flexDirection: 'row', flexWrap: 'wrap', columnGap: 18, rowGap: 6, marginBottom: 18 },
  name: { marginBottom: 14 },
  price: { marginBottom: 28, fontVariant: ['tabular-nums'] },
  // .pd-desc { max-width: 46ch }
  desc: { maxWidth: 340, marginBottom: 30 },
  notice: {
    borderTopWidth: 1,
    borderBottomWidth: 1,
    borderColor: c.line,
    paddingVertical: 14,
    marginBottom: 28,
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
    alignItems: 'center',
    columnGap: 12,
    rowGap: 4,
  },
  row: { marginBottom: 22 },
  rowLabel: { flexDirection: 'row', alignItems: 'center', gap: 12, marginBottom: 12 },
  rowStatic: { marginBottom: 22 },
  grow: { flex: 1 },
  qtyRow: { flexDirection: 'row', alignItems: 'center', gap: 14, marginBottom: 22 },
  addRow: { gap: 12, marginBottom: 36 },
}));
