import { Animated, Easing, Pressable, View } from 'react-native';

import type { Product } from '@/data/types';
import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { money } from '@/lib/format';
import { goProduct } from '@/lib/nav';
import { motion, palettes, photoInk } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

import { T } from '../T';

import { ProductMedia } from './ProductMedia';

/**
 * RW.productCardHtml — .card
 * 3:4 media, then a centred caption: name (700, 16px, 0.03em), direction
 * marker, price. RESTRICTED / LOW STOCK shows in the top-left corner.
 * The website's hover zoom (scale 1.03) plays while pressed.
 */
export function ProductCard({
  product,
  width,
  hideMarker,
}: {
  product: Product;
  /** Card width in px — the name is fitted to it on one line. */
  width: number;
  /** Collection pages omit the direction marker (it is the page title). */
  hideMarker?: boolean;
}) {
  const s = useStyles();
  const zoom = useAnimatedValue(1);

  const animate = (toValue: number) =>
    Animated.timing(zoom, {
      toValue,
      duration: motion.mediaZoom,
      easing: Easing.bezier(0.16, 1, 0.3, 1),
      useNativeDriver: true,
    }).start();

  const status = product.status === 'RESTRICTED' || product.status === 'LOW STOCK' ? product.status : null;

  return (
    <Pressable
      onPress={() => goProduct(product.id)}
      onPressIn={() => animate(1.03)}
      onPressOut={() => animate(1)}
      accessibilityRole="link"
      accessibilityLabel={`${product.name}, ${product.stream}, ${money(product.price)}`}
      style={{ width }}
    >
      <View style={s.media}>
        <Animated.View style={[s.fill, { transform: [{ scale: zoom }] }]}>
          <ProductMedia product={product} />
        </Animated.View>
        {status ? (
          <View style={s.status} pointerEvents="none">
            <T
              variant="label"
              size={12}
              tracking={0.11}
              ink={status === 'RESTRICTED' ? palettes.default.accent : photoInk.label}
            >
              {status}
            </T>
          </View>
        ) : null}
      </View>

      <View style={s.body}>
        <T variant="display" size={16} tracking={0.03} lineHeight={1.5} fit={width} align="center">
          {product.name}
        </T>
        {hideMarker ? null : (
          <T variant="label" size={12} tracking={0.12} color="dim" align="center">
            {product.stream}
          </T>
        )}
        <T size={14} align="center" fit style={s.price}>
          {money(product.price)}
        </T>
      </View>
    </Pressable>
  );
}

const useStyles = makeStyles(() => ({
  fill: { flex: 1 },
  media: { aspectRatio: 3 / 4, overflow: 'hidden' },
  status: { position: 'absolute', top: 14, left: 14, right: 14 },
  body: { paddingTop: 20, alignItems: 'center', gap: 6 },
  price: { marginTop: 2, fontVariant: ['tabular-nums'] },
}));
