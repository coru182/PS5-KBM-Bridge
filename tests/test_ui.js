// Drives the real web UI against tests/host_server with Chromium.
const { chromium } = require('playwright');
const { spawn } = require('child_process');
const path = require('path');
const PORT = 18111, OUT = process.argv[2] || '.';
(async () => {
  const srv = spawn(path.join(__dirname, 'host_server'), [String(PORT)]);
  await new Promise(r => srv.stdout.once('data', r));
  const browser = await chromium.launch();
  const browserReq = (await browser.newContext()).request;
  const errors = [];
  const st = async (edit) => (await (await browserReq.get(`http://127.0.0.1:${PORT}/api/state` + (edit ? '?edit=' + edit : ''))).json());
  const pick = async (page, id) => { await page.click('#pchg'); await page.click(`#plist button[data-p="${id}"]`); await page.waitForTimeout(500); };
  try {
    for (const [name, vp] of [['desktop', {width: 1200, height: 900}], ['mobile', {width: 390, height: 844}]]) {
      const page = await browser.newPage({ viewport: vp });
      page.on('pageerror', e => errors.push(name + ': ' + e.message));
      page.on('console', m => { if (m.type() === 'error') errors.push(name + ' console: ' + m.text()); });
      await page.goto(`http://127.0.0.1:${PORT}/`);
      await page.waitForSelector('#binds tr');
      const rows = await page.$$eval('#binds tr', r => r.length);
      if (rows !== 22) throw new Error('re2r rows ' + rows);
      const gname = await page.$eval('#game .gname', e => e.textContent);
      if (gname.toLowerCase() !== 'resident evil 2') throw new Error('game name ' + gname);
      if (name === 'desktop' && !(await page.$eval('[data-target="auto"]', b => b.classList.contains('on') && b.textContent.includes('Resident Evil 2')))) throw new Error('auto button');
      const overflow = await page.evaluate(() => document.documentElement.scrollWidth - innerWidth);
      if (overflow > 2) throw new Error(name + ' horizontal overflow ' + overflow);
      await page.screenshot({ path: `${OUT}/ui_${name}.png`, fullPage: false });
      if (name === 'desktop') {
        // profile list: open, groups, switch
        await page.click('#pchg');
        const groups = await page.$$eval('#plist h3', h => h.map(x => x.textContent));
        if (groups.join('|') !== 'En uso ahora|Menú de la PS5|Plantillas|Juegos (A–Z)') throw new Error('groups ' + groups);
        await page.click('#plist button[data-p="requiem"]'); await page.waitForTimeout(500);
        if (await page.$$eval('#binds tr', r => r.length) !== 22) throw new Error('requiem rows');
        // slider
        await page.$eval('#sens', el => { el.value = 21; el.dispatchEvent(new Event('input')); el.dispatchEvent(new Event('change')); });
        await page.waitForTimeout(400);
        if ((await st('requiem')).config.edit.sens !== 21) throw new Error('slider not applied');
        if (!(await page.$eval('#pcur', e => e.textContent.includes('•')))) throw new Error('modified marker');
        // feel tips for Requiem + curve slider
        const feel = await page.$eval('#params details', d => d.open && d.textContent);
        if (!feel || !feel.includes('F10')) throw new Error('requiem feel tips');
        await page.$eval('#curve', el => { el.value = 35; el.dispatchEvent(new Event('input')); el.dispatchEvent(new Event('change')); });
        await page.waitForTimeout(400);
        if ((await st('requiem')).config.edit.curve !== 35) throw new Error('curve slider');
        // pick key from list
        await page.selectOption('select[data-a="reload"][data-s="0"]', 'T');
        await page.waitForTimeout(400);
        const chip = await page.$eval('select[data-a="reload"][data-s="0"]', s => s.closest('td').querySelector('.chip').textContent);
        if (chip !== 'T') throw new Error('pick failed: ' + chip);
        // capture (fake press of H after 400ms)
        await page.click('button[data-a="flashlight"][data-s="1"]');
        await page.waitForTimeout(1600);
        const chip2 = await page.$eval('button[data-a="flashlight"][data-s="1"]', b => b.closest('td').querySelector('.chip').textContent);
        if (chip2 !== 'H') throw new Error('capture failed: ' + chip2);
        // game target
        // game profile buttons survive periodic refresh; select one, then native
        await page.waitForTimeout(1500);                 // let a few refreshes happen
        await page.click('[data-other]');
        await page.click('#game .plist [data-target="requiem"]'); await page.waitForTimeout(400);
        let stg = await st();
        if (stg.runtime.profile !== 'requiem' || stg.runtime.title_source !== 'rule') throw new Error('game target button');
        if (!(await page.$eval('#game .targets > [data-target="requiem"]', b => b.classList.contains('on')))) throw new Error('target not highlighted');
        // template button
        await page.click('[data-target="melee"]'); await page.waitForTimeout(400);
        if ((await st()).runtime.profile !== 'melee') throw new Error('template button');
        // "use this profile in this game" from the editor
        await pick(page, 'generic');
        await page.click('#btnUseGame'); await page.waitForTimeout(400);
        stg = await st();
        if (stg.runtime.profile !== 'generic') throw new Error('use-for-game button');
        await page.click('[data-target="auto"]'); await page.waitForTimeout(400);
        if ((await st()).runtime.profile !== 're2r') throw new Error('back to auto');
        await pick(page, 'requiem');
        await page.click('[data-target="native"]');
        await page.waitForTimeout(400);
        const t = await page.$eval('#titles', e => e.textContent);
        if (!t.includes('PPSA04289') || !t.includes('Nativo') || !t.toLowerCase().includes('resident evil 2')) throw new Error('title rule not shown');
        // ADS slider + pointer toggle
        await page.$eval('#ads_pct', el => { el.value = 180; el.dispatchEvent(new Event('input')); el.dispatchEvent(new Event('change')); });
        await page.waitForTimeout(400);
        let st3 = await st('requiem');
        if (st3.config.edit.ads_pct !== 180) throw new Error('ads slider');
        await page.click('#ptrBtn'); await page.waitForTimeout(400);
        st3 = await st();
        if (st3.runtime.pointer_mode !== 1) throw new Error('pointer toggle');
        // autostart toggle + idle select
        await page.check('#autostart'); await page.waitForTimeout(400);
        let st2 = await st();
        if (st2.runtime.autostart_on !== 1) throw new Error('autostart toggle');
        await page.selectOption('#idle', '30'); await page.waitForTimeout(400);
        st2 = await st();
        if (st2.config.idle_exit_min !== 30) throw new Error('idle select');
        await page.check('#autonat'); await page.waitForTimeout(400);
        if ((await st()).config.auto_native !== 1) throw new Error('auto_native');
        await page.screenshot({ path: `${OUT}/ui_desktop_after.png`, fullPage: true });
      }
      await page.close();
    }
    // ---- language: manual switch to English on the Spanish console ----
    {
      const page = await browser.newPage({ viewport: {width: 1200, height: 900} });
      page.on('pageerror', e => errors.push('lang: ' + e.message));
      await page.goto(`http://127.0.0.1:${PORT}/`);
      await page.waitForSelector('#binds tr');
      if (await page.$eval('#gameSec h2', e => e.textContent) !== 'Juego actual') throw new Error('spanish heading');
      await page.click('[data-lang="en"]'); await page.waitForTimeout(600);
      if (await page.$eval('#gameSec h2', e => e.textContent) !== 'Current game') throw new Error('english heading');
      if (await page.evaluate(() => document.documentElement.lang) !== 'en') throw new Error('html lang');
      await page.click('#pchg');
      const g = await page.$$eval('#plist h3', h => h.map(x => x.textContent).join('|'));
      if (g !== 'In use now|PS5 menu|Templates|Games (A–Z)') throw new Error('english groups ' + g);
      const firstOut = await page.$$eval('#binds td.out', t => t.map(x => x.textContent));
      if (!firstOut.some(x => x.includes('Left stick'))) throw new Error('english outputs');
      await page.screenshot({ path: `${OUT}/ui_english.png`, fullPage: false });
      await page.click('[data-lang="auto"]'); await page.waitForTimeout(600);
      if (await page.$eval('#gameSec h2', e => e.textContent) !== 'Juego actual') throw new Error('back to auto (es)');
      await page.close();
    }
    if (errors.length) throw new Error(errors.join('\n'));
    console.log('PASS: web UI (render, no overflow at 390px, game name + auto profile, grouped profile list, slider, key pick, capture, game/template/other buttons, title rule, auto native, ES/EN switch, autostart, idle, ADS, pointer, no JS errors)');
  } finally { await browser.close(); srv.kill(); }
})().catch(e => { console.error('FAIL:', e.message); process.exit(1); });
