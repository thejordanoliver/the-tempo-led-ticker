/** Read-only client for the existing Tempo backend. No backend code is duplicated. */
export async function fetchNbaGames(baseUrl, { date, fetchImpl = fetch } = {}) {
  const url = new URL('/api/games/basketball/nba', baseUrl);
  if (date) {
    if (!/^\d{8}$/.test(date)) throw new Error('date must be YYYYMMDD');
    url.searchParams.set('date', date);
  }
  const response = await fetchImpl(url.toString());
  if (!response.ok) throw new Error(`Tempo API returned HTTP ${response.status}`);
  const payload = await response.json();
  if (!Array.isArray(payload?.games)) throw new Error('Tempo API response has no games array');
  return payload.games;
}
