import { useEffect, useRef, type ReactNode } from 'react';
import { Animated, Easing, KeyboardAvoidingView, Platform, ScrollView, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import type { Section } from '@/lib/nav';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { motion } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

import { Footer } from './Footer';
import { Masthead } from './Masthead';
import { NavBar } from './NavBar';

/**
 * The website's page shell: masthead, sticky navigation, the page inside
 * .wrap, then the footer — all in one scroll, with the navigation staying
 * at the top exactly as `position: sticky` does on the website.
 *
 * Every navigation returns the page to the top and replays the page-in
 * motion (.page { animation: pagein .35s ease }).
 */
export function Page({
  active,
  children,
  bottomPadding = 0,
}: {
  /** Which nav link is underlined. */
  active?: Section;
  children: ReactNode;
  /** Space below the page content, before the footer (the website's page padding-bottom). */
  bottomPadding?: number;
}) {
  const s = useStyles();
  const insets = useSafeAreaInsets();
  const { gutter, frame } = useLayout();
  const navSeq = useUI((u) => u.navSeq);
  const scroll = useRef<ScrollView>(null);
  const progress = useAnimatedValue(0);

  useEffect(() => {
    scroll.current?.scrollTo({ y: 0, animated: false });
    progress.setValue(0);
    Animated.timing(progress, {
      toValue: 1,
      duration: motion.pageIn,
      easing: Easing.ease,
      useNativeDriver: true,
    }).start();
  }, [navSeq, progress]);

  return (
    <KeyboardAvoidingView
      style={[s.root, { paddingTop: insets.top }]}
      behavior={Platform.OS === 'web' ? undefined : 'padding'}
    >
      <ScrollView
        ref={scroll}
        style={s.scroll}
        contentContainerStyle={[s.content, { paddingBottom: insets.bottom }]}
        stickyHeaderIndices={[1]}
        keyboardShouldPersistTaps="handled"
        showsVerticalScrollIndicator={false}
      >
        <Masthead />
        <NavBar active={active} />
        <Animated.View
          style={[
            s.page,
            {
              opacity: progress,
              transform: [{ translateY: progress.interpolate({ inputRange: [0, 1], outputRange: [6, 0] }) }],
            },
          ]}
        >
          <View style={[s.wrap, { maxWidth: frame, paddingHorizontal: gutter, paddingBottom: bottomPadding }]}>
            {children}
          </View>
        </Animated.View>
        <Footer />
      </ScrollView>
    </KeyboardAvoidingView>
  );
}

const useStyles = makeStyles((c) => ({
  root: { flex: 1, backgroundColor: c.bg },
  scroll: { flex: 1 },
  content: { flexGrow: 1 },
  // #app { min-height: 60vh } — the footer stays low on short pages.
  page: { flexGrow: 1 },
  wrap: { width: '100%', alignSelf: 'center' },
}));
