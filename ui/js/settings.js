// The settings page (D8): the keys with a card that says what the selected key does, and five switches
// (run at startup, start minimized, run as admin, dark mode, left-handed). Start minimized can be used only
// while run at startup is on; otherwise it is drawn lighter and does nothing.
// The whole bridge contract is in ui/README.md; this header says what this file uses of it.
// Like the rest of the page it draws only what the last "state" says. A user action sends a
// message; the change shows when the next "state" arrives.
//
// state fields used here (the host sends them in "state", beside the ones app.js reads):
//   keys            [{ action: "read" | "pause" | "speech", key: "F8" }]  (an action "language" is ignored)
//                   key is the text of the key as settings.json writes it ("F8", "Ctrl+Alt+R"),
//                   or "" when the action has no key
//   runAtStartup    bool
//   startMinimised  bool       a start at logon stays in the tray; offered only while runAtStartup is on
//   runAsAdmin      bool       the stored setting
//   dark            bool       the theme in use; the dark mode switch shows it
//   leftHanded      bool
//
// page -> host
//   setRunAtStartup { on }   setStartMinimised { on }   setRunAsAdmin { on }   setLeftHanded { on }
//   setTheme { theme }       "dark" or "light": the dark mode switch sends the other of what `dark` says
//   captureKey { action }    the user selected a key chip. The page now waits for a key. The
//                            host may use this to stop the Lexia keys from acting while the
//                            user chooses one.
//   setKey { action, key }   the page heard the key (name as settings.json writes it, with
//                            modifiers: "Ctrl+Alt+R"). The host checks it (a letter needs a
//                            modifier, no two actions on one key), keeps it or not, and
//                            answers with a new "state". The chip shows the key in that state.
//   cancelKeyCapture { action }   Esc, a click outside the chip, or the focus left it: the capture ended with no key
//   readKeyCard { action }   the key card's play button: the host reads that key's description aloud
//
// The key is heard in the page (keydown), not by the host, because the page only shows what the
// state says: there is no "keyCaptured" message. What the page itself owns, as with the page that is
// shown: which key row is selected (a hover, a focus or a click selects it) and which chip waits for a key.
//
// For screenshots and for a host that opens the page on a place: ?select=speech selects that key,
// ?capture=read puts that key in capture.
(function () {
  "use strict";

  const lexia = window.lexia;
  const $ = function (id) {
    return document.getElementById(id);
  };

  let state = null;
  let selected = null; // the action of the selected row
  let capturing = null; // the action whose chip waits for a key

  const params = new URLSearchParams(location.search);
  let startUp = { select: params.get("select"), capture: params.get("capture") };

  // ---- Keys ------------------------------------------------------------------------------
  const rows = Object.create(null); // action -> { li, name, chip }

  function keysOf(s) {
    return (Array.isArray(s.keys) ? s.keys : []).filter(function (k) {
      return k && typeof k.action === "string" && k.action !== "language"; // no Language key in 0.5
    });
  }

  function select(action) {
    if (capturing || selected === action) return; // the card stays with the key that waits for a key
    selected = action;
    if (state) renderKeys();
  }

  function makeRow(action) {
    const li = document.createElement("li");
    li.className = "key-row";
    const name = document.createElement("span");
    name.className = "key-name";
    const chip = document.createElement("button");
    chip.type = "button";
    chip.className = "key-chip";
    li.append(name, chip);
    // A hover, a focus or a click on the row selects it (and shows its card).
    li.addEventListener("pointerenter", function () {
      select(action);
    });
    li.addEventListener("focusin", function () {
      select(action);
    });
    li.addEventListener("click", function () {
      select(action);
    });
    chip.addEventListener("click", function () {
      startCapture(action);
    });
    chip.addEventListener("blur", function () {
      if (capturing === action) endCapture(true);
    });
    return { li: li, name: name, chip: chip };
  }

  function renderKeys() {
    const list = keysOf(state);
    const ul = $("keys");
    if (list.length && !list.some(function (k) { return k.action === selected; })) selected = list[0].action;

    // Rows are kept, not rebuilt, so the focus stays where it is when a new state arrives.
    const wanted = list.map(function (k) { return k.action; });
    for (const action of Object.keys(rows)) {
      if (wanted.indexOf(action) < 0) {
        rows[action].li.remove();
        delete rows[action];
      }
    }
    list.forEach(function (k, i) {
      const action = k.action;
      if (!rows[action]) rows[action] = makeRow(action);
      const row = rows[action];
      if (ul.children[i] !== row.li) ul.insertBefore(row.li, ul.children[i] || null);

      const full = lexia.t("window.keys." + action);
      row.name.textContent = lexia.t("window.keys." + action + "Row");
      if (action === selected) row.li.dataset.selected = "";
      else delete row.li.dataset.selected;

      const none = k.key === "" || typeof k.key !== "string";
      const waiting = capturing === action;
      const shown = waiting ? lexia.t("window.keys.capturing") : none ? lexia.t("key.none") : k.key;
      row.chip.textContent = shown;
      row.chip.dataset.state = waiting ? "capturing" : none ? "none" : "set";
      row.chip.setAttribute("aria-label", lexia.tf("window.keys.chipLabel", { action: full, key: none ? lexia.t("key.none") : k.key }));
    });

    // The card of the selected key: its name, the read-aloud button and what it does (one paragraph per line).
    const sel = selected;
    const play = $("key-play");
    $("key-detail-title").textContent = sel ? lexia.t("window.keys." + sel) : "";
    const readAloud = lexia.tf("window.keys.readAloud", { action: sel ? lexia.t("window.keys." + sel) : "" });
    play.setAttribute("aria-label", readAloud);
    play.title = readAloud;
    const body = $("key-detail-text");
    body.textContent = "";
    if (sel) {
      for (const line of lexia.t("window.keys." + sel + "Desc").split("\n")) {
        const p = document.createElement("p");
        p.textContent = line;
        body.appendChild(p);
      }
    }
    $("key-hint").textContent = capturing ? lexia.t("window.keys.captureHint") : "";
  }

  // ---- key capture -----------------------------------------------------------------------
  function startCapture(action) {
    if (capturing === action) return;
    if (capturing) endCapture(true);
    capturing = action;
    selected = action;
    lexia.send("captureKey", { action: action });
    if (state) renderKeys();
  }

  function endCapture(sendCancel) {
    const action = capturing;
    capturing = null;
    if (sendCancel && action) lexia.send("cancelKeyCapture", { action: action });
    if (state) renderKeys();
  }

  // A press anywhere outside the chip that waits ends the capture (a click on the chip itself does not).
  document.addEventListener(
    "pointerdown",
    function (e) {
      if (!capturing) return;
      const row = rows[capturing];
      if (row && row.chip.contains(e.target)) return;
      endCapture(true);
    },
    true
  );

  // The play button: the host reads the card's text aloud (the page sends only which key, never text).
  $("key-play").addEventListener("click", function () {
    if (selected) lexia.send("readKeyCard", { action: selected });
  });

  // The name of a main key as settings.json writes it, or null when it is not one Lexia can use.
  const NAMED = {
    " ": "Space", Enter: "Enter", Insert: "Insert", Delete: "Delete", Home: "Home", End: "End",
    PageUp: "PageUp", PageDown: "PageDown", ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left",
    ArrowRight: "Right", Pause: "Pause", ScrollLock: "ScrollLock", PrintScreen: "PrintScreen",
  };
  function keyName(e) {
    if (/^F([1-9]|1\d|2[0-4])$/.test(e.key)) return e.key;
    if (Object.prototype.hasOwnProperty.call(NAMED, e.key)) return NAMED[e.key];
    if (e.key.length === 1 && /[a-z0-9]/i.test(e.key)) return e.key.toUpperCase();
    if (/^Digit\d$/.test(e.code)) return e.code.slice(5);
    if (/^Key[A-Z]$/.test(e.code)) return e.code.slice(3);
    return null;
  }

  document.addEventListener(
    "keydown",
    function (e) {
      if (!capturing) return;
      if (e.key === "Tab") {
        endCapture(true); // the user is moving on: the focus leaves, and the key stays as it was
        return;
      }
      if (e.key === "Escape") {
        e.preventDefault();
        e.stopPropagation();
        const action = capturing;
        endCapture(true);
        if (rows[action]) rows[action].chip.focus();
        return;
      }
      if (e.key === "Control" || e.key === "Shift" || e.key === "Alt" || e.key === "Meta") return; // wait for the main key
      e.preventDefault();
      e.stopPropagation();
      if (e.repeat) return;
      const main = keyName(e);
      if (!main) return; // not a key Lexia can use: keep waiting
      const mods = [];
      if (e.ctrlKey) mods.push("Ctrl");
      if (e.altKey) mods.push("Alt");
      if (e.shiftKey) mods.push("Shift");
      if (e.metaKey) mods.push("Windows");
      const action = capturing;
      capturing = null;
      lexia.send("setKey", { action: action, key: mods.concat(main).join("+") });
      renderKeys(); // the chip shows the key of the state, until the host answers
      if (rows[action]) rows[action].chip.focus();
    },
    true
  );

  // ---- the five switches -----------------------------------------------------------------
  function renderSwitch(button, on) {
    button.setAttribute("aria-checked", on ? "true" : "false");
  }

  $("toggle-startup").addEventListener("click", function () {
    if (state) lexia.send("setRunAtStartup", { on: state.runAtStartup !== true });
  });
  $("toggle-minimised").addEventListener("click", function () {
    // aria-disabled, not disabled: the switch keeps the focus and a screen reader still finds it.
    if (state && state.runAtStartup === true) lexia.send("setStartMinimised", { on: state.startMinimised !== true });
  });
  $("toggle-admin").addEventListener("click", function () {
    if (state) lexia.send("setRunAsAdmin", { on: state.runAsAdmin !== true });
  });
  $("toggle-dark").addEventListener("click", function () {
    // The switch shows the theme in use (`dark`); it sends the other one.
    if (state) lexia.send("setTheme", { theme: state.dark === true ? "light" : "dark" });
  });
  $("toggle-left").addEventListener("click", function () {
    if (state) lexia.send("setLeftHanded", { on: state.leftHanded !== true });
  });

  // ---- drawing the state -----------------------------------------------------------------
  function render() {
    if (!state || !lexia.i18n.ready) return;
    renderKeys();
    renderSwitch($("toggle-startup"), state.runAtStartup === true);
    renderSwitch($("toggle-minimised"), state.startMinimised === true);
    if (state.runAtStartup === true) $("toggle-minimised").removeAttribute("aria-disabled");
    else $("toggle-minimised").setAttribute("aria-disabled", "true");
    renderSwitch($("toggle-admin"), state.runAsAdmin === true);
    renderSwitch($("toggle-dark"), state.dark === true);
    renderSwitch($("toggle-left"), state.leftHanded === true);

    if (startUp) {
      const first = startUp;
      startUp = null;
      if (first.select && rows[first.select]) selected = first.select;
      if (first.capture && rows[first.capture]) startCapture(first.capture);
      else renderKeys();
    }
  }

  lexia.on("state", function (msg) {
    state = msg;
    render();
  });
  lexia.on("strings", render);
})();
