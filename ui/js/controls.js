// Shared parts of the page: the slider and the sheet (a modal dialog). Classic script, loaded before
// voice.js, voice_library.js and app.js. No message is sent here: the callers do that.
//
//   lexia.makeSlider(el, { get, onValue, log })
//     el        a <div class="slider" role="slider"> with .slider-track and .slider-thumb
//     get()     the control of the last "state": { value, min, max, def, step }, or null when there is none
//     onValue   called with a value on the step grid and inside the range, only when it is a new value
//     log       true = the position is on a log scale (Speed: 1.0 is at the centre of 0.5 .. 2)
//     returns   { draw(text) } which moves the thumb and the aria values to what get() says now
//   The slider never keeps a value of its own: draw() shows the state. A press or a drag on the track
//   sets the value under the pointer; arrow keys, PageUp / PageDown, Home and End work.
//
//   lexia.sheet.open(dialog, focusEl)   showModal() and focus; remembers what had the focus before
//   lexia.sheet.wire(dialog, closeBtn)  Esc and the close button close it; the focus goes back
(function () {
  "use strict";

  const lexia = (window.lexia = window.lexia || {});

  // ---- numbers ---------------------------------------------------------------------------------
  function rounded(value) {
    return Math.round(value * 1e6) / 1e6;
  }
  // `value` moved onto the step grid of the control and held in its range.
  function snap(ctl, value) {
    const steps = Math.round((value - ctl.min) / ctl.step);
    return Math.min(ctl.max, Math.max(ctl.min, rounded(ctl.min + steps * ctl.step)));
  }
  function usable(ctl) {
    return (
      ctl &&
      [ctl.value, ctl.min, ctl.max, ctl.def, ctl.step].every(Number.isFinite) &&
      ctl.min < ctl.max &&
      ctl.step > 0
    );
  }

  // ---- the slider ------------------------------------------------------------------------------
  lexia.makeSlider = function (el, opts) {
    let lastSent = null;

    function current() {
      const ctl = opts.get();
      return usable(ctl) ? ctl : null;
    }
    const useLog = function (ctl) {
      return opts.log === true && ctl.min > 0;
    };
    // Position 0..1 of a value, and the value at a position.
    function toPos(ctl, v) {
      if (useLog(ctl)) return (Math.log(v) - Math.log(ctl.min)) / (Math.log(ctl.max) - Math.log(ctl.min));
      return (v - ctl.min) / (ctl.max - ctl.min);
    }
    function fromPos(ctl, p) {
      if (useLog(ctl)) return ctl.min * Math.pow(ctl.max / ctl.min, p);
      return ctl.min + p * (ctl.max - ctl.min);
    }

    function send(value) {
      const ctl = current();
      if (!ctl) return;
      value = snap(ctl, value); // never outside the range from the state
      if (value === ctl.value || value === lastSent) return; // nothing changes: no message
      lastSent = value;
      opts.onValue(value);
    }

    el.addEventListener("keydown", function (e) {
      const ctl = current();
      if (!ctl) return;
      let target = null;
      if (e.key === "ArrowRight" || e.key === "ArrowUp") target = ctl.value + ctl.step;
      else if (e.key === "ArrowLeft" || e.key === "ArrowDown") target = ctl.value - ctl.step;
      else if (e.key === "PageUp") target = ctl.value + 10 * ctl.step;
      else if (e.key === "PageDown") target = ctl.value - 10 * ctl.step;
      else if (e.key === "Home") target = ctl.min;
      else if (e.key === "End") target = ctl.max;
      if (target === null) return;
      e.preventDefault();
      send(target);
    });

    // Pointer: a press or a drag on the track sets the value under the pointer.
    function fromPointer(e) {
      const ctl = current();
      if (!ctl) return;
      const box = el.getBoundingClientRect();
      if (box.width <= 0) return;
      const p = Math.min(1, Math.max(0, (e.clientX - box.left) / box.width));
      send(fromPos(ctl, p));
    }
    el.addEventListener("pointerdown", function (e) {
      if (e.button !== 0 || !current()) return;
      el.focus();
      try {
        el.setPointerCapture(e.pointerId); // keeps the drag when the pointer leaves the slider
      } catch (err) {
        // no such pointer (a test, or a pen that went away): the press still counts
      }
      el.dataset.dragging = "";
      fromPointer(e);
    });
    el.addEventListener("pointermove", function (e) {
      if ("dragging" in el.dataset) fromPointer(e);
    });
    function endDrag() {
      delete el.dataset.dragging;
    }
    el.addEventListener("pointerup", endDrag);
    el.addEventListener("pointercancel", endDrag);

    return {
      // Shows what the state says. `text` is the value as the person reads it (aria-valuetext).
      draw: function (text) {
        const ctl = current();
        if (ctl) {
          el.style.setProperty("--pos", String(toPos(ctl, ctl.value) * 100));
          el.setAttribute("aria-valuemin", String(ctl.min));
          el.setAttribute("aria-valuemax", String(ctl.max));
          el.setAttribute("aria-valuenow", String(ctl.value));
          if (text) el.setAttribute("aria-valuetext", text);
          el.removeAttribute("aria-disabled");
          el.tabIndex = 0;
          lastSent = null;
        } else {
          el.setAttribute("aria-disabled", "true");
          for (const a of ["aria-valuemin", "aria-valuemax", "aria-valuenow", "aria-valuetext"]) el.removeAttribute(a);
          el.tabIndex = -1;
        }
      },
    };
  };

  // ---- sheets ----------------------------------------------------------------------------------
  const openers = new WeakMap(); // dialog -> what had the focus when it opened

  lexia.sheet = {
    open: function (dialog, focus) {
      if (dialog.open) return;
      openers.set(dialog, document.activeElement);
      dialog.showModal();
      if (focus) focus.focus();
    },
    wire: function (dialog, closeButton) {
      dialog.addEventListener("close", function () {
        const opener = openers.get(dialog);
        if (opener && opener.isConnected) opener.focus();
        openers.delete(dialog);
      });
      // A real Esc closes the dialog by itself; this also covers a key event made by a script.
      dialog.addEventListener("keydown", function (e) {
        if (e.key === "Escape") {
          e.preventDefault();
          if (dialog.open) dialog.close();
        }
      });
      if (closeButton) {
        closeButton.addEventListener("click", function () {
          if (dialog.open) dialog.close();
        });
      }
    },
  };
})();
