import { Pressable, type StyleProp, type ViewStyle } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/**
 * .btn / .btn.primary / .btn.ghost — square, 1px border, 13px / 500 / 0.11em
 * uppercase, 16×30 padding. The website's hover state shows while pressed.
 * The label stays on one line and shrinks to fit a narrow button.
 */
export type ButtonVariant = 'default' | 'primary' | 'ghost';

type Props = {
  label: string;
  onPress?: () => void;
  variant?: ButtonVariant;
  /** .btn.block — full width */
  block?: boolean;
  disabled?: boolean;
  style?: StyleProp<ViewStyle>;
};

export function Button({ label, onPress, variant = 'default', block, disabled, style }: Props) {
  const s = useStyles();
  return (
    <Pressable
      onPress={onPress}
      disabled={disabled}
      accessibilityRole="button"
      accessibilityLabel={label}
      accessibilityState={{ disabled: !!disabled }}
      style={({ pressed }) => [
        s.base,
        block ? s.block : s.inline,
        variant === 'primary' && s.primary,
        variant === 'ghost' && s.ghost,
        pressed && (variant === 'primary' ? s.primaryPressed : variant === 'ghost' ? s.ghostPressed : s.defaultPressed),
        disabled && s.disabled,
        style,
      ]}
    >
      {({ pressed }) => (
        <T
          variant="label"
          size={13}
          tracking={0.11}
          align="center"
          color={
            variant === 'primary'
              ? pressed ? 'fg' : 'bg'
              : variant === 'ghost'
                ? pressed ? 'fg' : 'dim'
                : pressed ? 'bg' : 'fg'
          }
        >
          {label}
        </T>
      )}
    </Pressable>
  );
}

const useStyles = makeStyles((c) => ({
  base: {
    minHeight: 50,
    paddingVertical: 15,
    paddingHorizontal: 30,
    borderWidth: 1,
    borderColor: c.fg,
    alignItems: 'center',
    justifyContent: 'center',
    maxWidth: '100%',
  },
  inline: { alignSelf: 'flex-start' },
  // Full-width buttons can sit two to a row: less side padding leaves the label more room.
  block: { alignSelf: 'stretch', paddingHorizontal: 14 },
  // .btn:hover { background: fg; color: bg }
  defaultPressed: { backgroundColor: c.fg },
  // .btn.primary / :hover { background: transparent; color: fg }
  primary: { backgroundColor: c.fg },
  primaryPressed: { backgroundColor: 'transparent' },
  // .btn.ghost / :hover { border-color: fg; color: fg }
  ghost: { borderColor: c.line },
  ghostPressed: { borderColor: c.fg },
  disabled: { opacity: 0.35 },
}));
