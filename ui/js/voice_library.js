// The Voice library sheets (D0 ui 5 and 6): Download a voice, and the result of Add from file.
// The whole bridge contract is in ui/README.md; this header says what this file uses of it.
//
// Two sheets (modal dialogs) over the voices page (and a result sheet for a delete and for the preset files, below). They are the page's side of the Voice library
// (src/tts/voice_library.h, voice_catalogue.h, voice_download.h). The page never goes on the
// network and never opens a file: it only sends messages. The host fetches the catalogue, downloads
// and opens the native file dialog (ADR-0005: the network is used only when the person asks).
//
// Page -> host
//   openCatalogue            the person chose the download icon (or Try again on the sheet)
//   download { name }        the person chose Download (or Try again) on a row
//   cancelDownload { name }  the person chose Cancel on the row that downloads
//   addFromFile              the person chose the add-from-file icon (sent by js/voice.js); the host
//                            opens the native file dialog and answers voiceAdded or voiceRefused
//
// Host -> page
//   catalogue { voices: [{ name, language, sizeBytes, installed }] }
//       The downloadable Voices (FilterCatalogueForLexia: Medium Voices of Lexia's Languages).
//       `language` is the Lexia Language id ("eng"). The host sends only the Languages that Lexia
//       offers (ADR-0007), so the page lists every well-formed row. `sizeBytes` = model + config.
//       `installed` = the Voice is in the Voice library already. The sheet opens if it is not open.
//   catalogueFailed { problem }
//       The catalogue could not be fetched. `problem` is a ProblemId of voice_library.h; the core
//       gives only "network" here (FetchVoiceCatalogue).
//   downloadProgress { name, done, total }   bytes done and bytes in all (DownloadProgressFn)
//   downloadDone { name }                    the Voice is installed; the next "state" lists it
//   downloadFailed { name, problem, language?, quality? }
//       `problem` is a ProblemId: "network", "bad-download", "cannot-write", "bad-name",
//       "not-in-catalogue", "unsupported-language" (+ `language`: the English name), "not-medium"
//       (+ `quality`, lower case, may be ""), or "cancelled" (the answer to cancelDownload; the page
//       shows no error for it).
//   voiceAdded { voice, language }           Add from file worked; `language` is a Lexia Language id
//   voiceRefused { problem, language?, quality? }
//       Add from file was refused. `problem` is a ProblemId: "unsupported-language" (+ `language`: the
//       English name, "German"), "not-medium" (+ `quality`), "bad-name", "missing-file", "bad-config",
//       "bad-zip", "cannot-write".
//   voiceDeleteFailed { voice, problem }     deleteVoice did not work ("shipped" has its own text, others the general one)
//   presetSaved { name }, presetLoaded { name }, presetFailed { problem }
//       the answers of savePresetFile and loadPresetFile ("badFile", "cannotWrite", "noVoice")
// These three share the result sheet (#file-result) with voiceAdded and voiceRefused.
//
// The ProblemId strings are the ones of the core (ProblemId()). Every code has its own text in the
// strings table (window.problem.*), with {language} or {quality} where the outcome carries it. A
// code that the page does not know shows window.problem.unknown.
//
// What the page owns here is the sheet's own progress, not a setting: which sheet is open, the
// catalogue as sent, the download that runs, the last failure and the Voices installed since the
// catalogue came. The settings (the Voices that are installed) are still drawn by js/voice.js from
// "state". Closing the sheet does not stop a download; the sheet shows it again when it opens.
(function () {
  "use strict";

  const lexia = window.lexia;
  const $ = function (id) {
    return document.getElementById(id);
  };

  const sheet = $("catalogue");
  const body = $("catalogue-body");
  const note = $("catalogue-note");
  const result = $("file-result");
  if (!sheet || !result) return; // a page without the sheets

  const MB = 1024 * 1024;

  let state = null; // the last "state" (for the offered Languages)
  let loading = false; // the catalogue is being fetched
  let failure = null; // { problem } when the catalogue could not be fetched
  let voices = null; // the rows of the catalogue, as sent, or null before it came
  let download = null; // { name, done, total, cancelling } for the Voice that downloads
  let failedRow = null; // { name, problem, language, quality } of the last download that failed
  let message = null; // { kind: "done" | "cancelled", name } shown above the list
  let installedNow = new Set(); // Voices that finished downloading since the catalogue came
  let reveal = null; // the name of a row to scroll into view when the list is drawn next

  // ---- words ---------------------------------------------------------------------------------
  // "unsupported-language" -> "unsupportedLanguage"
  function camel(code) {
    return code.replace(/-([a-z])/g, function (whole, c) {
      return c.toUpperCase();
    });
  }
  // The text of a problem. `where` is "catalogue" (the list), "download" or "file": three codes read
  // differently in them (network, bad-name).
  function problemText(where, problem, language, quality) {
    const code = typeof problem === "string" ? problem : "";
    const facts = {
      language: typeof language === "string" && language !== "" ? language : lexia.t("window.problem.thisLanguage"),
      quality: typeof quality === "string" ? quality : "",
    };
    let id;
    switch (code) {
      case "network":
        id = where === "catalogue" ? "window.problem.catalogueNetwork" : "window.problem.network";
        break;
      case "bad-name":
        id = where === "file" ? "window.problem.badName" : "window.problem.badNameDownload";
        break;
      case "not-medium":
        id = facts.quality === "" ? "window.problem.notMediumUnknown" : "window.problem.notMedium";
        break;
      case "unsupported-language":
      case "missing-file":
      case "bad-config":
      case "bad-zip":
      case "cannot-write":
      case "not-in-catalogue":
      case "bad-download":
        id = "window.problem." + camel(code);
        break;
      default:
        id = "window.problem.unknown";
    }
    return lexia.tf(id, facts);
  }

  // Megabytes as the person reads them: one decimal under 10 MB, else whole.
  function decimalsFor(bytes) {
    return bytes / MB < 10 ? 1 : 0;
  }
  function megabytes(bytes, decimals) {
    const f = Math.pow(10, decimals);
    const n = Math.round((bytes / MB) * f) / f;
    return decimals === 0 ? String(n) : n.toFixed(decimals);
  }
  function sizeText(bytes) {
    return lexia.tf("window.download.size", { mb: megabytes(bytes, decimalsFor(bytes)) });
  }
  // "12 of 63 MB": the bytes done never read more than the total.
  function progressText(done, total) {
    const d = decimalsFor(total);
    const f = Math.pow(10, d);
    const floored = Math.floor(Math.min(done, total) / MB * f) / f;
    return lexia.tf("window.download.progress", {
      done: d === 0 ? String(floored) : floored.toFixed(d),
      total: megabytes(total, d),
    });
  }

  // A check mark or a warning mark, drawn for Lexia, so a result is never colour alone.
  const MARK_OK =
    '<svg class="mark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="10"/><path d="M7.5 12.5l3.2 3.2 5.8-6.4"/></svg>';
  const MARK_BAD =
    '<svg class="mark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="10"/><path d="M12 7v6.5"/><path d="M12 17v.1"/></svg>';

  // Fills a notice: the mark, a lead word ("Done", "Problem") and the text. Kind "plain" is the text alone.
  function fillNotice(el, kind, text) {
    el.textContent = "";
    el.dataset.kind = kind;
    if (kind === "plain") {
      el.textContent = text;
      return;
    }
    el.insertAdjacentHTML("beforeend", kind === "bad" ? MARK_BAD : MARK_OK);
    const lead = document.createElement("strong");
    lead.className = "notice-lead";
    lead.textContent = lexia.t(kind === "bad" ? "window.result.problem" : "window.result.done");
    const rest = document.createElement("span");
    rest.textContent = text;
    el.append(lead, rest);
  }

  // ---- opening and closing a sheet (lexia.sheet, js/controls.js) ----------------------------
  const openSheet = lexia.sheet.open;
  lexia.sheet.wire(sheet, $("catalogue-close"));
  lexia.sheet.wire(result, $("file-result-close"));

  // ---- the download sheet --------------------------------------------------------------------
  // The rows to show: the well-formed ones, in the order sent.
  function visibleVoices() {
    return voices || [];
  }

  function isInstalled(v) {
    return v.installed || installedNow.has(v.name);
  }

  function startDownload(v) {
    if (download) return;
    failedRow = null;
    message = null;
    download = { name: v.name, done: 0, total: v.sizeBytes, cancelling: false };
    lexia.send("download", { name: v.name });
    renderSheet();
  }
  function cancel() {
    if (!download || download.cancelling) return;
    download.cancelling = true;
    lexia.send("cancelDownload", { name: download.name });
    renderSheet();
  }
  function reload() {
    failure = null;
    loading = true;
    voices = null;
    lexia.send("openCatalogue");
    renderSheet();
  }

  // One row. The look of the action cell says what the row can do now; the second line is the
  // progress of a download or the error of a failed one.
  function makeRow(v) {
    const running = download && download.name === v.name ? download : null;
    const failed = failedRow && failedRow.name === v.name && !running ? failedRow : null;

    const li = document.createElement("li");
    li.className = "voice-row";
    li.dataset.name = v.name;
    li.dataset.mode = running ? (running.cancelling ? "cancelling" : "downloading") : failed ? "failed" : isInstalled(v) ? "installed" : "idle";

    const name = document.createElement("span");
    name.className = "voice-name";
    name.textContent = v.name;
    const size = document.createElement("span");
    size.className = "voice-size";
    size.textContent = sizeText(v.sizeBytes);
    const action = document.createElement("span");
    action.className = "voice-action";
    const info = document.createElement("span");
    info.className = "voice-info";
    info.append(name, size);
    li.append(info, action);

    function button(label, aria, onClick, disabled) {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "btn btn-small";
      b.textContent = label;
      b.setAttribute("aria-label", aria);
      b.dataset.focusKey = v.name;
      b.disabled = disabled === true;
      b.addEventListener("click", onClick);
      return b;
    }

    if (running) {
      const label = running.cancelling ? "window.download.cancelling" : "window.download.cancel";
      action.appendChild(
        button(lexia.t(label), lexia.tf("window.download.cancelFor", { voice: v.name }), cancel, running.cancelling)
      );
      const bar = document.createElement("div");
      bar.className = "voice-progress";
      bar.innerHTML = '<div class="bar" role="progressbar"></div><span class="progress-text"></span>';
      bar.querySelector("[role=progressbar]").setAttribute("aria-label", lexia.tf("window.download.progressLabel", { voice: v.name }));
      li.appendChild(bar);
    } else if (failed) {
      action.appendChild(
        button(lexia.t("window.download.retry"), lexia.tf("window.download.retryFor", { voice: v.name }), function () {
          startDownload(v);
        }, download !== null)
      );
      const err = document.createElement("p");
      err.className = "notice voice-error";
      err.setAttribute("role", "alert");
      fillNotice(err, "bad", problemText("download", failed.problem, failed.language, failed.quality));
      li.appendChild(err);
    } else if (isInstalled(v)) {
      const ok = document.createElement("span");
      ok.className = "installed";
      ok.insertAdjacentHTML("beforeend", MARK_OK);
      const word = document.createElement("span");
      word.textContent = lexia.t("window.download.installed");
      ok.appendChild(word);
      action.appendChild(ok);
    } else {
      action.appendChild(
        button(lexia.t("window.download.get"), lexia.tf("window.download.getFor", { voice: v.name }), function () {
          startDownload(v);
        }, download !== null)
      );
    }
    return li;
  }

  // The values that change often (the bar and its text) are set apart, so a progress message does not
  // rebuild the rows and take the focus away from Cancel.
  function drawProgress() {
    if (!download) return;
    const row = body.querySelector('li[data-name="' + CSS.escape(download.name) + '"]');
    if (!row) return;
    const total = download.total > 0 ? download.total : 1;
    const done = Math.min(download.done, total);
    const bar = row.querySelector("[role=progressbar]");
    if (!bar) return;
    const percent = Math.round((done / total) * 100);
    bar.style.setProperty("--fill", percent + "%");
    bar.setAttribute("aria-valuemin", "0");
    bar.setAttribute("aria-valuemax", "100");
    bar.setAttribute("aria-valuenow", String(percent));
    const text = progressText(done, total);
    bar.setAttribute("aria-valuetext", text);
    row.querySelector(".progress-text").textContent = text;
  }

  function renderSheet() {
    if (!lexia.i18n.ready) return;
    const focused = document.activeElement && sheet.contains(document.activeElement) ? document.activeElement.dataset.focusKey : null;

    // The note above the list.
    if (message && !failure && !loading) {
      note.hidden = false;
      if (message.kind === "done") fillNotice(note, "ok", lexia.tf("window.download.done", { voice: message.name }));
      else fillNotice(note, "plain", lexia.t("window.download.cancelled"));
    } else {
      note.hidden = true;
    }

    const scrolled = body.scrollTop; // the list is drawn again, but the person keeps the place
    body.textContent = "";
    body.dataset.view = failure ? "failed" : loading || voices === null ? "loading" : visibleVoices().length === 0 ? "empty" : "list";
    if (failure) {
      const err = document.createElement("div");
      err.className = "catalogue-state";
      const text = document.createElement("p");
      text.className = "notice";
      text.setAttribute("role", "alert");
      fillNotice(text, "bad", problemText("catalogue", failure.problem));
      const retry = document.createElement("button");
      retry.type = "button";
      retry.className = "btn";
      retry.id = "catalogue-retry";
      retry.dataset.focusKey = "retry";
      retry.textContent = lexia.t("window.download.retry");
      retry.addEventListener("click", reload);
      err.append(text, retry);
      body.appendChild(err);
    } else if (loading || voices === null) {
      const wait = document.createElement("p");
      wait.className = "catalogue-state catalogue-wait";
      wait.setAttribute("role", "status");
      wait.textContent = lexia.t("window.download.loading");
      body.appendChild(wait);
    } else {
      const rows = visibleVoices();
      if (rows.length === 0) {
        const none = document.createElement("p");
        none.className = "catalogue-state";
        none.textContent = lexia.t("window.download.empty");
        body.appendChild(none);
      } else {
        const list = document.createElement("ul");
        list.className = "voice-list";
        for (const v of rows) list.appendChild(makeRow(v));
        body.appendChild(list);
      }
    }

    body.scrollTop = scrolled;
    if (reveal) {
      const shown = body.querySelector('li[data-name="' + CSS.escape(reveal) + '"]');
      if (shown) shown.scrollIntoView({ block: "nearest" });
      reveal = null;
    }

    // The focus stays on the control that had it, also when the control became another one
    // (Download -> Cancel -> Try again).
    // If it cannot (the button is gone or disabled), it goes to Close, not to nowhere.
    if (focused) {
      const again = Array.prototype.filter.call(body.querySelectorAll("[data-focus-key]"), function (el) {
        return el.dataset.focusKey === focused && !el.disabled;
      })[0];
      (again || $("catalogue-close")).focus();
    }
    drawProgress();
  }

  // The download icon opens the sheet; js/voice.js sends openCatalogue.
  $("add-download").addEventListener("click", function () {
    message = null;
    failedRow = null;
    failure = null;
    voices = null;
    loading = true;
    installedNow = new Set();
    openSheet(sheet, $("catalogue-close"));
    renderSheet();
  });

  lexia.on("catalogue", function (msg) {
    const list = Array.isArray(msg.voices) ? msg.voices : [];
    voices = list
      .filter(function (v) {
        return v && typeof v.name === "string" && v.name !== "" && typeof v.language === "string" && Number.isFinite(v.sizeBytes) && v.sizeBytes >= 0;
      })
      .map(function (v) {
        return { name: v.name, language: v.language, sizeBytes: v.sizeBytes, installed: v.installed === true };
      });
    loading = false;
    failure = null;
    if (!sheet.open) {
      message = null;
      openSheet(sheet, $("catalogue-close"));
    }
    renderSheet();
  });
  lexia.on("catalogueFailed", function (msg) {
    loading = false;
    voices = null;
    failure = { problem: msg.problem };
    if (!sheet.open) openSheet(sheet, $("catalogue-close"));
    renderSheet();
  });
  lexia.on("downloadProgress", function (msg) {
    if (typeof msg.name !== "string" || !Number.isFinite(msg.done) || !Number.isFinite(msg.total)) return;
    const same = download && download.name === msg.name;
    download = { name: msg.name, done: msg.done, total: msg.total, cancelling: same ? download.cancelling : false };
    failedRow = null;
    if (same && body.querySelector('li[data-name="' + CSS.escape(msg.name) + '"][data-mode="downloading"]')) drawProgress();
    else renderSheet(); // the first message of a download, or one the page did not start
  });
  lexia.on("downloadDone", function (msg) {
    if (typeof msg.name !== "string") return;
    download = null;
    failedRow = null;
    installedNow.add(msg.name);
    message = { kind: "done", name: msg.name };
    reveal = msg.name;
    renderSheet();
  });
  lexia.on("downloadFailed", function (msg) {
    if (typeof msg.name !== "string") return;
    download = null;
    if (msg.problem === "cancelled") {
      failedRow = null;
      message = { kind: "cancelled", name: msg.name };
    } else {
      message = null;
      reveal = msg.name; // the error is in its row, which may be out of sight
      failedRow = { name: msg.name, problem: msg.problem, language: msg.language, quality: msg.quality };
    }
    renderSheet();
  });

  lexia.on("state", function (msg) {
    state = msg;
    renderSheet();
  });
  lexia.on("strings", function () {
    renderSheet();
    renderResult();
  });

  // ---- the result sheet: Add from file, Delete a Voice, a Preset file -------------------------
  // outcome = { ok, title: () => text, text: () => text, again } ; the texts are made when drawn, so a new strings table shows.
  let outcome = null;

  function renderResult() {
    if (!outcome || !lexia.i18n.ready) return;
    $("file-result-title").textContent = outcome.title();
    fillNotice($("file-result-text"), outcome.ok ? "ok" : "bad", outcome.text());
    $("file-result-again").hidden = outcome.again !== true;
  }
  function showResult(next) {
    outcome = next;
    renderResult();
    if (!lexia.i18n.ready) return;
    if (result.open) return;
    openSheet(result, $("file-result-close"));
  }
  function fileOutcome(ok, msg) {
    return {
      ok: ok,
      again: !ok,
      title: function () {
        return lexia.t(ok ? "window.addFile.addedTitle" : "window.addFile.refusedTitle");
      },
      text: function () {
        if (!ok) return problemText("file", msg.problem, msg.language, msg.quality);
        const name = typeof msg.language === "string" && msg.language !== "" ? lexia.t("language." + msg.language) : "";
        return lexia.tf("window.addFile.added", { voice: typeof msg.voice === "string" ? msg.voice : "", language: name });
      },
    };
  }
  function plainOutcome(ok, titleId, textId, facts) {
    return {
      ok: ok,
      again: false,
      title: function () {
        return lexia.t(titleId);
      },
      text: function () {
        return lexia.tf(textId, facts);
      },
    };
  }
  lexia.on("voiceAdded", function (msg) {
    showResult(fileOutcome(true, msg));
  });
  lexia.on("voiceRefused", function (msg) {
    showResult(fileOutcome(false, msg));
  });
  // Delete a Voice failed (the host answers a deleteVoice that did not work; a success is the next "state").
  lexia.on("voiceDeleteFailed", function (msg) {
    const voice = typeof msg.voice === "string" ? msg.voice : "";
    showResult(
      plainOutcome(false, "window.deleteVoice.failedTitle", msg.problem === "shipped" ? "window.deleteVoice.failedShipped" : "window.deleteVoice.failed", { voice: voice })
    );
  });
  // A preset file: saved, loaded, or not used. `name` is the file's name. problem: badFile, cannotWrite or noVoice.
  lexia.on("presetSaved", function (msg) {
    showResult(plainOutcome(true, "window.preset.savedTitle", "window.preset.saved", { name: typeof msg.name === "string" ? msg.name : "" }));
  });
  lexia.on("presetLoaded", function (msg) {
    showResult(plainOutcome(true, "window.preset.loadedTitle", "window.preset.loaded", { name: typeof msg.name === "string" ? msg.name : "" }));
  });
  lexia.on("presetFailed", function (msg) {
    const id = msg.problem === "cannotWrite" ? "window.preset.cannotWrite" : msg.problem === "noVoice" ? "window.preset.noVoice" : "window.preset.badFile";
    showResult(plainOutcome(false, "window.preset.failedTitle", id, {}));
  });
  // Choose another file: close the result, ask the host for the file dialog again.
  $("file-result-again").addEventListener("click", function () {
    if (result.open) result.close();
    lexia.send("addFromFile");
  });
})();
