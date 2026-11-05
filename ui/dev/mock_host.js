// The mock host: stands in for the C++ host in a browser, with sample data. Not used inside
// WebView2 (the real host is there), and not used by the tests (they play the host themselves).
//
// Switch in the URL, for screenshots and checks:
//   ?theme=system|light|dark       the stored theme; `dark` (the theme in use) follows it, "system" follows Windows
//   ?state=on|off                  Lexia On or Off (default off)
//   ?page=home|voices|settings|report|help|about   read by the page itself (?tab= is an older name)
//   ?startup=on  ?admin=on  ?left=on   run at startup / run as admin / left-handed On (default Off)
//   ?mv=<0..100>                   Master volume (default 50, so the thumb shows in the screenshots)
//   ?select=speech  ?capture=read  read by the page itself (js/settings.js)
//   ?hover=<id>                    shows the hover look of the element with that id (power, add-download, ...),
//                                  or of a control's slider (speed, pitch, volume), for a screenshot
//   ?voices=none                   no Voice installed
//   ?active=<Voice>                the Voice that speaks (en_GB-alba-medium is one a person added, so it can be deleted)
//   ?speed=<n> ?pitch=<n> ?volume=<n>   the value of a Voice control (default: the default, so no reset shows)
//   ?sheet=catalogue|downloading|done|failed|offline|added|refused|delete|deleteFailed|presetSaved|presetLoaded|presetFailed
//          &problem=<code>  &net=off  &file=<code>   the Voice library sheets and the result sheet
//                                  (see the sections below)
// Strings come from dev/strings.js (window.LEXIA_DEV_STRINGS), which scripts/ui_preview.ps1 writes
// from `lexia_cli --strings-json en`, so the C++ table stays the only source of text.
(function () {
  "use strict";

  const lexia = window.lexia;
  if ((window.chrome && window.chrome.webview) || !lexia || !lexia.bridge) return;

  const params = new URLSearchParams(location.search);
  const theme = ["system", "light", "dark"].indexOf(params.get("theme")) >= 0 ? params.get("theme") : "light";
  const effectiveDark = function (t) {
    return t === "dark" || (t === "system" && window.matchMedia("(prefers-color-scheme: dark)").matches);
  };
  const number = function (name, fallback) {
    const v = parseFloat(params.get(name));
    return isFinite(v) ? v : fallback;
  };

  const state = {
    type: "state",
    on: params.get("state") === "on",
    language: "eng",
    theme: theme,
    dark: effectiveDark(theme),
    version: "0.5.0",
    runAtStartup: params.get("startup") === "on",
    runAsAdmin: params.get("admin") === "on",
    elevated: false,
    leftHanded: params.get("left") === "on",
    masterVolume: { value: number("mv", 50), min: 0, max: 100, def: 100, step: 5 },
    startMinimised: true,
    keys: [
      { action: "read", key: "F8" },
      { action: "pause", key: "F7" },
      { action: "speech", key: "F6" },
    ],
  };

  if (!window.LEXIA_DEV_STRINGS) {
    console.warn("Lexia mock host: dev/strings.js is missing, so ids show instead of text. Run scripts\\ui_preview.ps1.");
  }

  // ?hover=<id>: a hover look that a screenshot can show (the page has no real pointer there).
  const hoverId = params.get("hover");
  if (hoverId) {
    window.addEventListener("load", function () {
      const target =
        document.getElementById(hoverId) ||
        document.querySelector('[data-control="' + hoverId + '"] .slider') ||
        document.querySelector('[data-page="' + hoverId + '"]');
      if (target) target.setAttribute("data-hover", "");
    });
  }

  // ==== voices page ==================================================================================
  // The Voices and their Voice controls, as the C++ host keeps them (voice_controls.h). The ranges and
  // defaults are RangeOf(); the steps are the page's own. `removable` = the Voices a person added (not shipped).
  const RANGES = {
    speed: { min: 0.5, max: 2, def: 1, step: 0.05 },
    pitch: { min: -6, max: 6, def: 0, step: 0.5 },
    volume: { min: 0, max: 2, def: 1, step: 0.05 },
  };
  const initial = function () {
    return { speed: number("speed", 1), pitch: number("pitch", 0), volume: number("volume", 1) };
  };
  const installedVoices = params.get("voices") === "none" ? [] : ["en_US-ljspeech-medium", "en_GB-alba-medium"];
  const userVoices = ["en_GB-alba-medium"]; // not shipped
  const voiceModel = {
    active: { eng: installedVoices[0] || "" },
    controls: {}, // by Voice name
  };
  if (installedVoices.indexOf(params.get("active")) >= 0) voiceModel.active.eng = params.get("active"); // ?active=<Voice>
  installedVoices.forEach(function (name, i) {
    voiceModel.controls[name] = i === 0 ? initial() : { speed: 1, pitch: 0, volume: 1 };
  });
  const clampControl = function (name, value) {
    const r = RANGES[name];
    return typeof value === "number" && isFinite(value) ? Math.min(r.max, Math.max(r.min, value)) : r.def;
  };
  const activeControls = function () {
    return voiceModel.controls[voiceModel.active[state.language]] || null;
  };
  // Adds voices and controls to the state before it is sent.
  function refreshVoiceState() {
    state.voices = {
      eng: {
        installed: installedVoices.slice(),
        active: voiceModel.active.eng,
        removable: installedVoices.filter(function (n) { return userVoices.indexOf(n) >= 0; }),
      },
    };
    const c = activeControls();
    if (!c) {
      delete state.controls;
      return;
    }
    state.controls = {};
    Object.keys(RANGES).forEach(function (name) {
      state.controls[name] = Object.assign({ value: c[name] }, RANGES[name]);
    });
  }
  // The preset files: the real host opens a file dialog. Here a save or a load works at once, or fails with
  // ?preset=badFile|cannotWrite|noVoice.
  function presetMessage(msg) {
    const fail = params.get("preset");
    if (msg.type === "savePresetFile" || msg.type === "loadPresetFile") {
      if (fail) {
        later({ type: "presetFailed", problem: fail }, 300);
      } else if (msg.type === "savePresetFile") {
        later({ type: "presetSaved", name: "Slow reading.json" }, 300);
      } else {
        const c = activeControls();
        if (c) {
          c.speed = 0.8;
          c.pitch = 1.5;
          c.volume = 1.2;
        }
        later({ type: "presetLoaded", name: "Slow reading.json" }, 300);
        later(null, 320);
      }
      return true;
    }
    return false;
  }
  // The page -> host messages of the voices page. True when handled (the state is then sent).
  function voiceMessage(msg) {
    const c = activeControls();
    switch (msg.type) {
      case "setVoice":
        if (msg.language === "eng" && installedVoices.indexOf(msg.voice) >= 0) voiceModel.active.eng = msg.voice;
        return true;
      case "setControl":
        if (c && RANGES[msg.name]) c[msg.name] = clampControl(msg.name, msg.value);
        return true;
      case "resetControl":
        if (c && RANGES[msg.name]) c[msg.name] = RANGES[msg.name].def;
        return true;
      case "deleteVoice": {
        const name = msg.voice;
        if (params.get("delete") === "fail") {
          later({ type: "voiceDeleteFailed", voice: name, problem: "cannot-write" }, 300);
          return false;
        }
        if (userVoices.indexOf(name) < 0 || installedVoices.indexOf(name) < 0) {
          later({ type: "voiceDeleteFailed", voice: name, problem: "shipped" }, 300);
          return false;
        }
        installedVoices.splice(installedVoices.indexOf(name), 1);
        delete voiceModel.controls[name];
        if (voiceModel.active.eng === name) voiceModel.active.eng = installedVoices[0] || "";
        return true;
      }
    }
    return null; // not a voices page message
  }
  // ==== end of the voices page =======================================================================

  // ==== Voice library sheets ===========================================================================
  // The catalogue, downloads and the Add from file dialog, as the C++ host will do them (the messages
  // are described in js/voice_library.js). Timers make it feel real: the catalogue comes after
  // 0.6 s and a download runs for about 4 s. A screenshot must not depend on timers, so
  //   ?sheet=catalogue|downloading|done|failed|offline|added|refused|delete|deleteFailed|presetSaved|presetLoaded|presetFailed
  // shows one fixed state (no timers; the sheet's buttons do nothing more), with
  //   &problem=<code>   the ProblemId of failed / offline / refused (default network / network / unsupported-language)
  // Other switches: ?net=off  the catalogue and every download fail with "network" (try Try again);
  //                 ?file=<code>  Add from file is refused with that code (else it works).
  const MiB = 1024 * 1024;
  const CATALOGUE = [
    { name: "en_GB-alba-medium", language: "eng", sizeBytes: 63 * MiB },
    { name: "en_GB-jenny_dioco-medium", language: "eng", sizeBytes: 63 * MiB },
    { name: "en_GB-northern_english_male-medium", language: "eng", sizeBytes: 63 * MiB },
    { name: "en_US-amy-medium", language: "eng", sizeBytes: 63 * MiB },
    { name: "en_US-lessac-medium", language: "eng", sizeBytes: 63 * MiB },
    { name: "en_US-ljspeech-medium", language: "eng", sizeBytes: 63 * MiB },
  ];
  const sheetMode = params.get("sheet");
  let libraryTimer = null;
  let libraryDownload = null;
  const later = function (msg, ms) {
    setTimeout(function () {
      if (msg) lexia.bridge.receive(msg);
      else sendState(); // a null message: the state, after a change (a loaded preset)
    }, ms);
  };
  const catalogueMessage = function () {
    return {
      type: "catalogue",
      voices: CATALOGUE.map(function (v) {
        return Object.assign({ installed: installedVoices.indexOf(v.name) >= 0 }, v);
      }),
    };
  };
  const catalogueByName = function (name) {
    return CATALOGUE.filter(function (v) { return v.name === name; })[0];
  };
  function installDownloaded(name) {
    if (installedVoices.indexOf(name) < 0) installedVoices.push(name);
    if (userVoices.indexOf(name) < 0) userVoices.push(name);
    if (!voiceModel.controls[name]) voiceModel.controls[name] = { speed: 1, pitch: 0, volume: 1 };
  }
  // The fixed states, sent once when the page says ready.
  function librarySheetStartup() {
    const amy = "en_US-amy-medium";
    switch (sheetMode) {
      case "catalogue":
        toPage(catalogueMessage());
        break;
      case "downloading":
        toPage(catalogueMessage());
        toPage({ type: "downloadProgress", name: amy, done: 12 * MiB, total: 63 * MiB });
        break;
      case "done":
        installDownloaded(amy);
        sendState();
        toPage(catalogueMessage());
        toPage({ type: "downloadDone", name: amy });
        break;
      case "failed":
        toPage(catalogueMessage());
        toPage({ type: "downloadFailed", name: amy, problem: params.get("problem") || "network", language: "German", quality: "low" });
        break;
      case "offline":
        toPage({ type: "catalogueFailed", problem: params.get("problem") || "network" });
        break;
      case "added":
        toPage({ type: "voiceAdded", voice: "en_US-amy-medium", language: "eng" });
        break;
      case "refused":
        toPage({ type: "voiceRefused", problem: params.get("problem") || "unsupported-language", language: "German", quality: "low" });
        break;
      case "deleteFailed":
        toPage({ type: "voiceDeleteFailed", voice: "en_GB-alba-medium", problem: params.get("problem") || "cannot-write" });
        break;
      case "presetSaved":
        toPage({ type: "presetSaved", name: "Slow reading.json" });
        break;
      case "presetLoaded":
        toPage({ type: "presetLoaded", name: "Slow reading.json" });
        break;
      case "presetFailed":
        toPage({ type: "presetFailed", problem: params.get("problem") || "badFile" });
        break;
      case "delete":
        // the confirmation is the page's own: choose the user's Voice, then press Delete
        voiceModel.active.eng = "en_GB-alba-medium";
        sendState();
        setTimeout(function () {
          const del = document.getElementById("voice-delete");
          if (del) del.click();
        }, 60);
        break;
    }
  }
  // The page -> host messages of the sheets. True when handled.
  function libraryMessage(msg) {
    if (["openCatalogue", "download", "cancelDownload", "addFromFile"].indexOf(msg.type) < 0) return false;
    if (sheetMode) {
      console.info("Lexia mock host: " + msg.type + " (a fixed sheet state is shown, ?sheet=" + sheetMode + ")", msg);
      return true;
    }
    const offline = params.get("net") === "off";
    switch (msg.type) {
      case "openCatalogue":
        later(offline ? { type: "catalogueFailed", problem: "network" } : catalogueMessage(), 600);
        break;
      case "download": {
        const v = catalogueByName(msg.name);
        if (!v || libraryDownload) break;
        if (offline) {
          later({ type: "downloadFailed", name: msg.name, problem: "network" }, 700);
          break;
        }
        libraryDownload = { name: msg.name, done: 0, total: v.sizeBytes };
        libraryTimer = setInterval(function () {
          libraryDownload.done = Math.min(libraryDownload.total, libraryDownload.done + libraryDownload.total / 16);
          lexia.bridge.receive({ type: "downloadProgress", name: msg.name, done: libraryDownload.done, total: libraryDownload.total });
          if (libraryDownload.done >= libraryDownload.total) {
            clearInterval(libraryTimer);
            libraryDownload = null;
            installDownloaded(msg.name);
            lexia.bridge.receive({ type: "downloadDone", name: msg.name });
            sendState();
          }
        }, 250);
        break;
      }
      case "cancelDownload":
        if (libraryDownload && libraryDownload.name === msg.name) {
          clearInterval(libraryTimer);
          libraryDownload = null;
          later({ type: "downloadFailed", name: msg.name, problem: "cancelled" }, 150);
        }
        break;
      case "addFromFile":
        // The real host opens the native file dialog here.
        if (params.get("file")) later({ type: "voiceRefused", problem: params.get("file"), language: "German", quality: "low" }, 400);
        else later({ type: "voiceAdded", voice: "en_US-amy-medium", language: "eng" }, 400);
        break;
    }
    return true;
  }
  // ==== end of the Voice library sheets ================================================================
  // The host answers a bit later, as the real one does.
  function toPage(msg) {
    setTimeout(function () {
      lexia.bridge.receive(msg);
    }, 0);
  }
  function sendState() {
    refreshVoiceState();
    toPage(JSON.parse(JSON.stringify(state)));
  }

  lexia.bridge.setTransport(function (msg) {
    if (libraryMessage(msg)) return;
    if (presetMessage(msg)) return;
    const voiceHandled = voiceMessage(msg);
    if (voiceHandled !== null) {
      if (voiceHandled) sendState();
      return;
    }
    switch (msg.type) {
      case "ready":
        toPage({ type: "strings", language: "en", strings: window.LEXIA_DEV_STRINGS || {} });
        sendState();
        librarySheetStartup();
        break;
      case "setOn":
        state.on = msg.on === true;
        sendState();
        break;
      case "setMasterVolume":
        if (typeof msg.value === "number" && isFinite(msg.value)) state.masterVolume.value = Math.min(100, Math.max(0, Math.round(msg.value)));
        sendState();
        break;
      case "setStartMinimised":
        state.startMinimised = msg.on === true;
        sendState();
        break;
      case "setRunAtStartup":
        state.runAtStartup = msg.on === true;
        sendState();
        break;
      case "setRunAsAdmin":
        state.runAsAdmin = msg.on === true;
        sendState();
        break;
      case "setLeftHanded":
        state.leftHanded = msg.on === true;
        sendState();
        break;
      case "setTheme":
        if (msg.theme === "light" || msg.theme === "dark") {
          state.theme = msg.theme;
          state.dark = msg.theme === "dark";
          sendState();
        }
        break;
      case "setKey": {
        // The real host checks the key (D6). Here: no two actions on one key; a refused key leaves the state as it was.
        const taken = state.keys.some(function (k) { return k.action !== msg.action && k.key === msg.key; });
        const mine = state.keys.filter(function (k) { return k.action === msg.action; })[0];
        if (mine && !taken && typeof msg.key === "string") mine.key = msg.key;
        else console.info("Lexia mock host: the key was refused", msg);
        sendState();
        break;
      }
      case "captureKey":
      case "cancelKeyCapture":
        console.info("Lexia mock host: " + msg.type + " (the real host stops the Lexia keys from acting while a key is chosen)", msg);
        break;
      case "openLink":
        console.info("Lexia mock host: openLink " + msg.link + " (the real host opens the fixed address of that name)");
        break;
      case "openNotices":
        console.info("Lexia mock host: openNotices (the real host opens THIRD-PARTY-NOTICES.txt)");
        break;
      default:
        console.info("Lexia mock host: no answer for '" + msg.type + "' yet", msg);
    }
  });
})();
