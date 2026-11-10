import { View } from 'react-native';

import type { Product } from '@/data/types';
import { useLayout } from '@/theme/responsive';
import { breakpoints } from '@/theme/tokens';

import { ProductCard } from './ProductCard';

/**
 * .grid — the website's columns for the window width: one up to 520px, two
 * up to 980px, three above; 48px (64px on wide screens) between rows and a
 * gutter between columns. Place it inside the page's .wrap.
 */
export function ProductGrid({ products, hideMarker }: { products: Product[]; hideMarker?: boolean }) {
  const { content, columns, gutter, width } = useLayout();
  const cardWidth = Math.floor((content - gutter * (columns - 1)) / columns);
  const rowGap = width <= breakpoints.grid2 ? 48 : 64;

  const rows: Product[][] = [];
  for (let i = 0; i < products.length; i += columns) rows.push(products.slice(i, i + columns));

  return (
    <View style={{ gap: rowGap }}>
      {rows.map((row) => (
        <View key={row.map((p) => p.id).join('|')} style={{ flexDirection: 'row', gap: gutter }}>
          {row.map((p) => (
            <ProductCard key={p.id} product={p} width={cardWidth} hideMarker={hideMarker} />
          ))}
        </View>
      ))}
    </View>
  );
}
