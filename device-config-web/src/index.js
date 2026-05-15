import "./styles.css";
import { deleteCredential, loadCredentials, scanNetworks, submitCredentials } from "./api.js";

const ssidList = document.querySelector("#ssid-list");
const ssidInput = document.querySelector("#ssid");
const passwordInput = document.querySelector("#password");
const submitButton = document.querySelector("#submit");
const refreshButton = document.querySelector("#refresh");
const togglePasswordButton = document.querySelector("#toggle-password");
const savedList = document.querySelector("#saved-list");
const statusEl = document.querySelector("#status");

let refreshTimerId = 0;

function setStatus(text, tone = "neutral") {
  statusEl.textContent = text;
  statusEl.dataset.tone = tone;
}

function setBusyState(isBusy) {
  submitButton.disabled = isBusy;
  refreshButton.disabled = isBusy;
}

function renderNetworks(items) {
  ssidList.innerHTML = "";
  const placeholder = document.createElement("option");
  placeholder.value = "";
  placeholder.textContent = items.length ? "选择 Wi‑Fi 或手动输入" : "暂未发现网络，请手动输入";
  ssidList.appendChild(placeholder);

  items.forEach((item) => {
    if (!item?.ssid) {
      return;
    }
    const option = document.createElement("option");
    option.value = item.ssid;
    option.textContent = `${item.ssid}  ${item.rssi} dBm`;
    ssidList.appendChild(option);
  });
}

function renderSavedCredentials(items) {
  savedList.innerHTML = "";

  if (!items.length) {
    const empty = document.createElement("li");
    empty.className = "saved-empty";
    empty.textContent = "还没有保存过 Wi‑Fi，成功配网后会自动记录。";
    savedList.appendChild(empty);
    return;
  }

  items.forEach((item) => {
    if (!item?.ssid) {
      return;
    }

    const row = document.createElement("li");
    row.className = "saved-item";

    const info = document.createElement("div");
    info.className = "saved-info";

    const name = document.createElement("strong");
    name.textContent = item.ssid;
    info.appendChild(name);

    const meta = document.createElement("span");
    meta.textContent = item.isOpen ? "开放网络" : "已保存密码";
    info.appendChild(meta);

    const action = document.createElement("button");
    action.type = "button";
    action.className = "ghost-button inline-button";
    action.textContent = "删除";
    action.dataset.ssid = item.ssid;

    row.append(info, action);
    savedList.appendChild(row);
  });
}

async function loadNetworks({ silent = false } = {}) {
  if (!silent) {
    setStatus("正在刷新附近网络...");
  }

  try {
    const data = await scanNetworks();
    const items = Array.isArray(data.aps) ? data.aps : [];
    renderNetworks(items);
    if (!silent) {
      setStatus(items.length ? "已刷新附近网络。" : "扫描完成，未发现可用网络。");
    }
  } catch (error) {
    setStatus(error instanceof Error ? error.message : "扫描失败，请稍后重试。", "error");
  }
}

async function refreshSavedCredentials() {
  try {
    const data = await loadCredentials();
    const items = Array.isArray(data.credentials) ? data.credentials : [];
    renderSavedCredentials(items);
  } catch (error) {
    renderSavedCredentials([]);
    setStatus(error instanceof Error ? error.message : "读取已保存网络失败。", "error");
  }
}

async function handleSubmit() {
  const ssid = ssidInput.value.trim();
  const password = passwordInput.value;

  if (!ssid) {
    setStatus("请填写 Wi‑Fi 名称。", "error");
    ssidInput.focus();
    return;
  }

  setBusyState(true);
  setStatus("正在验证并保存网络，请稍候...");

  try {
    const result = await submitCredentials(ssid, password);
    if (!result.success) {
      setBusyState(false);
      setStatus(result.error || "连接失败，请检查密码后重试。", "error");
      return;
    }

    setStatus("配网成功，设备正在切换到家庭网络...", "success");
    void refreshSavedCredentials();
    window.setTimeout(() => {
      window.location.href = "/done.html";
    }, 300);
  } catch (error) {
    setBusyState(false);
    setStatus(error instanceof Error ? error.message : "提交失败，请稍后重试。", "error");
  }
}

ssidList.addEventListener("change", () => {
  if (ssidList.value) {
    ssidInput.value = ssidList.value;
  }
});

refreshButton.addEventListener("click", () => {
  void loadNetworks();
});

savedList.addEventListener("click", (event) => {
  const target = event.target;
  if (!(target instanceof HTMLButtonElement)) {
    return;
  }

  const ssid = target.dataset.ssid?.trim();
  if (!ssid) {
    return;
  }

  target.disabled = true;
  setStatus(`正在删除 ${ssid}...`);
  void deleteCredential(ssid)
    .then(async (result) => {
      if (!result.success) {
        throw new Error(result.error || "删除失败，请稍后重试。");
      }
      setStatus(`已删除 ${ssid}。`, "success");
      await refreshSavedCredentials();
    })
    .catch((error) => {
      target.disabled = false;
      setStatus(error instanceof Error ? error.message : "删除失败，请稍后重试。", "error");
    });
});

togglePasswordButton.addEventListener("click", () => {
  const shouldShow = passwordInput.type === "password";
  passwordInput.type = shouldShow ? "text" : "password";
  togglePasswordButton.textContent = shouldShow ? "隐藏" : "显示";
});

submitButton.addEventListener("click", () => {
  void handleSubmit();
});

passwordInput.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    void handleSubmit();
  }
});

void loadNetworks();
void refreshSavedCredentials();
refreshTimerId = window.setInterval(() => {
  void loadNetworks({ silent: true });
}, 8000);

window.addEventListener("beforeunload", () => {
  if (refreshTimerId) {
    window.clearInterval(refreshTimerId);
  }
});
