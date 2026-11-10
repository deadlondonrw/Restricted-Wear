import type { ReactNode } from 'react';
import { Pressable, View, type StyleProp, type ViewStyle } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/**
 * .filter-chip — text only.
 *   idle     --dim
 *   pressed  --fg
 *   active   --fg with a 1px --fg rule beneath
 *   future   --dim-2, followed by the small STILL DEVELOPING note (.dev)
 */
type Props = {
  label: string;
  active?: boolean;
  onPress: () => void;
  /** 13 for the category row, 12.5 for sub-rows and the meta row (as on the website). */
  size?: number;
  future?: boolean;
};

export function Chip({ label, active, onPress, size = 13, future }: Props) {
  const s = useStyles();
  return (
    <Pressable
      onPress={onPress}
      hitSlop={{ top: 8, bottom: 8, left: 6, right: 6 }}
      accessibilityRole="button"
      accessibilityState={{ selected: !!active }}
      accessibilityLabel={future ? `${label}, still developing` : label}
      style={s.hit}
    >
      {({ pressed }) => (
        <View style={[s.chip, active && (future ? s.activeFuture : s.active)]}>
          <T variant="label" size={size} tracking={0.11} color={active || pressed ? 'fg' : future ? 'dim2' : 'dim'}>
            {label}
          </T>
          {future ? (
            <T variant="label" size={11} tracking={0.1} color="dim2" style={s.dev}>
              STILL DEVELOPING
            </T>
          ) : null}
        </View>
      )}
    </Pressable>
  );
}

/** .filters-group — chips wrap in centred rows (8px between rows, 26px between chips). */
export function ChipRow({ children, style }: { children: ReactNode; style?: StyleProp<ViewStyle> }) {
  const s = useStyles();
  return <View style={[s.row, style]}>{children}</View>;
}

const useStyles = makeStyles((c) => ({
  hit: { maxWidth: '100%' },
  chip: {
    flexDirection: 'row',
    alignItems: 'baseline',
    paddingVertical: 4,
    borderBottomWidth: 1,
    borderBottomColor: 'transparent',
    maxWidth: '100%',
  },
  active: { borderBottomColor: c.fg },
  activeFuture: { borderBottomColor: c.line },
  dev: { marginLeft: 6 },
  row: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'center',
    alignItems: 'center',
    columnGap: 26,
    rowGap: 8,
  },
}));
