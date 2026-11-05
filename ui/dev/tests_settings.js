// Bridge tests for the settings page (keys, four switches), and the report, help and about pages.
// The runner and the helpers are in dev/tests.js (window.lexiaTests); like the rest, these tests play
// the host and check only what a person sees and the messages the page sends.
(function () {
  "use strict";

  const T = window.lexiaTests;
  const test = T.test, assert = T.assert, equal = T.equal;
  const shows = T.shows, control = T.control, sent = T.sent;

  // The words of these pages (the real ones come from the C++ table).
  Object.assign(T.STRINGS, {
    "key.none": "(no key)",
    "window.keys.title": "keybinds",
    "window.keys.readRow": "read",
    "window.keys.pauseRow": "pause",
    "window.keys.speechRow": "speech",
    "window.keys.read": "Read key",
    "window.keys.pause": "Pause key",
    "window.keys.speech": "Speech key",
    "window.keys.readDesc": "Freezes the screen.\nClick or drag on text to hear it.\nPress again to leave.",
    "window.keys.pauseDesc": "Pauses what Lexia reads.",
    "window.keys.speechDesc": "Tap once to skip. Tap three times quickly to stop.",
    "window.keys.readAloud": "Read aloud what the {action} does",
    "window.keys.chipLabel": "{action}: {key}. Select to choose a new key.",
    "window.keys.capturing": "Press a key",
    "window.keys.captureHint": "Press the new key. Esc cancels.",
    "window.settings.misc": "misc",
    "window.settings.runAtStartup": "run at startup",
    "window.settings.startMinimised": "start minimized",
    "window.settings.runAsAdmin": "run as admin",
    "window.settings.darkMode": "dark mode",
    "window.settings.leftHanded": "left-handed",
    "window.report.title": "Found a bug?\nWant to request a feature?",
    "window.report.text": "Get in touch, I'll respond soon-ish!",
    "window.report.mail": "mail",
    "window.report.discord": "discord",
    "window.report.github": "github",
    "window.help.title": "Still working on this part",
    "window.help.soon": "Coming soon!",
    "window.about.name": "Lexia",
    "window.about.version": "ver. {version}",
    "window.about.intro": "A simple screen-reader built with {games} and {comics} in mind.",
    "window.about.games": "video games",
    "window.about.comics": "comics",
    "window.about.hope": "I hope you find this app useful.",
    "window.about.local": "The entire process runs locally.",
    "window.about.human": "The idea is human, the code is not.",
    "window.about.thanks": "With special thanks to my little sister.",
    "window.about.itch": "Lexia on itch.io",
    "window.about.github": "Lexia on GitHub",
    "window.about.licence": "Lexia is free to use, distribute and modify.",
    "window.about.notices": "here",
    "window.about.noticesLine": "View third-party notices {link}.",
    "window.about.noticesLabel": "View the third-party notices",
  });

  const KEYS = [
    { action: "read", key: "F8" },
    { action: "pause", key: "F7" },
    { action: "speech", key: "F6" },
  ];
  function settingsState(over) {
    return Object.assign(
      { keys: KEYS, runAtStartup: false, runAsAdmin: false, leftHanded: false, theme: "light", dark: false, version: "0.5.0" },
      over
    );
  }
  function showSettings(over) {
    T.sendStrings();
    T.sendState(settingsState(over));
    control("settings").click();
  }

  // A switch by its label.
  function toggle(label) {
    const found = Array.prototype.filter.call(document.querySelectorAll("[role=switch]"), function (el) {
      return el.textContent.trim() === label && el.getClientRects().length > 0;
    });
    if (found.length !== 1) throw new Error("expected one shown switch '" + label + "', found " + found.length);
    return found[0];
  }
  function keyDown(init, target) {
    (target || document.activeElement || document.body).dispatchEvent(
      new KeyboardEvent("keydown", Object.assign({ bubbles: true, cancelable: true }, init))
    );
  }
  // The key chip of a Key, by the Key's full name.
  function chip(keyName) {
    const found = Array.prototype.filter.call(document.querySelectorAll("button"), function (el) {
      return (el.getAttribute("aria-label") || "").indexOf(keyName + ": ") === 0 && el.getClientRects().length > 0;
    });
    if (found.length !== 1) throw new Error("expected one chip for '" + keyName + "', found " + found.length);
    return found[0];
  }
  function chipText(keyName) {
    return chip(keyName).textContent;
  }
  function clickChip(keyName) {
    const el = chip(keyName);
    el.focus();
    el.click();
    return el;
  }
  function row(keyName) {
    return chip(keyName).closest("li");
  }
  function cardTitle() {
    return document.getElementById("key-detail-title").textContent;
  }
  function cardParagraphs() {
    return Array.prototype.map.call(document.querySelectorAll("#key-detail-text p"), function (p) {
      return p.textContent;
    });
  }
  function pointer(el, type, init) {
    el.dispatchEvent(new PointerEvent(type, Object.assign({ bubbles: type !== "pointerenter", pointerId: 5 }, init)));
  }

  // ---- the four switches -----------------------------------------------------------------
  test("settings: the four switches follow the state", function () {
    showSettings({ runAtStartup: true, runAsAdmin: false, dark: true, leftHanded: false });
    equal(toggle("run at startup").getAttribute("aria-checked"), "true", "run at startup");
    equal(toggle("run as admin").getAttribute("aria-checked"), "false", "run as admin");
    equal(toggle("dark mode").getAttribute("aria-checked"), "true", "dark mode");
    equal(toggle("left-handed").getAttribute("aria-checked"), "false", "left-handed");
    T.sendState(settingsState({ runAtStartup: false, runAsAdmin: true, dark: false, leftHanded: true }));
    equal(toggle("run at startup").getAttribute("aria-checked"), "false", "run at startup after a new state");
    equal(toggle("run as admin").getAttribute("aria-checked"), "true", "run as admin after a new state");
    equal(toggle("dark mode").getAttribute("aria-checked"), "false", "dark mode after a new state");
    equal(toggle("left-handed").getAttribute("aria-checked"), "true", "left-handed after a new state");
  });

  test("settings: each switch sends its own message with the opposite value, and changes only with the next state", function () {
    showSettings({ runAtStartup: false, runAsAdmin: true, leftHanded: false, dark: false });
    sent.length = 0;
    toggle("run at startup").click();
    toggle("run as admin").click();
    toggle("left-handed").click();
    equal(
      sent,
      [
        { type: "setRunAtStartup", on: true },
        { type: "setRunAsAdmin", on: false },
        { type: "setLeftHanded", on: true },
      ],
      "messages"
    );
    equal(toggle("run at startup").getAttribute("aria-checked"), "false", "switch before the state");
    T.sendState(settingsState({ runAtStartup: true, runAsAdmin: false, leftHanded: true }));
    equal(toggle("run at startup").getAttribute("aria-checked"), "true", "switch after the state");
    equal(toggle("run as admin").getAttribute("aria-checked"), "false", "other switch after the state");
  });

  test("settings: dark mode sends setTheme with the other of the theme in use (dark), not of the stored choice", function () {
    showSettings({ dark: false, theme: "system" });
    sent.length = 0;
    toggle("dark mode").click();
    equal(sent, [{ type: "setTheme", theme: "dark" }], "from light");
    T.sendState(settingsState({ dark: true, theme: "system" }));
    equal(toggle("dark mode").getAttribute("aria-checked"), "true", "the switch shows the theme in use");
    sent.length = 0;
    toggle("dark mode").click();
    equal(sent, [{ type: "setTheme", theme: "light" }], "from dark");
  });

  test("settings: there is no theme choice or Interface language control (ADR-0007); five switches", function () {
    showSettings();
    const panel = document.getElementById("page-settings");
    assert(panel.querySelectorAll("select, [role=radio]").length === 0, "no selector or radio on the page");
    assert(!/interface language|system/i.test(panel.textContent), "no such words");
    equal(document.querySelectorAll("[role=switch]").length, 5, "five switches");
  });

  test("settings: start minimized works only while run at startup is on (D9)", function () {
    showSettings({ runAtStartup: false, startMinimised: true });
    const sw = toggle("start minimized");
    equal(sw.getAttribute("aria-disabled"), "true", "unavailable while run at startup is off");
    equal(sw.getAttribute("aria-checked"), "true", "it still shows the stored value");
    sent.length = 0;
    sw.click();
    equal(sent, [], "a click does nothing while it is unavailable");
    T.sendState(settingsState({ runAtStartup: true, startMinimised: true }));
    assert(!sw.hasAttribute("aria-disabled"), "available once run at startup is on");
    sw.click();
    equal(sent, [{ type: "setStartMinimised", on: false }], "sends the opposite value");
    const items = Array.from(document.querySelectorAll(".misc [role=switch]")).map(function (b) { return b.id; });
    equal(items.slice(0, 3), ["toggle-startup", "toggle-minimised", "toggle-admin"], "between run at startup and run as admin");
  });

  // ---- keys ------------------------------------------------------------------------------
  test("settings: the key rows come from the state, and a key with no key says so", function () {
    showSettings();
    equal(chipText("Read key"), "F8", "Read key");
    equal(chipText("Pause key"), "F7", "Pause key");
    equal(chipText("Speech key"), "F6", "Speech key");
    assert(shows("keybinds") && shows("misc"), "the two headings");
    equal(
      Array.prototype.map.call(document.querySelectorAll("#keys .key-name"), function (n) { return n.textContent; }),
      ["read", "pause", "speech"],
      "the short names of the rows"
    );
    T.sendState(settingsState({ keys: [{ action: "read", key: "Ctrl+Alt+R" }, { action: "speech", key: "" }] }));
    equal(chipText("Read key"), "Ctrl+Alt+R", "Read key in a new state");
    equal(chipText("Speech key"), "(no key)", "Speech key has no key");
    equal(document.querySelectorAll("#keys li").length, 2, "a key the state does not list has no row");
  });

  test("settings: a language entry in state.keys is not listed", function () {
    showSettings({ keys: KEYS.concat([{ action: "language", key: "F9" }]) });
    equal(document.querySelectorAll("#keys li").length, 3, "number of rows");
    assert(!/F9/.test(T.screenText()), "F9 is not shown");
  });

  test("settings: the card shows the selected key's name and its description, one paragraph per line", function () {
    showSettings();
    equal(cardTitle(), "Read key", "the first key is selected at the start");
    equal(cardParagraphs(), ["Freezes the screen.", "Click or drag on text to hear it.", "Press again to leave."], "paragraphs of the Read key");
    pointer(row("Speech key"), "pointerenter");
    equal(cardTitle(), "Speech key", "a hover selects the row");
    equal(cardParagraphs(), ["Tap once to skip. Tap three times quickly to stop."], "the Speech key says tap once = skip, three times = stop");
  });

  test("settings: a hover, a focus or a click on a row selects it; the selected row is bold", function () {
    showSettings();
    function selectedRows() {
      return Array.prototype.map.call(document.querySelectorAll("#keys li[data-selected] .key-name"), function (n) {
        return n.textContent;
      });
    }
    pointer(row("Read key"), "pointerenter");
    equal(selectedRows(), ["read"], "after a hover on the first row");
    pointer(row("Pause key"), "pointerenter");
    equal(selectedRows(), ["pause"], "after a hover");
    equal(cardTitle(), "Pause key", "card after a hover");
    chip("Speech key").focus();
    equal(selectedRows(), ["speech"], "after a focus");
    row("Read key").click();
    equal(selectedRows(), ["read"], "after a click");
    equal(cardTitle(), "Read key", "card after a click");
    const bold = getComputedStyle(document.querySelector("#keys li[data-selected] .key-name")).fontWeight;
    const plain = getComputedStyle(document.querySelector("#keys li:not([data-selected]) .key-name")).fontWeight;
    assert(parseInt(bold, 10) > parseInt(plain, 10), "the selected row is bolder: " + bold + " against " + plain);
  });

  test("settings: the play button of the card asks the host to read that key's text aloud (D10)", function () {
    showSettings();
    pointer(row("Read key"), "pointerenter");
    const play = control("Read aloud what the Read key does");
    assert(!play.hasAttribute("aria-disabled"), "it works");
    equal(play.title, "Read aloud what the Read key does", "its tooltip");
    sent.length = 0;
    play.click();
    equal(sent, [{ type: "readKeyCard", action: "read" }], "messages");
    pointer(row("Speech key"), "pointerenter");
    equal(T.accessibleName(document.getElementById("key-play")), "Read aloud what the Speech key does", "its name follows the selected key");
  });

  test("settings: selecting a chip waits for a key: captureKey, then setKey with the key that is pressed", function () {
    showSettings();
    sent.length = 0;
    clickChip("Read key");
    equal(sent, [{ type: "captureKey", action: "read" }], "message when the chip is selected");
    equal(chipText("Read key"), "Press a key", "the chip asks for a key");
    assert(shows("Press the new key. Esc cancels."), "the hint is shown");
    sent.length = 0;
    keyDown({ key: "F9" });
    equal(sent, [{ type: "setKey", action: "read", key: "F9" }], "message when a key is pressed");
    assert(!shows("Press the new key. Esc cancels."), "the hint is gone");
    equal(chipText("Read key"), "F8", "the chip shows the state, not the key, until the host answers");
    T.sendState(settingsState({ keys: [{ action: "read", key: "F9" }, { action: "pause", key: "F7" }] }));
    equal(chipText("Read key"), "F9", "the new key shows with the next state");
  });

  test("settings: a key with modifiers is sent in the form settings.json uses, and a modifier alone is not a key", function () {
    showSettings();
    clickChip("Pause key");
    sent.length = 0;
    keyDown({ key: "Control", ctrlKey: true });
    keyDown({ key: "Alt", ctrlKey: true, altKey: true });
    equal(sent, [], "nothing is sent for a modifier alone");
    equal(chipText("Pause key"), "Press a key", "the chip still waits");
    keyDown({ key: "r", ctrlKey: true, altKey: true });
    equal(sent, [{ type: "setKey", action: "pause", key: "Ctrl+Alt+R" }], "message");
    clickChip("Pause key");
    sent.length = 0;
    keyDown({ key: "ArrowLeft", shiftKey: true });
    equal(sent, [{ type: "setKey", action: "pause", key: "Shift+Left" }], "a named key");
  });

  test("settings: Esc cancels the capture: cancelKeyCapture, and no key is set", function () {
    showSettings();
    clickChip("Speech key");
    sent.length = 0;
    keyDown({ key: "Escape" });
    equal(sent, [{ type: "cancelKeyCapture", action: "speech" }], "message after Esc");
    equal(chipText("Speech key"), "F6", "the chip shows its key again");
    assert(!shows("Press the new key. Esc cancels."), "the hint is gone");
    sent.length = 0;
    keyDown({ key: "F2" });
    equal(sent, [], "a key after Esc is not taken");
  });

  test("settings: a press outside the chip cancels the capture; a press on the chip itself does not", function () {
    showSettings();
    clickChip("Read key");
    sent.length = 0;
    pointer(chip("Read key"), "pointerdown", { button: 0 });
    equal(sent, [], "a press on the chip that waits");
    equal(chipText("Read key"), "Press a key", "the chip still waits");
    pointer(document.getElementById("page-settings"), "pointerdown", { button: 0 });
    equal(sent, [{ type: "cancelKeyCapture", action: "read" }], "message after a press elsewhere");
    equal(chipText("Read key"), "F8", "the chip shows its key again");
    sent.length = 0;
    pointer(document.getElementById("page-settings"), "pointerdown", { button: 0 });
    equal(sent, [], "a press with no capture sends nothing");
  });

  test("settings: leaving the chip cancels the capture; selecting another chip moves it", function () {
    showSettings();
    clickChip("Read key");
    sent.length = 0;
    control("home").focus();
    equal(sent, [{ type: "cancelKeyCapture", action: "read" }], "message when the focus leaves");
    equal(chipText("Read key"), "F8", "the chip shows its key again");
    clickChip("Read key");
    sent.length = 0;
    clickChip("Pause key");
    equal(
      sent,
      [{ type: "cancelKeyCapture", action: "read" }, { type: "captureKey", action: "pause" }],
      "messages when another chip is selected"
    );
    equal(chipText("Read key"), "F8", "the first chip is back");
    equal(chipText("Pause key"), "Press a key", "the second chip waits");
    keyDown({ key: "Escape" });
  });

  test("settings: the card stays with the key that waits for a key, whatever is hovered", function () {
    showSettings();
    clickChip("Pause key");
    pointer(row("Speech key"), "pointerenter");
    equal(cardTitle(), "Pause key", "card while a key waits");
    keyDown({ key: "Escape" });
    pointer(row("Speech key"), "pointerenter");
    equal(cardTitle(), "Speech key", "card after the capture");
  });

  test("settings: a key that the host refuses leaves the chip as it was", function () {
    showSettings();
    clickChip("Read key");
    keyDown({ key: "F7" });
    T.sendState(settingsState()); // the host answers with the same state
    equal(chipText("Read key"), "F8", "Read key is still F8");
    equal(chipText("Pause key"), "F7", "Pause key is still F7");
  });

  // ---- report ----------------------------------------------------------------------------
  test("report: the invitation, and three links that send openLink with a name, never an address", function () {
    showSettings();
    control("report").click();
    equal(T.shownPages(), ["report"], "page");
    assert(shows("Found a bug?") && shows("Want to request a feature?"), "the title, on two lines");
    assert(shows("Get in touch, I'll respond soon-ish!"), "the text");
    sent.length = 0;
    control("mail").click();
    control("discord").click();
    control("github").click();
    equal(
      sent,
      [{ type: "openLink", link: "mail" }, { type: "openLink", link: "discord" }, { type: "openLink", link: "github" }],
      "messages"
    );
    for (const m of sent) assert(!/https?:|mailto:/i.test(JSON.stringify(m)), "no address in " + JSON.stringify(m));
    control("home").click();
  });

  // ---- help ------------------------------------------------------------------------------
  test("help: says that it is not done yet", function () {
    showSettings();
    control("help").click();
    equal(T.shownPages(), ["help"], "page");
    assert(shows("Still working on this part") && shows("Coming soon!"), "the two lines");
    control("home").click();
  });

  // ---- about -----------------------------------------------------------------------------
  test("about: the name and the version of the state, the intro with two bold words, the owner's text", function () {
    showSettings({ version: "0.5.0" });
    control("about").click();
    equal(T.shownPages(), ["about"], "page");
    assert(shows("Lexia | ver. 0.5.0"), "the heading: " + document.getElementById("about-title").textContent);
    equal(document.getElementById("about-intro").textContent, "A simple screen-reader built with video games and comics in mind.", "the intro");
    equal(
      Array.prototype.map.call(document.querySelectorAll("#about-intro strong"), function (s) { return s.textContent; }),
      ["video games", "comics"],
      "the two bold words"
    );
    for (const line of ["I hope you find this app useful.", "The entire process runs locally.", "With special thanks to my little sister."]) {
      assert(shows(line), line);
    }
    T.sendState(settingsState({ version: "0.6.1" }));
    assert(shows("Lexia | ver. 0.6.1"), "the version of a new state");
    control("home").click();
  });

  test("about: itch.io and GitHub send openLink; the notices link sends openNotices", function () {
    showSettings();
    control("about").click();
    sent.length = 0;
    control("Lexia on itch.io").click();
    control("Lexia on GitHub").click();
    control("View the third-party notices").click();
    equal(
      sent,
      [{ type: "openLink", link: "itch" }, { type: "openLink", link: "github" }, { type: "openNotices" }],
      "messages"
    );
    assert(shows("Lexia is free to use, distribute and modify."), "the licence line");
    equal(document.getElementById("about-notices-line").textContent, "View third-party notices here.", "the notices line, with the link in it");
    control("home").click();
  });
})();
