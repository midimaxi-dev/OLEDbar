(() => {
  "use strict";

  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const settings = { level: 70, delayMs: 3000, fadeDimMs: 1500, fadeWakeMs: 120 };
  const demos = [];

  // --- Simulated taskbar -------------------------------------------------

  class TaskbarDemo {
    constructor(stage) {
      this.stage = stage;
      const tpl = document.getElementById("taskbar-tpl");
      this.bar = tpl.content.firstElementChild.cloneNode(true);
      stage.appendChild(this.bar);
      this.overlay = this.bar.querySelector(".tb-overlay");
      this.statusText = stage.querySelector(".status-text");
      this.hovering = false;
      this.dimAt = 0;
      this.timer = 0;

      const wake = () => { this.hovering = true; this.wake(); };
      const leave = () => { this.hovering = false; this.scheduleDim(); };
      this.bar.addEventListener("pointerenter", wake);
      this.bar.addEventListener("pointerleave", leave);
      this.bar.addEventListener("focus", wake);
      this.bar.addEventListener("blur", leave);
      // Touch has no hover: a tap wakes, then it re-dims on its own.
      this.bar.addEventListener("pointerdown", (e) => {
        if (e.pointerType !== "mouse") { this.wake(); this.hovering = false; this.scheduleDim(); }
      });

      this.scheduleDim();
    }

    setOpacity(value, ms) {
      this.overlay.style.transitionDuration = `${ms}ms`;
      this.overlay.style.opacity = String(value);
    }

    wake() {
      clearTimeout(this.timer);
      this.dimAt = 0;
      this.stage.classList.remove("is-dim");
      this.setOpacity(0, settings.fadeWakeMs);
      this.statusText.textContent = this.hovering ? "Bright · you're using the taskbar" : "Bright";
    }

    scheduleDim() {
      clearTimeout(this.timer);
      if (this.hovering) return;
      this.dimAt = performance.now() + settings.delayMs;
      this.timer = setTimeout(() => this.dim(), settings.delayMs);
    }

    dim(instant = false) {
      clearTimeout(this.timer);
      this.dimAt = 0;
      this.stage.classList.add("is-dim");
      this.setOpacity(settings.level / 100, instant ? 0 : settings.fadeDimMs);
      this.statusText.textContent = `Dimmed · ${settings.level}% darker`;
    }

    tick(now) {
      if (this.dimAt) {
        const s = Math.max(0, Math.ceil((this.dimAt - now) / 1000));
        this.statusText.textContent = `Bright · dims in ${s}s`;
      }
    }

    applyLevel() {
      if (this.stage.classList.contains("is-dim")) this.dim(true);
    }
  }

  document.querySelectorAll("[data-demo]").forEach((el) => demos.push(new TaskbarDemo(el)));

  const tick = (now) => { demos.forEach((d) => d.tick(now)); requestAnimationFrame(tick); };
  requestAnimationFrame(tick);

  // --- Clock -------------------------------------------------------------

  const updateClock = () => {
    const d = new Date();
    const time = d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    const date = d.toLocaleDateString();
    document.querySelectorAll(".tb-time").forEach((el) => { el.textContent = time; });
    document.querySelectorAll(".tb-date").forEach((el) => { el.textContent = date; });
  };
  updateClock();
  setInterval(updateClock, 15000);

  // --- Settings controls -------------------------------------------------

  const level = document.getElementById("level");
  const delay = document.getElementById("delay");
  const levelOut = document.getElementById("level-out");
  const delayOut = document.getElementById("delay-out");
  const mini = demos.find((d) => d.stage.dataset.demo === "mini");
  let previewTimer = 0;

  const paintRange = (input) => {
    const p = ((input.value - input.min) / (input.max - input.min)) * 100;
    input.style.setProperty("--p", `${p}%`);
  };

  level.addEventListener("input", () => {
    settings.level = Number(level.value);
    levelOut.textContent = `${settings.level}% darker`;
    paintRange(level);
    demos.forEach((d) => d.applyLevel());
    // Live preview, like the real settings panel.
    if (mini && !mini.stage.classList.contains("is-dim")) {
      mini.dim(true);
      clearTimeout(previewTimer);
      previewTimer = setTimeout(() => { if (mini.hovering) mini.wake(); }, 1500);
    }
  });

  delay.addEventListener("input", () => {
    settings.delayMs = Number(delay.value) * 1000;
    delayOut.textContent = `${delay.value} s`;
    paintRange(delay);
  });

  paintRange(level);
  paintRange(delay);

  // --- Burn-in scroll story ---------------------------------------------

  const burn = document.getElementById("burn-in");
  const hoursNum = burn.querySelector(".hours-num");
  const ghostWithout = burn.querySelector('[data-ghost="without"]');
  const ghostWith = burn.querySelector('[data-ghost="with"]');
  const MAX_HOURS = 5000;
  let burnQueued = false;

  const updateBurn = () => {
    burnQueued = false;
    const rect = burn.getBoundingClientRect();
    const span = rect.height - window.innerHeight;
    const p = Math.min(1, Math.max(0, -rect.top / (span || 1)));
    const eased = p * p * (3 - 2 * p);
    hoursNum.textContent = Math.round(eased * MAX_HOURS).toLocaleString();
    ghostWithout.style.opacity = String(eased * 0.55);
    ghostWith.style.opacity = String(eased * 0.1);
  };
  const queueBurn = () => { if (!burnQueued) { burnQueued = true; requestAnimationFrame(updateBurn); } };
  window.addEventListener("scroll", queueBurn, { passive: true });
  window.addEventListener("resize", queueBurn);
  updateBurn();

  // --- Reveal and count-up ----------------------------------------------

  const countUp = (el) => {
    const target = Number(el.dataset.count);
    if (reduceMotion || !target) { el.textContent = String(target); return; }
    const start = performance.now();
    const dur = 1400;
    const step = (now) => {
      const t = Math.min(1, (now - start) / dur);
      el.textContent = String(Math.round(target * (1 - Math.pow(1 - t, 3))));
      if (t < 1) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
  };

  const revealTargets = document.querySelectorAll(".section-head, .big-stat, .stats li, .chips, .card, .dl-card, .shot, .faq-list, .download h2");
  if ("IntersectionObserver" in window && !reduceMotion) {
    revealTargets.forEach((el) => el.classList.add("reveal"));
    const io = new IntersectionObserver((entries) => {
      entries.forEach((e) => {
        if (!e.isIntersecting) return;
        e.target.classList.add("in");
        e.target.querySelectorAll("[data-count]").forEach(countUp);
        io.unobserve(e.target);
      });
    }, { threshold: 0.2 });
    revealTargets.forEach((el) => io.observe(el));
  }

  // --- Latest release ----------------------------------------------------

  const isGitHubUrl = (u) => typeof u === "string" && u.startsWith("https://github.com/midimaxi-dev/OLEDbar/");

  fetch("https://api.github.com/repos/midimaxi-dev/OLEDbar/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
    .then((r) => (r.ok ? r.json() : null))
    .then((rel) => {
      if (!rel || typeof rel.tag_name !== "string") return;
      document.querySelectorAll("[data-version]").forEach((el) => { el.textContent = rel.tag_name; });
      const assets = Array.isArray(rel.assets) ? rel.assets : [];
      const find = (suffix) => assets.find((a) => typeof a.name === "string" && a.name.endsWith(suffix));
      const urls = { setup: find("-setup.exe"), zip: find(".zip") };
      document.querySelectorAll("[data-dl]").forEach((el) => {
        const a = urls[el.dataset.dl];
        if (a && isGitHubUrl(a.browser_download_url)) el.href = a.browser_download_url;
      });
    })
    .catch(() => {});
})();
