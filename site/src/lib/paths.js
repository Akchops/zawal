// Every URL a script fetches is resolved against the site root, which each
// page states relative to itself (<html data-root="../../">). The built site
// then works from any folder: a domain root, or the private preview.
const root = new URL(document.documentElement.dataset.root || "./", location.href);

/** "/seq/a-p.json" -> absolute URL under the site root. */
export const url = (p) => new URL(String(p).replace(/^\//, ""), root).href;
