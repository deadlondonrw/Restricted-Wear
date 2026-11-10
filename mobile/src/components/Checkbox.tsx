import { Pressable, View } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/** .check — a square box and a sentence-case label in --dim at 14px. */
export function Checkbox({
  label,
  checked,
  onChange,
}: {
  label: string;
  checked: boolean;
  onChange: (next: boolean) => void;
}) {
  const s = useStyles();
  return (
    <Pressable
      onPress={() => onChange(!checked)}
      accessibilityRole="checkbox"
      accessibilityState={{ checked }}
      accessibilityLabel={label}
      hitSlop={8}
      style={s.row}
    >
      <View style={[s.box, checked && s.boxOn]}>{checked ? <View style={s.mark} /> : null}</View>
      <T size={14} color="dim" fit style={s.label}>
        {label}
      </T>
    </Pressable>
  );
}

const useStyles = makeStyles((c) => ({
  row: { flexDirection: 'row', alignItems: 'center', gap: 10, marginTop: 4, marginBottom: 20 },
  box: {
    width: 16,
    height: 16,
    borderWidth: 1,
    borderColor: c.dim2,
    alignItems: 'center',
    justifyContent: 'center',
  },
  boxOn: { borderColor: c.fg, backgroundColor: c.fg },
  mark: { width: 6, height: 6, backgroundColor: c.bg },
  label: { flex: 1 },
}));
