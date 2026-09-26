import { highlightRect } from "../src/lib/study.js";
import fs from "node:fs";
const home = JSON.parse(fs.readFileSync("src/content/study.json", "utf8")).home;
for (const b of [1, 1.5, 2, 3]) console.log("home below", b, JSON.stringify(highlightRect({ ...home, highlight: { below: b } })));
const P = JSON.parse(fs.readFileSync("src/content/projects.json", "utf8"));
for (const p of P) for (const hl of [{ below: 1.5 }, { below: 3 }, { above: 6 }, { above: 9 }]) console.log(p.slug, JSON.stringify(hl), JSON.stringify(highlightRect({ ...p.study, highlight: hl })));
