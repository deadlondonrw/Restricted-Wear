import { Animated, Easing } from 'react-native';

import { motion } from '@/theme/tokens';

import { useAnimatedValue } from './useAnimatedValue';

/**
 * Touch equivalent of the website's :hover transitions
 * (transition: background .18s ease, color .18s ease, border-color .18s ease).
 * Returns a 0→1 progress value to interpolate colours from.
 */
export function usePress(duration: number = motion.press) {
  const progress = useAnimatedValue(0);

  const animate = (toValue: number) =>
    Animated.timing(progress, {
      toValue,
      duration,
      easing: Easing.out(Easing.ease),
      useNativeDriver: false,
    }).start();

  return {
    progress,
    onPressIn: () => animate(1),
    onPressOut: () => animate(0),
  };
}

/** Same colour at zero alpha, so colour transitions don't pass through black. */
export const clear = (hex: string) => (hex.startsWith('#') && hex.length === 7 ? `${hex}00` : hex);
