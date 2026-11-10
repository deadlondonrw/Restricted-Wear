import { create } from 'zustand';
import { persist } from 'zustand/middleware';

import type { ThemeName } from '@/theme/tokens';

import { deviceStorage, STORAGE_KEYS } from './storage';

type SettingsState = {
  /** Website default is white; "inverted" is its black mode (rw_theme_v2). */
  theme: ThemeName;
  /** Product sound (rw_sound_v1) — off when the visitor has muted it. */
  sound: boolean;
  toggleTheme: () => void;
  setSound: (on: boolean) => void;
};

export const useSettings = create<SettingsState>()(
  persist(
    (set) => ({
      theme: 'default',
      sound: true,
      toggleTheme: () => set((s) => ({ theme: s.theme === 'inverted' ? 'default' : 'inverted' })),
      setSound: (sound) => set({ sound }),
    }),
    {
      name: STORAGE_KEYS.settings,
      storage: deviceStorage,
      partialize: ({ theme, sound }) => ({ theme, sound }),
      merge: (persisted, current) => {
        const p = (persisted ?? {}) as Partial<SettingsState>;
        return {
          ...current,
          theme: p.theme === 'inverted' ? 'inverted' : 'default',
          sound: typeof p.sound === 'boolean' ? p.sound : true,
        };
      },
    },
  ),
);
