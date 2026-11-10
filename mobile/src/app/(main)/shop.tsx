import { Pressable, TextInput, View } from 'react-native';

import { Chip, ChipRow } from '@/components/Chip';
import { PageHead } from '@/components/Heads';
import { ProductGrid } from '@/components/product/ProductGrid';
import { Page } from '@/components/site/Page';
import { T } from '@/components/T';
import { TextLink } from '@/components/TextLink';
import {
  CATEGORIES,
  categoryByKey,
  filterProducts,
  FUTURE_CATEGORIES,
  genderList,
  isFutureCategory,
  PRODUCTS,
  SORT_LABELS,
  STREAMS,
} from '@/data/products';
import { pieces } from '@/lib/format';
import { useUI } from '@/store/ui';
import { useLayout } from '@/theme/responsive';
import { fonts } from '@/theme/tokens';
import { makeStyles, useTheme } from '@/theme/useTheme';

/**
 * RW.renderShop — the piece count and title, category chips (future
 * categories marked STILL DEVELOPING), sub-categories, then directions, sort
 * and search above the grid.
 */
export default function ShopScreen() {
  const s = useStyles();
  const { c, name } = useTheme();
  const { content } = useLayout();
  const f = useUI((u) => u.filters);
  const setFilters = useUI((u) => u.setFilters);
  const resetFilters = useUI((u) => u.resetFilters);
  const openModal = useUI((u) => u.openModal);

  const future = isFutureCategory(f.category);
  const released = f.category === 'ALL' || future || PRODUCTS.some((p) => p.category === f.category);
  const list = filterProducts(f);
  const subs = categoryByKey(f.category)?.subs ?? [];
  const genders = genderList();

  const kicker = future ? 'SHOP' : released ? pieces(list.length) : pieces(0);
  const title = f.category === 'ALL' ? 'SHOP' : f.category;
  const pick = (category: string) => () => setFilters({ category, sub: 'ALL' });

  return (
    <Page active="shop">
      <PageHead kicker={kicker} title={title} />

      {/* .filters */}
      <View style={s.filters}>
        <ChipRow>
          <Chip label="ALL" active={f.category === 'ALL'} onPress={pick('ALL')} />
          {CATEGORIES.map((cat) => (
            <Chip key={cat.key} label={cat.key} active={f.category === cat.key} onPress={pick(cat.key)} />
          ))}
          {FUTURE_CATEGORIES.map((cat) => (
            <Chip key={cat.key} label={cat.key} future active={f.category === cat.key} onPress={pick(cat.key)} />
          ))}
        </ChipRow>

        {subs.length ? (
          <ChipRow>
            <Chip label={`ALL ${f.category}`} size={12.5} active={f.sub === 'ALL'} onPress={() => setFilters({ sub: 'ALL' })} />
            {subs.map((sub) => (
              <Chip key={sub} label={sub} size={12.5} active={f.sub === sub} onPress={() => setFilters({ sub })} />
            ))}
          </ChipRow>
        ) : null}

        {/* .filters-meta */}
        <View style={[s.meta, { maxWidth: Math.min(860, content) }]}>
          <Chip label="ALL DIRECTIONS" size={12.5} active={f.stream === 'ALL'} onPress={() => setFilters({ stream: 'ALL' })} />
          {STREAMS.map((st) => (
            <Chip key={st} label={st} size={12.5} active={f.stream === st} onPress={() => setFilters({ stream: st })} />
          ))}
          {genders.length ? (
            <>
              <View style={s.sep} />
              <Chip label="EVERYONE" size={12.5} active={f.gender === 'ALL'} onPress={() => setFilters({ gender: 'ALL' })} />
              {genders.map((g) => (
                <Chip key={g} label={g} size={12.5} active={f.gender === g} onPress={() => setFilters({ gender: g })} />
              ))}
            </>
          ) : null}
          <View style={s.sep} />
          <Pressable
            onPress={() => openModal({ kind: 'sort' })}
            accessibilityRole="button"
            accessibilityLabel={`Sort: ${SORT_LABELS[f.sort]}`}
            style={({ pressed }) => [s.control, pressed && s.controlPressed]}
          >
            <T variant="label">{SORT_LABELS[f.sort]}</T>
          </Pressable>
          <TextInput
            value={f.query}
            onChangeText={(query) => setFilters({ query })}
            placeholder="SEARCH"
            placeholderTextColor={c.dim2}
            selectionColor={c.accent}
            cursorColor={c.fg}
            keyboardAppearance={name === 'inverted' ? 'dark' : 'light'}
            autoCapitalize="characters"
            autoCorrect={false}
            returnKeyType="search"
            maxFontSizeMultiplier={1.2}
            accessibilityLabel="Search pieces"
            style={[s.control, s.search]}
          />
        </View>
      </View>

      {/* .grid */}
      {future ? (
        <EmptyNote title={f.category} kicker="STILL DEVELOPING" body="No pieces have been released in this category." />
      ) : !released ? (
        <EmptyNote title={f.category} body="No pieces have been released in this category yet." />
      ) : list.length === 0 ? (
        <View style={s.empty}>
          <T variant="label" color="dim" align="center" fit={content}>
            NO PIECES MATCH THE CURRENT SELECTION.
          </T>
          <TextLink label="RESET" onPress={resetFilters} />
        </View>
      ) : (
        <ProductGrid products={list} />
      )}
    </Page>
  );
}

/** .empty-note */
function EmptyNote({ title, kicker, body }: { title: string; kicker?: string; body: string }) {
  const s = useStyles();
  const { content } = useLayout();
  return (
    <View style={s.empty}>
      <T variant="display" size={22} fit={content} align="center">
        {title}
      </T>
      {kicker ? (
        <T variant="label" color="dim" align="center">
          {kicker}
        </T>
      ) : null}
      <T color="dim" align="center" style={s.emptyBody}>
        {body}
      </T>
    </View>
  );
}

const useStyles = makeStyles((c) => ({
  filters: { alignItems: 'center', gap: 18, marginBottom: 64 },
  meta: {
    width: '100%',
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'center',
    alignItems: 'center',
    columnGap: 22,
    rowGap: 14,
    paddingTop: 18,
    borderTopWidth: 1,
    borderTopColor: c.lineSoft,
  },
  sep: { width: 1, height: 14, backgroundColor: c.line },
  // .select / .search-input — transparent with a 1px --line underline
  control: {
    borderBottomWidth: 1,
    borderBottomColor: c.line,
    paddingVertical: 6,
    paddingHorizontal: 2,
    maxWidth: '100%',
  },
  controlPressed: { borderBottomColor: c.fg },
  search: {
    width: 170,
    color: c.fg,
    fontFamily: fonts.medium,
    fontSize: 12.5,
    letterSpacing: 1.25,
    // .search-input:focus { outline: none } — the underline carries focus instead.
    outlineWidth: 0,
  },
  empty: { paddingVertical: 90, alignItems: 'center', gap: 12 },
  // max-width: 40ch
  emptyBody: { maxWidth: 300, marginTop: 6 },
}));
