import { View } from 'react-native';

import { money } from '@/lib/format';
import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/** .cart-summary-line / .cart-total — SUBTOTAL, SHIPPING, TOTAL. */
export function Summary({ subtotal, shipping }: { subtotal: number; shipping: string }) {
  const s = useStyles();
  return (
    <View>
      <View style={s.line}>
        <T variant="label" color="dim" style={s.k}>
          SUBTOTAL
        </T>
        <T size={15} fit align="right" style={s.v}>
          {money(subtotal)}
        </T>
      </View>
      <View style={s.line}>
        <T variant="label" color="dim" style={s.k}>
          SHIPPING
        </T>
        <T variant="label" color="dim" align="right" style={s.v}>
          {shipping}
        </T>
      </View>
      <View style={s.total}>
        <T variant="label" size={14} tracking={0.08} weight="bold" style={s.k}>
          TOTAL
        </T>
        <T variant="label" size={14} tracking={0.08} weight="bold" align="right" style={s.v}>
          {money(subtotal)}
        </T>
      </View>
    </View>
  );
}

const useStyles = makeStyles(() => ({
  line: { flexDirection: 'row', alignItems: 'center', gap: 12, marginBottom: 10 },
  total: { flexDirection: 'row', alignItems: 'center', gap: 12, marginTop: 14, marginBottom: 18 },
  k: { flexShrink: 0 },
  v: { flex: 1, fontVariant: ['tabular-nums'] },
}));
