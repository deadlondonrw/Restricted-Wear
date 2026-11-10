import { Pressable, View } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from '../T';

/** .qty-control — 1px --line border, 36×40 steps, 34px value. */
export function QtyControl({
  value,
  onDecrement,
  onIncrement,
}: {
  value: number;
  onDecrement: () => void;
  onIncrement: () => void;
}) {
  const s = useStyles();

  const step = (fn: () => void, glyph: string, label: string) => (
    <Pressable
      onPress={fn}
      hitSlop={4}
      accessibilityRole="button"
      accessibilityLabel={label}
      style={({ pressed }) => [s.step, pressed && s.pressed]}
    >
      <T size={14} lineHeight={1.2} fit maxFontSizeMultiplier={1.1}>
        {glyph}
      </T>
    </Pressable>
  );

  return (
    <View style={s.control} accessibilityLabel={`Quantity ${value}`}>
      {step(onDecrement, '−', 'Decrease quantity')}
      <T size={14} lineHeight={1.2} align="center" fit={34} maxFontSizeMultiplier={1.1} style={s.value}>
        {value}
      </T>
      {step(onIncrement, '+', 'Increase quantity')}
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  control: {
    flexDirection: 'row',
    alignItems: 'center',
    alignSelf: 'flex-start',
    borderWidth: 1,
    borderColor: c.line,
  },
  step: { width: 36, height: 40, alignItems: 'center', justifyContent: 'center' },
  pressed: { backgroundColor: c.surface },
  value: { width: 34, fontVariant: ['tabular-nums'] },
}));
