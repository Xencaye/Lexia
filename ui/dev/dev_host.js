// Chooses what plays the host when the page is not inside WebView2. One classic script, run while
// the page is parsed, so what it writes runs before js/app.js (a script that is added later would
// run after the page has already said "ready").
//   index.html          the mock host with sample data (dev/mock_host.js) and the C++ strings
//   index.html?test     the bridge tests (dev/tests.js): the test plays the host
// Inside WebView2 (window.chrome.webview) it does nothing. D3 leaves dev\ out of the package and
// takes this tag out of index.html.
(function () {
  "use strict";
  if (window.chrome && window.chrome.webview) return;

  function load(src) {
    document.write('<script src="' + src + '"><\/script>');
  }

  if (new URLSearchParams(location.search).has("test")) {
    load("dev/tests.js");
    load("dev/tests_settings.js"); // the Settings tab, About and Help (D0 ui 2 and 3)
    load("dev/tests_voice_library.js"); // the Voice download and Add from file sheets (D0 ui 5 and 6)
  } else {
    load("dev/strings.js");
    load("dev/mock_host.js");
  }
})();
