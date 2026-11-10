import { View } from 'react-native';

import { categoriesIn, DIRECTIONS, productsInStream } from '@/data/products';
import type { Stream } from '@/data/types';
import { goSection } from '@/lib/nav';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { makeStyles } from '@/theme/useTheme';

import { Chip, ChipRow } from './Chip';
import { PageHead } from './Heads';
import { ProductGrid } from './product/ProductGrid';
import { Page } from './site/Page';
import { T } from './T';
import { TextLink } from './TextLink';

/**
 * RW.renderCollection — MAINSTREAM / SECONDSTREAM: kicker COLLECTION, the
 * direction's description, its categories, the grid (without the direction
 * marker) and a link to the other direction.
 */
export function CollectionPage({ stream }: { stream: Stream }) {
  const s = useStyles();
  const { content } = useLayout();
  const direction = DIRECTIONS[stream];
  const other = stream === 'MAINSTREAM' ? 'SECONDSTREAM' : 'MAINSTREAM';
  const stored = useUI((u) => u.collectionCat[stream]);
  const setCollectionCat = useUI((u) => u.setCollectionCat);

  const all = productsInStream(stream);
  const cats = categoriesIn(all);
  const active = stored !== 'ALL' && cats.some((c) => c.key === stored) ? stored : 'ALL';
  const list = active === 'ALL' ? all : all.filter((p) => p.category === active);

  return (
    <Page active={stream === 'MAINSTREAM' ? 'mainstream' : 'secondstream'}>
      <PageHead kicker="COLLECTION" title={stream} lede={direction.desc} />

      <ChipRow style={s.filters}>
        <Chip label="ALL" active={active === 'ALL'} onPress={() => setCollectionCat(stream, 'ALL')} />
        {cats.map((c) => (
          <Chip key={c.key} label={c.key} active={active === c.key} onPress={() => setCollectionCat(stream, c.key)} />
        ))}
      </ChipRow>

      {list.length ? (
        <ProductGrid products={list} hideMarker />
      ) : (
        <T variant="label" color="dim" align="center" fit={content} style={s.empty}>
          NO PIECES IN THIS COLLECTION YET.
        </T>
      )}

      <View style={s.other}>
        <TextLink label={other} onPress={() => goSection(other === 'MAINSTREAM' ? 'mainstream' : 'secondstream')} />
      </View>
    </Page>
  );
}

const useStyles = makeStyles(() => ({
  filters: { marginBottom: 64 },
  empty: { paddingVertical: 90 },
  other: { paddingTop: 110, alignItems: 'center' },
}));
