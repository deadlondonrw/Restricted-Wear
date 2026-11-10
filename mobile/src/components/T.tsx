import { Children } from 'react';
import { Text, type TextProps, type TextStyle } from 'react-native';

import { useOneLine } from '@/hooks/useOneLine';
import { fonts, type ColorName, type Weight } from '@/theme/tokens';
import { useTheme } from '@/theme/useTheme';

/**
 * Typography — the website's type utilities, all in Roboto Condensed:
 *   display  .display          700, uppercase, 0.01em
 *   label    .mono / .label    500, uppercase, 0.1em, 12.5px
 *   heading  h3                700, 19px
 *   body     body              400, 15px / 1.55
 *
 * display, label and heading are single-line: they shrink to fit and never
 * wrap or run off screen (see useOneLine). body text is running copy and
 * wraps normally. `wrap` turns the single line off for long notes; `fit`
 * turns it on for body text, or passes the known available width.
 */
type Variant = 'display' | 'label' | 'heading' | 'body';

const DEFAULTS: Record<
  Variant,
  { size: number; tracking: number; lineHeight: number; upper: boolean; weight: Weight; scale: number; oneLine: boolean }
> = {
  display: { size: 32, tracking: 0.01, lineHeight: 1.05, upper: true, weight: 'bold', scale: 1.15, oneLine: true },
  label: { size: 12.5, tracking: 0.1, lineHeight: 1.45, upper: true, weight: 'medium', scale: 1.2, oneLine: true },
  heading: { size: 19, tracking: 0, lineHeight: 1.3, upper: false, weight: 'bold', scale: 1.2, oneLine: true },
  body: { size: 15, tracking: 0, lineHeight: 1.55, upper: false, weight: 'regular', scale: 1.3, oneLine: false },
};

export type TProps = TextProps & {
  variant?: Variant;
  size?: number;
  color?: ColorName;
  /** Explicit colour for fixed inks (photo labels, the Red Card). */
  ink?: string;
  /** Letter-spacing in em (the website's unit). */
  tracking?: number;
  /** Line-height multiplier. */
  lineHeight?: number;
  weight?: Weight;
  align?: TextStyle['textAlign'];
  upper?: boolean;
  /** true: keep on one line and shrink to fit. A number: the width available, in px. */
  fit?: boolean | number;
  /** Running copy: allow more than one line. */
  wrap?: boolean;
};

function plainText(children: TextProps['children']): string {
  return Children.toArray(children)
    .filter((child) => typeof child === 'string' || typeof child === 'number')
    .join('');
}

export function T({
  variant = 'body',
  size,
  color,
  ink,
  tracking,
  lineHeight,
  weight,
  align,
  upper,
  fit,
  wrap,
  style,
  children,
  maxFontSizeMultiplier,
  ...rest
}: TProps) {
  const { c } = useTheme();
  const d = DEFAULTS[variant];
  const fontWeight = weight ?? d.weight;
  const isUpper = upper ?? d.upper;
  const trackingEm = tracking ?? d.tracking;
  const maxScale = maxFontSizeMultiplier ?? d.scale;
  const oneLine = !wrap && (fit !== undefined ? fit !== false : d.oneLine);
  const raw = plainText(children);

  const { fontSize, props } = useOneLine({
    enabled: oneLine,
    text: isUpper ? raw.toUpperCase() : raw,
    max: size ?? d.size,
    weight: fontWeight,
    tracking: trackingEm,
    maxScale,
    width: typeof fit === 'number' ? fit : undefined,
  });

  return (
    <Text
      maxFontSizeMultiplier={maxScale}
      {...props}
      style={[
        {
          fontFamily: fonts[fontWeight],
          fontSize,
          lineHeight: Math.round(fontSize * (lineHeight ?? d.lineHeight)),
          letterSpacing: fontSize * trackingEm,
          color: ink ?? c[color ?? 'fg'],
          textTransform: isUpper ? 'uppercase' : 'none',
          textAlign: align,
          flexShrink: 1,
        },
        style,
      ]}
      {...rest}
    >
      {children}
    </Text>
  );
}
