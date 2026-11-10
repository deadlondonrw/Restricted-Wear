import { Tabs } from 'expo-router/tabs';

import { useTheme } from '@/theme/useTheme';

/**
 * The website's top-level pages. They are sibling screens switched from the
 * navigation and the menu (there is no tab bar — the website has none):
 *   #/            index         #/authorization  red-card
 *   #/shop        shop          #/archive        archive
 *   #/mainstream  mainstream    #/account        account
 *   #/secondstream secondstream
 */
export default function MainLayout() {
  const { c } = useTheme();
  return (
    <Tabs
      backBehavior="history"
      tabBar={() => null}
      screenOptions={{ headerShown: false, animation: 'none', sceneStyle: { backgroundColor: c.bg } }}
    >
      <Tabs.Screen name="index" />
      <Tabs.Screen name="shop" />
      <Tabs.Screen name="mainstream" />
      <Tabs.Screen name="secondstream" />
      <Tabs.Screen name="red-card" />
      <Tabs.Screen name="archive" />
      <Tabs.Screen name="account" />
    </Tabs>
  );
}
