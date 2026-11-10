import { Pressable, View } from 'react-native';

import { isRestricted } from '@/data/products';
import { money } from '@/lib/format';
import type { ResolvedLine } from '@/store/cart';
import { makeStyles } from '@/theme/useTheme';

import { ProductMedia } from './product/ProductMedia';
import { QtyControl } from './product/QtyControl';
import { T } from './T';

/**
 * .cart-item — 60×78 thumbnail, name (700, 14px), meta (12px, --dim), then
 * quantity and line total, and REMOVE. Used by the bag drawer; the checkout
 * summary uses it without controls (`summary`).
 */
export function CartLine({
  line,
  summary,
  onOpen,
  onQty,
  onRemove,
}: {
  line: ResolvedLine;
  summary?: boolean;
  onOpen?: () => void;
  onQty?: (delta: number) => void;
  onRemove?: () => void;
}) {
  const s = useStyles();
  const { product, colour } = line;
  const meta = [
    colour.label,
    `SIZE ${line.size || '-'}`,
    summary ? `QTY ${line.qty}` : null,
    product.stream,
    !summary && isRestricted(product) ? 'RESTRICTED' : null,
  ]
    .filter(Boolean)
    .join(' · ');

  const thumb = <ProductMedia product={product} variant={line.variant} markWidth={22} />;

  return (
    <View style={s.item}>
      {onOpen ? (
        <Pressable onPress={onOpen} accessibilityRole="link" accessibilityLabel={`View ${product.name}`} style={s.thumb}>
          {thumb}
        </Pressable>
      ) : (
        <View style={s.thumb}>{thumb}</View>
      )}

      <View style={s.info}>
        <T variant="display" size={14} tracking={0.03} lineHeight={1.5}>
          {product.name}
        </T>
        <T variant="label" size={12} color="dim" style={s.meta}>
          {meta}
        </T>
        <View style={s.bottom}>
          {summary ? <View /> : <QtyControl value={line.qty} onDecrement={() => onQty?.(-1)} onIncrement={() => onQty?.(1)} />}
          <T size={15} fit align="right" style={s.total}>
            {money(line.lineTotal)}
          </T>
        </View>
        {summary ? null : (
          <Pressable onPress={onRemove} hitSlop={8} accessibilityRole="button" accessibilityLabel={`Remove ${product.name}`} style={s.remove}>
            {({ pressed }) => (
              <T variant="label" size={12} color={pressed ? 'fg' : 'dim'} style={s.underline}>
                REMOVE
              </T>
            )}
          </Pressable>
        )}
      </View>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  item: { flexDirection: 'row', gap: 14, paddingVertical: 18, borderBottomWidth: 1, borderBottomColor: c.lineSoft },
  thumb: { width: 60, height: 78, overflow: 'hidden' },
  info: { flex: 1 },
  meta: { marginTop: 4 },
  bottom: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'center', gap: 12, marginTop: 10 },
  total: { flex: 1, fontVariant: ['tabular-nums'] },
  remove: { alignSelf: 'flex-start', marginTop: 8 },
  underline: { textDecorationLine: 'underline' },
}));
