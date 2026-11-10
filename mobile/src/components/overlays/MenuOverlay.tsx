import { useEffect } from 'react';
import { Animated, Easing, Linking, Pressable, StyleSheet, View } from 'react-native';
import { usePathname } from 'expo-router';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { BRAND } from '@/data/products';
import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { goSection, type Section } from '@/lib/nav';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { motion } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

import { Mark } from '../Brand';
import { NAV_LINKS } from '../site/NavBar';
import { T } from '../T';

const LINKS: { section: Section; label: string }[] = [...NAV_LINKS, { section: 'account', label: 'ACCOUNT' }];

const PATHS: Record<string, Section> = {
  '/shop': 'shop',
  '/mainstream': 'mainstream',
  '/secondstream': 'secondstream',
  '/red-card': 'authorization',
  '/archive': 'archive',
  '/account': 'account',
};

/**
 * #mobile-menu — full screen: the mark and CLOSE, the links at 28px / 700 in
 * --dim (the current page in --fg), then the copyright and Telegram lines.
 */
export function MenuOverlay() {
  const s = useStyles();
  const insets = useSafeAreaInsets();
  const { gutter, navH, content } = useLayout();
  const open = useUI((u) => u.menuOpen);
  const setMenu = useUI((u) => u.setMenu);
  const pathname = usePathname();
  const active = pathname.startsWith('/product') ? 'shop' : PATHS[pathname];
  const progress = useAnimatedValue(0);

  useEffect(() => {
    if (!open) return;
    progress.setValue(0);
    Animated.timing(progress, { toValue: 1, duration: motion.pageIn, easing: Easing.ease, useNativeDriver: true }).start();
  }, [open, progress]);

  if (!open) return null;

  return (
    <Animated.View
      style={[
        s.root,
        {
          paddingTop: insets.top,
          paddingBottom: insets.bottom + gutter,
          paddingHorizontal: gutter,
          opacity: progress,
          transform: [{ translateY: progress.interpolate({ inputRange: [0, 1], outputRange: [6, 0] }) }],
        },
      ]}
      accessibilityViewIsModal
    >
      <View style={[s.top, { height: navH }]}>
        <Mark size={18} />
        <Pressable onPress={() => setMenu(false)} hitSlop={12} accessibilityRole="button" accessibilityLabel="Close menu">
          <T variant="label" size={13} tracking={0.11}>
            CLOSE
          </T>
        </Pressable>
      </View>

      <View style={s.links}>
        {LINKS.map((l) => (
          <Pressable key={l.section} onPress={() => goSection(l.section)} accessibilityRole="link">
            {({ pressed }) => (
              <T
                variant="display"
                size={28}
                tracking={0.02}
                lineHeight={1.3}
                fit={content}
                align="center"
                color={pressed || active === l.section ? 'fg' : 'dim'}
              >
                {l.label}
              </T>
            )}
          </Pressable>
        ))}
      </View>

      <View style={s.meta}>
        <T variant="label" color="dim" align="center" fit={content}>
          © 2026 RESTRICTED WEAR
        </T>
        <Pressable onPress={() => Linking.openURL(BRAND.telegramUrl).catch(() => {})} accessibilityRole="link" hitSlop={6}>
          <T variant="label" color="dim" align="center" fit={content}>
            TELEGRAM {BRAND.telegram}
          </T>
        </Pressable>
      </View>
    </Animated.View>
  );
}

const useStyles = makeStyles((c) => ({
  root: { ...StyleSheet.absoluteFill, backgroundColor: c.bg, zIndex: 300, elevation: 300 },
  top: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    borderBottomWidth: 1,
    borderBottomColor: c.lineSoft,
  },
  links: { marginTop: 44, alignItems: 'center', gap: 24 },
  meta: {
    marginTop: 'auto',
    paddingTop: 26,
    borderTopWidth: 1,
    borderTopColor: c.lineSoft,
    alignItems: 'center',
    gap: 8,
  },
}));
