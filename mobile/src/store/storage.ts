import AsyncStorage from '@react-native-async-storage/async-storage';
import { createJSONStorage } from 'zustand/middleware';

/**
 * Device-local persistence for every store. AsyncStorage maps to
 * localStorage on web. Keys follow the website's (rw_cart_v2, rw_theme_v2…)
 * with an app suffix, because the stored shapes are the app's own.
 */
export const deviceStorage = createJSONStorage(() => AsyncStorage);

export const STORAGE_KEYS = {
  cart: 'rw_app_cart_v2',
  settings: 'rw_app_settings_v3',
  account: 'rw_app_account_v1',
} as const;
