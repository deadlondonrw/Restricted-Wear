import { StyleSheet } from 'react-native';

import { useSettings } from '@/store/settings';

import { palettes, type Palette, type ThemeName } from './tokens';

export function useTheme(): { name: ThemeName; c: Palette } {
  const name = useSettings((s) => s.theme);
  return { name, c: palettes[name] };
}

/**
 * Themed StyleSheet factory. Styles are created once per theme and cached,
 * so components get stable style objects without re-creating them per render.
 *
 *   const useStyles = makeStyles((c) => ({ root: { backgroundColor: c.bg } }));
 *   const s = useStyles();
 */
export function makeStyles<T extends StyleSheet.NamedStyles<T>>(factory: (c: Palette) => T) {
  const cache: Partial<Record<ThemeName, T>> = {};
  return function useStyles(): T {
    const { name, c } = useTheme();
    let styles = cache[name];
    if (!styles) {
      styles = StyleSheet.create(factory(c));
      cache[name] = styles;
    }
    return styles;
  };
}
