import { test, expect } from '@playwright/test';
import { readFile, mkdir, copyFile } from 'node:fs/promises';

test.beforeEach(async ({ page, context }) => {
  // Every browser test removes real Web Serial before any application script.
  await page.addInitScript(() => { Object.defineProperty(navigator, 'serial', { value: undefined, configurable: true }); });
  await context.route('**/*', route => {
    const url = new URL(route.request().url());
    if (url.hostname !== 'localhost' && url.protocol !== 'blob:') throw new Error(`Unexpected runtime dependency: ${url.origin}`);
    return route.continue();
  });
  await mkdir('evidence/screenshots', { recursive: true });
});
async function prepare(page, scenario = 'app0') {
  await page.goto(`/simulation.html?scenario=${scenario}`);
  await page.locator('#install-btn').click();
  await expect(page.locator('#connect-read')).toBeEnabled();
  await page.locator('#physical-board').check();
  await page.locator('#connect-read').click();
}
async function confirm(page) {
  for (const id of ['backup-confirmed', 'tf-removed', 'plan-confirmed']) await page.locator(`#${id}`).check();
}
async function receipt(page) {
  const downloading = page.waitForEvent('download'); await page.locator('#save-receipt').click();
  const download = await downloading; const path = await download.path();
  return JSON.parse(await readFile(path, 'utf8'));
}

