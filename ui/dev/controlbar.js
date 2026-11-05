// Control bar preview (D8.4). Dev only. Draws the bar of the owner's design from the controlBar.* strings
// and a few sample keys, in the size and state the URL says, so each one can be shown and screenshotted:
//   ?size=large|small  ?state=analysing|idle|speaking|paused  ?theme=system|light|dark
//   &hover=pause|skip|stop|help|size   that button is drawn as hovered
//   &shot=plain|article|game   the sample Shot behind the bar
//   &tip=pause|skip|stop|help|size   a tooltip is shown
//   &pos=plain    the bar at 20,20 on a plain grey Shot (to cut it out at its own size, 433 x 60 or 250 x 60)
//   &chrome=0     only the screen, no links and no notes (the screenshots use this)
// The words come from the C++ strings table (dev/strings.js, written by scripts/ui_preview.ps1). The
// links and the notes under the screen are developer text, not Lexia text. The icons are the SVG files
// of ../icons as CSS masks: on file:// Edge needs --allow-file-access-from-files.
(function () {
  "use strict";

  const params = new URLSearchParams(location.search);
  const pick = (name, allowed, fallback) => (allowed.indexOf(params.get(name)) >= 0 ? params.get(name) : fallback);

  const view = {
    size: pick("size", ["large", "small"], "large"),
    state: pick("state", ["analysing", "idle", "speaking", "paused"], "speaking"),
    theme: pick("theme", ["system", "light", "dark"], "system"),
    hover: pick("hover", ["pause", "skip", "stop", "help", "size"], ""),
    pos: pick("pos", ["plain"], ""),
    tip: params.get("tip") || "",
    chrome: params.get("chrome") !== "0",
  };
  view.shot = pick("shot", ["plain", "article", "game"], view.pos === "plain" ? "plain" : "article");

  // Sample keys, as settings.json would give them. An action with no key shows its label as the
  // tooltip. The Speech key (F6) skips with a tap and stops with a second tap.
  const KEYS = { pause: "F7", skip: "F6", stop: "F6 F6" };

  // controlBar.* from the C++ table; the English fallback only keeps the page usable on its own.
  const FALLBACK = {
    "controlBar.pause": "pause", "controlBar.pauseTip": "Pause ({key})",
    "controlBar.resume": "resume", "controlBar.resumeTip": "Resume ({key})",
    "controlBar.skip": "skip", "controlBar.skipTip": "Skip ({key})",
    "controlBar.stop": "stop", "controlBar.stopTip": "Stop ({key})",
    "controlBar.helpTip": "Open the Help page",
    "controlBar.sizeSmall": "Show the Small bar", "controlBar.sizeLarge": "Show the Large bar",
    "controlBar.name": "Lexia controls",
  };
  const STRINGS = window.LEXIA_DEV_STRINGS || {};
  if (!window.LEXIA_DEV_STRINGS) console.warn("Lexia control bar preview: dev/strings.js is missing. Run scripts\\ui_preview.ps1.");
  const t = (id) => (STRINGS[id] !== undefined ? STRINGS[id] : FALLBACK[id] || id);
  const tf = (id, vars) => t(id).replace(/\{(\w+)\}/g, (m, k) => (vars[k] !== undefined ? vars[k] : m));
  const capital = (s) => s.charAt(0).toUpperCase() + s.slice(1);

  const root = document.documentElement;
  if (view.theme !== "system") root.setAttribute("data-theme", view.theme);
  if (!view.chrome) document.body.classList.add("bare");
  if (view.pos === "plain") document.body.classList.add("plainpos");

  // ---------- small DOM helpers ----------
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
  function icon(name) {
    const i = el("span", "ico", { "aria-hidden": "true" });
    i.style.setProperty("--svg", "url(../icons/" + name + ".svg)");
    return i;
  }

  // ---------- the bar ----------
  const screen = document.getElementById("screen");
  const bar = document.getElementById("bar");
  bar.setAttribute("role", "toolbar");

  function setTip(b, act, text) {
    b.setAttribute("data-tip", text);
    if (view.tip === act) b.setAttribute("data-tip-open", "");
    if (view.hover === act) b.classList.add("is-hot");
  }

  // Pause, Skip, Stop: a tile with its icon, and the word (hidden in Small).
  function tileButton(act, iconName, labelId, tipId) {
    const b = el("button", "cbtn tilebtn", { type: "button", "data-act": act });
    const label = t(labelId);
    const tile = el("span", "tile");
    tile.appendChild(icon(iconName));
    b.appendChild(tile);
    const word = el("span", "cbtn-word");
    word.textContent = label;
    b.appendChild(word);
    b.setAttribute("aria-label", label);
    setTip(b, act, KEYS[act] ? tf(tipId, { key: KEYS[act] }) : capital(label));
    return b;
  }

  // Help and the size icon: only the icon.
  function iconButton(act, iconName, tipText) {
    const b = el("button", "cbtn iconbtn", { type: "button", "data-act": act });
    b.appendChild(icon(iconName));
    b.setAttribute("aria-label", tipText);
    setTip(b, act, tipText);
    return b;
  }

  function render() {
    bar.setAttribute("data-size", view.size);
    bar.setAttribute("aria-label", t("controlBar.name"));
    bar.textContent = "";

    const paused = view.state === "paused";
    // Pause, Skip and Stop work while something speaks or is paused, and not while the Analysis runs.
    const enabled = (view.state === "speaking" || paused) && view.state !== "analysing";

    // The grip: 2 x 3 dots of 4 px, 8 px apart across and 9 px down.
    let dots = "";
    for (let row = 0; row < 3; row++) for (let col = 0; col < 2; col++) dots += '<rect x="' + col * 8 + '" y="' + row * 9 + '" width="4" height="4" rx="1.2"/>';
    bar.appendChild(svg('<svg class="cbar-grip" viewBox="0 0 12 22" aria-hidden="true">' + dots + "</svg>"));

    const pause = paused
      ? tileButton("pause", "play-03", "controlBar.resume", "controlBar.resumeTip")
      : tileButton("pause", "pause-03", "controlBar.pause", "controlBar.pauseTip");
    const skip = tileButton("skip", "fast-forward", "controlBar.skip", "controlBar.skipTip");
    const stop = tileButton("stop", "stop-01", "controlBar.stop", "controlBar.stopTip");
    for (const b of [pause, skip, stop]) {
      b.disabled = !enabled;
      bar.appendChild(b);
    }
    bar.appendChild(el("span", "cbar-sep", { "aria-hidden": "true" }));

    if (view.size === "large") bar.appendChild(iconButton("help", "help-circle-contained", t("controlBar.helpTip")));
    bar.appendChild(
      view.size === "large"
        ? iconButton("size", "minimise-01", t("controlBar.sizeSmall"))
        : iconButton("size", "maximise-02", t("controlBar.sizeLarge"))
    );
  }

  // ---------- behaviour, so the owner can try it ----------
  bar.addEventListener("click", function (e) {
    const b = e.target.closest("button");
    if (!b) return;
    switch (b.dataset.act) {
      case "pause":
        view.state = view.state === "paused" ? "speaking" : "paused";
        break;
      case "stop":
        view.state = "idle";
        break;
      case "size":
        view.size = view.size === "large" ? "small" : "large";
        break;
      default:
        return; // Skip and Help: nothing to show in a preview
    }
    render();
  });

  // Drag by the grip or any empty part of the bar. A press on a button is a button press, never a drag.
  let drag = null;
  bar.addEventListener("pointerdown", function (e) {
    if (e.target.closest("button")) return;
    const r = bar.getBoundingClientRect();
    const s = screen.getBoundingClientRect();
    drag = { dx: e.clientX - r.left, dy: e.clientY - r.top, sx: s.left, sy: s.top };
    bar.setPointerCapture(e.pointerId);
    bar.classList.add("dragging");
  });
  bar.addEventListener("pointermove", function (e) {
    if (!drag) return;
    const s = screen.getBoundingClientRect();
    const x = Math.min(Math.max(e.clientX - drag.dx - drag.sx, 0), s.width - bar.offsetWidth);
    const y = Math.min(Math.max(e.clientY - drag.dy - drag.sy, 0), s.height - bar.offsetHeight);
    bar.style.transform = "none";
    bar.style.left = x + "px";
    bar.style.top = y + "px";
  });
  function endDrag() {
    drag = null;
    bar.classList.remove("dragging");
  }
  bar.addEventListener("pointerup", endDrag);
  bar.addEventListener("pointercancel", endDrag);

  // ---------- the sample Shot ----------
  function drawShot() {
    const shot = document.getElementById("shot");
    if (view.shot === "plain") {
      shot.className = "shot shot-plain";
    } else if (view.shot === "game") {
      shot.className = "shot shot-game";
      shot.innerHTML =
        '<div class="g-moon"></div><div class="g-hill a"></div><div class="g-hill b"></div>' +
        '<div class="g-box">* The lantern still burns, but the room is empty.<br>* Someone has been here before you.</div>';
    } else {
      shot.className = "shot shot-article";
      shot.innerHTML =
        '<div class="a-top"><span></span></div><div class="a-wrap"><div>' +
        "<h2>A morning walk along the old canal</h2>" +
        '<p class="a-by">Posted by the editors, nine minutes to read</p>' +
        "<p>The towpath is quiet before eight. A heron stands on the lock gate and does not move when the first cyclist passes, and the water under the bridge is the colour of tea.</p>" +
        "<p>Most of the old warehouses are flats now, but one still has its crane over the door. Someone has hung a basket of flowers from the hook, which feels about right.</p>" +
        '</div><div class="a-side"><div class="a-card tall"></div><div class="a-card"></div></div></div>';
    }
  }

  // ---------- the links (developer chrome) ----------
  function href(over) {
    const q = Object.assign({}, { size: view.size, state: view.state, theme: view.theme, shot: view.shot }, over);
    return "?" + Object.keys(q).filter((k) => q[k] !== undefined && q[k] !== null && q[k] !== "").map((k) => k + "=" + q[k]).join("&");
  }
  function group(label, key, values) {
    const g = el("div", "pv-nav-group");
    const l = el("span", "pv-nav-label");
    l.textContent = label;
    g.appendChild(l);
    for (const v of values) {
      const a = el("a", "pv-link", { href: href({ [key]: v[0] }) });
      a.textContent = v[1];
      if (view[key] === v[0]) a.setAttribute("aria-current", "true");
      g.appendChild(a);
    }
    return g;
  }
  function drawChrome() {
    const nav = document.getElementById("nav");
    nav.appendChild(group("Size", "size", [["large", "Large"], ["small", "Small"]]));
    nav.appendChild(group("State", "state", [["analysing", "Analysing"], ["idle", "Idle"], ["speaking", "Speaking"], ["paused", "Paused"]]));
    nav.appendChild(group("Theme", "theme", [["system", "System"], ["light", "Light"], ["dark", "Dark"]]));
    nav.appendChild(group("Shot", "shot", [["plain", "Plain"], ["article", "Article"], ["game", "Game"]]));
    nav.appendChild(group("Hover", "hover", [["", "None"], ["pause", "Pause"], ["skip", "Skip"], ["stop", "Stop"], ["help", "Help"], ["size", "Size"]]));

    // Every combination, in one table.
    const table = document.getElementById("combos");
    const states = [["analysing", "Analysis running"], ["idle", "Idle"], ["speaking", "Speaking"], ["paused", "Paused"]];
    const head = el("tr");
    for (const h of ["State", "Large, light", "Large, dark", "Small, light", "Small, dark"]) {
      const th = el("th");
      th.textContent = h;
      head.appendChild(th);
    }
    table.appendChild(head);
    for (const s of states) {
      const tr = el("tr");
      const th = el("th");
      th.textContent = s[1];
      tr.appendChild(th);
      for (const size of ["large", "small"]) {
        for (const theme of ["light", "dark"]) {
          const td = el("td");
          const a = el("a", "", { href: "?size=" + size + "&state=" + s[0] + "&theme=" + theme + "&shot=" + view.shot });
          a.textContent = "Open";
          td.appendChild(a);
          tr.appendChild(td);
        }
      }
      table.appendChild(tr);
    }
  }

  // ---------- start ----------
  drawShot();
  if (view.chrome) drawChrome();
  render();
})();
