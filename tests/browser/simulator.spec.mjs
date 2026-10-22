import { test, expect } from '@playwright/test';

// Illustrative fixtures only. No real API, external assets, or backend credentials.
const logo = 'https://assets.example.test/team.svg';
const fixture = (status = { state: 'pre' }) => ({
  id: 101, date: '2026-10-16T23:00Z', status,
  away: { code: 'MIA', score: 105, record: '3-1', primaryColor: '#98002E', logo },
  home: { code: 'ORL', score: 102, record: '2-1', primaryColor: '#2050AF', logo },
});
let payload, fail, calls, imageCalls, errors;
test.beforeEach(async ({ page }) => {
  payload = { games: [fixture()] }; fail = false; calls = []; imageCalls = 0; errors = [];
  page.on('pageerror', e => errors.push(e.message));
  await page.route('**/*', async route => {
    const url = new URL(route.request().url());
    if (url.pathname.startsWith('/api/games/')) {
      calls.push(url);
      if (fail) return route.abort('failed');
      return route.fulfill({ json: payload, headers: { 'Access-Control-Allow-Origin': '*' } });
    }
    if (url.href === logo) {
      imageCalls++;
      return route.fulfill({ contentType: 'image/svg+xml', headers: { 'Access-Control-Allow-Origin': '*' }, body: '<svg xmlns="http://www.w3.org/2000/svg" width="8" height="8"><path fill="#00ff00" d="M0 0h8v8H0z"/><path fill="#fff" d="M2 2h4v4H2z"/></svg>' });
    }
    if (url.origin === 'http://127.0.0.1:8093') return route.continue();
    return route.abort();
  });
});
test.afterEach(() => expect(errors).toEqual([]));
async function open(page) {
  await page.goto('/simulator/');
  await expect(page.locator('#status')).toContainText('API:');
  await page.locator('#pause').click();
  await page.locator('#brightness').fill('100');
  await page.locator('#ledEffect').uncheck();
  // Wait for decoded logo pixels rather than an arbitrary network delay.
  if (payload.games.some(g => g.away.logo === logo)) {
    await expect.poll(() => page.locator('canvas').evaluate(c => Array.from(c.getContext('2d').getImageData(2, 3, 1, 1).data).join(','))).toBe('0,255,0,255');
  }
}
async function pixels(page) {
  // Capture the actual 128×64 backing buffer, independent of CSS/browser scaling.
  return Buffer.from(await page.locator('canvas').evaluate(c => c.toDataURL().split(',')[1]), 'base64');
}
for (const [name, status] of [
  ['scheduled-records', { state: 'pre' }],
  ['live', { state: 'in', period: 4, displayClock: '2:35' }],
  ['halftime', { state: 'in', description: 'Halftime' }],
  ['final', { state: 'post', completed: true }],
  ['overtime', { state: 'in', period: 6, displayClock: '1:08' }],
]) test(name + ' pixel layout', async ({ page }) => {
  payload = { games: [fixture(status)] };
  await open(page);
  expect(await page.locator('canvas').evaluate(c => [c.width, c.height])).toEqual([128, 64]);
  expect(await pixels(page)).toMatchSnapshot(name + '.png', { maxDiffPixels: 0 });
  expect(imageCalls).toBe(1); // Both teams share the fixture asset; only one request.
});

test('missing logo, record and zero score fallbacks fit long codes', async ({ page }) => {
  const g = fixture(); g.away = { code: 'ABCDE', record: '100-100', logo: 'https://assets.example.test/missing.png' }; g.home = { code: 'LONGER' };
  payload = { games: [g] }; await open(page);
  expect(await pixels(page)).toMatchSnapshot('pregame-fallbacks.png', { maxDiffPixels: 0 });
  g.status = { state: 'in', period: 1, displayClock: '12:00' }; g.away.score = 0;
  payload = { games: [g] }; await page.locator('#fetch').click(); await expect(page.locator('#status')).toContainText('API:');
  // Status updates before the next rendered frame.
  await page.waitForTimeout(70);
  expect(await pixels(page)).toMatchSnapshot('zero-missing-scores.png', { maxDiffPixels: 0 });
});

