// The report and about pages (D8). Static pages: the words come from the strings table.
//
// page -> host
//   openLink { link }   link is "mail", "discord", "github" or "itch". The page never sends a URL
//                       (ADR-0005): the host maps the name to its fixed address and opens it.
//   openNotices         the link of the about page: the host opens THIRD-PARTY-NOTICES.txt
// state field used: version ("0.5.0").
(function () {
  "use strict";

  const lexia = window.lexia;
  const $ = function (id) {
    return document.getElementById(id);
  };

  // Every button with data-link (report: mail, discord, github; about: itch, github).
  for (const button of document.querySelectorAll("[data-link]")) {
    button.addEventListener("click", function () {
      lexia.send("openLink", { link: button.dataset.link });
    });
  }
  $("about-notices").addEventListener("click", function () {
    lexia.send("openNotices");
  });

  // A text with {name} placeholders where the named words are bold: "built with {games} and {comics} in mind".
  function richText(target, template, bold) {
    target.textContent = "";
    for (const part of template.split(/(\{\w+\})/)) {
      const m = /^\{(\w+)\}$/.exec(part);
      if (m && Object.prototype.hasOwnProperty.call(bold, m[1])) {
        const strong = document.createElement("strong");
        strong.textContent = bold[m[1]];
        target.appendChild(strong);
      } else if (part !== "") {
        target.appendChild(document.createTextNode(part));
      }
    }
  }

  // A text with one {link} placeholder, and the link element put in its place: "View third-party notices
  // {link}." The element is moved, not copied, so its click handler stays.
  function linkText(target, template, link) {
    target.textContent = "";
    for (const part of template.split(/(\{link\})/)) {
      if (part === "{link}") target.appendChild(link);
      else if (part !== "") target.appendChild(document.createTextNode(part));
    }
    if (!link.isConnected) target.appendChild(link); // a template without {link}: the link goes at the end
  }

  let state = null;
  function render() {
    if (!state || !lexia.i18n.ready) return;
    $("about-version").textContent = " | " + lexia.tf("window.about.version", { version: state.version || "" });
    richText($("about-intro"), lexia.t("window.about.intro"), {
      games: lexia.t("window.about.games"),
      comics: lexia.t("window.about.comics"),
    });
    linkText($("about-notices-line"), lexia.t("window.about.noticesLine"), $("about-notices"));
  }
  lexia.on("state", function (msg) {
    state = msg;
    render();
  });
  lexia.on("strings", render);
})();
