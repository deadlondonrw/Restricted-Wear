import { useEffect, useState } from 'react';
import { Animated, Easing, Pressable, ScrollView, StyleSheet, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { pieces } from '@/lib/format';
import { goCheckout, goProduct, redirectForAuthorization } from '@/lib/nav';
import { purchaseBlocker, useUser } from '@/store/account';
import { useCart, useCartSummary } from '@/store/cart';
import { toast, useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { motion } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

import { Button } from '../Button';
import { CartLine } from '../CartLine';
import { Summary } from '../Summary';
import { T } from '../T';

/**
 * #cart-drawer — slides in from the right over a dimmed page, min(430px, 92vw)
 * wide: BAG and the piece count, the lines, then subtotal, shipping, total,
 * the authorization line and CHECKOUT.
 */
export function BagDrawer() {
  const s = useStyles();
  const insets = useSafeAreaInsets();
  const { width, gutter } = useLayout();
  const open = useUI((u) => u.bagOpen);
  const setBag = useUI((u) => u.setBag);
  const { lines, count, subtotal } = useCartSummary();
  const changeQty = useCart((c) => c.changeQty);
  const remove = useCart((c) => c.remove);
  const user = useUser();
  const [mounted, setMounted] = useState(open);
  const progress = useAnimatedValue(0);
  const panel = Math.min(430, Math.round(width * 0.92));

  useEffect(() => {
    if (open) setMounted(true);
    Animated.timing(progress, {
      toValue: open ? 1 : 0,
      duration: motion.drawer,
      easing: Easing.ease,
      useNativeDriver: true,
    }).start(({ finished }) => {
      if (finished && !open) setMounted(false);
    });
  }, [open, progress]);

  if (!mounted) return null;

  const blocker = lines.length ? purchaseBlocker(user, lines) : null;

  const checkout = () => {
    if (!lines.length) {
      toast('YOUR BAG IS EMPTY');
      return;
    }
    if (blocker) redirectForAuthorization(blocker, 'checkout');
    else goCheckout();
  };

  return (
    <View style={s.root} pointerEvents={open ? 'auto' : 'none'}>
      <Animated.View style={[s.backdrop, { opacity: progress }]}>
        <Pressable style={StyleSheet.absoluteFill} onPress={() => setBag(false)} accessibilityLabel="Close bag" />
      </Animated.View>

      <Animated.View
        style={[
          s.panel,
          {
            width: panel,
            paddingTop: insets.top,
            paddingBottom: insets.bottom,
            transform: [{ translateX: progress.interpolate({ inputRange: [0, 1], outputRange: [panel, 0] }) }],
          },
        ]}
        accessibilityViewIsModal
      >
        <View style={[s.head, { paddingHorizontal: gutter }]}>
          <View style={s.headTitle}>
            <T variant="display" size={18} style={s.title}>
              BAG
            </T>
            <T variant="label" color="dim">
              {count ? pieces(count) : 'EMPTY'}
            </T>
          </View>
          <Pressable onPress={() => setBag(false)} hitSlop={12} accessibilityRole="button" accessibilityLabel="Close bag">
            <T variant="label" tracking={0.11}>
              CLOSE
            </T>
          </Pressable>
        </View>

        <ScrollView style={s.items} contentContainerStyle={{ paddingHorizontal: gutter }}>
          {lines.length === 0 ? (
            <T variant="label" size={13} color="dim" align="center" lineHeight={1.8} style={s.empty}>
              YOUR BAG IS EMPTY.
            </T>
          ) : (
            lines.map((line) => (
              <CartLine
                key={`${line.id}|${line.size}|${line.variant}`}
                line={line}
                onOpen={() => goProduct(line.id)}
                onQty={(delta) => changeQty(line.id, line.size, line.variant, delta)}
                onRemove={() => remove(line.id, line.size, line.variant)}
              />
            ))
          )}
        </ScrollView>

        <View style={[s.foot, { padding: gutter }]}>
          <Summary subtotal={subtotal} shipping="CALCULATED AT CHECKOUT" />
          {blocker ? (
            <T variant="label" color="dim" align="center" style={s.auth}>
              {blocker === 'ACCOUNT' ? 'PURCHASING REQUIRES AN ACCOUNT' : 'RESTRICTED PIECES REQUIRE AN ACTIVE RED CARD'}
            </T>
          ) : null}
          <Button label="CHECKOUT" variant="primary" block onPress={checkout} />
        </View>
      </Animated.View>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  root: { ...StyleSheet.absoluteFill, zIndex: 400, elevation: 400 },
  backdrop: { ...StyleSheet.absoluteFill, backgroundColor: c.overlay },
  panel: {
    position: 'absolute',
    top: 0,
    right: 0,
    bottom: 0,
    backgroundColor: c.bg,
    borderLeftWidth: 1,
    borderLeftColor: c.line,
  },
  head: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    gap: 16,
    paddingVertical: 22,
    borderBottomWidth: 1,
    borderBottomColor: c.lineSoft,
  },
  headTitle: { flexShrink: 1 },
  title: { marginBottom: 4 },
  items: { flex: 1 },
  empty: { paddingVertical: 48 },
  foot: { borderTopWidth: 1, borderTopColor: c.lineSoft },
  auth: { marginBottom: 14 },
}));