test('manual navigation, pause and automatic rotation', async ({ page }) => {
  payload = { games: [fixture(), { ...fixture(), id: 102 }] }; await open(page);
  await page.locator('#prev').click(); await expect(page.locator('#gameCounter')).toHaveText('2 / 2');
  await page.locator('#next').click(); await expect(page.locator('#gameCounter')).toHaveText('1 / 2');
  await page.locator('#speed').fill('2');
  await page.waitForTimeout(2100); await expect(page.locator('#gameCounter')).toHaveText('1 / 2');
  await page.locator('#pause').click(); await expect(page.locator('#gameCounter')).toHaveText('2 / 2', { timeout: 3000 });
});

test('date, timezone and empty schedule', async ({ page }) => {
  await open(page); const eastern = await pixels(page);
  await page.locator('#timezone').fill('America/Los_Angeles'); await page.locator('#timezone').dispatchEvent('change'); await page.waitForTimeout(70);
  expect((await pixels(page)).equals(eastern)).toBe(false);
  await page.locator('#date').fill('20260104'); payload = { games: [] };
  await page.locator('#fetch').click(); await expect(page.locator('#status')).toContainText('No games scheduled');
  expect(calls.at(-1).searchParams.get('date')).toBe('20260104');
  await page.locator('#next').click(); await expect(page.locator('#gameCounter')).toHaveText('0 / 0');
  await page.waitForTimeout(70); expect(await pixels(page)).toMatchSnapshot('empty.png', { maxDiffPixels: 0 });
});

test('failed refresh retains data and automatically recovers', async ({ page }) => {
  await open(page); const original = await pixels(page);
  await page.locator('#refreshInterval').fill('5'); await page.locator('#refreshInterval').dispatchEvent('change');
  fail = true; await page.locator('#fetch').click(); await expect(page.locator('#status')).toContainText('(stale)');
  expect((await pixels(page)).equals(original)).toBe(true);
  fail = false; await expect(page.locator('#status')).toContainText('Updated', { timeout: 8000 });
  expect(calls.length).toBeGreaterThanOrEqual(3);
  payload = { unexpected: [] }; await page.locator('#fetch').click(); await expect(page.locator('#status')).toContainText('games array');
  await page.locator('#demo').click(); await expect(page.locator('#status')).toContainText('Demo (illustrative)');
});

