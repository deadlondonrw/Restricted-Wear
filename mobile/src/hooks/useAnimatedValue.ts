import { useState } from 'react';
import { Animated } from 'react-native';

/**
 * A stable Animated.Value for the component's lifetime.
 * React Native ships its own `useAnimatedValue`, but react-native-web does
 * not, so this keeps the web build (used for previews) working too.
 */
export function useAnimatedValue(initial: number): Animated.Value {
  const [value] = useState(() => new Animated.Value(initial));
  return value;
}
