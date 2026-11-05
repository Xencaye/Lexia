// Tray menu preview (D0 ui 7). Dev only. Draws the menu from the tray.* strings.
//   ?theme=light|dark|system   Windows' own mode (default: follow the browser)
//   ?state=on|off              Lexia On or Off (the radio dot and the tray icon follow)
//   &hover=on|off|open|quit    show one row hovered
//   &chrome=0                  only the sketch, no links and no notes (the screenshots use this)
// The links and notes are developer text, not Lexia text.
(function () {
  "use strict";

  const lexia = window.lexia;
  const params = new URLSearchParams(location.search);
  const pick = (name, allowed, fallback) => (allowed.indexOf(params.get(name)) >= 0 ? params.get(name) : fallback);
  const view = {
    theme: pick("theme", ["system", "light", "dark"], "system"),
    state: pick("state", ["on", "off"], "on"),
    hover: params.get("hover") || "",
    chrome: params.get("chrome") !== "0",
  };

  function el(tag, cls, attrs) {
    const node = document.createElement(tag);
    if (cls) node.className = cls;
    for (const k in attrs || {}) node.setAttribute(k, attrs[k]);
    return node;
  }
  function svg(markup) {
    const t = document.createElement("template");
    t.innerHTML = markup.trim();
    return t.content.firstChild;
  }

  const dark = view.theme === "dark" || (view.theme === "system" && matchMedia("(prefers-color-scheme: dark)").matches);
  if (view.theme !== "system") document.documentElement.setAttribute("data-theme", view.theme);
  if (!view.chrome) document.body.classList.add("bare");
  const desk = document.getElementById("desk");
  desk.setAttribute("data-theme", dark ? "dark" : "light");

  function row(id, labelId, opts) {
    const li = el("li", "", { role: "none" });
    const b = el("button", "item" + (opts.def ? " default" : ""), { type: "button", role: opts.radio ? "menuitemradio" : "menuitem", "data-id": id });
    if (opts.radio) b.setAttribute("aria-checked", String(opts.checked));
    if (view.hover === id) b.setAttribute("data-hover", "true");
    const glyph = el("span", "glyph", { "aria-hidden": "true" });
    if (opts.radio) glyph.appendChild(el("i"));
    b.appendChild(glyph);
    const text = el("span");
    text.textContent = lexia.t(labelId);
    b.appendChild(text);
    li.appendChild(b);
    return li;
  }

  function drawMenu() {
    const menu = document.getElementById("menu");
    const on = view.state === "on";
    menu.appendChild(row("on", "tray.on", { radio: true, checked: on }));
    menu.appendChild(row("off", "tray.off", { radio: true, checked: !on }));
    menu.appendChild(el("hr", "", { role: "separator" }));
    menu.appendChild(row("open", "tray.open", { def: true }));
    menu.appendChild(row("quit", "tray.quit", {}));
    // The radio pair: a click on On or Off moves the dot, as App::SetOn would.
    menu.addEventListener("click", function (e) {
      const b = e.target.closest(".item[role=menuitemradio]");
      if (!b) return;
      for (const r of menu.querySelectorAll("[role=menuitemradio]")) r.setAttribute("aria-checked", String(r === b));
      drawIcon(b.dataset.id === "on");
    });
  }

  const GLYPHS = {
    chevron: '<svg class="tb-glyph" viewBox="0 0 16 16" aria-hidden="true"><path d="M3 10l5-5 5 5"/></svg>',
    wifi: '<svg class="tb-glyph" viewBox="0 0 16 16" aria-hidden="true"><path d="M2 6.5a9 9 0 0 1 12 0M4.5 9a5.5 5.5 0 0 1 7 0M7 11.4a2 2 0 0 1 2 0"/></svg>',
    speaker: '<svg class="tb-glyph" viewBox="0 0 16 16" aria-hidden="true"><path d="M2 6h2.5L8 3v10L4.5 10H2zM11 5.5a3.5 3.5 0 0 1 0 5"/></svg>',
    battery: '<svg class="tb-glyph" viewBox="0 0 16 16" aria-hidden="true"><rect x="1.5" y="4.5" width="11" height="7" rx="1.5"/><path d="M14.5 7v2"/></svg>',
  };
  function slot(html, cls) {
    const s = el("div", "tb-slot" + (cls ? " " + cls : ""));
    s.innerHTML = html;
    return s;
  }
  function drawIcon(on) {
    const s = document.querySelector(".tb-slot.lexia");
    s.textContent = "";
    s.appendChild(el("img", "tray-icon" + (on ? "" : " off"), { src: "../../src/res/lexia.ico", alt: "", width: 20, height: 20 }));
    s.appendChild(el("span", "slash"));
    s.setAttribute("data-open", "true");
    s.setAttribute("title", lexia.t(on ? "tray.on" : "tray.off"));
  }
  function drawTaskbar() {
    const tb = document.getElementById("taskbar");
    tb.appendChild(slot(GLYPHS.chevron));
    tb.appendChild(slot("", "lexia"));
    tb.appendChild(slot(GLYPHS.wifi));
    tb.appendChild(slot(GLYPHS.speaker));
    tb.appendChild(slot(GLYPHS.battery));
    const clock = el("div", "tb-clock", { "aria-hidden": "true" });
    clock.appendChild(el("i"));
    clock.appendChild(el("i"));
    tb.appendChild(clock);
    drawIcon(view.state === "on");
  }

  function href(over) {
    const q = Object.assign({ theme: view.theme, state: view.state }, over);
    return "?" + Object.keys(q).map((k) => k + "=" + q[k]).join("&");
  }
  function drawChrome() {
    const nav = document.getElementById("nav");
    for (const g of [["Theme", "theme", [["system", "System"], ["light", "Light"], ["dark", "Dark"]]], ["State", "state", [["on", "On"], ["off", "Off"]]]]) {
      const group = el("div", "pv-nav-group");
      const label = el("span", "pv-nav-label");
      label.textContent = g[0];
      group.appendChild(label);
      for (const v of g[2]) {
        const a = el("a", "pv-link", { href: href({ [g[1]]: v[0] }) });
        a.textContent = v[1];
        if (view[g[1]] === v[0]) a.setAttribute("aria-current", "true");
        group.appendChild(a);
      }
      nav.appendChild(group);
    }
  }

  lexia.bridge.receive({ type: "strings", language: "en", strings: window.LEXIA_DEV_STRINGS || {} });
  if (!window.LEXIA_DEV_STRINGS) console.warn("Lexia tray preview: dev/strings.js is missing. Run scripts\\ui_preview.ps1.");
  drawTaskbar();
  drawMenu();
  if (view.chrome) drawChrome();
})();
