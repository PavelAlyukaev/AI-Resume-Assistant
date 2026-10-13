// Browser e2e: create a resume in the web UI, save, reload, reopen by ID, verify persistence.
// Usage: node web_e2e.mjs <path-to-web_e2e_server> [screenshot-path]
import { spawn } from "node:child_process";
import { mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";
import assert from "node:assert/strict";
import { chromium } from "playwright-core";

const [serverExe, shotArg] = process.argv.slice(2);
if (!serverExe) throw new Error("usage: node web_e2e.mjs <web_e2e_server> [screenshot]");
const screenshot = resolve(shotArg ?? "screenshots/web-e2e-sprint1.png");
mkdirSync(dirname(screenshot), { recursive: true });

const server = spawn(serverExe, [], { stdio: ["ignore", "pipe", "inherit"] });
const port = await new Promise((ok, fail) => {
  const timer = setTimeout(() => fail(new Error("server did not report a port")), 15000);
  server.on("exit", (code) => fail(new Error("server exited early: " + code)));
  server.stdout.on("data", (chunk) => {
    const match = /PORT=(\d+)/.exec(chunk.toString());
    if (match) { clearTimeout(timer); ok(Number(match[1])); }
  });
});

let browser;
try {
  browser = await chromium.launch({ channel: process.env.E2E_BROWSER_CHANNEL ?? "chrome", headless: true });
  const page = await browser.newPage({ viewport: { width: 960, height: 640 } });
  const pageErrors = [];
  page.on("pageerror", (e) => pageErrors.push(e.message));
  page.on("console", (m) => m.type() === "error" && pageErrors.push(m.text()));
  const url = `http://127.0.0.1:${port}/`;

  await page.goto(url);
  assert.match(await page.title(), /AI Resume Assistant/);

  await page.fill("#title", "E2E резюме");
  await page.click("#add");
  await page.fill("#position", "Разработчик C++");
  await page.fill("#company", "Acme");
  await page.fill("#period", "2022–2024");
  await page.fill("#description", "Qt desktop");
  await page.click("#apply");
  await page.click("#save");
  // Locator wait, not waitForFunction: the page CSP forbids eval.
  await page.locator("#status", { hasText: "Сохранено" }).waitFor();
  const status = await page.textContent("#status");
  const id = /ID:\s*(\d+)/.exec(status)?.[1];
  assert.ok(id, "status must contain resume ID, got: " + status);

  // Persistence: fresh page load, then open by ID through the prompt dialog.
  await page.reload();
  assert.equal(await page.locator("#stages li").count(), 0, "reloaded page starts empty");
  page.once("dialog", (dialog) => dialog.accept(id));
  await page.click("#open");
  await page.locator("#stages li", { hasText: "Разработчик C++" }).waitFor();
  await page.click("#stages li");
  assert.equal(await page.inputValue("#title"), "E2E резюме");
  assert.equal(await page.inputValue("#company"), "Acme");
  assert.equal(await page.inputValue("#period"), "2022–2024");
  assert.equal(await page.inputValue("#description"), "Qt desktop");

  // API agrees with what the UI shows.
  const saved = await (await page.request.get(`${url}api/resumes/${id}`)).json();
  assert.equal(saved.stages.length, 1);
  assert.equal(saved.stages[0].position, "Разработчик C++");

  await page.screenshot({ path: screenshot });
  assert.deepEqual(pageErrors, [], "no browser console/page errors");
  console.log(`web_e2e OK (resume ID ${id}); screenshot: ${screenshot}`);
} finally {
  await browser?.close();
  server.kill();
}
