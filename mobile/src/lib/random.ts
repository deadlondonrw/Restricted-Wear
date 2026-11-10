/* Randomness — crypto where the runtime provides it (as on the website). */

type CryptoLike = { getRandomValues?: <T extends ArrayBufferView>(array: T) => T };
const cryptoObj = (globalThis as { crypto?: CryptoLike }).crypto;

export function randomDigits(n: number): string {
  let out = '';
  while (out.length < n) {
    if (cryptoObj?.getRandomValues) {
      const buf = cryptoObj.getRandomValues(new Uint8Array(Math.max(8, n - out.length + 8)));
      for (let i = 0; i < buf.length && out.length < n; i += 1) {
        if (buf[i] < 250) out += String(buf[i] % 10); /* rejection sampling: uniform 0–9 */
      }
    } else {
      out += String(Math.floor(Math.random() * 10));
    }
  }
  return out;
}

export function randomUnit(): number {
  if (cryptoObj?.getRandomValues) {
    return cryptoObj.getRandomValues(new Uint32Array(1))[0] / 4294967296;
  }
  return Math.random();
}
