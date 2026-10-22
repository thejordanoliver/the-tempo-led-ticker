/** Return the league for a supported full team schedule URL. */
export function teamScheduleLeague(value) {
  const path = new URL(value).pathname;
  const match = path.match(/^\/api\/games\/(basketball|football)\/team\/(nba|nfl|cfb)\/[^/]+(?:\/\d{4})?\/?$/);
  if (!match || (match[1] === 'basketball') !== (match[2] === 'nba')) return null;
  return match[2];
}

export async function fetchTeamSchedule(url, { fetchImpl = fetch, signal } = {}) {
  if (!teamScheduleLeague(url)) throw new Error('Unsupported team schedule URL');
  const response = await fetchImpl(new URL(url).toString(), { signal });
  if (!response.ok) throw new Error(`Tempo API returned HTTP ${response.status}`);
  const payload = await response.json();
  if (!Array.isArray(payload?.games)) throw new Error('Tempo API response has no games array');
  return payload.games;
}

/** Read-only client for the existing Tempo backend. No backend code is duplicated. */
export async function fetchNbaGames(
  baseUrl,
  { date, fetchImpl = fetch, signal } = {},
) {
  const source = new URL(baseUrl);
  const url = new URL("/api/games/basketball/nba", source);
  const selectedDate = date || source.searchParams.get("date");
  if (selectedDate) {
    if (!/^\d{8}$/.test(selectedDate)) throw new Error("date must be YYYYMMDD");
    url.searchParams.set("date", selectedDate);
  }
  const response = await fetchImpl(url.toString(), { signal });
  if (!response.ok)
    throw new Error(`Tempo API returned HTTP ${response.status}`);
  const payload = await response.json();
  if (!Array.isArray(payload?.games))
    throw new Error("Tempo API response has no games array");
  return payload.games;
}

async function fetchFootballGames(league, baseUrl, { week, season, seasontype, fetchImpl = fetch, signal } = {}) {
  const source = new URL(baseUrl);
  const url = new URL(`/api/games/football/${league}`, source);
  for (const [key, value] of Object.entries({ week, season, seasontype })) {
    const selected = value || source.searchParams.get(key);
    if (selected) {
      if (!/^\d+$/.test(String(selected)) || Number(selected) < 1 ||
          (key === 'season' && !/^\d{4}$/.test(String(selected))) ||
          (key === 'seasontype' && Number(selected) > 3)) {
        throw new Error(`Invalid ${key}`);
      }
      url.searchParams.set(key, selected);
    }
  }
  if (!url.searchParams.has('week')) throw new Error('Select a football week');
  const response = await fetchImpl(url.toString(), { signal });
  if (!response.ok) throw new Error(`Tempo API returned HTTP ${response.status}`);
  const payload = await response.json();
  if (!Array.isArray(payload?.games)) throw new Error('Tempo API response has no games array');
  return payload.games;
}
export function fetchNflGames(baseUrl, options) {
  return fetchFootballGames('nfl', baseUrl, options);
}
export function fetchCfbGames(baseUrl, options) {
  return fetchFootballGames('cfb', baseUrl, options);
}
