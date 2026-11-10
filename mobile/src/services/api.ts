/* ==========================================================================
   Account & Red Card API — the website's Cloudflare Pages Functions
   (functions/api/[[path]].js). The app talks to the same server as the
   website, so an account and its Red Card are the same on both.

   The base URL comes from app.json → expo.extra.apiBase.
   No payment data is ever sent to this API.
   ========================================================================== */
import Constants from 'expo-constants';

const configured = (Constants.expoConfig?.extra as { apiBase?: string } | undefined)?.apiBase;

export const API_BASE = (configured || 'https://restrictedwear.pages.dev/api/').replace(/\/?$/, '/');

export type ApiResponse<T = Record<string, unknown>> = {
  ok: boolean;
  /** 0 = the request never reached the server. */
  status: number;
  data: T & { error?: string };
};

export async function api<T = Record<string, unknown>>(
  method: 'GET' | 'POST' | 'PUT' | 'DELETE',
  path: string,
  options: { body?: unknown; token?: string | null; timeoutMs?: number } = {},
): Promise<ApiResponse<T>> {
  const headers: Record<string, string> = { Accept: 'application/json' };
  if (options.token) headers.Authorization = `Bearer ${options.token}`;
  if (options.body !== undefined) headers['Content-Type'] = 'application/json';

  const controller = typeof AbortController === 'function' ? new AbortController() : null;
  const timer = controller ? setTimeout(() => controller.abort(), options.timeoutMs ?? 12000) : null;

  try {
    const res = await fetch(API_BASE + path, {
      method,
      headers,
      body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
      cache: 'no-store',
      signal: controller?.signal,
    });
    const data = (await res.json().catch(() => ({}))) as T & { error?: string };
    return { ok: res.ok, status: res.status, data: data ?? ({} as T) };
  } catch {
    return { ok: false, status: 0, data: { error: 'CONNECTION FAILED. TRY AGAIN.' } as T & { error?: string } };
  } finally {
    if (timer) clearTimeout(timer);
  }
}