test('v0.1 Chinese visual baseline and one main action', async ({ page }) => {
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  await page.goto('/'); await expect(page.locator('h1')).toHaveText('草稿机');
  await expect(page.locator('main .install-primary')).toHaveCount(1);
  await expect(page.locator('.positioning')).toHaveText('无打扰口袋中文写作');
  await expect(page.locator('.values')).toHaveText('开源，无网络，无 AI，无广告');
  expect(await page.locator('body').evaluate(el => getComputedStyle(el).backgroundColor)).toBe('rgb(247, 245, 239)');
  expect(await page.locator('body').evaluate(el => getComputedStyle(el).color)).toBe('rgb(41, 41, 36)');
  expect(await page.locator('.brand-seal').evaluate(el => getComputedStyle(el).backgroundColor)).toBe('rgb(167, 69, 57)');
  await expect(page.locator('.hero-image')).toBeHidden(); expect(errors).toEqual([]);
  await page.screenshot({ path: 'evidence/screenshots/home-zh.png', fullPage: true });
});
test('English switch, unavailable ST7306 and information entries', async ({ page }) => {
  await page.goto('/'); await page.locator('#lang-toggle').click();
  await expect(page.locator('#install-btn')).toContainText('Connect & Install');
  await page.locator('#board-change').click(); await expect(page.locator('input[value="prototype"]')).toBeDisabled();
  for (const [key, title] of [['draftor', 'What is Draftor?'], ['bgime', 'What is bgIME?'], ['help', 'Installation help'], ['advanced', 'Advanced']]) {
    await page.locator(`[data-modal="${key}"]`).click(); await expect(page.locator('#dialog-heading')).toHaveText(title);
    if (key === 'help') await expect(page.locator('#dialog-body')).toContainText('LearningIndex');
    await page.locator('#dialog-close').click();
  }
  await page.screenshot({ path: 'evidence/screenshots/home-en.png', fullPage: true });
});
test('no Web Serial gives explicit feedback and no connect control', async ({ page }) => {
  await page.goto('/'); await page.locator('#install-btn').click();
  await expect(page.locator('#workflow-status')).toContainText('不支持安全的 Web Serial');
  await expect(page.locator('#connect-section')).toBeHidden();
  await page.screenshot({ path: 'evidence/screenshots/unsupported.png', fullPage: true });
});
test('real entry requires board and expected identity before even the mocked chooser', async ({ page }) => {
  await page.addInitScript(() => {
    window.serialChooserCalls = 0;
    Object.defineProperty(navigator, 'serial', { configurable: true, value: { requestPort: async () => { window.serialChooserCalls++; throw new DOMException('Synthetic cancellation', 'NotFoundError'); } } });
  });
  await page.goto('/'); await page.locator('#install-btn').click(); await expect(page.locator('#connect-read')).toBeEnabled();
  await page.locator('#connect-read').click(); await expect(page.locator('#workflow-error')).toContainText('BOARD');
  expect(await page.evaluate(() => window.serialChooserCalls)).toBe(0);
  await page.locator('#physical-board').check(); await page.locator('#connect-read').click(); await expect(page.locator('#workflow-error')).toContainText('IDENTITY');
  expect(await page.evaluate(() => window.serialChooserCalls)).toBe(0);
  await page.locator('#expected-mac').fill('02:00:00:00:00:01'); await page.locator('#connect-read').click();
  await expect(page.locator('#workflow-error')).toContainText('CONNECTION'); expect(await page.evaluate(() => window.serialChooserCalls)).toBe(1);
});
for (const slot of ['app0', 'app1']) test(`${slot} UI has exact reviewed plan, backup gates, local receipt and pending boot`, async ({ page }) => {
  await prepare(page, slot); await expect(page.locator('#plan-title')).toContainText(slot);
  await expect(page.locator('#plan-title')).toContainText(slot === 'app0' ? '0x10000' : '0x410000');
  await expect(page.locator('#execute-plan')).toBeDisabled(); await expect(page.locator('#erase-label')).toBeHidden();
  await confirm(page); await expect(page.locator('#execute-plan')).toBeEnabled();
  if (slot === 'app1') await page.screenshot({ path: 'evidence/screenshots/app1-plan.png', fullPage: true });
  await page.locator('#execute-plan').click(); await expect(page.locator('#workflow-status')).toContainText('VERIFIED_BOOT_PENDING');
  await expect(page.locator('#finished-note')).toContainText('不能证明真实设备');
  const r = await receipt(page); expect(r.simulation).toBe(true); expect(r.independentVerification).toBe(true);
  expect(r.tfResources).toBe('NOT_CHECKED'); expect(r.deviceQualification).toBe('DEVICE_PENDING');
  expect(JSON.stringify(r)).not.toContain('02:00:00:00:00:01');
  if (slot === 'app1') await page.screenshot({ path: 'evidence/screenshots/app1-verified.png', fullPage: true });
});
test('fresh UI is a unified-entry branch with independent unchecked erase gate', async ({ page }) => {
  await prepare(page, 'fresh'); await expect(page.locator('#plan-title')).toContainText('全新安装');
  await expect(page.locator('#erase-confirmed')).not.toBeChecked(); await confirm(page);
  await expect(page.locator('#execute-plan')).toBeDisabled(); await page.locator('#erase-confirmed').check();
  await expect(page.locator('#execute-plan')).toBeEnabled(); await page.locator('#execute-plan').click();
  await expect(page.locator('#workflow-status')).toContainText('VERIFIED_BOOT_PENDING');
  const r = await receipt(page); expect(r.erased).toBe(true); expect(r.simulation).toBe(true);
  expect(r.plan.writes.map(w => w.offset)).toEqual([0, 0x8000, 0xe000, 0x10000]);
});
for (const [scenario, code] of [['partition', 'PARTITIONS'], ['pending', 'OTA'], ['unknown', 'UNKNOWN'], ['security', 'SECURITY'], ['flash8', 'FLASH']]) test(`${scenario} UI stops without a write plan or fresh fallback`, async ({ page }) => {
  await prepare(page, scenario); await expect(page.locator('#workflow-error')).toContainText(code);
  await expect(page.locator('#plan-section')).toBeHidden(); expect((await receipt(page)).writeStarted).toBe(false);
});
for (const [scenario, code] of [['disconnect', 'DISCONNECTED'], ['verify', 'VERIFY']]) test(`${scenario} UI never reports success after a failed write`, async ({ page }) => {
  await prepare(page, scenario); await confirm(page); await page.locator('#execute-plan').click();
  await expect(page.locator('#workflow-error')).toContainText(code); await expect(page.locator('#finished-note')).toBeHidden();
  const r = await receipt(page); expect(r.status).toBe('STOP'); expect(r.flashOutcome).toBe('POSSIBLY_PARTIAL'); expect(r.independentVerification).toBe(false);
  await page.screenshot({ path: `evidence/screenshots/stop-${scenario}.png`, fullPage: true });
});
for (const type of ['abort', 'tamper']) test(`firmware ${type} blocks connection`, async ({ page }) => {
  await page.route('**/firmware/app.bin', async route => {
    if (type === 'abort') return route.abort('connectionfailed');
    const data = Buffer.from(await readFile('public/firmware/app.bin')); data[0] ^= 1;
    return route.fulfill({ status: 200, body: data, contentType: 'application/octet-stream' });
  });
  await page.goto('/simulation.html'); await page.locator('#install-btn').click();
  await expect(page.locator('#workflow-error')).toContainText(type === 'abort' ? 'DOWNLOAD' : 'ASSET');
  await expect(page.locator('#connect-read')).toBeDisabled();
});
test('mobile layout preserves one column and English feedback without overflow', async ({ page }) => {
  await page.setViewportSize({ width: 390, height: 844 }); await page.goto('/');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.screenshot({ path: 'evidence/screenshots/mobile-zh.png', fullPage: true });
  await page.locator('#lang-toggle').click(); await page.locator('#install-btn').click();
  await expect(page.locator('#workflow-status')).toContainText('Web Serial is unavailable');
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});
test('TF resource help is local, bilingual and lists all 18 exact public files', async ({ page }) => {
  await page.goto('/docs/TF_GUIDE.html'); await expect(page.locator('tbody tr')).toHaveCount(18);
  await expect(page.locator('body')).toContainText('Prepare TF resources separately');
  await expect(page.locator('body')).toContainText('relations.hnv'); await expect(page.locator('body')).toContainText('matched28.hbf1');
});
test('closing and reopening during download never lets the old request change the new session', async ({ page }) => {
  let calls = 0, releaseOld;
  const gate = new Promise(resolve => { releaseOld = resolve; });
  await page.route('**/firmware/FIRMWARE_MANIFEST.json', async route => {
    calls++;
    if (calls === 1) { await gate; return route.abort('connectionfailed'); }
    return route.continue();
  });
  await page.goto('/simulation.html'); await page.locator('#install-btn').click();
  await expect.poll(() => calls).toBe(1);
  await page.locator('#install-close').click(); await expect(page.locator('#install-dialog')).not.toBeVisible();
  await page.locator('#install-btn').click(); await expect(page.locator('#connect-read')).toBeEnabled();
  const failed = page.waitForEvent('requestfailed', req => req.url().endsWith('FIRMWARE_MANIFEST.json'));
  releaseOld(); await failed;
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
  await expect(page.locator('#workflow-status')).toContainText('四份固件与发行清单已核验');
  await expect(page.locator('#workflow-error')).toBeHidden();
});
