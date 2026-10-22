import { defineConfig } from '@playwright/test';
export default defineConfig({
  testDir: './tests/browser',
  fullyParallel: true,
  snapshotPathTemplate: '{testDir}/{testFilePath}-snapshots/{arg}{ext}',
  workers: 2,
  timeout: 20000,
  use: { baseURL: 'http://127.0.0.1:8093', browserName: 'chromium', headless: true, viewport: { width: 1100, height: 1000 }, trace: 'retain-on-failure' },
  webServer: { command: 'node tests/browser/server.mjs', url: 'http://127.0.0.1:8093/simulator/', reuseExistingServer: false },
});
