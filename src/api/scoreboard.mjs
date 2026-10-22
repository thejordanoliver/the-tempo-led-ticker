const scalar = (v) => (typeof v === "string" ? v.trim() : "");
const code = (v, fallback) =>
  scalar(v)
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, "")
    .slice(0, 5) || fallback;
const color = (v) =>
  /^#?[0-9a-f]{6}$/i.test(scalar(v)) ? "#" + v.replace("#", "") : "#AABBDD";
const score = (v) =>
  (typeof v === "number" || (typeof v === "string" && v.trim() !== "")) &&
  Number.isFinite(Number(v)) &&
  Number(v) >= 0 &&
  Number(v) <= 999 &&
  Number.isInteger(Number(v))
    ? Number(v)
    : null;
const record = (v) => (/^\d{1,3}-\d{1,3}$/.test(scalar(v)) ? scalar(v) : null);
const rank = value => {
  if (typeof value !== 'number' && typeof value !== 'string') return null;
  const number = Number(value);
  return Number.isInteger(number) && number > 0 && number < 99 ? number : null;
};
export function rankedTeamLabel(team, rank) {
  return rank == null ? team : `#${rank} ${team}`;
}
export function normalizeGame(g, timeZone = "America/New_York") {
  if (!g || typeof g !== "object" || Array.isArray(g) || !g.away || !g.home)
    throw Error("Invalid game: expected away and home teams");
  const s = g.status;
  const structured = s && typeof s === "object";
  const state = structured ? s.state : g.state;
  const detail = structured
    ? [s.description, s.shortDetail]
        .filter((v) => typeof v === "string")
        .join(" ")
    : scalar(s);
  const phase =
    (structured && (s.completed === true || state === "post")) ||
    /^FINAL/i.test(detail)
      ? "final"
      : /halftime|half time/i.test(detail)
        ? "halftime"
        : state === "in" || /^Q[1-4]|^OT/i.test(detail)
          ? "live"
          : "scheduled";
  const date = scalar(g.date);
  let status = "SCHEDULED";
  if (phase === "final") status = "FINAL";
  else if (phase === "halftime") status = "HALFTIME";
  else if (phase === "live") {
    const period = Number(s?.period);
    const quarter =
      Number.isInteger(period) && period > 0
        ? period > 4
          ? "OT" + (period > 5 ? period - 4 : "")
          : "Q" + period
        : "LIVE";
    const clock = scalar(s?.displayClock);
    status = structured
      ? quarter +
        (/^\d{1,2}:\d{2}$|^\d{1,2}\.\d$/.test(clock) ? " " + clock : "")
      : detail;
  } else if (date && Number.isFinite(Date.parse(date))) {
    status = new Intl.DateTimeFormat("en-US", {
      hour: "numeric",
      minute: "2-digit",
      timeZone,
    }).format(new Date(date));
  }
  if (/postponed|delayed|canceled|cancelled|suspended/i.test(detail))
    status = /postponed/i.test(detail)
      ? "POSTPONED"
      : /delay/i.test(detail)
        ? "DELAYED"
        : /suspend/i.test(detail)
          ? "SUSPENDED"
          : "CANCELED";
  return {
    id: typeof g.id === "number" || typeof g.id === "string" ? g.id : null,
    date: date || null,
    phase,
    away: code(g.away.code ?? g.away, "AWY"),
    home: code(g.home.code ?? g.home, "HME"),
    awayRank: rank(g.away.rank ?? g.awayRank),
    homeRank: rank(g.home.rank ?? g.homeRank),
    awayRecord: record(g.away.record ?? g.awayRecord),
    homeRecord: record(g.home.record ?? g.homeRecord),
    awayScore: score(g.away.score ?? g.awayScore),
    homeScore: score(g.home.score ?? g.homeScore),
    awayColor: color(g.away.primaryColor ?? g.awayColor),
    homeColor: color(g.home.primaryColor ?? g.homeColor),
    awayLogoUrl: (scalar(g.away.logoDark) || scalar(g.away.logo ?? g.awayLogoUrl)) || null,
    homeLogoUrl: (scalar(g.home.logoDark) || scalar(g.home.logo ?? g.homeLogoUrl)) || null,
    status,
  };
}
export function normalizeGames(games, timeZone) {
  if (!Array.isArray(games)) throw Error("Expected a games array");
  // Validate the timezone even for an empty schedule.
  new Intl.DateTimeFormat("en-US", { timeZone });
  return games.map((g) => normalizeGame(g, timeZone));
}
export function nextIndex(index, length, direction = 1) {
  return length ? (index + direction + length) % length : 0;
}
export function showScore(value) {
  return value === null || value === undefined ? "-" : String(value);
}

/** Lift dark colors for readability while preserving the API's original color. */
export function readableColor(hex) {
  const rgb = hex
    .slice(1)
    .match(/../g)
    .map((v) => parseInt(v, 16));
  return Math.max(...rgb) < 190
    ? "#" +
        rgb
          .map((v) =>
            Math.round(v + (255 - v) * 0.45)
              .toString(16)
              .padStart(2, "0"),
          )
          .join("")
    : hex;
}
