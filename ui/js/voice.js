// The voices page (D8): the Voice with three icon buttons (download, add from file, delete), the
// three Voice controls (speed, pitch, volume) with a reset beside a changed one, and the preset files
// (save, load).
//
// The whole bridge contract is in ui/README.md; this header says what this file uses of it.
//
// Like the rest of the page it draws only what the last "state" says and keeps no copy of a
// setting. A user action sends a message; the control moves when the next "state" comes. It never
// sends a value outside the range that the state gives (the host clamps too, VoiceControls).
//
// The parts of "state" that this page reads (see ui/dev/mock_host.js for a full example):
//   language: "eng"                                  only finds the entry of `voices` (English only in 0.5, ADR-0007)
//   voices: { eng: { installed: ["name", ...],       installed Voices, by name
//                    active: "name",                 the one that speaks; "" or absent = none
//                    removable: ["name", ...] } }    the Voices a person may delete (not the shipped ones)
//   controls: { speed|pitch|volume:                  the active Voice's controls
//               { value, min, max, def, step } }     RangeOf() of voice_controls.h, and the step
// Page -> host: setVoice {language, voice}, setControl {name, value}, resetControl {name},
//   openCatalogue, addFromFile (these two are sent from the download and add buttons),
//   deleteVoice {voice}, savePresetFile, loadPresetFile.
// The results (voiceDeleteFailed, presetSaved, presetLoaded, presetFailed) are shown by js/voice_library.js.
(function () {
  "use strict";

  const lexia = window.lexia;
  const $ = function (id) {
    return document.getElementById(id);
  };

  // The Voice controls, in the order of the page.
  const CONTROLS = ["speed", "pitch", "volume"];

  const el = {
    voice: $("voice-voice"),
    none: $("voice-none"),
    controls: $("voice-controls"),
    file: $("add-file"),
    download: $("add-download"),
    del: $("voice-delete"),
    save: $("preset-save"),
    load: $("preset-load"),
    dialog: $("delete-voice"),
  };
  if (!el.controls) return; // a page without the voices page

  let state = null; // the last "state" message
  let hasVoice = false; // a Voice is installed and active
  let deleting = ""; // the Voice the open confirmation is about

  // The value as the person reads it. Speed 1.0x is the Voice's own pace; Pitch is in semitones
  // (the speech keeps its length); Volume is a percentage of the Voice's own.
  function text(name, value) {
    switch (name) {
      case "speed": {
        let s = String(Math.round(value * 100) / 100);
        if (s.indexOf(".") < 0) s += ".0";
        return lexia.tf("window.voice.speedValue", { value: s });
      }
      case "pitch": {
        const n = Math.round(value * 10) / 10;
        const sign = n > 0 ? "+" : n < 0 ? "−" : "";
        return lexia.tf(Math.abs(n) === 1 ? "window.voice.pitchValueOne" : "window.voice.pitchValue", {
          value: sign + Math.abs(n),
        });
      }
      default:
        return lexia.tf("window.voice.percentValue", { value: Math.round(value * 100) });
    }
  }

  // ---- the sliders ---------------------------------------------------------------------------
  const rows = {};
  for (const name of CONTROLS) {
    const row = el.controls.querySelector('[data-control="' + name + '"]');
    const slider = row.querySelector(".slider");
    rows[name] = {
      slider: slider,
      reset: row.querySelector("button"),
      api: lexia.makeSlider(slider, {
        get: function () {
          return hasVoice && state && state.controls ? state.controls[name] : null;
        },
        onValue: function (value) {
          lexia.send("setControl", { name: name, value: value });
        },
        log: name === "speed", // 1.0 is the centre of 0.5 .. 2
      }),
    };
    rows[name].reset.addEventListener("click", function () {
      lexia.send("resetControl", { name: name });
    });
  }

  // ---- Voice ---------------------------------------------------------------------------------
  function entry() {
    return state && state.voices ? state.voices[state.language] : null;
  }
  function installed() {
    const e = entry();
    return e && Array.isArray(e.installed) ? e.installed : [];
  }
  function removable() {
    const e = entry();
    return e && Array.isArray(e.removable) ? e.removable : [];
  }
  function active() {
    const e = entry();
    return e && typeof e.active === "string" ? e.active : "";
  }
  function fillSelect(select, values) {
    const signature = values.join("\n");
    if (select.dataset.signature !== signature) {
      select.textContent = "";
      for (const v of values) {
        const option = document.createElement("option");
        option.value = v;
        option.textContent = v;
        select.appendChild(option);
      }
      select.dataset.signature = signature;
    }
  }

  el.voice.addEventListener("change", function () {
    lexia.send("setVoice", { language: state.language, voice: el.voice.value });
    render();
  });

  // The two buttons that add a Voice. js/voice_library.js also listens to the download one (it opens its sheet).
  el.download.addEventListener("click", function () {
    lexia.send("openCatalogue");
  });
  el.file.addEventListener("click", function () {
    lexia.send("addFromFile");
  });

  // Delete: only a Voice that is not shipped, and only after a confirmation.
  function canDelete() {
    return hasVoice && removable().indexOf(active()) >= 0;
  }
  lexia.sheet.wire(el.dialog, $("delete-voice-cancel"));
  el.del.addEventListener("click", function () {
    if (!canDelete()) return; // aria-disabled: the button explains itself with its tooltip
    deleting = active();
    $("delete-voice-title").textContent = lexia.tf("window.deleteVoice.title", { voice: deleting });
    lexia.sheet.open(el.dialog, $("delete-voice-cancel"));
  });
  $("delete-voice-confirm").addEventListener("click", function () {
    const voice = deleting;
    deleting = "";
    if (el.dialog.open) el.dialog.close();
    if (voice) lexia.send("deleteVoice", { voice: voice });
  });

  // ---- preset files ----------------------------------------------------------------------------
  el.save.addEventListener("click", function () {
    lexia.send("savePresetFile");
  });
  el.load.addEventListener("click", function () {
    lexia.send("loadPresetFile");
  });

  // ---- drawing the state ---------------------------------------------------------------------
  function render() {
    if (!state || !lexia.i18n.ready) return;

    const names = installed();
    const now = active();
    hasVoice = names.length > 0 && names.indexOf(now) >= 0;
    fillSelect(el.voice, names);
    el.voice.value = now;
    el.voice.disabled = names.length === 0;

    el.none.hidden = names.length > 0;
    el.none.textContent = lexia.tf("window.voice.none", { language: lexia.t("language." + state.language) });

    // Delete: the tooltip says why it cannot be used for a Voice that comes with Lexia.
    const may = canDelete();
    el.del.setAttribute("aria-disabled", may ? "false" : "true");
    el.del.title = lexia.t(hasVoice && !may ? "window.voice.deleteShipped" : "window.voice.delete");
    el.save.disabled = !hasVoice;
    el.load.disabled = !hasVoice;

    for (const name of CONTROLS) {
      const row = rows[name];
      const ctl = hasVoice && state.controls ? state.controls[name] : null;
      const label = lexia.t("window.voice." + name);
      row.slider.setAttribute("aria-label", label);
      const shown = ctl && Number.isFinite(ctl.value) ? text(name, ctl.value) : "";
      row.slider.title = shown;
      row.api.draw(shown);
      // Reset shows only beside a value that is not the default.
      row.reset.hidden = !(ctl && Number.isFinite(ctl.value) && Number.isFinite(ctl.def) && ctl.value !== ctl.def);
      row.reset.setAttribute("aria-label", lexia.tf("window.voice.resetFor", { control: label }));
      row.reset.title = lexia.tf("window.voice.resetFor", { control: label });
    }
  }

  lexia.on("state", function (msg) {
    state = msg;
    render();
  });
  lexia.on("strings", render);
})();
