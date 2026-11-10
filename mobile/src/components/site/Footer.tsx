import type { ReactNode } from 'react';
import { Linking, Pressable, View } from 'react-native';

import { BRAND, CATEGORIES } from '@/data/products';
import { goSection, goShop } from '@/lib/nav';
import { toast } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { breakpoints } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

import { Mark } from '../Brand';
import { T } from '../T';

const open = (url: string, fallback: string) => () => {
  Linking.openURL(url).catch(() => toast(fallback));
};

/**
 * .site-foot — the R mark, four link columns (one column up to 480px, two up
 * to 860px), contact details and the bottom line.
 */
export function Footer() {
  const s = useStyles();
  const { width, gutter, frame, content } = useLayout();
  const columns = width <= breakpoints.xs ? 1 : width <= breakpoints.foot2 ? 2 : 4;
  const colWidth = Math.floor((content - gutter * (columns - 1)) / columns);
  const col = { width: colWidth } as const;

  return (
    <View style={s.footer}>
      <View style={[s.wrap, { maxWidth: frame, paddingHorizontal: gutter }]}>
        <View style={s.mark}>
          <Mark size={30} />
        </View>

        <View style={[s.grid, { columnGap: gutter }]}>
          <Column title="SHOP" style={col}>
            <FootLink label="ALL PIECES" onPress={() => goShop()} />
            {CATEGORIES.map((c) => (
              <FootLink key={c.key} label={c.key} onPress={() => goShop(c.slug)} />
            ))}
          </Column>
          <Column title="COLLECTIONS" style={col}>
            <FootLink label="MAINSTREAM" onPress={() => goSection('mainstream')} />
            <FootLink label="SECONDSTREAM" onPress={() => goSection('secondstream')} />
            <FootLink label="ARCHIVE" onPress={() => goSection('archive')} />
          </Column>
          <Column title="CLIENT SERVICES" style={col}>
            <FootLink label="ACCOUNT" onPress={() => goSection('account')} />
            <FootLink label="RED CARD" onPress={() => goSection('authorization')} />
            <FootLink label="WORLDWIDE SHIPPING" />
            <FootLink label="PRICES IN EUR" />
          </Column>
          <Column title="CONTACT" style={col}>
            <ContactLabel label="BUSINESS / PROPOSALS" first />
            <FootLink
              label={BRAND.businessEmail}
              onPress={open(`mailto:${BRAND.businessEmail}`, `WRITE TO ${BRAND.businessEmail}`)}
            />
            <ContactLabel label="RED CARD" />
            <FootLink
              label={BRAND.redCardEmail}
              onPress={open(`mailto:${BRAND.redCardEmail}`, `WRITE TO ${BRAND.redCardEmail}`)}
            />
            <ContactLabel label="TELEGRAM" />
            <FootLink label={BRAND.telegram} onPress={open(BRAND.telegramUrl, `TELEGRAM ${BRAND.telegram}`)} />
          </Column>
        </View>

        <View style={s.bottom}>
          <T variant="label" size={12} color="dim2" align="center" fit={content}>
            © 2026 RESTRICTED WEAR. ALL RIGHTS RESERVED.
          </T>
          <T variant="label" size={12} color="dim2" align="center" fit={content}>
            CHECKOUT IS A PROTOTYPE. NO PAYMENTS ARE PROCESSED.
          </T>
        </View>
      </View>
    </View>
  );
}

function Column({ title, style, children }: { title: string; style: { width: number }; children: ReactNode }) {
  const s = useStyles();
  return (
    <View style={[s.col, style]}>
      <T variant="label" size={12} tracking={0.12} color="dim2" align="center" style={s.heading}>
        {title}
      </T>
      {children}
    </View>
  );
}

/** .foot-col a / span — 14px, --dim, --fg while pressed. Plain text when it leads nowhere. */
function FootLink({ label, onPress }: { label: string; onPress?: () => void }) {
  const s = useStyles();
  if (!onPress) {
    return (
      <T size={14} color="dim" align="center" fit style={s.link}>
        {label}
      </T>
    );
  }
  return (
    <Pressable onPress={onPress} accessibilityRole="link" hitSlop={4} style={s.linkHit}>
      {({ pressed }) => (
        <T size={14} color={pressed ? 'fg' : 'dim'} align="center" fit>
          {label}
        </T>
      )}
    </Pressable>
  );
}

function ContactLabel({ label, first }: { label: string; first?: boolean }) {
  const s = useStyles();
  return (
    <T variant="label" size={11.5} tracking={0.11} color="dim2" align="center" style={[s.cLabel, first && s.cLabelFirst]}>
      {label}
    </T>
  );
}

const useStyles = makeStyles((c) => ({
  footer: { borderTopWidth: 1, borderTopColor: c.lineSoft, paddingTop: 80, paddingBottom: 34, marginTop: 60 },
  wrap: { width: '100%', alignSelf: 'center' },
  mark: { alignItems: 'center', marginBottom: 56 },
  grid: { flexDirection: 'row', flexWrap: 'wrap', rowGap: 40, marginBottom: 64 },
  col: { alignItems: 'center' },
  heading: { marginBottom: 18 },
  link: { marginBottom: 9, maxWidth: '100%' },
  linkHit: { marginBottom: 9, maxWidth: '100%' },
  cLabel: { marginTop: 16, marginBottom: 4 },
  cLabelFirst: { marginTop: 0 },
  bottom: {
    paddingTop: 22,
    borderTopWidth: 1,
    borderTopColor: c.lineSoft,
    alignItems: 'center',
    gap: 10,
  },
}));
