// Two jobs: reveal sections on entry, and keep the download link honest.

const revealTargets = [
  ".hero-copy h1",
  ".hero-copy .lede",
  ".hero-copy .cta-row",
  ".hero .shot",
  ".facts-grid .fact",
  ".why-head",
  ".why-list li",
  ".preset-head",
  ".preset-grid .tile",
  ".auto-row",
  ".mirror .shot",
  ".mirror-copy",
  ".req-grid > div",
  ".limit-grid li",
  ".download-panel",
];

const pending = new Set();
let observer = null;

function arm() {
  const seen = new Map();
  for (const selector of revealTargets) {
    for (const node of document.querySelectorAll(selector)) {
      node.setAttribute("data-reveal", "");
      const index = seen.get(selector) || 0;
      seen.set(selector, index + 1);
      node.style.setProperty("--d", `${Math.min(index, 5) * 60}ms`);
      pending.add(node);
    }
  }
}

function revealNow(node) {
  node.classList.add("is-in");
  pending.delete(node);
  if (observer) observer.unobserve(node);
}

// Anything the observer has not caught that is genuinely on screen gets shown.
// Anchor jumps and fast scrolls never fire an intersection for the sections they
// skip past, and a section stuck at opacity 0 is worse than no animation at all.
function sweep() {
  const floor = window.innerHeight * 0.94;
  for (const node of [...pending]) {
    // Anything at or above the fold line counts, including sections already
    // scrolled past, so scrolling back up never meets an invisible block.
    if (node.getBoundingClientRect().top < floor) revealNow(node);
  }
}

function start() {
  arm();

  if (!("IntersectionObserver" in window)) {
    [...pending].forEach(revealNow);
    return;
  }

  observer = new IntersectionObserver(
    (entries) => {
      for (const entry of entries) {
        if (entry.isIntersecting) revealNow(entry.target);
      }
    },
    { rootMargin: "0px 0px -6% 0px", threshold: 0.08 }
  );

  sweep();
  for (const node of pending) observer.observe(node);

  let frame = 0;
  const onScroll = () => {
    cancelAnimationFrame(frame);
    frame = requestAnimationFrame(sweep);
  };
  window.addEventListener("scroll", onScroll, { passive: true });
  window.addEventListener("resize", onScroll, { passive: true });
  window.addEventListener("hashchange", () => requestAnimationFrame(sweep));

  // Last resort: nothing stays hidden indefinitely.
  setTimeout(() => [...pending].forEach(revealNow), 4000);
}

start();
