// Resolves the newest AirGlass Windows asset and redirects straight to it, so a
// visitor never has to open the repository page to find the download.
// Cached at the edge; falls back to the releases page if the API is unreachable.

const REPO = "bagjasatrio/AirGlass";

module.exports = async function handler(req, res) {
  try {
    const api = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, {
      headers: {
        Accept: "application/vnd.github+json",
        "User-Agent": "airglass-site",
      },
    });
    if (!api.ok) throw new Error(`github ${api.status}`);

    const release = await api.json();
    const assets = Array.isArray(release.assets) ? release.assets : [];
    const asset =
      assets.find((a) => /win64.*\.zip$/i.test(a.name)) ||
      assets.find((a) => /\.zip$/i.test(a.name));

    if (!asset) throw new Error("no zip asset");
    if (asset.size) res.setHeader("X-Asset-Size", String(asset.size));

    res.setHeader("Cache-Control", "public, max-age=300, s-maxage=1800, stale-while-revalidate=86400");
    res.writeHead(302, { Location: asset.browser_download_url });
    res.end();
  } catch (error) {
    res.setHeader("Cache-Control", "public, max-age=60, s-maxage=300");
    res.writeHead(302, { Location: `https://github.com/${REPO}/releases/latest` });
    res.end();
  }
};
