import { Image, StyleSheet, View } from 'react-native';

import { variantOf } from '@/data/products';
import type { Product } from '@/data/types';
import { makeStyles } from '@/theme/useTheme';

import { Mark } from '../Brand';

/**
 * RW.mediaHtml — .ph
 *   with a photo   --photo-bg (#F4F0E8 in both modes), image contain-fit
 *   without one    --surface with the R mark as a quiet 7% watermark
 * Fills its parent absolutely.
 */
export function ProductMedia({
  product,
  variant,
  index = 0,
  markWidth,
}: {
  product?: Product;
  /** Colourway key; the default colourway when omitted. */
  variant?: string;
  /** 0 = FRONT, 1 = BACK. */
  index?: number;
  /** Watermark size in px for the placeholder. */
  markWidth?: number;
}) {
  const s = useStyles();
  const colour = product ? variantOf(product, variant) : undefined;
  const image = colour?.images[index];

  if (product && image) {
    return (
      <View style={s.photo}>
        <Image
          source={image.source}
          resizeMode="contain"
          style={s.image}
          accessibilityRole="image"
          accessibilityLabel={`${product.name} — ${image.view.toLowerCase()}`}
        />
      </View>
    );
  }
  return <Placeholder markWidth={markWidth} />;
}

/** RW.editorialHtml(null) — the placeholder for imagery that has not been supplied. */
export function Placeholder({ markWidth = 56 }: { markWidth?: number }) {
  const s = useStyles();
  return (
    <View style={s.placeholder}>
      <Mark size={markWidth} style={s.mark} />
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  photo: { ...StyleSheet.absoluteFill, backgroundColor: c.photoBg },
  image: { width: '100%', height: '100%' },
  placeholder: {
    ...StyleSheet.absoluteFill,
    backgroundColor: c.surface,
    alignItems: 'center',
    justifyContent: 'center',
  },
  mark: { opacity: 0.07 },
}));
