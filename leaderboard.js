/* leaderboard.js - shared top-100 leaderboard on Supabase (web/leaderboard.sql).
 *
 * Config: leaderboard-config.js (project URL + anon key; empty = off).
 * Scores are queued in localStorage and sent until the server accepts them,
 * so a run that ends offline is submitted next time the page loads. The
 * server ignores a run_id it already has, so retries can't duplicate.
 */
'use strict';

const Leaderboard = (() => {
  const cfg = window.ULARN_LEADERBOARD || {};
  const enabled = !!(cfg.url && cfg.key);
  const base = enabled ? cfg.url.replace(/\/+$/, '') + '/rest/v1' : '';
  const headers = { apikey: cfg.key, 'Content-Type': 'application/json' };
  // Legacy anon keys are JWTs and also go in Authorization; the newer
  // publishable keys (sb_publishable_...) must not.
  if (enabled && /^eyJ/.test(cfg.key)) headers.Authorization = 'Bearer ' + cfg.key;

  const ls = {
    get(k, d) { try { const v = localStorage.getItem('ularn.lb.' + k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem('ularn.lb.' + k, JSON.stringify(v)); } catch (e) { /* private mode */ } },
  };

  async function top() {
    const q = 'select=run_id,name,score,outcome,reason,char_class,char_level,dungeon,turns,wild,created_at'
      + '&order=score.desc,created_at.asc&limit=100';
    const r = await fetch(base + '/ularn_scores?' + q, { headers });
    if (!r.ok) throw new Error('leaderboard ' + r.status);
    return r.json();
  }

  async function send(e) {
    const r = await fetch(base + '/rpc/submit_score', {
      method: 'POST', headers,
      body: JSON.stringify({
        p_run_id: e.run_id, p_name: e.name, p_score: e.score, p_outcome: e.outcome, p_reason: e.reason,
        p_class: e.char_class, p_level: e.char_level, p_dungeon: e.dungeon, p_turns: e.turns,
        p_kills: e.kills, p_difficulty: e.difficulty, p_wild: e.wild,
      }),
    });
    if (!r.ok) throw new Error('submit ' + r.status + ' ' + (await r.text()).slice(0, 200));
  }

  /** Queue a finished run and try to send everything queued. */
  async function submit(entry) {
    if (!enabled || !entry.run_id) return false;
    const mine = ls.get('mine', []);
    if (!mine.includes(entry.run_id)) { mine.push(entry.run_id); ls.set('mine', mine.slice(-200)); }
    const q = ls.get('pending', []).filter((p) => p.run_id !== entry.run_id);
    q.push(entry);
    ls.set('pending', q);
    return flush();
  }

  /** Send queued runs; true when the queue is empty afterwards. */
  async function flush() {
    if (!enabled) return true;
    let q = ls.get('pending', []);
    for (const e of q.slice()) {
      try {
        await send(e);
        q = q.filter((p) => p.run_id !== e.run_id);
        ls.set('pending', q);
      } catch (err) {
        console.warn(err);
        // 4xx other than rate limits: the server rejected this entry; drop it.
        const m = /submit (\d+)/.exec(err.message);
        if (m && +m[1] >= 400 && +m[1] < 500 && +m[1] !== 429) { q = q.filter((p) => p.run_id !== e.run_id); ls.set('pending', q); }
        else break;
      }
    }
    return q.length === 0;
  }

  function cell(tr, text, cls) {
    const td = document.createElement('td');
    td.textContent = text;
    if (cls) td.className = cls;
    tr.appendChild(td);
    return td;
  }

  /** Draws the table into `box` (a <section> with a <table> and a <p class=lb-status>). */
  async function show(box, highlightRun) {
    if (!enabled) { box.hidden = true; return; }
    box.hidden = false;
    const status = box.querySelector('.lb-status');
    const body = box.querySelector('tbody');
    status.textContent = 'Loading…';
    let rows;
    try { rows = await top(); } catch (e) { status.textContent = 'Could not load the leaderboard.'; console.warn(e); return; }
    const mine = new Set(ls.get('mine', []));
    body.textContent = '';
    let myRank = 0;
    rows.forEach((r, i) => {
      const tr = document.createElement('tr');
      if (mine.has(r.run_id)) tr.className = 'mine';
      if (highlightRun && r.run_id === highlightRun) { tr.classList.add('just'); myRank = i + 1; }
      cell(tr, String(i + 1), 'num');
      cell(tr, r.name);
      cell(tr, Number(r.score).toLocaleString(), 'num');
      cell(tr, [r.char_class, r.char_level ? 'lvl ' + r.char_level : ''].filter(Boolean).join(' '));
      const how = cell(tr, r.outcome === 'won' ? 'Won!' : r.reason || r.outcome);
      how.title = r.reason || '';
      cell(tr, new Date(r.created_at).toLocaleDateString(), 'date');
      body.appendChild(tr);
    });
    status.textContent = rows.length === 0 ? 'No scores yet. Be the first!'
      : highlightRun ? (myRank ? `Your run placed #${myRank} of ${rows.length}.` : (rows.length >= 100 ? 'Your run did not make the top 100.' : ''))
        : `Top ${rows.length} runs from everyone who has played.`;
    const just = body.querySelector('tr.just');
    if (just) just.scrollIntoView({ block: 'nearest' });
  }

  return { enabled, submit, flush, show };
})();
