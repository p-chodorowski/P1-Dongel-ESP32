// CDN location follows the script URL, so a firmware that rewrites the shell
// to @5.9.5.3 also loads language files from that same tag.
(function () {
  const src = (document.currentScript && document.currentScript.src) || "";
  const match = src.match(/\/gh\/([^/]+\/[^@/]+)@([^/]+)\//);
  window.CDN_REPO = match ? match[1] : "p-chodorowski/P1-Dongel-ESP32";
  window.CDN_REF = match ? match[2] : "5.9.5.3";
  window.CDN_BASE = `https://cdn.jsdelivr.net/gh/${window.CDN_REPO}@${window.CDN_REF}/cdn`;
})();
