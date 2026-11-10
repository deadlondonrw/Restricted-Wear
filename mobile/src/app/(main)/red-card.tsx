import { useState } from 'react';
import { Linking, Pressable, View } from 'react-native';

import { Button } from '@/components/Button';
import { PageHead } from '@/components/Heads';
import { RedCardDoc } from '@/components/RedCardDoc';
import { Rows } from '@/components/Rows';
import { Page } from '@/components/site/Page';
import { T } from '@/components/T';
import { BRAND } from '@/data/products';
import { continueAfterAuth, goSection } from '@/lib/nav';
import { hasRedCard, redCardStatus, useAccount, useUser } from '@/store/account';
import { toast, useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { makeStyles } from '@/theme/useTheme';

/**
 * RW.renderAuthorization — what the Red Card is, the three status rows, the
 * action for the current state, and the document itself.
 */
export default function RedCardScreen() {
  const s = useStyles();
  const { content, gutterLg, stacked } = useLayout();
  const user = useUser();
  const next = useUI((u) => u.next);
  const openModal = useUI((u) => u.openModal);
  const [busy, setBusy] = useState<'reactivate' | 'revoke' | null>(null);

  const active = hasRedCard(user);
  const status = redCardStatus(user);
  const column = stacked ? content : Math.floor((content - gutterLg) / 2);

  const toAccount = (tab: 'signin' | 'create') => () => {
    const ui = useUI.getState();
    ui.setAccountTab(tab);
    if (!ui.next) ui.setNext('authorization');
    goSection('account');
  };

  const reactivate = async () => {
    if (!user || busy) return;
    setBusy('reactivate');
    const res = await useAccount.getState().issueRedCard(user.name);
    setBusy(null);
    if (res.error) return toast(res.error);
    toast('RED CARD REACTIVATED');
    if (useUI.getState().next) continueAfterAuth();
  };

  const revoke = async () => {
    if (!user || busy) return;
    setBusy('revoke');
    const res = await useAccount.getState().revokeRedCard();
    setBusy(null);
    if (res.error) return toast(res.error);
    toast('RED CARD REVOKED');
  };

  return (
    <Page active="authorization" bottomPadding={100}>
      <PageHead kicker="AUTHORIZATION" title="RED CARD" />
      <View style={[s.grid, stacked ? s.gridStacked : { flexDirection: 'row', gap: gutterLg }]}>
        <View style={{ width: column }}>
          <T color="dim" style={s.copy}>
            The Red Card is a digital authorization document connected to your account. It carries a unique 94-digit
            identifier. The identifier and the date of issue are permanent.
          </T>
          <T color="dim" style={s.copy}>
            Browsing is open to everyone. Purchasing requires an account. Pieces marked restricted require an active Red
            Card.
          </T>

          <View style={s.fields}>
            <Rows
              kind="auth"
              rows={[
                { k: 'ACCOUNT', v: user ? 'SIGNED IN' : 'NOT SIGNED IN', on: !user },
                { k: 'RED CARD', v: status, on: !active },
                { k: 'RESTRICTED PURCHASE', v: active ? 'PERMITTED' : 'NOT PERMITTED', on: !active },
              ]}
            />
          </View>

          <View style={s.actions}>
            {!user ? (
              <>
                <Button label="SIGN IN" variant="primary" onPress={toAccount('signin')} />
                <Button label="CREATE ACCOUNT" variant="ghost" onPress={toAccount('create')} />
              </>
            ) : status === 'REVOKED' ? (
              <Button
                label={busy === 'reactivate' ? 'REACTIVATING...' : 'REACTIVATE RED CARD'}
                variant="primary"
                disabled={!!busy}
                onPress={reactivate}
              />
            ) : !active ? (
              <Button label="APPLY FOR A RED CARD" variant="primary" onPress={() => openModal({ kind: 'redcard-application' })} />
            ) : (
              <>
                {next ? (
                  <Button label="CONTINUE" variant="primary" onPress={() => continueAfterAuth('shop')} />
                ) : (
                  <Button label="SHOP" variant="primary" onPress={() => goSection('shop')} />
                )}
                <Button label="REVOKE RED CARD" variant="ghost" disabled={!!busy} onPress={revoke} />
              </>
            )}
          </View>

          <View style={s.contact}>
            <T size={14} color="dim" fit>
              Red Card enquiries:{' '}
            </T>
            <Pressable
              onPress={() => Linking.openURL(`mailto:${BRAND.redCardEmail}`).catch(() => toast('NO MAIL APP AVAILABLE'))}
              accessibilityRole="link"
              accessibilityLabel={`Email ${BRAND.redCardEmail}`}
              hitSlop={10}
              style={s.mail}
            >
              <T size={14} fit>
                {BRAND.redCardEmail}
              </T>
            </Pressable>
          </View>
        </View>

        <View style={[s.docCol, { width: column }]}>
          <RedCardDoc user={user} width={column} />
        </View>
      </View>
    </Page>
  );
}

const useStyles = makeStyles((c) => ({
  grid: { marginTop: 10, alignItems: 'flex-start' },
  gridStacked: { flexDirection: 'column', gap: 48 },
  // .auth-copy p { max-width: 50ch }
  copy: { maxWidth: 370, marginBottom: 18 },
  fields: { marginTop: 12 },
  actions: { marginTop: 30, flexDirection: 'row', flexWrap: 'wrap', gap: 12 },
  contact: { marginTop: 28, flexDirection: 'row', alignItems: 'baseline', maxWidth: '100%' },
  mail: { borderBottomWidth: 1, borderBottomColor: c.line, flexShrink: 1 },
  docCol: { alignItems: 'center' },
}));
