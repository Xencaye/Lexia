// Bridge tests for the page, with a tiny runner (no dependency). Loaded by dev_host.js for
// index.html?test. A test plays the host: it sends host -> page messages through
// lexia.bridge.receive, then checks only what a person sees (visible text, accessible names, which
// page is shown) and the page -> host messages the page sends. It does not look at CSS classes or the
// page's own functions.
//
// This file: the runner, the frame (links, theme, Master volume), Home and the voices page. The other
// pages and the sheets are in tests_settings.js and tests_voice_library.js (they use window.lexiaTests).
//
// Result: the page title is "PASS <n>" or "FAIL <k> of <n>", and a list is added to the page.
// ctest (ui_bridge) opens this page in headless Edge and looks for "PASS" in the title.
(function () {
  "use strict";

  const lexia = window.lexia;

  // ---- play the host --------------------------------------------------------------------
  const sent = []; // page -> host messages, in order
  lexia.bridge.setTransport(function (msg) {
    sent.push(JSON.parse(JSON.stringify(msg)));
  });

  const warnings = [];
  const realWarn = console.warn;
  console.warn = function () {
    warnings.push(Array.prototype.join.call(arguments, " "));
    realWarn.apply(console, arguments);
  };

  // The test brings its own small set of words, so it needs neither lexia_cli nor the preview.
  const STRINGS = {
    "window.nav.main": "Pages",
    "window.nav.more": "More",
    "window.nav.home": "home",
    "window.nav.voices": "voices",
    "window.nav.settings": "settings",
    "window.nav.report": "report",
    "window.nav.help": "help",
    "window.nav.about": "about",
    "window.volume": "Lexia volume",
    "window.volumeValue": "{value}%",
    "window.home.power": "Lexia On / Off",
    "window.home.enable": "enable",
    "window.home.disable": "disable",
    "language.eng": "English",
  };

  // The words of the voices page.
  Object.assign(STRINGS, {
    "window.voice.voice": "voice",
    "window.voice.none": "No voice is installed for {language}. Download one, or add one from a file.",
    "window.voice.download": "Download a voice",
    "window.voice.addFromFile": "Add a voice from a file",
    "window.voice.delete": "Delete this voice",
    "window.voice.deleteShipped": "This voice comes with Lexia. You cannot delete it.",
    "window.voice.controls": "Voice controls",
    "window.voice.speed": "speed",
    "window.voice.pitch": "pitch",
    "window.voice.volume": "volume",
    "window.voice.speedValue": "{value}×",
    "window.voice.pitchValue": "{value} semitones",
    "window.voice.pitchValueOne": "{value} semitone",
    "window.voice.percentValue": "{value}%",
    "window.voice.resetFor": "Reset {control}",
    "window.voice.savePreset": "save preset",
    "window.voice.loadPreset": "load preset",
    "window.deleteVoice.title": "Delete {voice}?",
    "window.deleteVoice.text": "Lexia removes the voice files from this computer.",
    "window.deleteVoice.confirm": "Delete",
    "window.deleteVoice.cancel": "Keep it",
  });

  function host(msg) {
    lexia.bridge.receive(msg);
  }
  function sendStrings(strings) {
    host({ type: "strings", language: "en", strings: strings || STRINGS });
  }
  function sendState(over) {
    host(
      Object.assign(
        {
          type: "state",
          on: false,
          language: "eng",
          theme: "light",
          dark: false,
          masterVolume: { value: 50, min: 0, max: 100, def: 100, step: 5 },
        },
        over
      )
    );
  }

  // ---- what a person sees ----------------------------------------------------------------
  function accessibleName(el) {
    return (el.getAttribute("aria-label") || el.textContent).replace(/\s+/g, " ").trim();
  }
  // A button by its name; fails if there is none, or it is not shown.
  function control(name) {
    const found = Array.prototype.filter.call(document.querySelectorAll("button"), function (el) {
      return accessibleName(el) === name && el.getClientRects().length > 0;
    });
    if (found.length !== 1) throw new Error("expected one shown control named '" + name + "', found " + found.length);
    return found[0];
  }
  function screenText() {
    return document.body.innerText; // hidden elements are not in it
  }
  function shows(text) {
    return screenText().split("\n").some(function (line) {
      return line.trim() === text;
    });
  }
  // The pages that are shown now, by their name in the links ("home", "voices", ...).
  function shownPages() {
    return Array.prototype.filter
      .call(document.querySelectorAll("section.page"), function (el) {
        return el.getClientRects().length > 0;
      })
      .map(function (el) {
        return el.dataset.page;
      });
  }
  function sentOfType(type) {
    return sent.filter(function (m) {
      return m.type === type;
    });
  }
  function background() {
    return getComputedStyle(document.body).backgroundColor;
  }

  // ---- runner ----------------------------------------------------------------------------
  const tests = [];
  function test(name, fn) {
    tests.push({ name: name, fn: fn });
  }
  function assert(cond, message) {
    if (!cond) throw new Error(message || "assertion failed");
  }
  function equal(actual, expected, what) {
    if (JSON.stringify(actual) !== JSON.stringify(expected)) {
      throw new Error((what || "value") + ": expected " + JSON.stringify(expected) + ", got " + JSON.stringify(actual));
    }
  }

  async function run() {
    const failures = [];
    const lines = [];
    for (const t of tests) {
      // A sheet that a test left open (a modal dialog) would make the page behind it inert for the next test.
      for (const d of document.querySelectorAll("dialog[open]")) d.close();
      try {
        await t.fn();
        lines.push("pass  " + t.name);
      } catch (e) {
        failures.push(t.name + ": " + e.message);
        lines.push("FAIL  " + t.name + ": " + e.message);
      }
    }
    document.title = failures.length ? "FAIL " + failures.length + " of " + tests.length : "PASS " + tests.length;
    const out = document.createElement("pre");
    out.id = "test-result";
    out.textContent = lines.join("\n");
    document.body.appendChild(out);
  }

  // ---- the tests: the frame and Home -----------------------------------------------------
  // The page starts first, before any host message: it says "ready" and the power button cannot be pressed.
  test("the page says ready, and Lexia cannot be switched before the first state", function () {
    equal(sent.length > 0 && sent[0].type, "ready", "first message");
    sendStrings();
    assert(control("Lexia On / Off").disabled, "the power button should be disabled until a state arrives");
  });

  test("On or Off: the button word and the pressed state follow the state", function () {
    sendState({ on: false });
    equal(control("Lexia On / Off").getAttribute("aria-pressed"), "false", "pressed when Off");
    assert(shows("enable") && !shows("disable"), "the word enable, and not disable");
    sendState({ on: true });
    equal(control("Lexia On / Off").getAttribute("aria-pressed"), "true", "pressed when On");
    assert(shows("disable") && !shows("enable"), "the word disable, and not enable");
  });

  test("a click on the power circle or on the button under it sends setOn with the opposite value", function () {
    sendState({ on: false });
    sent.length = 0;
    control("Lexia On / Off").click();
    control("enable").click();
    equal(sent, [{ type: "setOn", on: true }, { type: "setOn", on: true }], "messages after a click when Off");
    sendState({ on: true });
    sent.length = 0;
    control("Lexia On / Off").click();
    control("disable").click();
    equal(sent, [{ type: "setOn", on: false }, { type: "setOn", on: false }], "messages after a click when On");
  });

  test("the screen does not change until the next state", function () {
    sendState({ on: false });
    control("Lexia On / Off").click();
    equal(control("Lexia On / Off").getAttribute("aria-pressed"), "false", "pressed after the click");
    assert(shows("enable"), "still the word enable after the click");
    sendState({ on: true });
    equal(control("Lexia On / Off").getAttribute("aria-pressed"), "true", "pressed after the state");
    assert(shows("disable"), "the word disable after the state");
  });

  // ---- the six pages ---------------------------------------------------------------------
  const PAGES = ["home", "voices", "settings", "report", "help", "about"];
  test("the six links show their own page, and the current link is marked", function () {
    sendState({});
    control("home").click();
    equal(shownPages(), ["home"], "page at the start");
    for (const name of PAGES) {
      control(name).click();
      equal(shownPages(), [name], "page after choosing " + name);
      equal(control(name).getAttribute("aria-current"), "page", name + " is the current page");
      for (const other of PAGES) {
        if (other !== name) assert(!control(other).hasAttribute("aria-current"), other + " is not the current page on " + name);
      }
    }
    control("home").click();
    equal(shownPages(), ["home"], "page after going back");
  });

  test("the links are navigation, not tabs: two <nav>s of buttons, no tablist", function () {
    equal(document.querySelectorAll("[role=tab], [role=tablist], [role=tabpanel]").length, 0, "tab roles");
    const navs = Array.prototype.map.call(document.querySelectorAll("nav"), function (n) {
      return n.getAttribute("aria-label") + ": " +
        Array.prototype.map.call(n.querySelectorAll("button"), function (b) { return accessibleName(b); }).join(",");
    });
    equal(navs, ["Pages: home,voices,settings", "More: report,help,about"], "the two navs");
  });

  test("no page has a Language control, and nothing the page offers sends setLanguage", function () {
    sendVoiceState({});
    sent.length = 0;
    for (const name of PAGES) {
      control(name).click();
      const panel = document.querySelector("section.page:not([hidden])");
      for (const el of panel.querySelectorAll("select, label, button, [role=slider], [role=switch]")) {
        assert(!/language/i.test(accessibleName(el)), "a control named '" + accessibleName(el) + "' on " + name);
      }
      assert(!/language/i.test(panel.innerText), "the word Language on " + name);
      // Use every button that is on, and look at what is sent. A key chip waits for a key, so Esc ends that.
      for (const b of panel.querySelectorAll("button")) {
        if (b.getClientRects().length > 0 && !b.disabled) b.click();
        document.dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true, cancelable: true }));
        for (const d of document.querySelectorAll("dialog[open]")) d.close();
        control(name).click();
      }
    }
    equal(sentOfType("setLanguage"), [], "setLanguage messages");
    control("home").click();
  });

  test("a missing string shows its id and is reported once", function () {
    const gap = Object.assign({}, STRINGS);
    delete gap["window.home.enable"];
    warnings.length = 0;
    sendStrings(gap);
    sendState({});
    sendState({ on: false });
    control("home").click();
    assert(shows("window.home.enable"), "the id is shown where the text is missing");
    const reported = warnings.filter(function (w) {
      return w.indexOf("window.home.enable") >= 0;
    });
    equal(reported.length, 1, "number of reports");
    sendStrings(); // put the words back
    assert(shows("enable"), "the text is shown again when the table has it");
  });

  // ---- theme ------------------------------------------------------------------------------
  test("the page takes the theme in use (state.dark), not the stored choice", function () {
    sendState({ dark: false, theme: "system" });
    const light = background();
    sendState({ dark: true, theme: "system" });
    assert(background() !== light, "the page background changes when dark is true");
    sendState({ dark: false, theme: "dark" });
    equal(background(), light, "the light theme again when dark is false");
  });

  // ---- Master volume ---------------------------------------------------------------------
  function masterVolume() {
    const found = document.getElementById("master-volume");
    if (!found || found.getAttribute("aria-label") !== "Lexia volume") throw new Error("no Master volume slider");
    return found;
  }
  function press(el, key) {
    el.dispatchEvent(new KeyboardEvent("keydown", { key: key, bubbles: true, cancelable: true }));
  }

  test("Master volume: a slider on every page that shows the state's value", function () {
    sendState({ masterVolume: { value: 40, min: 0, max: 100, def: 100, step: 5 } });
    for (const name of PAGES) {
      control(name).click();
      assert(masterVolume().getClientRects().length > 0, "the slider is shown on " + name);
    }
    equal(masterVolume().getAttribute("aria-valuenow"), "40", "value");
    equal(masterVolume().getAttribute("aria-valuemax"), "100", "maximum");
    equal(masterVolume().getAttribute("aria-valuetext"), "40%", "value in words");
    control("home").click();
  });

  test("Master volume: arrow keys move one step of 5, PageUp ten; Home and End; the slider waits for the next state", function () {
    sendState({ masterVolume: { value: 40, min: 0, max: 100, def: 100, step: 5 } });
    sent.length = 0;
    press(masterVolume(), "ArrowRight");
    press(masterVolume(), "ArrowLeft");
    press(masterVolume(), "PageUp");
    press(masterVolume(), "Home");
    press(masterVolume(), "End");
    equal(
      sent,
      [
        { type: "setMasterVolume", value: 45 },
        { type: "setMasterVolume", value: 35 },
        { type: "setMasterVolume", value: 90 },
        { type: "setMasterVolume", value: 0 },
        { type: "setMasterVolume", value: 100 },
      ],
      "messages"
    );
    equal(masterVolume().getAttribute("aria-valuenow"), "40", "value before the next state");
    sendState({ masterVolume: { value: 100, min: 0, max: 100, def: 100, step: 5 } });
    equal(masterVolume().getAttribute("aria-valuenow"), "100", "value after the state");
    sent.length = 0;
    press(masterVolume(), "ArrowRight");
    equal(sent, [], "a key at the end sends nothing");
  });

  test("Master volume: a press on the track sets the value under the pointer", function () {
    sendState({ masterVolume: { value: 40, min: 0, max: 100, def: 100, step: 5 } });
    sent.length = 0;
    const box = masterVolume().getBoundingClientRect();
    function at(x) {
      masterVolume().dispatchEvent(new PointerEvent("pointerdown", { clientX: x, button: 0, pointerId: 3, bubbles: true }));
      masterVolume().dispatchEvent(new PointerEvent("pointerup", { clientX: x, pointerId: 3, bubbles: true }));
    }
    at(box.left + box.width * 0.75);
    at(box.right + 100);
    at(box.left - 100);
    equal(
      sent,
      [
        { type: "setMasterVolume", value: 75 },
        { type: "setMasterVolume", value: 100 },
        { type: "setMasterVolume", value: 0 },
      ],
      "messages"
    );
  });

  // ==== the voices page =============================================================================
  // The ranges are RangeOf() of voice_controls.h; the steps are the page's own.
  const RANGES = {
    speed: { min: 0.5, max: 2, def: 1, step: 0.05 },
    pitch: { min: -6, max: 6, def: 0, step: 0.5 },
    volume: { min: 0, max: 2, def: 1, step: 0.05 },
  };
  function controlsState(values) {
    const out = {};
    for (const name of Object.keys(RANGES)) {
      out[name] = Object.assign({ value: values && name in values ? values[name] : RANGES[name].def }, RANGES[name]);
    }
    return out;
  }
  const VOICES = ["en_US-ljspeech-medium", "en_GB-alba-medium"];
  // A state with Voices installed: the first is shipped; the second was added by the person (removable).
  function sendVoiceState(over) {
    sendState(
      Object.assign(
        {
          voices: { eng: { installed: VOICES, active: VOICES[0], removable: [VOICES[1]] } },
          controls: controlsState(),
        },
        over
      )
    );
  }
  function openVoices() {
    control("voices").click();
  }
  // The shown slider of a Voice control, by its name.
  function slider(name) {
    const found = Array.prototype.filter.call(document.querySelectorAll("[role=slider]"), function (el) {
      return el.getAttribute("aria-label") === name && el.getClientRects().length > 0;
    });
    if (found.length !== 1) throw new Error("expected one shown slider '" + name + "', found " + found.length);
    return found[0];
  }
  // Where the thumb is, 0 .. 1 along its slider.
  function thumbAt(name) {
    const s = slider(name);
    const box = s.getBoundingClientRect();
    const t = s.querySelector(".slider-thumb").getBoundingClientRect();
    return (t.left + t.width / 2 - box.left) / box.width;
  }
  // The shown select with this label.
  function field(label) {
    const found = Array.prototype.filter.call(document.querySelectorAll("label"), function (el) {
      return accessibleName(el) === label && el.getClientRects().length > 0;
    });
    if (found.length !== 1) throw new Error("expected one shown field labelled '" + label + "', found " + found.length);
    return document.getElementById(found[0].htmlFor);
  }
  function optionsOf(select) {
    return Array.prototype.map.call(select.options, function (o) {
      return o.textContent;
    });
  }

  test("voices: the installed Voices, the active one chosen", function () {
    sendVoiceState({});
    openVoices();
    equal(optionsOf(field("voice")), VOICES, "Voices listed");
    equal(field("voice").value, VOICES[0], "the Voice that speaks");
    assert(!shows("No voice is installed for English. Download one, or add one from a file."), "no 'no voice' note when a Voice is installed");
  });

  test("voices: choosing a Voice sends setVoice, and the selector shows the state until the host answers", function () {
    sendVoiceState({});
    openVoices();
    sent.length = 0;
    field("voice").value = VOICES[1];
    field("voice").dispatchEvent(new Event("change", { bubbles: true }));
    equal(sent, [{ type: "setVoice", language: "eng", voice: VOICES[1] }], "messages after choosing");
    equal(field("voice").value, VOICES[0], "the selector before the new state");
    sendVoiceState({ voices: { eng: { installed: VOICES, active: VOICES[1], removable: [VOICES[1]] } } });
    equal(field("voice").value, VOICES[1], "the selector after the new state");
  });

  test("voices: with no Voice installed the page says so and nothing can be changed", function () {
    sendVoiceState({ voices: { eng: { installed: [], active: "", removable: [] } }, controls: undefined });
    openVoices();
    assert(shows("No voice is installed for English. Download one, or add one from a file."), "the note is shown");
    sent.length = 0;
    press(slider("speed"), "ArrowRight");
    equal(sent, [], "messages after a key on a slider with no Voice");
    assert(control("save preset").disabled && control("load preset").disabled, "save preset and load preset are disabled");
    assert(field("voice").disabled, "the Voice selector is disabled");
    assert(control("Download a voice") && control("Add a voice from a file"), "download and add are still there");
  });

  test("voices: each control shows its value in words (the slider's text and its tooltip)", function () {
    sendVoiceState({ controls: controlsState({ speed: 1.25, pitch: -2, volume: 1.2 }) });
    openVoices();
    equal(slider("speed").getAttribute("aria-valuetext"), "1.25×", "speed as read by a screen reader");
    equal(slider("speed").getAttribute("aria-valuenow"), "1.25", "speed number");
    equal(slider("pitch").getAttribute("aria-valuetext"), "−2 semitones", "pitch");
    equal(slider("volume").getAttribute("aria-valuetext"), "120%", "volume");
    equal(slider("speed").title, "1.25×", "the tooltip of the thumb");
    sendVoiceState({ controls: controlsState({ speed: 1, pitch: 1, volume: 1 }) });
    equal(slider("speed").getAttribute("aria-valuetext"), "1.0×", "speed 1 is 1.0×");
    equal(slider("pitch").getAttribute("aria-valuetext"), "+1 semitone", "one semitone is singular, with a sign");
    sendVoiceState({ controls: controlsState({ pitch: 0 }) });
    equal(slider("pitch").getAttribute("aria-valuetext"), "0 semitones", "no change is 0 semitones");
  });

  test("voices: speed is on a log scale (1.0 is at the centre), pitch and volume are linear", function () {
    sendVoiceState({ controls: controlsState({ speed: 1, pitch: 0, volume: 1 }) });
    openVoices();
    function near(a, b, what) {
      assert(Math.abs(a - b) < 0.01, what + ": expected " + b + ", got " + a);
    }
    near(thumbAt("speed"), 0.5, "speed 1.0");
    near(thumbAt("pitch"), 0.5, "pitch 0");
    near(thumbAt("volume"), 0.5, "volume 100%");
    sendVoiceState({ controls: controlsState({ speed: 2, pitch: 6, volume: 0 }) });
    near(thumbAt("speed"), 1, "speed 2");
    near(thumbAt("pitch"), 1, "pitch 6");
    near(thumbAt("volume"), 0, "volume 0");
    sendVoiceState({ controls: controlsState({ speed: 0.5 }) });
    near(thumbAt("speed"), 0, "speed 0.5");
    sendVoiceState({ controls: controlsState({ speed: 1.5 }) });
    near(thumbAt("speed"), Math.log(3) / Math.log(4), "speed 1.5 is on the log scale (a linear scale would give 0.67)");
  });

  test("voices: arrow keys send setControl by one step, Page keys by ten, Home and End to the ends", function () {
    sendVoiceState({ controls: controlsState({ speed: 1, pitch: 0 }) });
    openVoices();
    sent.length = 0;
    press(slider("speed"), "ArrowRight");
    press(slider("speed"), "ArrowLeft");
    press(slider("speed"), "ArrowUp");
    press(slider("speed"), "PageDown");
    press(slider("pitch"), "ArrowRight");
    press(slider("pitch"), "Home");
    press(slider("pitch"), "End");
    equal(
      sent,
      [
        { type: "setControl", name: "speed", value: 1.05 },
        { type: "setControl", name: "speed", value: 0.95 },
        { type: "setControl", name: "speed", value: 1.05 },
        { type: "setControl", name: "speed", value: 0.5 },
        { type: "setControl", name: "pitch", value: 0.5 },
        { type: "setControl", name: "pitch", value: -6 },
        { type: "setControl", name: "pitch", value: 6 },
      ],
      "messages"
    );
  });

  test("voices: a key at the end of the range sends nothing, and no value is outside the range", function () {
    sendVoiceState({ controls: controlsState({ speed: 2, volume: 0, pitch: -6 }) });
    openVoices();
    sent.length = 0;
    press(slider("speed"), "ArrowRight");
    press(slider("speed"), "End");
    press(slider("volume"), "ArrowLeft");
    press(slider("pitch"), "PageDown");
    equal(sent, [], "messages at the ends");
    // Near the end, a big step is held at the end of the range.
    sendVoiceState({ controls: controlsState({ speed: 1.9, volume: 0.2, pitch: 5 }) });
    sent.length = 0;
    press(slider("speed"), "PageUp");
    press(slider("volume"), "PageDown");
    press(slider("pitch"), "PageUp");
    equal(
      sent.map(function (m) {
        return [m.name, m.value];
      }),
      [["speed", 2], ["volume", 0], ["pitch", 6]],
      "values held in the range"
    );
  });

  test("voices: the slider does not move until the next state", function () {
    sendVoiceState({ controls: controlsState({ speed: 1 }) });
    openVoices();
    press(slider("speed"), "ArrowRight");
    equal(slider("speed").getAttribute("aria-valuenow"), "1", "value after the key");
    sendVoiceState({ controls: controlsState({ speed: 1.05 }) });
    equal(slider("speed").getAttribute("aria-valuenow"), "1.05", "value after the state");
  });

  test("voices: a press on the track sets the value under the pointer (speed on its log scale), within the range", function () {
    sendVoiceState({ controls: controlsState({ speed: 1.5, pitch: 0 }) });
    openVoices();
    sent.length = 0;
    function at(name, p, beyond) {
      const s = slider(name);
      const box = s.getBoundingClientRect();
      const x = beyond ? (p > 0.5 ? box.right + 200 : box.left - 200) : box.left + box.width * p;
      s.dispatchEvent(new PointerEvent("pointerdown", { clientX: x, button: 0, pointerId: 7, bubbles: true }));
      s.dispatchEvent(new PointerEvent("pointerup", { clientX: x, pointerId: 7, bubbles: true }));
    }
    at("speed", 0.5); // the centre of the track is 1.0
    at("speed", 0.75); // sqrt(2) = 1.414, on the step grid 1.4
    at("speed", 1, true);
    at("speed", 0, true);
    at("pitch", 0.75);
    equal(
      sent,
      [
        { type: "setControl", name: "speed", value: 1 },
        { type: "setControl", name: "speed", value: 1.4 },
        { type: "setControl", name: "speed", value: 2 },
        { type: "setControl", name: "speed", value: 0.5 },
        { type: "setControl", name: "pitch", value: 3 },
      ],
      "messages"
    );
  });

  test("voices: reset shows only beside a value that is not the default, and sends resetControl for its own control", function () {
    sendVoiceState({ controls: controlsState({ speed: 1.5, volume: 1 }) });
    openVoices();
    assert(!shows("Reset volume") && Array.prototype.every.call(document.querySelectorAll("button"), function (b) {
      return !(accessibleName(b) === "Reset volume" && b.getClientRects().length > 0);
    }), "no reset beside volume at the default");
    sent.length = 0;
    control("Reset speed").click();
    equal(sent, [{ type: "resetControl", name: "speed" }], "messages after Reset speed");
    sendVoiceState({ controls: controlsState({ speed: 1 }) });
    let again = 0;
    for (const b of document.querySelectorAll("button")) if (accessibleName(b) === "Reset speed" && b.getClientRects().length > 0) again++;
    equal(again, 0, "Reset speed is gone after the state shows the default");
    sendVoiceState({ controls: controlsState({ pitch: -1 }) });
    assert(control("Reset pitch"), "a changed pitch shows its reset");
  });

  test("voices: download and add from a file send openCatalogue and addFromFile", function () {
    sendVoiceState({});
    openVoices();
    sent.length = 0;
    control("Download a voice").click();
    control("Add a voice from a file").click();
    equal(sent, [{ type: "openCatalogue" }, { type: "addFromFile" }], "messages");
  });

  test("voices: delete is for a Voice the person added: a tooltip says why it is off for a shipped Voice", function () {
    sendVoiceState({});
    openVoices();
    equal(control("Delete this voice").getAttribute("aria-disabled"), "true", "off for the shipped Voice");
    equal(control("Delete this voice").title, "This voice comes with Lexia. You cannot delete it.", "its tooltip");
    sent.length = 0;
    control("Delete this voice").click();
    assert(!document.getElementById("delete-voice").open, "no confirmation for a shipped Voice");
    equal(sent, [], "messages");
    sendVoiceState({ voices: { eng: { installed: VOICES, active: VOICES[1], removable: [VOICES[1]] } } });
    equal(control("Delete this voice").getAttribute("aria-disabled"), "false", "on for a Voice the person added");
    equal(control("Delete this voice").title, "Delete this voice", "the tooltip of a button that works");
  });

  test("voices: delete asks first; Delete sends deleteVoice with the name, Keep it sends nothing", function () {
    sendVoiceState({ voices: { eng: { installed: VOICES, active: VOICES[1], removable: [VOICES[1]] } } });
    openVoices();
    const opener = control("Delete this voice");
    opener.focus();
    sent.length = 0;
    opener.click();
    assert(document.getElementById("delete-voice").open, "the confirmation is open");
    assert(shows("Delete en_GB-alba-medium?"), "it names the Voice");
    equal(sent, [], "asking sends nothing");
    control("Keep it").click();
    assert(!document.getElementById("delete-voice").open, "closed by Keep it");
    equal(sent, [], "Keep it sends nothing");
    equal(document.activeElement, opener, "the focus goes back to the delete button");
    opener.click();
    control("Delete").click();
    equal(sent, [{ type: "deleteVoice", voice: VOICES[1] }], "messages after Delete");
    assert(!document.getElementById("delete-voice").open, "closed by Delete");
    opener.click();
    document.getElementById("delete-voice").dispatchEvent(new KeyboardEvent("keydown", { key: "Escape", bubbles: true, cancelable: true }));
    assert(!document.getElementById("delete-voice").open, "closed by Esc");
    equal(sent.length, 1, "Esc sends nothing");
  });

  test("voices: save preset and load preset send savePresetFile and loadPresetFile", function () {
    sendVoiceState({});
    openVoices();
    sent.length = 0;
    control("save preset").click();
    control("load preset").click();
    equal(sent, [{ type: "savePresetFile" }, { type: "loadPresetFile" }], "messages");
  });

  test("voices: there is no preset list, no preset name field and no expressiveness control", function () {
    sendVoiceState({});
    openVoices();
    equal(document.querySelectorAll("#page-voices select").length, 1, "one selector (the Voice)");
    equal(document.querySelectorAll("#page-voices input").length, 0, "no text field");
    equal(document.querySelectorAll("#page-voices [role=slider]").length, 3, "three sliders");
    assert(!/expressiveness/i.test(document.getElementById("page-voices").textContent), "no expressiveness");
  });

  test("voices: a state with no Voice data does not break the page", function () {
    sendState({});
    openVoices();
    assert(control("save preset").disabled, "the page is drawn");
    control("home").click();
  });

  // The runner and the helpers, for the test files of the other pages (dev/tests_settings.js, ...). They
  // load after this file and add their tests to the same list.
  window.lexiaTests = {
    test: test, assert: assert, equal: equal, STRINGS: STRINGS, host: host, sendStrings: sendStrings,
    sendState: sendState, control: control, shows: shows, screenText: screenText, shownPages: shownPages,
    sentOfType: sentOfType, accessibleName: accessibleName, sent: sent, background: background,
    sendVoiceState: sendVoiceState, controlsState: controlsState, VOICES: VOICES,
  };

  window.addEventListener("load", function () {
    run();
  });
})();
