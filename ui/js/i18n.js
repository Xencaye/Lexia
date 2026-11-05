// Words. The host sends the Interface language's table in a "strings" message
// ({ type: "strings", language: "en", strings: { id: text } }), made from the C++ table
// (src/strings/). The page holds no text of its own.
//
//   <span data-text="window.tab.home">     textContent
//   ... data-bold-room=""                  also gets the text, for the CSS that keeps room for its bold form
//   data-aria-label="id"                   aria-label
//   data-title="id"                        title
//   lexia.t("window.home.on")              text for code
//   lexia.tf("controlBar.skipTip", {key})  text with {name} placeholders filled
//
// A missing id shows as the id itself and is reported once with console.warn, so a gap is seen
// at once (and a test can see it).
(function () {
  "use strict";

  const lexia = (window.lexia = window.lexia || {});
  let strings = null;
  const reported = new Set();

  lexia.t = function (id) {
    if (strings && typeof strings[id] === "string") return strings[id];
    if (!reported.has(id)) {
      reported.add(id);
      console.warn("Lexia: missing string \"" + id + "\"");
    }
    return id;
  };

  lexia.tf = function (id, args) {
    return lexia.t(id).replace(/\{(\w+)\}/g, function (whole, name) {
      return args && Object.prototype.hasOwnProperty.call(args, name) ? String(args[name]) : whole;
    });
  };

  lexia.i18n = {
    get ready() {
      return strings !== null;
    },
    apply: function (root) {
      for (const el of root.querySelectorAll("[data-text]")) {
        el.textContent = lexia.t(el.dataset.text);
        // A menu word keeps the width of its bold form (components.css, .navlink): the CSS draws an
        // invisible bold copy from this attribute.
        if (el.hasAttribute("data-bold-room")) el.dataset.boldRoom = el.textContent;
      }
      for (const el of root.querySelectorAll("[data-aria-label]")) el.setAttribute("aria-label", lexia.t(el.dataset.ariaLabel));
      for (const el of root.querySelectorAll("[data-title]")) el.title = lexia.t(el.dataset.title);
    },
  };

  lexia.on("strings", function (msg) {
    strings = msg && typeof msg.strings === "object" && msg.strings ? msg.strings : {};
    reported.clear(); // a new table: a gap in it is reported again
    if (typeof msg.language === "string") document.documentElement.lang = msg.language;
    lexia.i18n.apply(document);
  });
})();
