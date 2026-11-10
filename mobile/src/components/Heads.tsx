import type { ReactNode } from 'react';
import { View } from 'react-native';

import { useLayout } from '@/theme/responsive';
import { makeStyles } from '@/theme/useTheme';

import { T } from './T';

/**
 * .page-head — centred, 88px above and 56px below: label kicker in --dim,
 * display title at clamp(36px, 5.5vw, 64px), --dim lede (max 46ch).
 * The title stays on one line and is fitted to the page width.
 */
export function PageHead({
  kicker,
  title,
  lede,
  children,
}: {
  kicker?: string;
  title: string;
  lede?: string;
  children?: ReactNode;
}) {
  const s = useStyles();
  const { content, vw } = useLayout();
  return (
    <View style={s.pageHead}>
      {kicker !== undefined ? (
        <T variant="label" color="dim" align="center" fit={content} style={s.kicker}>
          {kicker}
        </T>
      ) : null}
      <T variant="display" size={vw(36, 5.5, 64)} tracking={0} fit={content} align="center" accessibilityRole="header">
        {title}
      </T>
      {lede ? (
        <T color="dim" align="center" style={s.lede}>
          {lede}
        </T>
      ) : null}
      {children}
    </View>
  );
}

/** .block-title / .form-section — a --dim label; the block title sits over a --line-soft rule. */
export function BlockTitle({ label, ruled = true, first }: { label: string; ruled?: boolean; first?: boolean }) {
  const s = useStyles();
  return (
    <View style={[ruled ? s.blockTitle : s.formSection, first && s.first]}>
      <T variant="label" tracking={0.12} color="dim" accessibilityRole="header">
        {label}
      </T>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  pageHead: { paddingTop: 88, paddingBottom: 56, alignItems: 'center' },
  kicker: { marginBottom: 18 },
  // .lede { max-width: 46ch; margin-top: 22px }
  lede: { maxWidth: 340, marginTop: 22 },
  blockTitle: {
    paddingBottom: 14,
    marginBottom: 24,
    borderBottomWidth: 1,
    borderBottomColor: c.lineSoft,
  },
  formSection: { marginTop: 34, marginBottom: 16 },
  first: { marginTop: 0 },
}));
