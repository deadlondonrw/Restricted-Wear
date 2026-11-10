import { useEffect, useState } from 'react';
import { Animated, Easing, KeyboardAvoidingView, Platform, Pressable, ScrollView, StyleSheet, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';

import { SORT_LABELS, type ShopSort } from '@/data/products';
import { useAnimatedValue } from '@/hooks/useAnimatedValue';
import { continueAfterAuth, goSection, redirectForAuthorization } from '@/lib/nav';
import { formatBirthDate, isBirthDate } from '@/lib/validation';
import { useAccount, useUser } from '@/store/account';
import { toast, useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { breakpoints, motion } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

import { Button } from '../Button';
import { Field } from '../Field';
import { T } from '../T';

/**
 * #modal-overlay / .modal-box — a --bg box with a 1px --line border and 36px
 * padding (max 480px) over the dimmed page. Tapping outside closes it.
 */
export function ModalHost() {
  const s = useStyles();
  const insets = useSafeAreaInsets();
  const { gutter, width } = useLayout();
  const modal = useUI((u) => u.modal);
  const closeModal = useUI((u) => u.closeModal);
  const progress = useAnimatedValue(0);

  useEffect(() => {
    if (!modal) return;
    progress.setValue(0);
    Animated.timing(progress, { toValue: 1, duration: motion.pageIn, easing: Easing.ease, useNativeDriver: true }).start();
  }, [modal, progress]);

  if (!modal) return null;
  const pad = width <= breakpoints.xs ? 24 : 36;
  const inner = Math.min(480, width - gutter * 2) - pad * 2 - 2;

  return (
    <KeyboardAvoidingView style={s.root} behavior={Platform.OS === 'web' ? undefined : 'padding'}>
      <Pressable style={s.backdrop} onPress={closeModal} accessibilityLabel="Close" />
      <Animated.View
        style={[
          s.center,
          {
            padding: gutter,
            paddingTop: insets.top + gutter,
            paddingBottom: insets.bottom + gutter,
            opacity: progress,
            transform: [{ translateY: progress.interpolate({ inputRange: [0, 1], outputRange: [6, 0] }) }],
          },
        ]}
        pointerEvents="box-none"
      >
        <View style={s.box} accessibilityViewIsModal>
          <ScrollView bounces={false} keyboardShouldPersistTaps="handled" contentContainerStyle={{ padding: pad }}>
            {modal.kind === 'redcard-application' ? <RedCardApplication width={inner} /> : null}
            {modal.kind === 'order-received' ? <OrderReceived reference={modal.reference} width={inner} /> : null}
            {modal.kind === 'sort' ? <SortOptions width={inner} /> : null}
          </ScrollView>
        </View>
      </Animated.View>
    </KeyboardAvoidingView>
  );
}

function CloseRow() {
  const s = useStyles();
  const closeModal = useUI((u) => u.closeModal);
  return (
    <View style={s.closeRow}>
      <Pressable onPress={closeModal} hitSlop={12} accessibilityRole="button" accessibilityLabel="Close">
        <T variant="label" tracking={0.11}>
          CLOSE
        </T>
      </Pressable>
    </View>
  );
}

function Title({ children, width }: { children: string; width: number }) {
  return (
    <T variant="display" size={22} tracking={0.02} lineHeight={1.3} fit={width} align="center" accessibilityRole="header">
      {children}
    </T>
  );
}

/**
 * RW.openAuthModal — RED CARD APPLICATION.
 * Date of birth and note are not stored. Country is saved to the profile.
 */
function RedCardApplication({ width }: { width: number }) {
  const s = useStyles();
  const user = useUser();
  const issueRedCard = useAccount((a) => a.issueRedCard);
  const closeModal = useUI((u) => u.closeModal);
  const [name, setName] = useState(user?.name ?? '');
  const [dob, setDob] = useState('');
  const [country, setCountry] = useState(user?.profile?.country ?? '');
  const [note, setNote] = useState('');
  const [busy, setBusy] = useState(false);
  const [checked, setChecked] = useState(false);

  useEffect(() => {
    if (!user) {
      closeModal();
      redirectForAuthorization('ACCOUNT', 'authorization');
    }
  }, [user, closeModal]);
  if (!user) return null;

  const invalid = { name: !name.trim(), dob: !isBirthDate(dob), country: !country.trim() };

  const submit = async () => {
    setChecked(true);
    if (invalid.name || invalid.country) {
      toast('FILL IN THE REQUIRED FIELDS');
      return;
    }
    if (invalid.dob) {
      toast('DATE OF BIRTH: YYYY-MM-DD');
      return;
    }
    setBusy(true);
    const [res] = await Promise.all([
      issueRedCard(name.trim(), country.trim()),
      new Promise((resolve) => setTimeout(resolve, 1300)),
    ]);
    if (res.error) {
      setBusy(false);
      toast(res.error);
      return;
    }
    closeModal();
    toast('RED CARD ISSUED');
    if (useUI.getState().next) continueAfterAuth();
  };

  return (
    <View>
      <CloseRow />
      <Title width={width}>RED CARD APPLICATION</Title>
      <T variant="label" color="dim" align="center" lineHeight={1.8} fit={width} style={s.sub}>
        THE RED CARD IS ISSUED TO YOUR ACCOUNT.
      </T>
      <Field label="FULL NAME" value={name} onChangeText={setName} error={checked && invalid.name} autoCapitalize="words" autoComplete="name" />
      <Field label="EMAIL" value={user.email} readOnly />
      <Field
        label="DATE OF BIRTH"
        value={dob}
        onChangeText={(v) => setDob(formatBirthDate(v))}
        error={checked && invalid.dob}
        placeholder="YYYY-MM-DD"
        keyboardType="number-pad"
        maxLength={10}
      />
      <Field label="COUNTRY" value={country} onChangeText={setCountry} error={checked && invalid.country} autoCapitalize="words" autoComplete="country" />
      <Field label="NOTE (OPTIONAL)" value={note} onChangeText={setNote} multiline />
      <Button label={busy ? 'REVIEWING...' : 'SUBMIT'} variant="primary" block disabled={busy} onPress={submit} />
      <T variant="label" color="dim" align="center" lineHeight={1.7} fit={width} style={s.note}>
        PROTOTYPE. DATE OF BIRTH AND NOTE ARE NOT STORED.
      </T>
    </View>
  );
}

/** The confirmation shown after PLACE ORDER. */
function OrderReceived({ reference, width }: { reference: string; width: number }) {
  const s = useStyles();
  return (
    <View>
      <Title width={width}>ORDER RECEIVED</Title>
      <T variant="label" color="dim" align="center" lineHeight={1.8} fit={width} style={s.reference}>
        REFERENCE {reference}
      </T>
      <T variant="label" color="dim" align="center" lineHeight={1.8} fit={width} style={s.proto}>
        VISUAL PROTOTYPE — NO PAYMENT WAS PROCESSED.
      </T>
      <View style={s.actions}>
        <Button label="RETURN TO SHOP" variant="primary" onPress={() => goSection('shop')} style={s.action} />
        <Button label="VIEW ORDERS" variant="ghost" onPress={() => goSection('account')} style={s.action} />
      </View>
    </View>
  );
}

/** The website's SORT <select>, as a list. */
function SortOptions({ width }: { width: number }) {
  const s = useStyles();
  const sort = useUI((u) => u.filters.sort);
  const setFilters = useUI((u) => u.setFilters);
  const closeModal = useUI((u) => u.closeModal);
  const options = Object.keys(SORT_LABELS) as ShopSort[];
  return (
    <View>
      <CloseRow />
      <Title width={width}>SORT</Title>
      <View style={s.options}>
        {options.map((o) => (
          <Pressable
            key={o}
            onPress={() => {
              setFilters({ sort: o });
              closeModal();
            }}
            accessibilityRole="button"
            accessibilityState={{ selected: sort === o }}
            style={s.option}
          >
            {({ pressed }) => (
              <View style={[s.optionRule, sort === o && s.optionRuleOn]}>
                <T variant="label" tracking={0.1} color={pressed || sort === o ? 'fg' : 'dim'} fit={width} align="center">
                  {o === 'DEFAULT' ? 'DEFAULT ORDER' : SORT_LABELS[o]}
                </T>
              </View>
            )}
          </Pressable>
        ))}
      </View>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  root: { ...StyleSheet.absoluteFill, zIndex: 500, elevation: 500 },
  backdrop: { ...StyleSheet.absoluteFill, backgroundColor: c.overlay },
  center: { flex: 1, alignItems: 'center', justifyContent: 'center' },
  box: { width: '100%', maxWidth: 480, maxHeight: '100%', backgroundColor: c.bg, borderWidth: 1, borderColor: c.line },
  closeRow: { flexDirection: 'row', justifyContent: 'flex-end', marginBottom: 10 },
  sub: { marginTop: 12, marginBottom: 26 },
  note: { marginTop: 16 },
  reference: { marginTop: 12 },
  proto: { marginTop: 8 },
  actions: { flexDirection: 'row', flexWrap: 'wrap', justifyContent: 'center', gap: 10, marginTop: 26 },
  action: { alignSelf: 'center' },
  options: { marginTop: 22, gap: 16, alignItems: 'center' },
  option: { maxWidth: '100%' },
  optionRule: { paddingVertical: 4, borderBottomWidth: 1, borderBottomColor: 'transparent' },
  optionRuleOn: { borderBottomColor: c.fg },
}));
