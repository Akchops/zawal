import { defineConfig } from "astro/config";

// Static site. Trailing-slash directory URLs (/work/ghaf-house/) so the pages
// work from any static host, including the private preview.
export default defineConfig({
  output: "static",
  trailingSlash: "always",
  build: { format: "directory", inlineStylesheets: "always", assets: "z" },
  compressHTML: true,
  vite: {
    build: { assetsInlineLimit: 0, cssCodeSplit: true },
  },
  devToolbar: { enabled: false },
});
