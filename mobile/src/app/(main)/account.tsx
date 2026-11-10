import { useState } from 'react';
import { Pressable, View } from 'react-native';

import { Button } from '@/components/Button';
import { Field } from '@/components/Field';
import { BlockTitle, PageHead } from '@/components/Heads';
import { RedCardDoc } from '@/components/RedCardDoc';
import { Page } from '@/components/site/Page';
import { T } from '@/components/T';
import { TextLink } from '@/components/TextLink';
import { formatDate, money } from '@/lib/format';
import { continueAfterAuth, goSection } from '@/lib/nav';
import { isEmail } from '@/lib/validation';
import { hasRedCard, redCardStatus, useAccount, useUser, type User } from '@/store/account';
import { useCartSummary } from '@/store/cart';
import { toast, useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { breakpoints } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

/** RW.renderAccount — the sign-in / create forms, or the account itself. */
export default function AccountScreen() {
  const user = useUser();
  return (
    <Page active="account" bottomPadding={100}>
      {user ? <Dashboard key={user.email} user={user} /> : <AuthForms />}
    </Page>
  );
}

/* --------------------------------------------------------------------------
   RW.renderAccountAuth
   -------------------------------------------------------------------------- */
function AuthForms() {
  const s = useStyles();
  const { content } = useLayout();
  const tab = useUI((u) => u.accountTab);
  const setTab = useUI((u) => u.setAccountTab);
  const next = useUI((u) => u.next);
  const mode = useAccount((a) => a.mode);
  const width = Math.min(440, content);

  const [name, setName] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [busy, setBusy] = useState(false);
  const [missing, setMissing] = useState<string[]>([]);

  const after = (message: string) => {
    toast(message);
    if (useUI.getState().next) continueAfterAuth();
  };

  const submit = async () => {
    if (busy) return;
    const lacking = [
      tab === 'create' && !name.trim() ? 'name' : null,
      !isEmail(email) ? 'email' : null,
      !password ? 'password' : null,
    ].filter((x): x is string => !!x);
    setMissing(lacking);
    if (lacking.length) return toast(lacking[0] === 'email' ? 'ENTER A VALID EMAIL' : 'COMPLETE ALL FIELDS');
    if (tab === 'create' && password.length < 8) {
      setMissing(['password']);
      return toast('PASSWORD MUST BE AT LEAST 8 CHARACTERS');
    }
    setBusy(true);
    const account = useAccount.getState();
    const res = tab === 'signin' ? await account.signIn(email, password) : await account.createAccount(name, email, password);
    if (res.error) {
      setBusy(false);
      return toast(res.error);
    }
    after(tab === 'signin' ? 'SIGNED IN' : 'ACCOUNT CREATED');
  };

  const notes =
    mode === 'server'
      ? ['YOUR ACCOUNT AND RED CARD ARE AVAILABLE ON ANY DEVICE.', 'DO NOT REUSE A PASSWORD FROM ANOTHER SERVICE.']
      : ['PROTOTYPE. ACCOUNTS ARE KEPT ON THIS DEVICE ONLY.', 'DO NOT USE A REAL PASSWORD.'];

  return (
    <>
      <PageHead
        kicker="ACCOUNT"
        title={tab === 'signin' ? 'SIGN IN' : 'CREATE ACCOUNT'}
        lede={next ? 'Sign in or create an account to continue.' : undefined}
      />
      <View style={[s.auth, { width }]}>
        <View style={s.tabs} accessibilityRole="tablist">
          {(['signin', 'create'] as const).map((key) => (
            <Pressable
              key={key}
              onPress={() => {
                setTab(key);
                setMissing([]);
              }}
              accessibilityRole="tab"
              accessibilityState={{ selected: tab === key }}
              hitSlop={8}
              style={[s.tab, tab === key && s.tabActive]}
            >
              <T variant="label" size={13} tracking={0.11} color={tab === key ? 'fg' : 'dim'}>
                {key === 'signin' ? 'SIGN IN' : 'CREATE ACCOUNT'}
              </T>
            </Pressable>
          ))}
        </View>

        {tab === 'create' ? (
          <Field
            label="FULL NAME"
            value={name}
            onChangeText={setName}
            error={missing.includes('name')}
            autoComplete="name"
            autoCapitalize="words"
            returnKeyType="next"
          />
        ) : null}
        <Field
          label="EMAIL"
          value={email}
          onChangeText={setEmail}
          error={missing.includes('email')}
          keyboardType="email-address"
          autoComplete="email"
          autoCapitalize="none"
          autoCorrect={false}
          returnKeyType="next"
        />
        <Field
          label="PASSWORD"
          value={password}
          onChangeText={setPassword}
          error={missing.includes('password')}
          secureTextEntry
          autoComplete={tab === 'signin' ? 'current-password' : 'new-password'}
          autoCapitalize="none"
          autoCorrect={false}
          returnKeyType="go"
          onSubmitEditing={submit}
        />
        <Button
          block
          variant="primary"
          disabled={busy}
          label={tab === 'signin' ? (busy ? 'SIGNING IN...' : 'SIGN IN') : busy ? 'CREATING...' : 'CREATE ACCOUNT'}
          onPress={submit}
        />

        <View style={s.note}>
          {notes.map((line) => (
            <T key={line} variant="label" color="dim" align="center" lineHeight={1.7} fit={width}>
              {line}
            </T>
          ))}
        </View>
      </View>
    </>
  );
}

/* --------------------------------------------------------------------------
   The signed-in account
   -------------------------------------------------------------------------- */
function Dashboard({ user }: { user: User }) {
  const s = useStyles();
  const { width, content, gutterLg, stacked } = useLayout();
  const cart = useCartSummary();
  const openModal = useUI((u) => u.openModal);
  const setBag = useUI((u) => u.setBag);
  const column = stacked ? content : Math.floor((content - gutterLg) / 2);
  const paired = width > breakpoints.grid1;

  const [form, setForm] = useState({
    name: user.name ?? '',
    phone: user.profile?.phone ?? '',
    country: user.profile?.country ?? '',
    city: user.profile?.city ?? '',
    address: user.profile?.address ?? '',
    postal: user.profile?.postal ?? '',
  });
  const [busy, setBusy] = useState<'save' | 'out' | null>(null);
  const set = (key: keyof typeof form) => (value: string) => setForm((f) => ({ ...f, [key]: value }));

  const save = async () => {
    if (busy) return;
    const name = form.name.trim();
    if (!name) return toast('NAME IS REQUIRED');
    setBusy('save');
    const res = await useAccount.getState().saveProfile(name, {
      phone: form.phone.trim(),
      country: form.country.trim(),
      city: form.city.trim(),
      address: form.address.trim(),
      postal: form.postal.trim(),
    });
    setBusy(null);
    toast(res.error ?? 'SAVED');
  };

  const signOut = async () => {
    if (busy) return;
    setBusy('out');
    await useAccount.getState().signOut();
    toast('SIGNED OUT');
  };

  const status = redCardStatus(user);
  const orders = [...(user.orders ?? [])].reverse();

  return (
    <>
      <PageHead kicker="ACCOUNT" title={user.name || 'ACCOUNT'} lede={user.email} />
      <View style={stacked ? s.gridStacked : { flexDirection: 'row', gap: gutterLg, alignItems: 'flex-start' }}>
        {/* .account-col */}
        <View style={{ width: column }}>
          <BlockTitle label="PERSONAL INFORMATION" />
          <Field label="FULL NAME" value={form.name} onChangeText={set('name')} autoComplete="name" autoCapitalize="words" />
          <Field label="EMAIL" value={user.email} readOnly />
          <Field label="PHONE" value={form.phone} onChangeText={set('phone')} keyboardType="phone-pad" autoComplete="tel" />
          <BlockTitle label="SAVED ADDRESS" ruled={false} />
          <View style={paired ? s.fieldRow : undefined}>
            <View style={paired ? s.fieldCell : undefined}>
              <Field label="COUNTRY" value={form.country} onChangeText={set('country')} autoComplete="country" />
            </View>
            <View style={paired ? s.fieldCell : undefined}>
              <Field label="CITY" value={form.city} onChangeText={set('city')} />
            </View>
          </View>
          <Field label="ADDRESS" value={form.address} onChangeText={set('address')} autoComplete="street-address" />
          <Field label="POSTAL CODE" value={form.postal} onChangeText={set('postal')} autoComplete="postal-code" />
          <Button block variant="primary" disabled={!!busy} label={busy === 'save' ? 'SAVING...' : 'SAVE'} onPress={save} />
        </View>

        {/* .account-side */}
        <View style={[s.side, { width: column }]}>
          <View>
            <BlockTitle label="RED CARD" />
            <RedCardDoc user={user} width={column} small />
            <View style={s.linkRow}>
              {hasRedCard(user) ? (
                <TextLink label="VIEW RED CARD" onPress={() => goSection('authorization')} style={s.left} />
              ) : status === 'REVOKED' ? (
                <TextLink label="REACTIVATE RED CARD" onPress={() => goSection('authorization')} style={s.left} />
              ) : (
                <TextLink label="APPLY FOR A RED CARD" onPress={() => openModal({ kind: 'redcard-application' })} style={s.left} />
              )}
            </View>
          </View>

          <View>
            <BlockTitle label="BAG" />
            <T size={14} color="dim" fit={column}>
              {cart.count
                ? `${cart.count} ${cart.count === 1 ? 'piece' : 'pieces'} · ${money(cart.subtotal)}`
                : 'Your bag is empty.'}
            </T>
            {cart.count ? (
              <View style={s.linkRow}>
                <TextLink label="OPEN BAG" onPress={() => setBag(true)} style={s.left} />
              </View>
            ) : null}
          </View>

          <View>
            <BlockTitle label="ORDER HISTORY" />
            {orders.length ? (
              orders.map((o) => (
                <View key={o.ref} style={s.order}>
                  <View style={s.orderLine}>
                    <T variant="label" style={s.grow}>
                      ORDER {o.ref}
                    </T>
                    <T size={15} fit style={s.num}>
                      {money(o.total)}
                    </T>
                  </View>
                  <T variant="label" color="dim">
                    {formatDate(o.date)}
                  </T>
                  {o.items.map((i, n) => (
                    <T key={`${i.id}-${i.size}-${n}`} size={14} color="dim" fit={column}>
                      {`${i.name}${i.colour ? ` · ${i.colour}` : ''} · ${i.size || '-'} × ${i.qty}`}
                    </T>
                  ))}
                </View>
              ))
            ) : (
              <T size={14} color="dim">
                No orders yet.
              </T>
            )}
          </View>

          <Button label={busy === 'out' ? 'SIGNING OUT...' : 'SIGN OUT'} variant="ghost" disabled={!!busy} onPress={signOut} />
        </View>
      </View>
    </>
  );
}

const useStyles = makeStyles((c) => ({
  auth: { alignSelf: 'center' },
  tabs: {
    flexDirection: 'row',
    justifyContent: 'center',
    gap: 30,
    marginBottom: 36,
    borderBottomWidth: 1,
    borderBottomColor: c.lineSoft,
  },
  tab: { paddingBottom: 12, marginBottom: -1, borderBottomWidth: 1, borderBottomColor: 'transparent', flexShrink: 1 },
  tabActive: { borderBottomColor: c.fg },
  note: { marginTop: 16 },
  gridStacked: { gap: 56 },
  fieldRow: { flexDirection: 'row', gap: 14 },
  fieldCell: { flex: 1 },
  side: { gap: 56 },
  linkRow: { marginTop: 20 },
  left: { alignSelf: 'flex-start' },
  order: { paddingVertical: 16, borderBottomWidth: 1, borderBottomColor: c.lineSoft, gap: 4 },
  orderLine: { flexDirection: 'row', alignItems: 'center', gap: 16 },
  grow: { flex: 1 },
  num: { fontVariant: ['tabular-nums'], flexShrink: 0 },
}));
