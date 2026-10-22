import test from 'node:test';
import assert from 'node:assert/strict';
import {rankedTeamLabel,normalizeGame,normalizeGames,nextIndex,showScore} from '../src/api/scoreboard.mjs';
const game=()=>({id:401898406,date:'2026-10-16T23:00Z',status:{state:'pre'},away:{code:'MIA',score:0,primaryColor:'#98002E',logo:'https://example.com/logo.png'},home:{code:'ORL',score:0}});
test('scheduled nested data preserves identity, colors, logos and timezone',()=>{const g=normalizeGame(game());assert.equal(g.status,'7:00 PM');assert.equal(g.id,401898406);assert.equal(g.date,game().date);assert.equal(g.away,'MIA');assert.equal(g.awayColor,'#98002E');assert.equal(g.awayLogoUrl,game().away.logo);assert.equal(normalizeGame(game(),'America/Los_Angeles').status,'4:00 PM');assert.equal(normalizeGame({...game(),date:'2026-12-16T23:00Z'}).status,'6:00 PM');});
for(const [status,expected,phase] of [[{state:'in',period:4,displayClock:'2:35'},'Q4 2:35','live'],[{state:'in',description:'Halftime'},'HALFTIME','halftime'],[{state:'post'},'FINAL','final'],[{state:'in',period:5,displayClock:'1:00'},'OT 1:00','live']])test(expected,()=>{const g=normalizeGame({...game(),status});assert.equal(g.status,expected);assert.equal(g.phase,phase);assert.equal(g.awayScore,0)});
test('missing, invalid and zero scores remain distinct',()=>{for(const score of [null,undefined,'',{},false,'NaN',-1])assert.equal(normalizeGame({...game(),away:{score}}).awayScore,null);assert.equal(normalizeGame(game()).awayScore,0);assert.equal(showScore(null),'-');});
test('fallbacks and long abbreviations',()=>{const g=normalizeGame({...game(),away:{code:'ABCDEFG',primaryColor:{}},home:{}});assert.equal(g.away,'ABCDE');assert.equal(g.home,'HME');assert.equal(g.awayColor,'#AABBDD')});
test('empty and malformed schedules',()=>{assert.deepEqual(normalizeGames([]),[]);for(const value of [null,{},[null],[{}]])assert.throws(()=>normalizeGames(value));assert.throws(()=>normalizeGames([],'invalid/timezone'));});
test('rotation wraps in both directions and handles empty schedules',()=>{assert.equal(nextIndex(2,3),0);assert.equal(nextIndex(0,3,-1),2);assert.equal(nextIndex(0,0),0);assert.equal(normalizeGames([game(),game()]).length,2)});

test('prefers official dark-background logos with standard fallback',()=>{
  const g=game();g.away.logoDark='https://example.com/dark.png';g.home.logoDark='';g.home.logo='https://example.com/home.png';
  const normalized=normalizeGame(g);
  assert.equal(normalized.awayLogoUrl,g.away.logoDark);
  assert.equal(normalized.homeLogoUrl,g.home.logo);
});

test('normalizes optional rankings and ignores unranked sentinels',()=>{
  for(const value of [null,undefined,0,99,100,-1,1.5,'',false,{},'NR']) {
    const g=normalizeGame({...game(),away:{rank:value},home:{rank:value}});
    assert.equal(g.awayRank,null);assert.equal(g.homeRank,null);
  }
  const g=normalizeGame({...game(),away:{code:'BAMA',rank:'12'},home:{code:'UGA',rank:1}});
  assert.equal(g.awayRank,12);assert.equal(g.homeRank,1);
  assert.equal(rankedTeamLabel(g.away,g.awayRank),'#12 BAMA');
  assert.equal(rankedTeamLabel('ORL',null),'ORL');
  assert.equal(normalizeGame({...game(),awayRank:25}).awayRank,25);
});
