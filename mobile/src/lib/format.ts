/** RW.money — "€ " + amount with two decimals and en-US grouping: € 1,249.99 */
export function money(amount: number): string {
  const cents = Math.round(Math.abs(Number(amount) || 0) * 100);
  const whole = String(Math.floor(cents / 100)).replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  const fraction = String(cents % 100).padStart(2, '0');
  return `${amount < 0 ? '-' : ''}€ ${whole}.${fraction}`;
}

const MONTHS = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC'];

/** RW.formatDate — NOV 10, 2026 */
export function formatDate(iso?: string): string {
  const d = iso ? new Date(iso) : new Date();
  return `${MONTHS[d.getMonth()]} ${d.getDate()}, ${d.getFullYear()}`;
}

/** "1 PIECE" / "3 PIECES" — the website's catalog and bag count. */
export function pieces(n: number): string {
  return `${n} ${n === 1 ? 'PIECE' : 'PIECES'}`;
}
