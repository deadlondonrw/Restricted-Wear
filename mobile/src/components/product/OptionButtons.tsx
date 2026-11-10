import { Pressable, View } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from '../T';

/**
 * .size-btn / .colour-btn — min 48×44, 1px --line border, 8px apart, wrapping.
 * The selected one inverts to --fg; an unavailable one is struck through.
 */
export type Option = { key: string; label: string; disabled?: boolean };

export function OptionButtons({
  options,
  selected,
  onSelect,
  kind,
}: {
  options: Option[];
  selected: string | null;
  onSelect: (key: string) => void;
  /** size: 13px / 0.08em, 10px padding. colour: 13px / 0.11em, 16px padding. */
  kind: 'size' | 'colour';
}) {
  const s = useStyles();
  return (
    <View style={s.row} accessibilityRole="radiogroup">
      {options.map((o) => {
        const active = selected === o.key;
        return (
          <Pressable
            key={o.key}
            disabled={o.disabled}
            onPress={() => onSelect(o.key)}
            accessibilityRole="radio"
            accessibilityLabel={`${kind === 'size' ? 'Size' : 'Colour'} ${o.label}${o.disabled ? ', unavailable' : ''}`}
            accessibilityState={{ selected: active, disabled: !!o.disabled }}
            style={({ pressed }) => [
              s.btn,
              kind === 'colour' && s.colour,
              o.disabled && s.disabled,
              pressed && !active && s.pressed,
              active && s.active,
            ]}
          >
            <T
              variant="label"
              size={13}
              tracking={kind === 'size' ? 0.08 : 0.11}
              color={active ? 'bg' : o.disabled ? 'dim2' : 'fg'}
              style={o.disabled ? s.struck : undefined}
            >
              {o.label}
            </T>
          </Pressable>
        );
      })}
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  row: { flexDirection: 'row', flexWrap: 'wrap', gap: 8 },
  btn: {
    minWidth: 48,
    height: 44,
    paddingHorizontal: 10,
    borderWidth: 1,
    borderColor: c.line,
    alignItems: 'center',
    justifyContent: 'center',
    maxWidth: '100%',
  },
  colour: { paddingHorizontal: 16 },
  pressed: { borderColor: c.fg },
  active: { backgroundColor: c.fg, borderColor: c.fg },
  disabled: { borderColor: c.lineSoft },
  struck: { textDecorationLine: 'line-through' },
}));
