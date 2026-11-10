import { View } from 'react-native';

import { PageHead } from '@/components/Heads';
import { Page } from '@/components/site/Page';
import { T } from '@/components/T';
import { ARCHIVE } from '@/data/products';
import { useLayout } from '@/theme/responsive';
import { makeStyles } from '@/theme/useTheme';

/** RW.renderArchive — kicker NOTES; each entry: label, 19px bold title, --dim text. */
export default function ArchiveScreen() {
  const s = useStyles();
  const { width, content } = useLayout();
  const wide = width > 600;
  const listWidth = Math.min(820, content);
  const textWidth = wide ? listWidth - 180 : listWidth;

  return (
    <Page active="archive" bottomPadding={100}>
      <PageHead kicker="NOTES" title="ARCHIVE" />
      <View style={[s.list, { width: listWidth }]}>
        {ARCHIVE.map((entry, i) => (
          <View key={entry.title} style={[s.entry, wide && s.entryWide, i === ARCHIVE.length - 1 && s.last]}>
            <T variant="label" color="dim" style={wide ? s.labelWide : undefined}>
              {entry.label}
            </T>
            <View style={s.text}>
              <T variant="heading" fit={textWidth} style={s.title}>
                {entry.title}
              </T>
              <T color="dim">{entry.body}</T>
            </View>
          </View>
        ))}
      </View>
    </Page>
  );
}

const useStyles = makeStyles((c) => ({
  list: { alignSelf: 'center' },
  entry: { paddingVertical: 30, gap: 8, borderTopWidth: 1, borderTopColor: c.lineSoft },
  entryWide: { flexDirection: 'row', gap: 30 },
  last: { borderBottomWidth: 1, borderBottomColor: c.lineSoft },
  labelWide: { width: 150, paddingTop: 4 },
  text: { flex: 1 },
  title: { marginBottom: 8 },
}));
