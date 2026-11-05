// The bridge: the one seam between the page and its host. Plain classic script (a page opened as
// file:// cannot load ES modules), one namespace: window.lexia.
//
// Both ways carry JSON objects of the form { "type": string, ...payload }.
//   lexia.send(type, payload)  page -> host, e.g. lexia.send("setOn", { on: true })
//   lexia.on(type, fn)         host -> page; fn gets the whole message
// Inside WebView2 the transport is window.chrome.webview. In a browser the mock host
// (dev/mock_host.js) sets itself as the transport. A test sets its own and calls lexia.bridge.receive.
//
// The whole contract (every message and the full "state") is in ui/README.md. This file knows no
// message by name.
(function () {
  "use strict";

  const lexia = (window.lexia = window.lexia || {});
  const handlers = Object.create(null);
  let transport = null;

  function receive(msg) {
    if (!msg || typeof msg !== "object" || typeof msg.type !== "string") {
      console.warn("Lexia bridge: a message without a type was ignored", msg);
      return;
    }
    for (const fn of handlers[msg.type] || []) {
      try {
        fn(msg);
      } catch (e) {
        console.error("Lexia bridge: the handler of '" + msg.type + "' failed", e);
      }
    }
  }

  lexia.on = function (type, fn) {
    (handlers[type] = handlers[type] || []).push(fn);
  };

  lexia.send = function (type, payload) {
    const msg = Object.assign({ type: type }, payload);
    if (transport) transport(msg);
    else console.warn("Lexia bridge: no host to send '" + type + "' to");
  };

  lexia.bridge = {
    receive: receive,
    // For the mock host and for tests: where page -> host messages go.
    setTransport: function (fn) {
      transport = fn;
    },
    get hasHost() {
      return transport !== null;
    },
  };

  if (window.chrome && window.chrome.webview) {
    const wv = window.chrome.webview;
    transport = function (msg) {
      wv.postMessage(msg);
    };
    wv.addEventListener("message", function (e) {
      receive(e.data);
    });
  }
})();
