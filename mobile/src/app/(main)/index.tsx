import { Pressable, View } from 'react-native';

import { ProductGrid } from '@/components/product/ProductGrid';
import { Placeholder } from '@/components/product/ProductMedia';
import { Page } from '@/components/site/Page';
import { T } from '@/components/T';
import { TextLink, TextLinkLabel } from '@/components/TextLink';
import { DIRECTIONS, featuredProducts, STREAMS } from '@/data/products';
import { goSection } from '@/lib/nav';
import { useLayout } from '@/theme/responsive';
import { breakpoints } from '@/theme/tokens';
import { makeStyles } from '@/theme/useTheme';

/**
 * RW.renderHome — editorial: the lead with CURRENT PIECES, the two
 * directions, SELECTED and the Red Card note. The website has no editorial
 * photography yet, so the lead and the directions show its placeholder.
 */
export default function HomeScreen() {
  const s = useStyles();
  const { width, content, gutter, vw } = useLayout();
  const single = width <= breakpoints.directions;
  const directionWidth = single ? content : Math.floor((content - gutter) / 2);

  return (
    <Page>
      <View style={{ paddingTop: vw(28, 4, 56) }}>
        {/* .home-lead */}
        <Pressable onPress={() => goSection('shop')} accessibilityRole="link" accessibilityLabel="Current pieces">
          <View style={{ aspectRatio: single ? 4 / 5 : 16 / 8, overflow: 'hidden' }}>
            <Placeholder markWidth={Math.round(content * 0.09)} />
          </View>
        </Pressable>
        <View style={s.leadCaption}>
          <T variant="label" color="dim" align="center">
            CURRENT PIECES
          </T>
          <TextLink label="VIEW ALL" onPress={() => goSection('shop')} />
        </View>

        {/* .home-directions */}
        <View style={[s.section, s.directions, single ? s.directionsStacked : { gap: gutter }]}>
          {STREAMS.map((stream) => {
            const d = DIRECTIONS[stream];
            return (
              <Pressable
                key={stream}
                onPress={() => goSection(stream === 'MAINSTREAM' ? 'mainstream' : 'secondstream')}
                accessibilityRole="link"
                accessibilityLabel={`${stream}. ${d.desc}`}
                style={{ width: directionWidth }}
              >
                {({ pressed }) => (
                  <>
                    <View style={s.directionMedia}>
                      <Placeholder markWidth={Math.min(88, Math.round(directionWidth * 0.16))} />
                    </View>
                    <View style={s.directionBody}>
                      <T variant="display" size={vw(22, 2.6, 30)} fit={directionWidth} align="center">
                        {stream}
                      </T>
                      <T color="dim" align="center" style={s.directionDesc}>
                        {d.desc}
                      </T>
                      <TextLinkLabel label="VIEW COLLECTION" pressed={pressed} />
                    </View>
                  </>
                )}
              </Pressable>
            );
          })}
        </View>

        {/* SELECTED */}
        <View style={s.selected}>
          <T variant="label" color="dim" align="center" style={s.sectionHead}>
            SELECTED
          </T>
          <ProductGrid products={featuredProducts()} />
        </View>

        {/* .home-note */}
        <View style={s.note}>
          <T variant="label" color="dim" align="center">
            RED CARD
          </T>
          <T color="dim" align="center" style={s.noteText}>
            Browsing is open. Purchasing requires an account. Selected pieces require an active Red Card.
          </T>
          <TextLink label="LEARN MORE" onPress={() => goSection('authorization')} />
        </View>
      </View>
    </Page>
  );
}

const useStyles = makeStyles(() => ({
  leadCaption: { paddingTop: 40, alignItems: 'center', gap: 22 },
  section: { paddingVertical: 96 },
  directions: { flexDirection: 'row' },
  directionsStacked: { flexDirection: 'column', gap: 64 },
  directionMedia: { aspectRatio: 4 / 5, overflow: 'hidden' },
  directionBody: { paddingTop: 30, alignItems: 'center', gap: 12 },
  // .direction-body p { max-width: 38ch }
  directionDesc: { maxWidth: 285 },
  selected: { paddingTop: 20, paddingBottom: 96 },
  sectionHead: { marginBottom: 48 },
  note: { paddingTop: 110, paddingBottom: 40, alignItems: 'center', gap: 22 },
  // .home-note p { max-width: 44ch }
  noteText: { maxWidth: 330 },
}));
