import { Pressable, View } from 'react-native';

import { goSection } from '@/lib/nav';
import { useLayout } from '@/theme/responsive';

import { Wordmark } from '../Brand';

/**
 * .masthead — the full wordmark, isolated above the navigation.
 * padding: clamp(28px, 4.5vw, 64px) above, clamp(24px, 3.75vw, 52px) below;
 * logo width: clamp(220px, 34vw, 470px).
 */
export function Masthead() {
  const { vw } = useLayout();
  return (
    <View style={{ alignItems: 'center', paddingTop: vw(28, 4.5, 64), paddingBottom: vw(24, 3.75, 52) }}>
      <Pressable onPress={() => goSection('home')} accessibilityRole="link" accessibilityLabel="Restricted Wear — Home">
        <Wordmark width={vw(220, 34, 470)} />
      </Pressable>
    </View>
  );
}
