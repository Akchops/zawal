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
