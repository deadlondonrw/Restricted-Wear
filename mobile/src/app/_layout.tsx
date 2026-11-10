import { DarkTheme, DefaultTheme, Stack, ThemeProvider } from 'expo-router';
import { useFonts } from 'expo-font';
import * as SplashScreen from 'expo-splash-screen';
import { StatusBar } from 'expo-status-bar';
import * as SystemUI from 'expo-system-ui';
import { useEffect, useMemo } from 'react';
import { AppState, BackHandler, View } from 'react-native';
import { SafeAreaProvider } from 'react-native-safe-area-context';

import { BagDrawer } from '@/components/overlays/BagDrawer';
import { IntroLoader } from '@/components/overlays/IntroLoader';
import { MenuOverlay } from '@/components/overlays/MenuOverlay';
import { ModalHost } from '@/components/overlays/ModalHost';
import { Qty67 } from '@/components/overlays/Qty67';
import { Toast } from '@/components/overlays/Toast';
import { ZoomViewer } from '@/components/overlays/ZoomViewer';
import { useAccount } from '@/store/account';
import { useUI } from '@/store/ui';
import { useTheme } from '@/theme/useTheme';

/*
 * The website sets everything in Roboto Condensed (400 / 500 / 700). Files
 * are required individually so only these weights ship in the app.
 */
const FONT_FILES = {
  RobotoCondensed_400Regular: require('@expo-google-fonts/roboto-condensed/400Regular/RobotoCondensed_400Regular.ttf'),
  RobotoCondensed_500Medium: require('@expo-google-fonts/roboto-condensed/500Medium/RobotoCondensed_500Medium.ttf'),
  RobotoCondensed_700Bold: require('@expo-google-fonts/roboto-condensed/700Bold/RobotoCondensed_700Bold.ttf'),
} as const;

// A deep link into a product or the checkout still gets the sections underneath it.
export const unstable_settings = { anchor: '(main)' };

SplashScreen.preventAutoHideAsync().catch(() => {});
SplashScreen.setOptions({ fade: true, duration: 300 });

export default function RootLayout() {
  const [fontsLoaded, fontError] = useFonts(FONT_FILES);
  const fontsReady = fontsLoaded || !!fontError;
  const { name, c } = useTheme();
  const accountReady = useAccount((a) => a.ready);

  useEffect(() => {
    if (fontsReady) SplashScreen.hideAsync().catch(() => {});
  }, [fontsReady]);

  useEffect(() => {
    SystemUI.setBackgroundColorAsync(c.bg).catch(() => {});
  }, [c.bg]);

  /* RW.backend.init — once the saved session has been read from the device. */
  useEffect(() => {
    const start = () => {
      useAccount.getState().init().catch(() => useAccount.setState({ mode: 'local', ready: true }));
    };
    if (useAccount.persist.hasHydrated()) {
      start();
      return;
    }
    return useAccount.persist.onFinishHydration(start);
  }, []);

  /* Back in the foreground: pick up changes made on the website or another device. */
  useEffect(() => {
    const sub = AppState.addEventListener('change', (state) => {
      if (state === 'active') useAccount.getState().refresh();
    });
    return () => sub.remove();
  }, []);

  /* Android back closes whatever floats above the page first. */
  useEffect(() => {
    const sub = BackHandler.addEventListener('hardwareBackPress', () => {
      const ui = useUI.getState();
      if (ui.qty67) return true;
      if (ui.zoom) ui.closeZoom();
      else if (ui.modal) ui.closeModal();
      else if (ui.bagOpen) ui.setBag(false);
      else if (ui.menuOpen) ui.setMenu(false);
      else return false;
      return true;
    });
    return () => sub.remove();
  }, []);

  const navTheme = useMemo(() => {
    const inverted = name === 'inverted';
    const base = inverted ? DarkTheme : DefaultTheme;
    return {
      ...base,
      dark: inverted,
      colors: { ...base.colors, primary: c.accent, background: c.bg, card: c.bg, text: c.fg, border: c.lineSoft, notification: c.accent },
    };
  }, [name, c]);

  if (!fontsReady) return null;

  return (
    <SafeAreaProvider>
      <ThemeProvider value={navTheme}>
        <View style={{ flex: 1, backgroundColor: c.bg }}>
          <Stack screenOptions={{ headerShown: false, contentStyle: { backgroundColor: c.bg }, animation: 'fade' }}>
            <Stack.Screen name="(main)" />
            <Stack.Screen name="product/[id]" />
            <Stack.Screen name="checkout" />
          </Stack>
          <MenuOverlay />
          <BagDrawer />
          <ModalHost />
          <ZoomViewer />
          <Qty67 />
          <Toast />
          <IntroLoader ready={accountReady} />
        </View>
        <StatusBar style={name === 'inverted' ? 'light' : 'dark'} />
      </ThemeProvider>
    </SafeAreaProvider>
  );
}
