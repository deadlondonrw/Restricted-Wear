import { View } from 'react-native';

import { formatDate } from '@/lib/format';
import { redCardStatus, type User } from '@/store/account';
import { measureEm } from '@/theme/responsive';
import { makeStyles, useTheme } from '@/theme/useTheme';

import { Mark } from './Brand';
import { T } from './T';

/**
 * RW.redCardHtml — .rc-doc, the Red Card document (1.586 : 1).
 *   ACTIVE                 solid --redcard with white type, in both modes
 *   NOT ISSUED / REVOKED   transparent, 1px --line border, --dim type
 * A revoked card keeps its identifier and date.
 *
 * The 94-digit identifier cannot fit on one line at a readable size, so it is
 * set in equal rows (the website breaks it the same way with word-break).
 */
const RATIO = 1.586;
const WHITE = '#ffffff';
const DASH = '—';

export function RedCardDoc({ user, width, small }: { user: User | null; width: number; small?: boolean }) {
  const s = useStyles();
  const { c } = useTheme();
  const status = redCardStatus(user);
  const card = status !== 'NOT ISSUED' && user ? user.redCard : null;
  const active = status === 'ACTIVE';
  const ink = active ? WHITE : c.dim;

  const docWidth = Math.min(width, small ? 340 : 460);
  const pad = small ? 20 : 26;
  const inner = docWidth - pad * 2 - (active ? 0 : 2);
  const numberSize = small ? 11 : 12;
  const perRow = Math.max(8, Math.floor((inner * 0.97) / (measureEm('0', 'regular', 0.06) * numberSize)));
  const rows: string[] = [];
  if (card) for (let i = 0; i < card.id.length; i += perRow) rows.push(card.id.slice(i, i + perRow));

  return (
    <View
      style={[
        s.doc,
        active ? s.active : s.unissued,
        { width: docWidth, minHeight: Math.round(docWidth / RATIO), padding: pad, paddingBottom: small ? 20 : 22, gap: small ? 12 : 18 },
      ]}
      accessible
      accessibilityLabel={`Red Card, ${status}`}
    >
      <View style={s.top}>
        <T variant="label" size={15} tracking={0.14} weight="bold" ink={ink} maxFontSizeMultiplier={1.1}>
          RED CARD
        </T>
        <Mark size={26} color={active ? WHITE : c.fg} style={active ? undefined : s.markDim} />
      </View>

      <View style={s.fields}>
        <DocField label="HOLDER" value={card && user ? user.name : DASH} ink={ink} width={inner} />
        <View style={s.pair}>
          <DocField label="ISSUED" value={card ? formatDate(card.issued) : DASH} ink={ink} width={(inner - 18) / 2} />
          <DocField label="STATUS" value={status} ink={ink} width={(inner - 18) / 2} />
        </View>
      </View>

      <View>
        <T variant="label" size={11} tracking={0.12} ink={ink} maxFontSizeMultiplier={1.1} style={s.label}>
          IDENTIFIER
        </T>
        {rows.length ? (
          rows.map((row) => (
            <T key={row} size={numberSize} tracking={0.06} lineHeight={1.5} ink={ink} fit={inner} maxFontSizeMultiplier={1} style={s.num}>
              {row}
            </T>
          ))
        ) : (
          <T size={numberSize} lineHeight={1.5} ink={ink} fit>
            {DASH}
          </T>
        )}
      </View>
    </View>
  );
}

function DocField({ label, value, ink, width }: { label: string; value: string; ink: string; width: number }) {
  const s = useStyles();
  return (
    <View style={{ width }}>
      <T variant="label" size={11} tracking={0.12} ink={ink} maxFontSizeMultiplier={1.1} style={s.label}>
        {label}
      </T>
      <T variant="label" size={13} tracking={0.08} ink={ink} fit={width} maxFontSizeMultiplier={1.1}>
        {value}
      </T>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  doc: { justifyContent: 'space-between' },
  active: { backgroundColor: c.redcard },
  unissued: { borderWidth: 1, borderColor: c.line },
  top: { flexDirection: 'row', justifyContent: 'space-between', alignItems: 'flex-start', gap: 12 },
  markDim: { opacity: 0.35 },
  fields: { gap: 12 },
  pair: { flexDirection: 'row', gap: 18 },
  label: { opacity: 0.66, marginBottom: 3 },
  num: { fontVariant: ['tabular-nums'] },
}));
