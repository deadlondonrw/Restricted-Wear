import { useEffect, useMemo, useRef, useState } from 'react';
import { Animated, Easing, PanResponder, Pressable, StyleSheet, View, type GestureResponderEvent } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { findProduct, variantOf } from '@/data/products';
import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { palettes, photoInk } from '@/theme/tokens';

import { T } from '../T';

/* ==========================================================================
   Image zoom — the website's .zoom-viewer.
   Full screen on the photo background. A tap zooms in at that point (2.5×)
   or resets; pinch and − / + adjust (up to 4×); drag moves the image; a
   swipe at fit scale, or PREVIOUS / NEXT, changes the view.
   A colourway's larger `zoom` files are used when the website has them.
   ========================================================================== */
const ZOOM = { TAP: 2.5, MAX: 4, STEP: 1.6 };
/** Product images are 3 : 4. */
const IMAGE_RATIO = 3 / 4;
const PHOTO_BG = palettes.default.photoBg;

type Point = { x: number; y: number };

export function ZoomViewer() {
  const target = useUI((u) => u.zoom);
  if (!target) return null;
  return <Viewer key={`${target.productId}|${target.variant}`} productId={target.productId} variant={target.variant} index={target.index} />;
}

function Viewer({ productId, variant, index }: { productId: string; variant: string; index: number }) {
  const insets = useSafeAreaInsets();
  const { gutter, navH } = useLayout();
  const closeZoom = useUI((u) => u.closeZoom);
  const setZoomIndex = useUI((u) => u.setZoomIndex);
  const product = findProduct(productId);
  const colour = product ? variantOf(product, variant) : undefined;
  const images = colour?.images ?? [];
  const count = images.length;

  const [stage, setStage] = useState({ w: 0, h: 0 });
  const [level, setLevel] = useState(1);
  const fade = useAnimatedValue(0);
  const scaleV = useAnimatedValue(1);
  const xV = useAnimatedValue(0);
  const yV = useAnimatedValue(0);

  /* Geometry and gesture state live in a ref: the responder callbacks are created once. */
  const g = useRef({
    s: 1, x: 0, y: 0, sw: 0, sh: 0, w: 0, h: 0, ox: 0, oy: 0,
    start: null as null | { p: Point; x: number; y: number; moved: boolean; pinch: boolean; dist: number; mid: Point },
    index, count,
  });
  g.current.index = index;
  g.current.count = count;

  const stageRef = useRef<View>(null);

  useEffect(() => {
    Animated.timing(fade, { toValue: 1, duration: 300, easing: Easing.ease, useNativeDriver: true }).start();
  }, [fade]);

  const api = useMemo(() => {
    const set = (s: number, x: number, y: number, animate: boolean) => {
      const st = g.current;
      s = Math.max(1, Math.min(ZOOM.MAX, s));
      const mx = Math.max(0, (st.w * s - st.sw) / 2);
      const my = Math.max(0, (st.h * s - st.sh) / 2);
      st.s = s;
      st.x = Math.max(-mx, Math.min(mx, x));
      st.y = Math.max(-my, Math.min(my, y));
      setLevel(s);
      if (animate) {
        const cfg = { duration: 450, easing: Easing.bezier(0.16, 1, 0.3, 1), useNativeDriver: true };
        Animated.parallel([
          Animated.timing(scaleV, { toValue: st.s, ...cfg }),
          Animated.timing(xV, { toValue: st.x, ...cfg }),
          Animated.timing(yV, { toValue: st.y, ...cfg }),
        ]).start();
      } else {
        scaleV.setValue(st.s);
        xV.setValue(st.x);
        yV.setValue(st.y);
      }
    };
    /** Zoom to scale s while keeping the stage point (px, py) in place. */
    const at = (px: number, py: number, s: number, animate: boolean) => {
      const st = g.current;
      s = Math.max(1, Math.min(ZOOM.MAX, s));
      const r = s / st.s;
      const cx = st.sw / 2 + st.x;
      const cy = st.sh / 2 + st.y;
      set(s, px - (px - cx) * r - st.sw / 2, py - (py - cy) * r - st.sh / 2, animate);
    };
    const show = (next: number) => {
      const n = g.current.count;
      if (!n) return;
      set(1, 0, 0, false);
      setZoomIndex(((next % n) + n) % n);
    };
    return { set, at, show };
  }, [scaleV, xV, yV, setZoomIndex]);

  const responder = useMemo(() => {
    const local = (t: { pageX: number; pageY: number }): Point => ({ x: t.pageX - g.current.ox, y: t.pageY - g.current.oy });
    const dist = (a: Point, b: Point) => Math.hypot(a.x - b.x, a.y - b.y) || 1;
    const mid = (a: Point, b: Point): Point => ({ x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 });

    const begin = (e: GestureResponderEvent) => {
      const st = g.current;
      const touches = e.nativeEvent.touches;
      const first = local(touches[0] ?? e.nativeEvent);
      if (touches.length >= 2) {
        const second = local(touches[1]);
        st.start = { p: first, x: st.x, y: st.y, moved: true, pinch: true, dist: dist(first, second), mid: mid(first, second) };
      } else {
        st.start = { p: first, x: st.x, y: st.y, moved: st.start?.pinch ?? false, pinch: false, dist: 1, mid: first };
      }
    };

    return PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onMoveShouldSetPanResponder: () => true,
      onPanResponderGrant: (e) => {
        g.current.start = null;
        begin(e);
      },
      onPanResponderMove: (e) => {
        const st = g.current;
        const touches = e.nativeEvent.touches;
        if (!st.start) return;
        if (touches.length >= 2) {
          if (!st.start.pinch) begin(e);
          const a = local(touches[0]);
          const b = local(touches[1]);
          const d = dist(a, b);
          const m = mid(a, b);
          api.at(m.x, m.y, st.s * (d / st.start!.dist), false);
          api.set(st.s, st.x + (m.x - st.start!.mid.x), st.y + (m.y - st.start!.mid.y), false);
          st.start!.dist = d;
          st.start!.mid = m;
        } else if (touches.length === 1) {
          if (st.start.pinch) {
            /* one finger left after a pinch: continue as a drag, never as a tap */
            begin(e);
            return;
          }
          const p = local(touches[0]);
          const dx = p.x - st.start.p.x;
          const dy = p.y - st.start.p.y;
          if (!st.start.moved && Math.abs(dx) + Math.abs(dy) > 6) st.start.moved = true;
          if (st.start.moved && st.s > 1) api.set(st.s, st.start.x + dx, st.start.y + dy, false);
        }
      },
      onPanResponderRelease: (e) => {
        const st = g.current;
        const start = st.start;
        st.start = null;
        if (!start || start.pinch) return;
        const p = local(e.nativeEvent);
        if (!start.moved) {
          if (st.s > 1.01) api.set(1, 0, 0, true);
          else api.at(p.x, p.y, ZOOM.TAP, true);
        } else if (st.s <= 1.01) {
          /* swipe at fit scale changes the view */
          const dx = p.x - start.p.x;
          const dy = p.y - start.p.y;
          if (Math.abs(dx) > 60 && Math.abs(dx) > Math.abs(dy) * 1.5) api.show(st.index + (dx < 0 ? 1 : -1));
        }
      },
      onPanResponderTerminate: () => {
        g.current.start = null;
      },
    });
  }, [api]);

  if (!product || !colour || !count) return null;
  const image = images[Math.min(index, count - 1)];

  /* Fit the image to the stage at scale 1. */
  const fit = Math.min(stage.w / IMAGE_RATIO, stage.h);
  const w = fit * IMAGE_RATIO;
  const h = fit;
  g.current.sw = stage.w;
  g.current.sh = stage.h;
  g.current.w = w;
  g.current.h = h;

  const zoomed = level > 1.01;
  const title = [product.name, colour.label, image.view].filter(Boolean).join(' · ');
  const bar = { height: navH, paddingHorizontal: gutter };

  return (
    <Animated.View
      style={[styles.root, { paddingTop: insets.top, paddingBottom: insets.bottom, opacity: fade }]}
      accessibilityViewIsModal
      accessibilityLabel="Image viewer"
    >
      <View style={[styles.bar, bar]}>
        <T variant="label" ink={photoInk.text} style={styles.title}>
          {title}
        </T>
        <BarButton label="CLOSE" onPress={closeZoom} />
      </View>

      <View
        ref={stageRef}
        style={styles.stage}
        onLayout={(e) => {
          setStage({ w: e.nativeEvent.layout.width, h: e.nativeEvent.layout.height });
          stageRef.current?.measureInWindow((x, y) => {
            g.current.ox = x;
            g.current.oy = y;
          });
        }}
        {...responder.panHandlers}
      >
        {stage.w > 0 ? (
          <Animated.Image
            source={image.zoom ?? image.source}
            resizeMode="contain"
            accessibilityLabel={`${product.name} — ${image.view.toLowerCase()}`}
            style={{
              position: 'absolute',
              left: (stage.w - w) / 2,
              top: (stage.h - h) / 2,
              width: w,
              height: h,
              transform: [{ translateX: xV }, { translateY: yV }, { scale: scaleV }],
            }}
          />
        ) : null}
      </View>

      <View style={[styles.bar, bar]}>
        <View style={[styles.group, count > 1 ? null : styles.hidden]}>
          <BarButton label="PREVIOUS" onPress={() => api.show(index - 1)} />
          <T variant="label" size={12} ink={photoInk.faint} style={styles.num}>
            {index + 1} / {count}
          </T>
          <BarButton label="NEXT" onPress={() => api.show(index + 1)} />
        </View>
        <View style={styles.levels}>
          <BarButton label="−" step disabled={!zoomed} onPress={() => api.at(g.current.sw / 2, g.current.sh / 2, g.current.s / ZOOM.STEP, true)} a11y="Zoom out" />
          <T variant="label" size={12} ink={photoInk.faint} align="center" style={[styles.num, styles.level]}>
            {Math.round(level * 100)}%
          </T>
          <BarButton label="+" step disabled={level >= ZOOM.MAX - 0.001} onPress={() => api.at(g.current.sw / 2, g.current.sh / 2, g.current.s * ZOOM.STEP, true)} a11y="Zoom in" />
        </View>
      </View>
    </Animated.View>
  );
}

