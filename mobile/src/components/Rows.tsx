import { View } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/**
 * Key / value rows.
 *   specs  .specs .row       value 14px regular, rule beneath every row
 *   auth   .auth-fields .row value 13px / 500 / 0.09em uppercase, `on` in --accent
 * The key keeps its size; the value takes the rest of the row on one line.
 */
export type Row = { k: string; v: string; on?: boolean };

export function Rows({ rows, kind = 'specs' }: { rows: Row[]; kind?: 'specs' | 'auth' }) {
  const s = useStyles();
  return (
    <View style={s.list}>
      {rows.map((r) => (
        <View key={r.k} style={[s.row, kind === 'auth' && s.rowAuth]}>
          <T variant="label" color="dim" style={s.k}>
            {r.k}
          </T>
          {kind === 'auth' ? (
            <T variant="label" size={13} tracking={0.09} color={r.on ? 'accent' : 'fg'} align="right" style={s.v}>
              {r.v}
            </T>
          ) : (
            <T size={14} lineHeight={1.4} fit align="right" style={s.v}>
              {r.v}
            </T>
          )}
        </View>
      ))}
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  list: { borderTopWidth: 1, borderTopColor: c.lineSoft },
  row: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    gap: 20,
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: c.lineSoft,
  },
  rowAuth: { gap: 16, paddingVertical: 13 },
  k: { flexShrink: 0, maxWidth: '62%' },
  v: { flex: 1 },
}));
