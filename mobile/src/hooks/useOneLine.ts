import { useLayoutEffect, useRef, useState } from 'react';
import { Platform, useWindowDimensions, type LayoutChangeEvent, type Text } from 'react-native';

import { fitOneLine, measureEm } from '@/theme/responsive';
import type { Weight } from '@/theme/tokens';

/* ==========================================================================
   One-line text that always fits.

   A label is never allowed to wrap onto a second line or to run past the
   edge of the screen. Three mechanisms make sure of it, in this order:

   1. Calculation. The string's width is known from the font's glyph widths,
      so the size that fits the available width is computed directly — from
      `width` when the caller knows it, otherwise from the width the text is
      given by the layout.
   2. Android / iOS: the system's own shrink-to-fit (adjustsFontSizeToFit)
      covers any rounding difference.
   3. Web: the rendered text is measured and scaled if it is still too wide.
   ========================================================================== */

/** Text measurably wider than its box before the calculated size is reduced. */
const OVERFLOW_TOLERANCE = 1.03;

type Options = {
  enabled: boolean;
  /** The string as displayed (already uppercased when the style uppercases). */
  text: string;
  /** The designed size — text is only ever reduced from it. */
  max: number;
  weight: Weight;
  /** Letter-spacing in em. */
  tracking: number;
  maxScale: number;
  /** Available width in px, when the caller knows it. */
  width?: number;
};

export function useOneLine({ enabled, text, max, weight, tracking, maxScale, width }: Options) {
  const { width: windowWidth, fontScale } = useWindowDimensions();
  const ref = useRef<Text>(null);
  const [seen, setSeen] = useState<{ window: number; avail: number } | null>(null);
  const [web, setWeb] = useState<{ key: string; scale: number } | null>(null);

  const scale = Math.max(1, Math.min(fontScale, maxScale));
  const laidOut = seen && seen.window === windowWidth ? seen.avail : undefined;

  let fontSize = max;
  if (enabled) {
    if (width !== undefined) {
      fontSize = fitOneLine({ text, max, width, weight, tracking, fontScale, maxScale });
    } else if (laidOut !== undefined) {
      const needed = measureEm(text, weight, tracking) * max * scale;
      if (needed > laidOut * OVERFLOW_TOLERANCE) {
        fontSize = fitOneLine({ text, max, width: laidOut, weight, tracking, fontScale, maxScale });
      }
    }
  }

  const webKey = `${text}|${windowWidth}|${max}|${fontSize}`;
  if (web && web.key === webKey) fontSize = Math.max(6, Math.floor(fontSize * web.scale * 4) / 4);

  /* Web: measure what was actually rendered. */
  useLayoutEffect(() => {
    if (!enabled || Platform.OS !== 'web') return;
    const node = ref.current as unknown as { scrollWidth?: number; clientWidth?: number } | null;
    if (!node || !node.scrollWidth || !node.clientWidth) return;
    if (node.scrollWidth > node.clientWidth + 0.5) {
      const factor = (node.clientWidth / node.scrollWidth) * 0.98;
      setWeb((prev) => ({ key: webKey, scale: (prev && prev.key === webKey ? prev.scale : 1) * factor }));
    }
  });

  const onLayout =
    enabled && width === undefined
      ? (e: LayoutChangeEvent) => {
          const w = e.nativeEvent.layout.width;
          /* Only ever grows for a given window: shrinking the text must not shrink its own budget. */
          setSeen((prev) =>
            prev && prev.window === windowWidth && w <= prev.avail + 0.5 ? prev : { window: windowWidth, avail: w },
          );
        }
      : undefined;

  const props = enabled
    ? ({
        ref,
        onLayout,
        numberOfLines: 1,
        adjustsFontSizeToFit: Platform.OS !== 'web',
        minimumFontScale: 0.5,
      } as const)
    : ({ ref } as const);

  return { fontSize, props };
}
