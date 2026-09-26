// Site chrome shared by every page: the mobile menu (a full-screen sheet
// whose items are uncovered by a shadow sweep), and escape/focus handling.
const toggle = document.querySelector("[data-menu-toggle]");
const menu = document.querySelector("[data-menu]");
if (toggle && menu) {
  const set = (open) => {
    toggle.setAttribute("aria-expanded", String(open));
    toggle.textContent = open ? "Close" : "Menu";
    menu.hidden = !open;
    menu.classList.toggle("open", open);
    document.documentElement.style.overflow = open ? "hidden" : "";
    if (open) menu.querySelector("a")?.focus();
  };
  toggle.addEventListener("click", () => set(menu.hidden));
  document.addEventListener("keydown", (e) => { if (e.key === "Escape" && !menu.hidden) { set(false); toggle.focus(); } });
  menu.addEventListener("click", (e) => { if (e.target.closest("a")) set(false); });
}

// Page transitions: the page being left takes the pattern of the page it goes
// to (links to a project carry data-vt), so both halves share one shadow.
document.addEventListener("click", (e) => {
  const a = e.target.closest?.("a[href]");
  if (!a || a.target || e.defaultPrevented || e.metaKey || e.ctrlKey || e.shiftKey) return;
  document.documentElement.dataset.vt = a.dataset.vt || "stars";
});

// The nav steps out of the way while reading down and comes back on the way
// up (or near the top), so it never sits over a line being read.
const nav = document.querySelector("[data-nav]");
if (nav) {
  let lastY = scrollY, ticking = false;
  addEventListener("scroll", () => {
    if (ticking) return;
    ticking = true;
    requestAnimationFrame(() => {
      const y = scrollY, dy = y - lastY;
      if (Math.abs(dy) > 6) { nav.classList.toggle("away", dy > 0 && y > 120); lastY = y; }
      ticking = false;
    });
  }, { passive: true });
  nav.addEventListener("focusin", () => nav.classList.remove("away"));
}
