const CINE_ROWS = [
  {
    label: "LUT",
    values: ["RAW", "CSP-LOG12", "REC709"],
    index: 0,
  },
  {
    label: "FRAME RATE",
    values: ["24P", "25P", "50P", "100P"],
    index: 1,
  },
  {
    label: "Resolution",
    values: [
      "1920x1080 [FF]",
      "UHD 3840x2160 [4K25]",
      "UHD 4160x2560 [4K]",
      "1920x1080 [100P]",
    ],
    index: 2,
  },
  {
    label: "CODEC",
    values: ["CSP STREAM", "CSP TEMPORAL", "PACK12"],
    index: 0,
  },
  {
    label: "AUDIO",
    values: ["STEREO", "MONO", "OFF"],
    index: 0,
  },
  {
    label: "ASPECT RATIO",
    values: ["16:9", "1.85:1", "2.39:1", "4:3"],
    index: 1,
  },
];

const OTHER = {
  settings: ["Preview scale", "Thermal governor", "Write ceiling", "Language"],
  photo: ["Drive mode", "AF", "ISO safety", "Still format"],
  addons: ["Dual stripe", "SSD bridge", "HDMI proxy cue", "Fan control"],
  hacks: ["EDMAC dump", "Register peek", "Force Frozen LV", "Diag overlay"],
};

const MODE_MAP = {
  "1920x1080 [FF]": "FF1080P50",
  "UHD 3840x2160 [4K25]": "CROP4K25",
  "UHD 4160x2560 [4K]": "CROP4K50_FULL (needs CFast/SSD bridge)",
  "1920x1080 [100P]": "CROP1080P100",
};

let tab = "cine";
let sel = 2;

const panel = document.getElementById("panel");
const armed = document.getElementById("armed");
const storage = document.getElementById("storage");

function render() {
  document.querySelectorAll(".tab").forEach((btn) => {
    btn.classList.toggle("active", btn.dataset.tab === tab);
  });

  if (tab !== "cine") {
    const items = OTHER[tab] || [];
    panel.innerHTML = `<div class="panel-empty"><div><strong>${tab.toUpperCase()}</strong><p>${items.join(" · ")}</p></div></div>`;
    return;
  }

  panel.innerHTML = CINE_ROWS.map((row, i) => {
    const value = row.values[row.index];
    return `<div class="row ${i === sel ? "selected" : ""}" data-i="${i}">
      <span class="label">${row.label}</span>
      <span class="value">${value}</span>
    </div>`;
  }).join("");

  const res = CINE_ROWS[2].values[CINE_ROWS[2].index];
  const fps = CINE_ROWS[1].values[CINE_ROWS[1].index];
  const codec = CINE_ROWS[3].values[CINE_ROWS[3].index];
  armed.textContent = `Armed: ${MODE_MAP[res] || res} · ${fps} · ${codec} · budget ≤100 MB/s (bridge for full 4K50)`;
  storage.textContent = res.includes("4160") ? "CFast A: 128GB" : "CF A: 128GB";
}

document.querySelectorAll(".tab").forEach((btn) => {
  btn.addEventListener("click", () => {
    tab = btn.dataset.tab;
    render();
  });
});

panel.addEventListener("click", (e) => {
  const row = e.target.closest(".row");
  if (!row) return;
  sel = Number(row.dataset.i);
  render();
});

window.addEventListener("keydown", (e) => {
  const tabs = ["settings", "photo", "cine", "addons", "hacks"];
  if (e.key === "ArrowRight") {
    tab = tabs[Math.min(tabs.length - 1, tabs.indexOf(tab) + 1)];
    render();
  } else if (e.key === "ArrowLeft") {
    tab = tabs[Math.max(0, tabs.indexOf(tab) - 1)];
    render();
  } else if (tab === "cine" && e.key === "ArrowDown") {
    sel = Math.min(CINE_ROWS.length - 1, sel + 1);
    render();
  } else if (tab === "cine" && e.key === "ArrowUp") {
    sel = Math.max(0, sel - 1);
    render();
  } else if (tab === "cine" && (e.key === "Enter" || e.key === " ")) {
    const row = CINE_ROWS[sel];
    row.index = (row.index + 1) % row.values.length;
    render();
    e.preventDefault();
  }
});

render();
