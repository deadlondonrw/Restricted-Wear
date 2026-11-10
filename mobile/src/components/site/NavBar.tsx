import { Pressable, View } from 'react-native';

import { goSection, type Section } from '@/lib/nav';
import { useCartCount } from '@/store/cart';
import { useSettings } from '@/store/settings';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { breakpoints } from '@/theme/tokens';
import { makeStyles, useTheme } from '@/theme/useTheme';

import { Mark } from '../Brand';
import { T } from '../T';

/** The website's nav links, in order. */
export const NAV_LINKS: { section: Section; label: string }[] = [
  { section: 'shop', label: 'SHOP' },
  { section: 'mainstream', label: 'MAINSTREAM' },
  { section: 'secondstream', label: 'SECONDSTREAM' },
  { section: 'authorization', label: 'RED CARD' },
  { section: 'archive', label: 'ARCHIVE' },
];

/**
 * #nav — sticky, --nav-h tall, hairlines above and below.
 *   left    the R mark (home)
 *   centre  the nav links (wider than 960px only)
 *   right   colour-mode toggle · ACCOUNT (wider than 480px) · BAG n · menu
 */
export function NavBar({ active }: { active?: Section }) {
  const s = useStyles();
  const { c } = useTheme();
  const { width, burger, navH, gutter, frame } = useLayout();
  const count = useCartCount();
  const inverted = useSettings((st) => st.theme === 'inverted');
  const toggleTheme = useSettings((st) => st.toggleTheme);
  const setMenu = useUI((u) => u.setMenu);
  const setBag = useUI((u) => u.setBag);
  const narrow = width <= breakpoints.xs;

  return (
    <View style={[s.nav, { height: navH }]}>
      <View style={[s.inner, { maxWidth: frame, paddingHorizontal: gutter }]}>
        <View style={s.left}>
          <Pressable
            onPress={() => goSection('home')}
            hitSlop={12}
            accessibilityRole="link"
            accessibilityLabel="Restricted Wear — Home"
          >
            <Mark size={18} />
          </Pressable>
        </View>

        {burger ? null : (
          <View style={s.links}>
            {NAV_LINKS.map((l) => (
              <Pressable key={l.section} onPress={() => goSection(l.section)} hitSlop={8} accessibilityRole="link">
                {({ pressed }) => (
                  <View style={s.link}>
                    <T variant="label" size={13} tracking={0.11} color={pressed || active === l.section ? 'fg' : 'dim'}>
                      {l.label}
                    </T>
                    <View style={[s.linkRule, active === l.section && s.linkRuleOn]} />
                  </View>
                )}
              </Pressable>
            ))}
          </View>
        )}

        <View style={[s.right, { gap: narrow ? 14 : 22 }]}>
          <Pressable
            onPress={toggleTheme}
            hitSlop={12}
            accessibilityRole="button"
            accessibilityLabel="Invert colours"
            accessibilityState={{ selected: inverted }}
          >
            {({ pressed }) => {
              const ink = pressed ? c.fg : c.dim;
              return (
                <View style={[s.toggle, { borderColor: ink }]}>
                  <View style={[s.toggleHalf, { backgroundColor: ink }]} />
                </View>
              );
            }}
          </Pressable>

          {narrow ? null : (
            <Pressable onPress={() => goSection('account')} hitSlop={8} accessibilityRole="link">
              {({ pressed }) => (
                <T variant="label" size={13} tracking={0.11} color={pressed || active === 'account' ? 'fg' : 'dim'}>
                  ACCOUNT
                </T>
              )}
            </Pressable>
          )}

          <Pressable
            onPress={() => setBag(true)}
            hitSlop={10}
            accessibilityRole="button"
            accessibilityLabel={`Open bag, ${count} pieces`}
            style={s.bag}
          >
            <T variant="label" size={13} tracking={0.11}>
              BAG
            </T>
            <T variant="label" size={13} tracking={0.11} color="accent" style={s.num}>
              {count}
            </T>
          </Pressable>

          {burger ? (
            <Pressable
              onPress={() => setMenu(true)}
              hitSlop={10}
              accessibilityRole="button"
              accessibilityLabel="Open menu"
              style={s.burger}
            >
              <View style={s.burgerLine} />
              <View style={s.burgerLine} />
              <View style={s.burgerLine} />
            </Pressable>
          ) : null}
        </View>
      </View>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  nav: {
    backgroundColor: c.bg,
    borderTopWidth: 1,
    borderBottomWidth: 1,
    borderColor: c.lineSoft,
    justifyContent: 'center',
  },
  inner: { width: '100%', alignSelf: 'center', flexDirection: 'row', alignItems: 'center' },
  left: { flex: 1, flexDirection: 'row' },
  links: { flexDirection: 'row', alignItems: 'center', gap: 38 },
  link: { paddingVertical: 4 },
  // .nav-links a.active::after — a 1px accent rule 3px below the label
  linkRule: { position: 'absolute', left: 0, right: 0, bottom: -3, height: 1 },
  linkRuleOn: { backgroundColor: c.accent },
  right: { flex: 1, flexDirection: 'row', alignItems: 'center', justifyContent: 'flex-end' },
  // The website's half-filled circle (16px, 1.6px stroke).
  toggle: { width: 16, height: 16, borderRadius: 8, borderWidth: 1.5, overflow: 'hidden' },
  toggleHalf: { position: 'absolute', top: 0, bottom: 0, right: 0, width: '50%' },
  bag: { flexDirection: 'row', alignItems: 'center', gap: 7, paddingVertical: 4 },
  num: { fontVariant: ['tabular-nums'] },
  burger: { gap: 5, paddingVertical: 6, paddingLeft: 6 },
  burgerLine: { width: 20, height: 1, backgroundColor: c.fg },
}));
