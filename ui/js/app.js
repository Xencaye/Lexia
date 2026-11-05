// The Settings window page (D8). It draws only what the last "state" message says: it keeps no copy
// of a setting. A user action sends a message to the host; the screen changes when the next
// "state" arrives (the mock host answers at once, and so will the C++ host). The one thing the
// page owns is which of the six pages is shown.
//
// The whole bridge contract, with the full "state", is in ui/README.md. This file reads on, dark and
// masterVolume; it sends ready, setOn and setMasterVolume.
//
// Pages: home, voices, settings (the top links) and report, help, about (the bottom-left links).
// ?page=voices opens that page first (for screenshots, and for the host to open Lexia on a page);
// ?tab=voice is an older name that still works.
(function () {
  "use strict";

  const lexia = window.lexia;
  const PAGES = ["home", "voices", "settings", "report", "help", "about"];

  const $ = function (id) {
    return document.getElementById(id);
  };
  const el = {
    power: $("power"),
    enable: $("enable"),
    bee: $("bee"),
    volume: $("master-volume"),
  };

  let state = null; // the last "state" message
  let page = "home";

  // ---- pages -----------------------------------------------------------------------------
  function showPage(name) {
    if (PAGES.indexOf(name) < 0) name = "home";
    page = name;
    for (const section of document.querySelectorAll("section.page")) section.hidden = section.dataset.page !== name;
    for (const link of document.querySelectorAll("nav [data-page]")) {
      if (link.dataset.page === name) link.setAttribute("aria-current", "page");
      else link.removeAttribute("aria-current");
    }
  }
  lexia.showPage = showPage;

  for (const link of document.querySelectorAll("nav [data-page]")) {
    link.addEventListener("click", function () {
      showPage(link.dataset.page);
    });
  }

  // ---- Home ------------------------------------------------------------------------------
  // The circle and the button under it both turn Lexia On or Off.
  function toggle() {
    if (state) lexia.send("setOn", { on: state.on !== true });
  }
  el.power.addEventListener("click", toggle);
  el.enable.addEventListener("click", toggle);

  // ---- Master volume ---------------------------------------------------------------------
  const volume = lexia.makeSlider(el.volume, {
    get: function () {
      return state ? state.masterVolume : null;
    },
    onValue: function (value) {
      lexia.send("setMasterVolume", { value: value });
    },
  });

  // ---- drawing the state -----------------------------------------------------------------
  function render() {
    if (!state || !lexia.i18n.ready) return;

    // The host says which theme is in use (`dark`); an older host that does not gets Windows' own choice.
    const root = document.documentElement;
    let dark = state.dark;
    if (typeof dark !== "boolean") {
      dark = state.theme === "dark" || (state.theme !== "light" && window.matchMedia("(prefers-color-scheme: dark)").matches);
    }
    root.dataset.theme = dark ? "dark" : "light";

    const on = state.on === true;
    el.power.disabled = false;
    el.power.setAttribute("aria-pressed", on ? "true" : "false");
    el.enable.disabled = false;
    el.enable.dataset.on = on ? "true" : "false";
    el.enable.textContent = lexia.t(on ? "window.home.disable" : "window.home.enable");
    el.bee.dataset.on = on ? "true" : "false";

    const mv = state.masterVolume;
    volume.draw(mv && Number.isFinite(mv.value) ? lexia.tf("window.volumeValue", { value: Math.round(mv.value) }) : "");
    el.volume.title = mv && Number.isFinite(mv.value) ? lexia.tf("window.volumeValue", { value: Math.round(mv.value) }) : "";

    // Transitions start only after the first state has been painted (base.css).
    if (!("drawn" in root.dataset)) {
      requestAnimationFrame(function () {
        requestAnimationFrame(function () {
          root.dataset.drawn = "";
        });
      });
    }
  }

  lexia.on("state", function (msg) {
    state = msg;
    render();
  });
  lexia.on("strings", render);

  // ---- start -----------------------------------------------------------------------------
  const params = new URLSearchParams(location.search);
  let first = params.get("page") || params.get("tab") || "home";
  if (first === "voice") first = "voices";
  showPage(first);
  lexia.send("ready");
})();
