/* Mockup defaults: LUT RAW, FRAME RATE 25P, Resolution UHD 4K selected, CODEC MOV, AUDIO STEREO, 1.85:1 */
const CINE_ROWS = [
  {
    label: "LUT",
    icon: "lut",
    values: ["RAW", "CSP-LOG12", "REC709"],
    index: 0,
  },
  {
    label: "FRAME RATE",
    icon: "fps",
    values: ["24P", "25P", "50P", "100P"],
    index: 1,
  },
  {
    label: "Resolution",
    icon: "cam",
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
    icon: "codec",
    values: ["MOV", "CSP STREAM", "CSP TEMPORAL", "PACK12"],
    index: 0,
  },
  {
    label: "AUDIO",
    icon: "audio",
    values: ["STEREO", "MONO", "OFF"],
    index: 0,
  },
  {
    label: "ASPECT RATIO",
    icon: "aspect",
    values: ["16:9", "1.85:1", "2.39:1", "4:3"],
    index: 1,
  },
];

const ICONS = {
  lut: `<svg class="row-ico" viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M4 4h6v6H4V4zm10 0h6v6h-6V4zM4 14h6v6H4v-6zm10 3h6v3h-6v-3zm0-3h6v2h-6v-2z"/></svg>`,
  fps: `<svg class="row-ico" viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M12 2a10 10 0 1 0 10 10A10 10 0 0 0 12 2zm1 10.5V7h-2v7h6v-2z"/></svg>`,
  cam: `<svg class="row-ico" viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M3 6h11a2 2 0 0 1 2 2v1l4-2v10l-4-2v1a2 2 0 0 1-2 2H3a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2z"/></svg>`,
  codec: `<svg class="row-ico" viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M4 6h16v12H4V6zm2 2v8h12V8H6zm2 2h2v2H8v-2zm4 0h2v2h-2v-2zm4 0h2v2h-2v-2z"/></svg>`,
  audio: `<svg class="row-ico" viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M9 4v10.5A2.5 2.5 0 1 1 7 12V8h2V4h6v4h-2v5.5a2.5 2.5 0 1 1-2-2.45V4H9z"/></svg>`,
  aspect: `<svg class="row-ico" viewBox="0 0 24 24" aria-hidden="true"><path fill="currentColor" d="M3 7h18v10H3V7zm2 2v6h14V9H5z"/></svg>`,
};

const OTHER = {
  settings: ["Preview scale", "Thermal governor", "Write ceiling", "Language"],
  photo: ["Drive mode", "AF", "ISO safety", "Still format"],
  addons: ["Dual stripe", "SSD bridge", "HDMI proxy cue", "Fan control"],
  hacks: ["EDMAC dump", "Register peek", "Force Frozen LV", "Diag overlay"],
};

const MODE_MAP = {
  "1920x1080 [FF]": "FF1080P50",
  "UHD 3840x2160 [4K25]": "CROP4K25",
  "UHD 4160x2560 [4K]": "CROP4K50_FULL (needs CFast / SSD bridge)",
  "1920x1080 [100P]": "CROP1080P100",
};

let tab = "cine";
let sel = 2; // Resolution — matches mockup highlight

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
    const ico = ICONS[row.icon] || "";
    return `<div class="row ${i === sel ? "selected" : ""}" data-i="${i}" role="option" aria-selected="${i === sel}">
      <span class="label">${ico}<span class="label-text">${row.label}</span></span>
      <span class="value">${value}</span>
    </div>`;
  }).join("");

  const res = CINE_ROWS[2].values[CINE_ROWS[2].index];
  const fps = CINE_ROWS[1].values[CINE_ROWS[1].index];
  const codec = CINE_ROWS[3].values[CINE_ROWS[3].index];
  armed.textContent = `Armed: ${MODE_MAP[res] || res} · ${fps} · ${codec} · budget ≤100 MB/s`;
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
