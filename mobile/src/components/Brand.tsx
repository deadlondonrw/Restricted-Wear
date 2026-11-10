import { Image, type ImageStyle, type StyleProp } from 'react-native';

import { useTheme } from '@/theme/useTheme';

/* ==========================================================================
   Brand marks — the website's own files (img/rw-mark.png, img/rw-wordmark.png).

   Mark      The R mark is white on transparent. The website inverts it to
             black in the default mode (--mark-filter); here it is tinted with
             the foreground colour, which gives the same result in both modes.
   Wordmark  The supplied PNG sits on a #D9D9D9 plate. The website remaps the
             plate to the page colour with a colour matrix; the two results
             are generated once by scripts/sync-site.mjs with the same matrix.
   ========================================================================== */
const MARK = require('../../assets/brand/mark.png');
const WORDMARK = {
  default: require('../../assets/brand/wordmark-default.png'),
  inverted: require('../../assets/brand/wordmark-inverted.png'),
};
/** rw-wordmark.png is 674 × 160 on the website. */
export const WORDMARK_RATIO = 674 / 160;

export function Mark({
  size,
  color,
  style,
}: {
  size: number;
  /** Defaults to the foreground colour. */
  color?: string;
  style?: StyleProp<ImageStyle>;
}) {
  const { c } = useTheme();
  return (
    <Image
      source={MARK}
      resizeMode="contain"
      accessible={false}
      style={[{ width: size, height: size, tintColor: color ?? c.fg }, style]}
    />
  );
}

export function Wordmark({ width, style }: { width: number; style?: StyleProp<ImageStyle> }) {
  const { name } = useTheme();
  return (
    <Image
      source={WORDMARK[name]}
      resizeMode="contain"
      accessibilityRole="image"
      accessibilityLabel="Restricted Wear"
      style={[{ width, height: Math.round(width / WORDMARK_RATIO) }, style]}
    />
  );
}
