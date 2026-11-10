/* ==========================================================================
   Product sound — the website's RW.soundStart / RW.soundStop.

   A product may carry `audio: { source, volume, loop }`. The track plays
   while that product's page is open: it fades in over 1.4 s and fades out
   over 0.7 s when the page is left. SOUND ON / SOUND OFF on the product page
   switches it; the choice is remembered (settings store).
   ========================================================================== */
import { createAudioPlayer, setAudioModeAsync, type AudioPlayer } from 'expo-audio';
import { Platform } from 'react-native';

import type { Product } from '@/data/types';
import { useSettings } from '@/store/settings';

const FADE_IN_MS = 1400;
const FADE_OUT_MS = 700;
const STEP_MS = 50;

type Playing = { id: string; player: AudioPlayer; fade: ReturnType<typeof setInterval> | null };

let current: Playing | null = null;
let modeSet = false;
let armed: (() => void) | null = null;

const clampVolume = (v: number) => Math.max(0, Math.min(1, v));

function release(p: Playing) {
  if (p.fade) clearInterval(p.fade);
  p.fade = null;
  try {
    p.player.pause();
    p.player.remove();
  } catch {
    /* already released */
  }
}

function fade(p: Playing, target: number, ms: number, done?: () => void) {
  if (p.fade) clearInterval(p.fade);
  p.fade = null;
  let from = 0;
  try {
    from = p.player.volume;
  } catch {
    done?.();
    return;
  }
  const steps = Math.max(1, Math.round(ms / STEP_MS));
  let n = 0;
  p.fade = setInterval(() => {
    n += 1;
    try {
      p.player.volume = clampVolume(from + (target - from) * (n / steps));
    } catch {
      n = steps;
    }
    if (n >= steps) {
      if (p.fade) clearInterval(p.fade);
      p.fade = null;
      done?.();
    }
  }, STEP_MS);
}

/** Browsers block sound until the visitor has interacted with the page. */
function canPlayNow(): boolean {
  if (Platform.OS !== 'web') return true;
  const activation = (globalThis.navigator as { userActivation?: { hasBeenActive?: boolean } } | undefined)?.userActivation;
  return activation ? !!activation.hasBeenActive : true;
}

function disarm() {
  if (armed && typeof document !== 'undefined') document.removeEventListener('pointerdown', armed, true);
  armed = null;
}

function armForWeb(product: Product) {
  if (Platform.OS !== 'web' || typeof document === 'undefined') return;
  disarm();
  armed = () => {
    disarm();
    if (current?.id === product.id) {
      const p = current;
      try {
        p.player.play();
        fade(p, clampVolume(product.audio?.volume ?? 0.5), FADE_IN_MS);
      } catch {
        /* still blocked */
      }
    }
  };
  document.addEventListener('pointerdown', armed, true);
}

export const sound = {
  /** Stops any other product's track; plays this one if it has audio and sound is on. */
  start(product: Product) {
    const audio = product.audio;
    if (!audio || !useSettings.getState().sound) {
      sound.stop();
      return;
    }
    if (current?.id === product.id) {
      /* already loaded for this product (a quick return to the page): bring it back up */
      try {
        current.player.play();
      } catch {
        /* handled by the fade below */
      }
      fade(current, clampVolume(audio.volume), FADE_IN_MS);
      return;
    }
    if (current) release(current);
    current = null;

    if (!modeSet) {
      modeSet = true;
      setAudioModeAsync({ playsInSilentMode: true }).catch(() => {});
    }

    try {
      const player = createAudioPlayer(audio.source);
      player.loop = audio.loop;
      player.volume = 0;
      const playing: Playing = { id: product.id, player, fade: null };
      current = playing;
      if (canPlayNow()) {
        player.play();
        fade(playing, clampVolume(audio.volume), FADE_IN_MS);
      } else {
        armForWeb(product);
      }
    } catch {
      current = null;
    }
  },

  /** Fades the current track out and releases it. */
  stop() {
    disarm();
    const playing = current;
    if (!playing) return;
    current = null;
    fade(playing, 0, FADE_OUT_MS, () => release(playing));
  },
};
