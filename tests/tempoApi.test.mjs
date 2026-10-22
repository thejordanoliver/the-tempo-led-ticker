import assert from "node:assert/strict";
import test from "node:test";
import { fetchTeamSchedule, teamScheduleLeague, fetchNbaGames, fetchNflGames, fetchCfbGames } from "../src/api/tempoApi.mjs";

test("fetches NBA games without changing their original shape", async () => {
  let requested;
  const games = [
    { id: 1, home: { code: "ORL", score: 0 }, away: { code: "MIA", score: 0 } },
  ];
  const actual = await fetchNbaGames("http://localhost:4000", {
    date: "20261016",
    fetchImpl: async (url) => {
      requested = url;
      return { ok: true, json: async () => ({ games }) };
    },
  });
  assert.equal(
    requested,
    "http://localhost:4000/api/games/basketball/nba?date=20261016",
  );
  assert.deepEqual(actual, games);
});

test("rejects unexpected API responses", async () => {
  await assert.rejects(
    () =>
      fetchNbaGames("http://localhost:4000", {
        fetchImpl: async () => ({
          ok: true,
          json: async () => ({ results: [] }),
        }),
      }),
    /games array/,
  );
});

for (const [name, fetchImpl] of [
 ['network', async()=>{throw Error('offline')}],
 ['HTTP', async()=>({ok:false,status:503})],
 ['JSON', async()=>({ok:true,json:async()=>{throw Error('invalid JSON')}})]
]) test(name+' errors propagate',async()=>assert.rejects(()=>fetchNbaGames('http://localhost:4000',{fetchImpl})));
test('empty schedule accepted',async()=>assert.deepEqual(await fetchNbaGames('http://localhost:4000',{fetchImpl:async()=>({ok:true,json:async()=>({games:[]})})}),[]));
test('date validation',async()=>assert.rejects(()=>fetchNbaGames('http://localhost:4000',{date:'bad'}),/YYYYMMDD/));

for (const override of [undefined, '20261020']) {
  test(`preserves endpoint date with override ${override}`, async () => {
    let requested;
    await fetchNbaGames('http://localhost:4000/api/games/basketball/nba?date=20261016', {
      date: override,
      fetchImpl: async url => {
        requested = new URL(url);
        return { ok: true, json: async () => ({ games: [] }) };
      },
    });
    assert.equal(requested.searchParams.get('date'), override || '20261016');
  });
}


for (const [league, fetchGames] of [['nfl', fetchNflGames], ['cfb', fetchCfbGames]]) {
  test(`${league} preserves weekly endpoint params and allows overrides`, async () => {
    for (const options of [{}, {week: '8', season: '2025', seasontype: '3'}]) {
      let requested;
      await fetchGames(`http://localhost:4000/api/games/football/${league}?week=7&season=2026&seasontype=2&date=20261016`, {
        ...options,
        fetchImpl: async url => {requested = new URL(url); return {ok:true,json:async()=>({games:[]})};},
      });
      assert.equal(requested.pathname, `/api/games/football/${league}`);
      for (const [key, fallback] of Object.entries({week:'7',season:'2026',seasontype:'2'})) assert.equal(requested.searchParams.get(key),options[key] || fallback);
      assert.equal(requested.searchParams.has('date'), false);
    }
  });
  test(`${league} requires a week`, async () => {
    await assert.rejects(()=>fetchGames('http://localhost:4000'), /week/);
  });
}

for (const [sport, league] of [['basketball','nba'],['football','nfl'],['football','cfb']]) {
  test(`${league} accepts full team schedules and preserves filters`, async () => {
    for (const suffix of ['?season=2026&seasontype=2','/2025']) {
      const url = `http://localhost:4000/api/games/${sport}/team/${league}/13${suffix}`;
      assert.equal(teamScheduleLeague(url),league);
      let requested;
      const games = [{id:1},{id:2}];
      assert.deepEqual(await fetchTeamSchedule(url,{fetchImpl:async value=>{
        requested=value;return {ok:true,json:async()=>({games})};
      }}),games);
      assert.equal(requested,url);
    }
  });
}
