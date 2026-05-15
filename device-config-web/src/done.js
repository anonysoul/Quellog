import "./styles.css";
import { exitConfiguration } from "./api.js";

const exitButton = document.querySelector("#exit");
const statusEl = document.querySelector("#status");

function setStatus(text, tone = "neutral") {
  statusEl.textContent = text;
  statusEl.dataset.tone = tone;
}

exitButton.addEventListener("click", async () => {
  exitButton.disabled = true;
  setStatus("正在通知设备退出配网模式...");

  try {
    const result = await exitConfiguration();
    if (!result.success) {
      exitButton.disabled = false;
      setStatus(result.error || "请求失败，请稍后重试。", "error");
      return;
    }

    setStatus("已发送退出请求，设备即将断开当前热点。", "success");
  } catch (error) {
    exitButton.disabled = false;
    setStatus(error instanceof Error ? error.message : "请求失败，请稍后重试。", "error");
  }
});
