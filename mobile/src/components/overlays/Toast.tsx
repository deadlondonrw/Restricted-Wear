import { useEffect, useState } from 'react';
import { AccessibilityInfo, Animated, Easing, StyleSheet, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { useUI } from '@/store/ui';
import { motion } from '@/theme/tokens';
import { useTheme } from '@/theme/useTheme';

import { T } from '../T';

/**
 * #toast — --fg strip, --bg text at 13px / 500 / 0.1em uppercase, 24px from
 * the bottom, fades and rises in, held for 2.4 s (RW.toast).
 */
export function Toast() {
  const { c } = useTheme();
  const insets = useSafeAreaInsets();
  const current = useUI((u) => u.toast);
  const [message, setMessage] = useState('');
  const progress = useAnimatedValue(0);

  useEffect(() => {
    if (!current) return;
    setMessage(current.message);
    AccessibilityInfo.announceForAccessibility(current.message);
    Animated.timing(progress, { toValue: 1, duration: motion.toast, easing: Easing.ease, useNativeDriver: true }).start();
    const timer = setTimeout(() => {
      Animated.timing(progress, { toValue: 0, duration: motion.toast, easing: Easing.ease, useNativeDriver: true }).start();
    }, motion.toastHold);
    return () => clearTimeout(timer);
  }, [current, progress]);

  return (
    <View pointerEvents="none" style={[styles.wrap, { bottom: insets.bottom + 24 }]}>
      <Animated.View
        style={[
          styles.toast,
          {
            backgroundColor: c.fg,
            opacity: progress,
            transform: [{ translateY: progress.interpolate({ inputRange: [0, 1], outputRange: [20, 0] }) }],
          },
        ]}
      >
        <T variant="label" size={13} color="bg" align="center">
          {message}
        </T>
      </Animated.View>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { position: 'absolute', left: 16, right: 16, alignItems: 'center', zIndex: 600, elevation: 600 },
  toast: { paddingVertical: 13, paddingHorizontal: 22, maxWidth: '100%' },
});
