/* Public player shell; normal joypad inputs only. Native runtime is unchanged. */
"use strict";
const statusText = document.getElementById("player-status");
const startButton = document.getElementById("start-game");
function sendKey(type, key) {
  window.dispatchEvent(new KeyboardEvent(type, { key, bubbles: true }));
}
function tapStart() {
  if (startButton.disabled) return;
  sendKey("keydown", "Enter");
  window.setTimeout(() => sendKey("keyup", "Enter"), 150);
}
startButton.addEventListener("click", tapStart);

// Touch keeps GB Studio's native handling. Mouse and keyboard can also use
// the visible controls on a narrow window, with release on cancellation.
document.querySelectorAll("[data-key]").forEach((button) => {
  button.disabled = true;
  let held = false;
  let pointerClick = false;
  let pressedAt = 0;
  let releaseTimer;
  const release = () => {
    if (held) {
      const remaining = Math.max(0, 150 - (performance.now() - pressedAt));
      releaseTimer = window.setTimeout(() => sendKey("keyup", button.dataset.key), remaining);
    }
    held = false;
    button.classList.remove("btnPressed");
  };
  button.addEventListener("pointerdown", (event) => {
    if (event.pointerType === "touch") return;
    event.preventDefault();
    button.setPointerCapture(event.pointerId);
    window.clearTimeout(releaseTimer);
    pressedAt = performance.now();
    pointerClick = true;
    held = true;
    button.classList.add("btnPressed");
    sendKey("keydown", button.dataset.key);
  });
  button.addEventListener("pointerup", release);
  button.addEventListener("pointercancel", () => {
    pointerClick = false;
    held = false;
    window.clearTimeout(releaseTimer);
    sendKey("keyup", button.dataset.key);
    button.classList.remove("btnPressed");
  });
  button.addEventListener("lostpointercapture", release);
  button.addEventListener("touchcancel", () => {
    sendKey("keyup", button.dataset.key);
    button.classList.remove("btnPressed");
  });
  button.addEventListener("click", (event) => {
    if (pointerClick) { pointerClick = false; return; }
    sendKey("keydown", button.dataset.key);
    window.setTimeout(() => sendKey("keyup", button.dataset.key), 150);
  });
});
document.getElementById("controller_dpad").addEventListener("touchcancel", () => {
  ["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].forEach((key) => sendKey("keyup", key));
});
function showFailure() {
  statusText.textContent = "游戏加载失败，请刷新重试，或从 GitHub 下载 ROM。Game could not load; refresh to retry.";
  startButton.disabled = true;
  startButton.textContent = "加载失败";
}
window.addEventListener("error", showFailure);
window.addEventListener("unhandledrejection", showFailure);
const readyDeadline = performance.now() + 20000;
function checkReady() {
  // Wait for cartridge startup, not only for the WASM instance to exist.
  if (typeof emulator !== "undefined" && emulator && emulator.ticks >= 120 * 70224) {
    startButton.disabled = false;
    document.querySelectorAll("[data-key]").forEach((button) => { button.disabled = false; });
    startButton.textContent = "开始 / 暂停";
    statusText.textContent = "已就绪 · 电脑按 Enter，手机点 Start。Ready to play.";
    return;
  }
  if (performance.now() > readyDeadline) { showFailure(); return; }
  requestAnimationFrame(checkReady);
}
checkReady();
