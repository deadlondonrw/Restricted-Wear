import { Pressable, View, type StyleProp, type ViewStyle } from 'react-native';

import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/**
 * .text-link — 13px, 500, 0.11em, uppercase, with a 1px --line rule beneath
 * that turns --fg while pressed.
 */
export function TextLink({
  label,
  onPress,
  style,
}: {
  label: string;
  /** Omit inside a pressable parent: the link is then only drawn, and `pressed` drives its rule. */
  onPress?: () => void;
  style?: StyleProp<ViewStyle>;
}) {
  if (!onPress) return <TextLinkLabel label={label} style={style} />;
  return (
    <Pressable onPress={onPress} hitSlop={12} accessibilityRole="link" accessibilityLabel={label} style={[linkStyle, style]}>
      {({ pressed }) => <TextLinkLabel label={label} pressed={pressed} />}
    </Pressable>
  );
}

export function TextLinkLabel({
  label,
  pressed,
  style,
}: {
  label: string;
  pressed?: boolean;
  style?: StyleProp<ViewStyle>;
}) {
  const s = useStyles();
  return (
    <View style={[s.rule, pressed && s.rulePressed, style]}>
      <T variant="label" size={13} tracking={0.11} align="center">
        {label}
      </T>
    </View>
  );
}

const linkStyle = { alignSelf: 'center', maxWidth: '100%' } as const;

const useStyles = makeStyles((c) => ({
  rule: { borderBottomWidth: 1, borderBottomColor: c.line, paddingBottom: 4, maxWidth: '100%', alignSelf: 'center' },
  rulePressed: { borderBottomColor: c.fg },
}));
