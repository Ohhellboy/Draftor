import { defineConfig } from '@playwright/test';
export default defineConfig({
  testDir: './tests/browser', fullyParallel: false, workers: 1, retries: 0,
  timeout: 30000, expect: { timeout: 10000 },
  reporter: [['list'], ['json', { outputFile: 'evidence/browser-tests.json' }]],
  use: { baseURL: 'http://localhost:4173', channel: process.env.DRAFTOR_BROWSER || 'chrome', headless: true,
    viewport: { width: 1280, height: 900 }, screenshot: 'only-on-failure', trace: 'off' },
  webServer: { command: 'node tools/serve.mjs', url: 'http://localhost:4173', reuseExistingServer: true },
});
