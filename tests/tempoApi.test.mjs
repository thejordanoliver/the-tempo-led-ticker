import test from 'node:test';
import assert from 'node:assert/strict';
import { fetchNbaGames } from '../src/api/tempoApi.mjs';

test('fetches NBA games without changing their original shape', async () => {
  let requested;
  const games = [{ id: 1, home: { code: 'ORL', score: 0 }, away: { code: 'MIA', score: 0 } }];
  const actual = await fetchNbaGames('http://localhost:4000', {
    date: '20261016',
    fetchImpl: async (url) => { requested = url; return { ok: true, json: async () => ({ games }) }; }
  });
  assert.equal(requested, 'http://localhost:4000/api/games/basketball/nba?date=20261016');
  assert.deepEqual(actual, games);
});

test('rejects unexpected API responses', async () => {
  await assert.rejects(() => fetchNbaGames('http://localhost:4000', {
    fetchImpl: async () => ({ ok: true, json: async () => ({ results: [] }) })
  }), /games array/);
});