test('mobile layout and logo preview', async ({ page }) => {
  payload.games[0].home = { ...payload.games[0].away };
  await open(page); await page.setViewportSize({ width: 375, height: 800 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.locator('#mode').selectOption('logo'); await page.waitForTimeout(70);
  // Logo preview switches teams over time; both use the identical fixture image.
  expect(await pixels(page)).toMatchSnapshot('logo-preview.png', { maxDiffPixels: 0 });
});

test('changing schedule date refreshes automatically', async ({ page }) => {
  await open(page);
  payload = { games: [{ ...fixture(), id: 202 }] };
  await page.locator('#date').fill('20261016');
  await page.locator('#date').dispatchEvent('change');
  await expect.poll(() => calls.at(-1).searchParams.get('date')).toBe('20261016');
  await expect(page.locator('#json')).toHaveValue(/"id": 202/);
});


for (const league of ['nfl', 'cfb']) {
  test(`${league} uses season and week controls and pasted weekly URLs`, async ({ page }) => {
    await open(page);
    await page.locator('#league').selectOption(league);
    await expect(page.locator('#date')).toBeHidden();
    await expect(page.locator('#week')).toBeVisible();
    await page.locator('#season').fill('2026');
    await page.locator('#week').fill('7');
    await page.locator('#week').dispatchEvent('change');
    await expect.poll(() => calls.at(-1).searchParams.get('week')).toBe('7');
    expect(calls.at(-1).pathname).toBe(`/api/games/football/${league}`);
    expect(calls.at(-1).searchParams.get('season')).toBe('2026');
    expect(calls.at(-1).searchParams.has('date')).toBe(false);
    await page.locator('#endpoint').fill(`http://localhost:4000/api/games/football/${league}?week=2&season=2025&seasontype=3`);
    await page.locator('#endpoint').dispatchEvent('change');
    await expect.poll(() => calls.at(-1).searchParams.get('week')).toBe('2');
    expect(calls.at(-1).searchParams.get('season')).toBe('2025');
    expect(calls.at(-1).searchParams.get('seasontype')).toBe('3');
    await expect(page.locator('#status')).toContainText('API: 1 games');
    await page.locator('#league').selectOption('nba');
    await expect(page.locator('#date')).toBeVisible();
    await expect(page.locator('#week')).toBeHidden();
  });
}

test('logo renderer trims transparent padding and preserves wide artwork', async ({ page }) => {
  await open(page);
  const result = await page.evaluate(async () => {
    const {prepareLogo} = await import('/src/api/logoRenderer.mjs');
    const image = new Image();
    image.src = 'data:image/svg+xml,' + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><path fill="#ff0000" d="M30 40h40v20H30z"/></svg>');
    await image.decode();
    const render = prepareLogo(image);
    const sprite = render(8);
    const data = sprite.getContext('2d').getImageData(0,0,8,8).data;
    return {cached: render(8) === sprite, opaque: Array.from({length:64},(_,i)=>data[i*4+3]).filter(a=>a>0).length,
      center:Array.from(data.slice((3*8+3)*4,(3*8+3)*4+4)),top:data[3]};
  });
  expect(result).toEqual({cached:true,opaque:32,center:[255,0,0,255],top:0});
});

for (const [sport, league] of [['basketball','nba'],['football','nfl'],['football','cfb']]) {
  test(`${league} team schedule URL rotates games and returns to scoreboard`, async ({page}) => {
    await open(page);
    payload={games:[fixture(),{...fixture(),id:202}]};
    const url=`http://localhost:4000/api/games/${sport}/team/${league}/13?season=2026`;
    await page.locator('#endpoint').fill(url);
    await page.locator('#endpoint').dispatchEvent('change');
    await expect.poll(()=>calls.at(-1).href).toBe(url);
    await expect(page.locator('#status')).toContainText('API: 2 games');
    await expect(page.locator('#league')).toHaveValue(league);
    await expect(page.locator('#date')).toBeHidden();
    await expect(page.locator('#week')).toBeHidden();
    await page.locator('#next').click();
    await expect(page.locator('#gameCounter')).toHaveText('2 / 2');
    await page.locator('#fetch').click();
    await expect(page.locator('#status')).toContainText('API: 2 games');
    expect(calls.at(-1).href).toBe(url);
    await page.locator('#league').selectOption(league==='nba'?'nfl':'nba');
    await expect.poll(()=>calls.at(-1).pathname.includes('/team/')).toBe(false);
  });
}

test('rankings fit long team codes beside full size logos', async ({page})=>{
  const g=fixture();g.away.code='ABCDE';g.away.rank=25;g.home.rank=1;
  payload={games:[g]};await open(page);
  await page.waitForTimeout(70);
  expect(await pixels(page)).toMatchSnapshot('ranked-teams.png',{maxDiffPixels:0});
});

test('rankings preserve the original team name pixels', async ({page})=>{
  await open(page);
  const teamPixels=()=>page.locator('canvas').evaluate(c=>Array.from(c.getContext('2d').getImageData(0,38,36,10).data));
  const before=await teamPixels();
  payload={games:[{...fixture(),away:{...fixture().away,rank:25},home:{...fixture().home,rank:1}}]};
  await page.locator('#fetch').click();await expect(page.locator('#status')).toContainText('API:');
  await expect.poll(teamPixels).toEqual(before);
  await page.waitForTimeout(70);
  expect(await pixels(page)).toMatchSnapshot('ranked-standard-teams.png',{maxDiffPixels:0});
});