/** .zv-btn — #5e5e5c, black while pressed, 30% when disabled. */
function BarButton({ label, onPress, disabled, step, a11y }: { label: string; onPress: () => void; disabled?: boolean; step?: boolean; a11y?: string }) {
  return (
    <Pressable
      onPress={onPress}
      disabled={disabled}
      hitSlop={10}
      accessibilityRole="button"
      accessibilityLabel={a11y ?? label}
      style={[step && styles.step, disabled && styles.disabled]}
    >
      {({ pressed }) => (
        <T variant="label" size={step ? 17 : 12.5} tracking={step ? 0 : 0.11} lineHeight={step ? 1.2 : 1.45} ink={pressed ? photoInk.text : photoInk.label} align="center">
          {label}
        </T>
      )}
    </Pressable>
  );
}

const styles = StyleSheet.create({
  root: { ...StyleSheet.absoluteFill, zIndex: 700, elevation: 700, backgroundColor: PHOTO_BG },
  bar: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', gap: 18 },
  title: { flex: 1 },
  stage: { flex: 1, overflow: 'hidden' },
  group: { flexDirection: 'row', alignItems: 'center', gap: 18, flexShrink: 1 },
  hidden: { opacity: 0 },
  levels: { flexDirection: 'row', alignItems: 'center', gap: 10 },
  level: { minWidth: 44 },
  num: { fontVariant: ['tabular-nums'] },
  step: { width: 26, alignItems: 'center' },
  disabled: { opacity: 0.3 },
});
