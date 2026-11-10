import { useEffect, useState } from 'react';
import { Animated, Easing, StyleSheet, Text } from 'react-native';

import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { fonts } from '@/theme/tokens';
import { useTheme } from '@/theme/useTheme';

import { T } from '../T';

/**
 * RW.qty67 — the full-screen overlay shown once per launch when the quantity
 * is held at 67 for three seconds: "67" blinks for 2 s (0.1 s, 20 times),
 * then "Are you stupid?" fades out over 2 s.
 */
export function Qty67() {
  const { c } = useTheme();
  const { width, height, gutter } = useLayout();
  const showing = useUI((u) => u.qty67);
  const setQty67 = useUI((u) => u.setQty67);
  const [phase, setPhase] = useState<'num' | 'msg'>('num');
  const [on, setOn] = useState(true);
  const fade = useAnimatedValue(1);

  useEffect(() => {
    if (!showing) return;
    setPhase('num');
    setOn(true);
    fade.setValue(1);
    const blink = setInterval(() => setOn((v) => !v), 50);
    const toMessage = setTimeout(() => {
      clearInterval(blink);
      setPhase('msg');
      Animated.timing(fade, { toValue: 0, duration: 2000, easing: Easing.ease, useNativeDriver: true }).start();
    }, 2000);
    const done = setTimeout(() => setQty67(false), 4000);
    return () => {
      clearInterval(blink);
      clearTimeout(toMessage);
      clearTimeout(done);
    };
  }, [showing, fade, setQty67]);

  if (!showing) return null;

  // .qty67-num { font-size: min(62vw, 78vh) }
  const numSize = Math.floor(Math.min(width * 0.62, height * 0.78));

  return (
    <Animated.View
      style={[styles.root, { backgroundColor: c.bg, padding: gutter, opacity: fade }]}
      accessibilityLiveRegion="assertive"
    >
      {phase === 'num' ? (
        <Text
          allowFontScaling={false}
          style={{ fontFamily: fonts.bold, fontSize: numSize, lineHeight: numSize, color: c.fg, opacity: on ? 1 : 0 }}
        >
          67
        </Text>
      ) : (
        <T variant="display" upper={false} size={Math.max(36, Math.min(120, width * 0.08))} lineHeight={1.2} fit={width - gutter * 2} align="center">
          Are you stupid?
        </T>
      )}
    </Animated.View>
  );
}

const styles = StyleSheet.create({
  root: { ...StyleSheet.absoluteFill, zIndex: 900, elevation: 900, alignItems: 'center', justifyContent: 'center' },
});
