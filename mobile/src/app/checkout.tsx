import { useEffect, useState } from 'react';
import { View } from 'react-native';

import { Button } from '@/components/Button';
import { CartLine } from '@/components/CartLine';
import { Checkbox } from '@/components/Checkbox';
import { Field } from '@/components/Field';
import { BlockTitle, PageHead } from '@/components/Heads';
import { Page } from '@/components/site/Page';
import { Summary } from '@/components/Summary';
import { T } from '@/components/T';
import { goSection, redirectForAuthorization } from '@/lib/nav';
import { formatCardNumber, formatExpiry, isEmail } from '@/lib/validation';
import { purchaseBlocker, useAccount, useUser, type User } from '@/store/account';
import { useCart, useCartSummary } from '@/store/cart';
import { toast, useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { breakpoints } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

/**
 * RW.renderCheckout — a visual prototype only. No payment is processed:
 * the payment fields live in this screen's state, are never sent anywhere
 * and are never stored. The authorization check runs here and again on
 * submit, so the flow cannot be skipped by opening the checkout directly.
 */
export default function CheckoutScreen() {
  const s = useStyles();
  const user = useUser();
  const cart = useCartSummary();
  const blocker = cart.lines.length ? purchaseBlocker(user, cart.lines) : null;

  useEffect(() => {
    if (blocker) useUI.getState().setNext('checkout');
  }, [blocker]);

  if (!cart.lines.length) {
    return (
      <Page>
        <PageHead kicker="CHECKOUT" title="YOUR BAG IS EMPTY">
          <Button label="RETURN TO SHOP" variant="primary" onPress={() => goSection('shop')} style={s.headAction} />
        </PageHead>
      </Page>
    );
  }

  if (blocker || !user) {
    return (
      <Page>
        <PageHead
          kicker="CHECKOUT"
          title="AUTHORIZATION REQUIRED"
          lede={
            blocker === 'REDCARD'
              ? 'Your bag contains restricted pieces. An active Red Card is required.'
              : 'Purchasing requires an account.'
          }
        >
          <View style={s.headActions}>
            {blocker === 'REDCARD' ? (
              <Button label="RED CARD" variant="primary" onPress={() => goSection('authorization')} />
            ) : (
              <Button
                label="SIGN IN"
                variant="primary"
                onPress={() => {
                  useUI.getState().setAccountTab('signin');
                  goSection('account');
                }}
              />
            )}
            <Button label="RETURN TO SHOP" variant="ghost" onPress={() => goSection('shop')} />
          </View>
        </PageHead>
      </Page>
    );
  }

  return (
    <Page bottomPadding={100}>
      <Form key={user.email} user={user} />
    </Page>
  );
}

const REQUIRED = ['email', 'name', 'country', 'city', 'address', 'postal', 'cardName', 'cardNumber', 'expiry', 'cvv'] as const;
type Key = (typeof REQUIRED)[number];

function Form({ user }: { user: User }) {
  const s = useStyles();
  const { width, content, gutterLg } = useLayout();
  const cart = useCartSummary();
  const single = width <= 860;
  const column = single ? content : Math.floor((content - gutterLg) / 2);
  const paired = width > breakpoints.grid1;

  const [f, setF] = useState<Record<Key, string>>({
    email: user.email ?? '',
    name: user.name ?? '',
    country: user.profile?.country ?? '',
    city: user.profile?.city ?? '',
    address: user.profile?.address ?? '',
    postal: user.profile?.postal ?? '',
    cardName: '',
    cardNumber: '',
    expiry: '',
    cvv: '',
  });
  const [save, setSave] = useState(true);
  const [missing, setMissing] = useState<Key[]>([]);
  const [busy, setBusy] = useState(false);
  const set = (key: Key, format?: (v: string) => string) => (value: string) =>
    setF((prev) => ({ ...prev, [key]: format ? format(value) : value }));
  const bad = (key: Key) => missing.includes(key);

  const placeOrder = async () => {
    if (busy) return;
    const lacking = REQUIRED.filter((k) => !f[k].trim() || (k === 'email' && !isEmail(f.email)));
    setMissing(lacking);
    if (lacking.length) return toast(lacking[0] === 'email' ? 'ENTER A VALID EMAIL' : 'COMPLETE ALL FIELDS');

    /* Re-check at the moment of purchase. */
    const lines = cart.lines;
    const blocker = purchaseBlocker(user, lines);
    if (blocker) return redirectForAuthorization(blocker, 'checkout');
    if (!lines.length) return goSection('shop');

    const profile = save
      ? { country: f.country.trim(), city: f.city.trim(), address: f.address.trim(), postal: f.postal.trim() }
      : null;

    /* Only product lines and totals are recorded — never payment data. */
    const items = lines.map((l) => ({
      id: l.id,
      name: l.product.name,
      size: l.size,
      qty: l.qty,
      price: l.product.price,
      colour: l.colour.label ?? '',
    }));

    setBusy(true);
    const res = await useAccount.getState().placeOrder(items, profile);
    if (res.error !== undefined) {
      setBusy(false);
      return toast(res.error);
    }
    useUI.getState().openModal({ kind: 'order-received', reference: res.order.ref });
    useCart.getState().clear();
  };

  return (
    <>
      <PageHead kicker="VISUAL PROTOTYPE — NO PAYMENT IS PROCESSED" title="CHECKOUT" />
      <View style={[s.grid, single ? s.gridSingle : { flexDirection: 'row', gap: gutterLg }]}>
        <View style={{ width: column }}>
          <BlockTitle label="CONTACT" ruled={false} first />
          <Field
            label="EMAIL"
            value={f.email}
            onChangeText={set('email')}
            error={bad('email')}
            keyboardType="email-address"
            autoCapitalize="none"
            autoCorrect={false}
            autoComplete="email"
          />

          <BlockTitle label="SHIPPING" ruled={false} />
          <Field label="FULL NAME" value={f.name} onChangeText={set('name')} error={bad('name')} autoComplete="name" autoCapitalize="words" />
          <View style={paired ? s.fieldRow : undefined}>
            <View style={paired ? s.fieldCell : undefined}>
              <Field label="COUNTRY" value={f.country} onChangeText={set('country')} error={bad('country')} autoComplete="country" />
            </View>
            <View style={paired ? s.fieldCell : undefined}>
              <Field label="CITY" value={f.city} onChangeText={set('city')} error={bad('city')} />
            </View>
          </View>
          <Field label="ADDRESS" value={f.address} onChangeText={set('address')} error={bad('address')} autoComplete="street-address" />
          <Field label="POSTAL CODE" value={f.postal} onChangeText={set('postal')} error={bad('postal')} autoComplete="postal-code" />
          <Checkbox label="Save shipping details to my account" checked={save} onChange={setSave} />

          <BlockTitle label="PAYMENT" ruled={false} />
          <Field
            label="CARDHOLDER NAME"
            value={f.cardName}
            onChangeText={set('cardName')}
            error={bad('cardName')}
            autoComplete="off"
            autoCorrect={false}
            autoCapitalize="characters"
          />
          <Field
            label="CARD NUMBER"
            value={f.cardNumber}
            onChangeText={set('cardNumber', formatCardNumber)}
            error={bad('cardNumber')}
            placeholder="0000 0000 0000 0000"
            keyboardType="number-pad"
            maxLength={19}
            autoComplete="off"
          />
          <View style={s.fieldRow}>
            <View style={s.fieldCell}>
              <Field
                label="EXPIRY DATE"
                value={f.expiry}
                onChangeText={set('expiry', formatExpiry)}
                error={bad('expiry')}
                placeholder="MM/YY"
                keyboardType="number-pad"
                maxLength={5}
                autoComplete="off"
              />
            </View>
            <View style={s.fieldCell}>
              <Field
                label="CVV"
                value={f.cvv}
                onChangeText={set('cvv', (v) => v.replace(/\D/g, '').slice(0, 4))}
                error={bad('cvv')}
                keyboardType="number-pad"
                maxLength={4}
                secureTextEntry
                autoComplete="off"
              />
            </View>
          </View>

          <T variant="label" color="dim" lineHeight={1.7} wrap style={s.proto}>
            VISUAL PROTOTYPE. PAYMENT DETAILS ARE NEVER TRANSMITTED, PROCESSED OR STORED. DO NOT ENTER REAL CARD DETAILS.
          </T>
          <Button block variant="primary" disabled={busy} label={busy ? 'PLACING ORDER...' : 'PLACE ORDER'} onPress={placeOrder} />
        </View>

        <View style={{ width: column }}>
          <BlockTitle label="ORDER SUMMARY" ruled={false} first />
          <View style={s.items}>
            {cart.lines.map((line) => (
              <CartLine key={`${line.id}-${line.size}-${line.variant}`} line={line} summary />
            ))}
          </View>
          <View style={s.foot}>
            <Summary subtotal={cart.subtotal} shipping="COMPLIMENTARY" />
          </View>
        </View>
      </View>
    </>
  );
}

const useStyles = makeStyles((c) => ({
  headAction: { marginTop: 34, alignSelf: 'center' },
  headActions: { marginTop: 34, flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'center', gap: 12 },
  grid: { marginTop: 10, alignItems: 'flex-start' },
  gridSingle: { flexDirection: 'column', gap: 48 },
  fieldRow: { flexDirection: 'row', gap: 14 },
  fieldCell: { flex: 1 },
  proto: { marginTop: 18, marginBottom: 22 },
  items: { borderTopWidth: 1, borderTopColor: c.lineSoft },
  foot: { paddingVertical: 24 },
}));
