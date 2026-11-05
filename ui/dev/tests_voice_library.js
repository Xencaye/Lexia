// Bridge tests for the Voice library sheets (D0 ui 5 and 6): Download a voice and the result of Add
// from file. The runner and the helpers are in dev/tests.js (window.lexiaTests); like the rest, these
// tests play the host and check only what a person sees and the messages the page sends.
(function () {
  "use strict";

  const T = window.lexiaTests;
  const test = T.test, assert = T.assert, equal = T.equal;
  const shows = T.shows, control = T.control, sent = T.sent;

  const MB = 1024 * 1024;

  // The words of the sheets (the real ones come from the C++ table).
  Object.assign(T.STRINGS, {
    "window.download.title": "Download a voice",
    "window.download.network": "Lexia uses the network only now, for this download.",
    "window.download.loading": "Getting the list of voices",
    "window.download.empty": "There is no voice to download.",
    "window.download.size": "{mb} MB",
    "window.download.get": "Download",
    "window.download.getFor": "Download {voice}",
    "window.download.installed": "Installed",
    "window.download.progress": "{done} of {total} MB",
    "window.download.progressLabel": "Download of {voice}",
    "window.download.cancel": "Cancel",
    "window.download.cancelFor": "Cancel download of {voice}",
    "window.download.cancelling": "Cancelling",
    "window.download.done": "{voice} is installed. You can choose it on the voices page.",
    "window.download.cancelled": "The download is cancelled.",
    "window.download.retry": "Try again",
    "window.download.retryFor": "Try {voice} again",
    "window.download.close": "Close",
    "window.result.done": "Done",
    "window.result.problem": "Problem",
    "window.addFile.addedTitle": "Voice added",
    "window.addFile.added": "{voice} is added. It speaks {language}. You can choose it on the voices page.",
    "window.addFile.refusedTitle": "Voice not added",
    "window.addFile.again": "Choose another file",
    "window.addFile.close": "Close",
    "window.problem.unsupportedLanguage": "Lexia can't read {language} yet. Choose a voice for a language that Lexia reads.",
    "window.problem.thisLanguage": "this language",
    "window.problem.notMedium": "This voice has {quality} quality. Lexia uses only voices of Medium quality. Choose a Medium voice.",
    "window.problem.notMediumUnknown": "Lexia uses only voices of Medium quality, and this voice is not marked as Medium. Choose a Medium voice.",
    "window.problem.badName": "The name of this voice has characters that Lexia can't use. Rename the files, then add them again.",
    "window.problem.badNameDownload": "Lexia can't use the name of this voice. Choose another voice.",
    "window.problem.missingFile": "Lexia could not read the voice files. Choose a .zip file, or a .onnx file with its .onnx.json file beside it.",
    "window.problem.badConfig": "The .onnx.json file is not the settings file of a Piper voice.",
    "window.problem.badZip": "This zip file is not a voice that Lexia can use.",
    "window.problem.cannotWrite": "Lexia could not save the voice on this computer. Check that the disk has free space, then try again.",
    "window.problem.notInCatalogue": "This voice is not in the list any more.",
    "window.problem.network": "The download did not finish. Check your network connection, then try again.",
    "window.problem.catalogueNetwork": "Lexia could not get the list of voices. Check your network connection, then try again.",
    "window.problem.badDownload": "The downloaded files are damaged, so Lexia did not keep them. Try again.",
    "window.problem.unknown": "Something went wrong. Try again.",
    "window.deleteVoice.failedTitle": "Voice not deleted",
    "window.deleteVoice.failed": "Lexia could not delete {voice}. Check that no other program uses its files, then try again.",
    "window.deleteVoice.failedShipped": "{voice} comes with Lexia. You cannot delete it.",
    "window.preset.savedTitle": "Preset saved",
    "window.preset.saved": "The speed, pitch and volume are saved in {name}.",
    "window.preset.loadedTitle": "Preset loaded",
    "window.preset.loaded": "The speed, pitch and volume of {name} are in use now.",
    "window.preset.failedTitle": "Preset not used",
    "window.preset.badFile": "This is not a Lexia preset file. Choose a file that Lexia saved.",
    "window.preset.cannotWrite": "Lexia could not save the preset. Check that the disk has free space, then try again.",
    "window.preset.noVoice": "No voice is installed, so there is nothing to save or load. Add a voice first.",
  });

  const VOICES = [
    { name: "en_GB-alba-medium", language: "eng", sizeBytes: 63 * MB, installed: true },
    { name: "en_US-amy-medium", language: "eng", sizeBytes: 63 * MB, installed: false },
    { name: "en_US-lessac-medium", language: "eng", sizeBytes: 5 * MB, installed: false },
    { name: "bg_BG-dimitar-medium", language: "bul", sizeBytes: 63 * MB, installed: false },
  ];
  const AMY = "en_US-amy-medium";

  function host(msg) {
    T.host(msg);
  }
  function dialog(id) {
    return document.getElementById(id);
  }
  // Back to a known start: the sheets closed, no download running, the voices page shown.
  function start(stateOver) {
    for (const id of ["catalogue", "file-result", "delete-voice"]) if (dialog(id).open) dialog(id).close();
    T.sendStrings();
    host({ type: "downloadFailed", name: "reset", problem: "cancelled" });
    T.sendState(Object.assign({ voices: { eng: { installed: ["en_GB-alba-medium"], active: "en_GB-alba-medium", removable: [] } } }, stateOver));
    control("voices").click();
    sent.length = 0;
  }
  function openSheet() {
    const button = control("Download a voice");
    button.focus();
    button.click();
  }
  function openCatalogue(voices) {
    start();
    openSheet();
    host({ type: "catalogue", voices: voices || VOICES });
  }
  function sheetText() {
    return dialog("catalogue").innerText;
  }
  function sheetShows(text) {
    return sheetText().split("\n").some(function (line) {
      return line.trim() === text;
    });
  }
  function row(name) {
    const el = dialog("catalogue").querySelector('li[data-name="' + name + '"]');
    if (!el) throw new Error("no row for '" + name + "'");
    return el;
  }
  function rowNames() {
    return Array.prototype.map.call(dialog("catalogue").querySelectorAll("li[data-name]"), function (li) {
      return li.dataset.name;
    });
  }
  function rowText(name) {
    return row(name).innerText.replace(/\s+/g, " ").trim();
  }
  function downloadButton(name) {
    return control("Download " + name);
  }
  function key(el, k) {
    el.dispatchEvent(new KeyboardEvent("keydown", { key: k, bubbles: true, cancelable: true }));
  }

  // ---- the download sheet ----------------------------------------------------------------
  test("Download voice: the sheet opens with the network line and a quiet loading text, and the page asks the host", function () {
    start();
    control("Download a voice").click();
    assert(dialog("catalogue").open, "the sheet is open");
    equal(sent, [{ type: "openCatalogue" }], "messages");
    assert(sheetShows("Download a voice"), "the title");
    assert(sheetShows("Lexia uses the network only now, for this download."), "the network line (ADR-0005)");
    assert(sheetShows("Getting the list of voices"), "the loading text");
    assert(!sheetText().includes("Try again"), "no error while it loads");
  });

  test("Download voice: a row for each Voice the host sends, with its size and no Language", function () {
    openCatalogue();
    equal(rowNames(), VOICES.map(function (v) { return v.name; }), "rows: nothing is filtered out");
    equal(rowText(AMY).startsWith("en_US-amy-medium 63 MB"), true, "name and size of a row: " + rowText(AMY));
    assert(rowText("en_US-lessac-medium").includes("5.0 MB"), "a small size has one decimal: " + rowText("en_US-lessac-medium"));
    assert(rowNames().indexOf("bg_BG-dimitar-medium") >= 0, "a Voice of any Language is listed");
    assert(!/English|Bulgarian|Language/.test(sheetText()), "no Language name or column: " + sheetText());
    assert(!sheetShows("Getting the list of voices"), "the loading text is gone");
  });

  test("Download voice: a Voice that is installed says Installed and has no Download button", function () {
    openCatalogue();
    assert(rowText("en_GB-alba-medium").includes("Installed"), "the word Installed");
    let found = 0;
    for (const b of dialog("catalogue").querySelectorAll("button")) if (b.getAttribute("aria-label") === "Download en_GB-alba-medium") found++;
    equal(found, 0, "Download buttons of the installed Voice");
    assert(!downloadButton(AMY).disabled, "another Voice can be downloaded");
  });

  test("Download voice: an empty catalogue, and rows that are not Voices, are said or left out", function () {
    openCatalogue([]);
    assert(sheetShows("There is no voice to download."), "the empty text");
    host({ type: "catalogue", voices: [null, { name: "", language: "eng", sizeBytes: 1 }, { name: "x", language: "eng" }, VOICES[1]] });
    equal(rowNames(), [AMY], "only the well-formed row");
  });

  test("Download voice: Download sends download with the name, and the other Download buttons are disabled", function () {
    openCatalogue();
    sent.length = 0;
    downloadButton(AMY).focus();
    downloadButton(AMY).click();
    equal(sent, [{ type: "download", name: AMY }], "messages");
    assert(downloadButton("en_US-lessac-medium").disabled, "the other Download button is disabled");
    assert(rowText(AMY).includes("0 of 63 MB"), "the row shows the start: " + rowText(AMY));
    assert(rowText(AMY).includes("Cancel"), "the row offers Cancel");
    equal(document.activeElement, control("Cancel download of " + AMY), "the focus follows to Cancel");
    // A second click on the disabled button does nothing.
    downloadButton("en_US-lessac-medium").click();
    equal(sent.length, 1, "messages after a click on a disabled button");
  });

  test("Download voice: downloadProgress shows the text value and fills the bar", function () {
    openCatalogue();
    downloadButton(AMY).click();
    host({ type: "downloadProgress", name: AMY, done: 12 * MB, total: 63 * MB });
    assert(rowText(AMY).includes("12 of 63 MB"), "the text value: " + rowText(AMY));
    const bar = row(AMY).querySelector("[role=progressbar]");
    assert(bar && bar.getClientRects().length > 0, "a progress bar is shown for the running download");
    equal(bar.getAttribute("aria-valuemin"), "0", "the bar's minimum");
    equal(bar.getAttribute("aria-valuemax"), "100", "the bar's maximum");
    equal(bar.getAttribute("aria-valuenow"), "19", "the bar's value in percent");
    equal(bar.getAttribute("aria-valuetext"), "12 of 63 MB", "the bar's text");
    host({ type: "downloadProgress", name: AMY, done: 63 * MB, total: 63 * MB });
    assert(rowText(AMY).includes("63 of 63 MB"), "the end: " + rowText(AMY));
    host({ type: "downloadProgress", name: AMY, done: 70 * MB, total: 63 * MB });
    assert(rowText(AMY).includes("63 of 63 MB"), "never more than the total: " + rowText(AMY));
  });

  test("Download voice: progress does not take the focus away from Cancel", function () {
    openCatalogue();
    downloadButton(AMY).click();
    const cancel = control("Cancel download of " + AMY);
    cancel.focus();
    host({ type: "downloadProgress", name: AMY, done: 5 * MB, total: 63 * MB });
    host({ type: "downloadProgress", name: AMY, done: 6 * MB, total: 63 * MB });
    equal(document.activeElement, cancel, "the focus");
    assert(cancel.isConnected, "the same button");
  });

  test("Download voice: Cancel sends cancelDownload, and the answer shows that it is cancelled", function () {
    openCatalogue();
    downloadButton(AMY).click();
    host({ type: "downloadProgress", name: AMY, done: 12 * MB, total: 63 * MB });
    sent.length = 0;
    control("Cancel download of " + AMY).focus();
    control("Cancel download of " + AMY).click();
    equal(sent, [{ type: "cancelDownload", name: AMY }], "messages");
    assert(rowText(AMY).includes("Cancelling"), "the row says Cancelling until the host answers");
    assert(downloadButton("en_US-lessac-medium").disabled, "the other Download buttons stay disabled until then");
    host({ type: "downloadFailed", name: AMY, problem: "cancelled" });
    assert(sheetShows("The download is cancelled."), "the cancelled note");
    assert(!sheetText().includes("Problem"), "a cancel is not a problem");
    assert(!downloadButton(AMY).disabled && !downloadButton("en_US-lessac-medium").disabled, "Download works again");
    assert(dialog("catalogue").contains(document.activeElement), "the focus stays in the sheet");
  });

  test("Download voice: downloadDone shows success and the Voice as Installed", function () {
    openCatalogue();
    downloadButton(AMY).click();
    host({ type: "downloadDone", name: AMY });
    assert(sheetText().includes("Done"), "the word Done");
    assert(sheetText().includes("en_US-amy-medium is installed. You can choose it on the voices page."), "the success text");
    assert(rowText(AMY).includes("Installed"), "the row says Installed: " + rowText(AMY));
    assert(!downloadButton("en_US-lessac-medium").disabled, "another Download is possible");
  });

  test("Download voice: downloadFailed shows the text with the word Problem, and Try again sends download again", function () {
    openCatalogue();
    downloadButton(AMY).click();
    host({ type: "downloadProgress", name: AMY, done: 10 * MB, total: 63 * MB });
    host({ type: "downloadFailed", name: AMY, problem: "network" });
    assert(rowText(AMY).includes("Problem"), "the word Problem: " + rowText(AMY));
    assert(rowText(AMY).includes("The download did not finish. Check your network connection, then try again."), "the text");
    assert(!downloadButton("en_US-lessac-medium").disabled, "another Voice can be downloaded");
    sent.length = 0;
    control("Try " + AMY + " again").click();
    equal(sent, [{ type: "download", name: AMY }], "messages");
    assert(!rowText(AMY).includes("Problem"), "the error is gone while it tries");
    assert(rowText(AMY).includes("Cancel"), "Cancel again");
  });

  test("Download voice: the catalogue failing shows its text and Try again sends openCatalogue", function () {
    start();
    openSheet();
    host({ type: "catalogueFailed", problem: "network" });
    assert(sheetText().includes("Problem"), "the word Problem");
    assert(sheetShows("Lexia could not get the list of voices. Check your network connection, then try again.") ||
           sheetText().includes("Lexia could not get the list of voices. Check your network connection, then try again."), "the text");
    sent.length = 0;
    control("Try again").click();
    equal(sent, [{ type: "openCatalogue" }], "messages");
    assert(sheetShows("Getting the list of voices"), "the loading text again");
    host({ type: "catalogue", voices: VOICES });
    equal(rowNames().length, VOICES.length, "rows after the second try");
  });

  test("Download voice: a catalogue that arrives with the sheet closed opens it", function () {
    start();
    assert(!dialog("catalogue").open, "closed");
    host({ type: "catalogue", voices: VOICES });
    assert(dialog("catalogue").open, "open");
    equal(rowNames().length, VOICES.length, "rows");
  });

  test("Download voice: a download that runs shows again when the sheet is opened again", function () {
    openCatalogue();
    downloadButton(AMY).click();
    host({ type: "downloadProgress", name: AMY, done: 20 * MB, total: 63 * MB });
    dialog("catalogue").close();
    openSheet();
    host({ type: "catalogue", voices: VOICES });
    assert(rowText(AMY).includes("20 of 63 MB"), "the progress: " + rowText(AMY));
    assert(downloadButton("en_US-lessac-medium").disabled, "one download at a time");
  });

  // Every code of the core that can come with a download or with a file has its own text.
  const DOWNLOAD_CODES = {
    "network": "The download did not finish. Check your network connection, then try again.",
    "bad-download": "The downloaded files are damaged, so Lexia did not keep them. Try again.",
    "cannot-write": "Lexia could not save the voice on this computer. Check that the disk has free space, then try again.",
    "bad-name": "Lexia can't use the name of this voice. Choose another voice.",
    "not-in-catalogue": "This voice is not in the list any more.",
    "unsupported-language": "Lexia can't read German yet. Choose a voice for a language that Lexia reads.",
    "not-medium": "This voice has low quality. Lexia uses only voices of Medium quality. Choose a Medium voice.",
  };
  test("Download voice: each problem code of a download shows its own text, with the language name or quality", function () {
    const seen = [];
    for (const code of Object.keys(DOWNLOAD_CODES)) {
      openCatalogue();
      downloadButton(AMY).click();
      host({ type: "downloadFailed", name: AMY, problem: code, language: "German", quality: "low" });
      const text = rowText(AMY);
      assert(text.includes(DOWNLOAD_CODES[code]), code + ": " + text);
      assert(text.includes("Problem"), code + ": the word Problem");
      seen.push(DOWNLOAD_CODES[code]);
    }
    equal(new Set(seen).size, Object.keys(DOWNLOAD_CODES).length, "every code has its own text");
  });

  test("Download voice: a Voice refused for its quality without a quality still has a text", function () {
    openCatalogue();
    downloadButton(AMY).click();
    host({ type: "downloadFailed", name: AMY, problem: "not-medium", quality: "" });
    assert(rowText(AMY).includes("this voice is not marked as Medium"), rowText(AMY));
    host({ type: "downloadFailed", name: AMY, problem: "something-new" });
    assert(rowText(AMY).includes("Something went wrong. Try again."), "an unknown code: " + rowText(AMY));
  });

  // ---- Add from file ---------------------------------------------------------------------
  function resultText() {
    return dialog("file-result").innerText.replace(/\s+/g, " ").trim();
  }
  test("Add from file: the add button sends addFromFile and opens no sheet by itself", function () {
    start();
    control("Add a voice from a file").click();
    equal(sent, [{ type: "addFromFile" }], "messages");
    assert(!dialog("file-result").open && !dialog("catalogue").open, "no sheet before the host answers");
  });

  test("Add from file: voiceAdded shows success with the Voice name and its Language", function () {
    start();
    host({ type: "voiceAdded", voice: "en_US-amy-medium", language: "eng" });
    assert(dialog("file-result").open, "the result sheet is open");
    assert(resultText().includes("Voice added"), "the title: " + resultText());
    assert(resultText().includes("Done"), "the word Done");
    assert(resultText().includes("en_US-amy-medium is added. It speaks English. You can choose it on the voices page."), resultText());
    assert(!resultText().includes("Choose another file"), "no second try after a success");
  });

  const FILE_CODES = {
    "unsupported-language": "Lexia can't read German yet. Choose a voice for a language that Lexia reads.",
    "not-medium": "This voice has low quality. Lexia uses only voices of Medium quality. Choose a Medium voice.",
    "bad-name": "The name of this voice has characters that Lexia can't use. Rename the files, then add them again.",
    "missing-file": "Lexia could not read the voice files. Choose a .zip file, or a .onnx file with its .onnx.json file beside it.",
    "bad-config": "The .onnx.json file is not the settings file of a Piper voice.",
    "bad-zip": "This zip file is not a voice that Lexia can use.",
    "cannot-write": "Lexia could not save the voice on this computer. Check that the disk has free space, then try again.",
  };
  test("Add from file: each VoiceProblem code of a file shows its own text, with the language name or quality", function () {
    const seen = [];
    for (const code of Object.keys(FILE_CODES)) {
      start();
      host({ type: "voiceRefused", problem: code, language: "German", quality: "low" });
      assert(dialog("file-result").open, code + ": the sheet is open");
      assert(resultText().includes("Voice not added"), code + ": the title");
      assert(resultText().includes("Problem"), code + ": the word Problem");
      assert(resultText().includes(FILE_CODES[code]), code + ": " + resultText());
      seen.push(FILE_CODES[code]);
    }
    equal(new Set(seen).size, Object.keys(FILE_CODES).length, "every code has its own text");
  });

  test("Add from file: the text names the language the host sends (\"Lexia can't read German yet\")", function () {
    start();
    host({ type: "voiceRefused", problem: "unsupported-language", language: "Hindi" });
    assert(resultText().includes("Lexia can't read Hindi yet."), resultText());
    dialog("file-result").close();
    host({ type: "voiceRefused", problem: "unsupported-language" });
    assert(resultText().includes("Lexia can't read this language yet."), "no name sent: " + resultText());
    dialog("file-result").close();
    host({ type: "voiceRefused", problem: "not-medium", quality: "high" });
    assert(resultText().includes("This voice has high quality."), resultText());
  });

  test("Add from file: Choose another file closes the result and sends addFromFile again", function () {
    start();
    host({ type: "voiceRefused", problem: "bad-zip" });
    sent.length = 0;
    control("Choose another file").click();
    assert(!dialog("file-result").open, "closed");
    equal(sent, [{ type: "addFromFile" }], "messages");
  });

  // ---- Delete a Voice and the preset files: the same result sheet -------------------------------
  test("Delete a voice: voiceDeleteFailed shows a problem with the Voice's name; a shipped Voice has its own text", function () {
    start();
    host({ type: "voiceDeleteFailed", voice: "en_GB-alba-medium", problem: "cannot-write" });
    assert(dialog("file-result").open, "the result sheet is open");
    assert(resultText().includes("Voice not deleted"), "the title: " + resultText());
    assert(resultText().includes("Problem"), "the word Problem");
    assert(resultText().includes("Lexia could not delete en_GB-alba-medium."), resultText());
    assert(!resultText().includes("Choose another file"), "no second try for a delete");
    dialog("file-result").close();
    host({ type: "voiceDeleteFailed", voice: "en_US-ljspeech-medium", problem: "shipped" });
    assert(resultText().includes("en_US-ljspeech-medium comes with Lexia. You cannot delete it."), resultText());
  });

  test("Preset files: presetSaved and presetLoaded show success with the file's name", function () {
    start();
    host({ type: "presetSaved", name: "Slow reading.json" });
    assert(dialog("file-result").open, "the result sheet is open");
    assert(resultText().includes("Preset saved"), "the title: " + resultText());
    assert(resultText().includes("Done"), "the word Done");
    assert(resultText().includes("The speed, pitch and volume are saved in Slow reading.json."), resultText());
    dialog("file-result").close();
    host({ type: "presetLoaded", name: "Bright.json" });
    assert(resultText().includes("Preset loaded"), "the title: " + resultText());
    assert(resultText().includes("The speed, pitch and volume of Bright.json are in use now."), resultText());
    assert(!resultText().includes("Choose another file"), "no second try after a success");
  });

  test("Preset files: presetFailed shows its own text for each problem, and an unknown one the bad-file text", function () {
    const PRESET = {
      badFile: "This is not a Lexia preset file. Choose a file that Lexia saved.",
      cannotWrite: "Lexia could not save the preset. Check that the disk has free space, then try again.",
      noVoice: "No voice is installed, so there is nothing to save or load. Add a voice first.",
    };
    const seen = [];
    for (const code of Object.keys(PRESET)) {
      start();
      host({ type: "presetFailed", problem: code });
      assert(dialog("file-result").open, code + ": the sheet is open");
      assert(resultText().includes("Preset not used"), code + ": the title");
      assert(resultText().includes("Problem"), code + ": the word Problem");
      assert(resultText().includes(PRESET[code]), code + ": " + resultText());
      seen.push(PRESET[code]);
    }
    equal(new Set(seen).size, 3, "every code has its own text");
    dialog("file-result").close();
    host({ type: "presetFailed", problem: "something-new" });
    assert(resultText().includes(PRESET.badFile), "an unknown code: " + resultText());
  });

  // ---- both sheets -----------------------------------------------------------------------
  test("Sheets: Esc closes a sheet and the focus goes back to where it was", function () {
    openCatalogue();
    const opener = control("Download a voice");
    key(document.activeElement, "Escape");
    assert(!dialog("catalogue").open, "the download sheet is closed");
    equal(document.activeElement, opener, "the focus after the download sheet");

    opener.focus();
    host({ type: "voiceRefused", problem: "bad-zip" });
    assert(dialog("file-result").open, "the result is open");
    key(document.activeElement, "Escape");
    assert(!dialog("file-result").open, "the result is closed");
    equal(document.activeElement, opener, "the focus after the result");
  });

  test("Sheets: the Close button closes them", function () {
    openCatalogue();
    sent.length = 0;
    control("Close").click();
    assert(!dialog("catalogue").open, "the download sheet is closed");
    host({ type: "voiceAdded", voice: "x", language: "eng" });
    control("Close").click();
    assert(!dialog("file-result").open, "the result is closed");
    equal(sent, [], "closing sends nothing");
  });

  test("Sheets: they never make a network request themselves; they only send messages", function () {
    const calls = [];
    const real = { fetch: window.fetch, open: XMLHttpRequest.prototype.open, beacon: navigator.sendBeacon, ws: window.WebSocket };
    window.fetch = function () { calls.push("fetch"); return Promise.reject(new Error("blocked")); };
    XMLHttpRequest.prototype.open = function () { calls.push("xhr"); };
    navigator.sendBeacon = function () { calls.push("beacon"); return false; };
    window.WebSocket = function () { calls.push("websocket"); throw new Error("blocked"); };
    try {
      openCatalogue();
      downloadButton(AMY).click();
      host({ type: "downloadProgress", name: AMY, done: 1 * MB, total: 63 * MB });
      control("Cancel download of " + AMY).click();
      host({ type: "downloadFailed", name: AMY, problem: "cancelled" });
      downloadButton(AMY).click();
      host({ type: "downloadFailed", name: AMY, problem: "network" });
      control("Try " + AMY + " again").click();
      host({ type: "downloadDone", name: AMY });
      host({ type: "catalogueFailed", problem: "network" });
      control("Try again").click();
      dialog("catalogue").close();
      host({ type: "voiceAdded", voice: "v", language: "eng" });
      dialog("file-result").close();
      host({ type: "voiceRefused", problem: "bad-zip" });
      dialog("file-result").close();
    } finally {
      window.fetch = real.fetch;
      XMLHttpRequest.prototype.open = real.open;
      navigator.sendBeacon = real.beacon;
      window.WebSocket = real.ws;
    }
    equal(calls, [], "network calls");
    assert(sent.length > 0, "the page did send messages");
    for (const m of sent) assert(typeof m.type === "string", "every thing the page sends is a message with a type");
  });
})();
