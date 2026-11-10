import { useEffect, useState } from 'react';
import { Animated, Easing, StyleSheet, View } from 'react-native';

import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { useLayout } from '@/theme/responsive';
import { useTheme } from '@/theme/useTheme';

import { Wordmark } from '../Brand';

/**
 * #loader — the wordmark (240px, at most 60vw) on --bg with a 1px, 160px
 * progress line 34px beneath it. The line fills over 1.4 s; the loader stays
 * for at least 1.6 s and until the account has been restored, then fades
 * out over 0.6 s (RW.runLoader).
 */
export function IntroLoader({ ready }: { ready: boolean }) {
  const { c } = useTheme();
  const { width } = useLayout();
  const [minimum, setMinimum] = useState(false);
  const [gone, setGone] = useState(false);
  const bar = useAnimatedValue(0);
  const fade = useAnimatedValue(1);

  useEffect(() => {
    const start = setTimeout(() => {
      Animated.timing(bar, { toValue: 1, duration: 1400, easing: Easing.bezier(0.4, 0, 0.2, 1), useNativeDriver: false }).start();
    }, 60);
    const min = setTimeout(() => setMinimum(true), 1600);
    return () => {
      clearTimeout(start);
      clearTimeout(min);
    };
  }, [bar]);

  useEffect(() => {
    if (!minimum || !ready) return;
    Animated.timing(fade, { toValue: 0, duration: 600, easing: Easing.ease, useNativeDriver: true }).start(() => setGone(true));
  }, [minimum, ready, fade]);

  if (gone) return null;

  return (
    <Animated.View style={[styles.root, { backgroundColor: c.bg, opacity: fade }]} accessibilityLabel="Restricted Wear">
      <Wordmark width={Math.min(240, width * 0.6)} />
      <View style={[styles.track, { backgroundColor: c.lineSoft }]}>
        <Animated.View
          style={[styles.fill, { backgroundColor: c.fg, width: bar.interpolate({ inputRange: [0, 1], outputRange: [0, 160] }) }]}
        />
      </View>
    </Animated.View>
  );
}

const styles = StyleSheet.create({
  root: { ...StyleSheet.absoluteFill, zIndex: 999, elevation: 999, alignItems: 'center', justifyContent: 'center', padding: 20 },
  track: { width: 160, height: 1, marginTop: 34, overflow: 'hidden' },
  fill: { height: 1 },
});
