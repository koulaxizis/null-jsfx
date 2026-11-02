// Shared brand data for the media kit generator. Marks use the exact geometry
// of each project's own icon source (null-jsfx hub/app/icons/icon.svg,
// oros icon.svg, noxpress favicon.svg).
const BRAND = {
  null: {
    name: 'NULL JSFX', tag: 'AUDIO WITHOUT NOISE', font: 'Noto', site: 'nulljsfx.tech',
    vb: 80,
    mark: (c, box) => (box ? `<rect x="2" y="2" width="76" height="76" rx="14" fill="${box.fill}" stroke="${box.stroke}" stroke-width="1.5"/>` : '') +
      `<path d="M24 20 L24 60 L56 20 L56 60" stroke="${c}" stroke-width="7" stroke-linejoin="round" stroke-linecap="round" fill="none"/>`,
    pal: {
      dark:  { bg0: '#2a2926', bg1: '#131211', fg: '#d4d4d4', sub: '#8a8a8a', acc: '#c4c4c4', g0: '#9a9a9a', g1: '#d4d4d4', box: '#232220', boxS: '#4a4a4a' },
      light: { bg0: '#fbfaf8', bg1: '#e6e5e1', fg: '#2a2926', sub: '#6a6a6a', acc: '#555555', g0: '#9a9a9a', g1: '#d4d4d4', box: '#232220', boxS: '#4a4a4a' }
    }
  },
  oros: {
    name: 'orOS', tag: 'A static operating system in your browser.', font: 'Nunito', site: 'useoros.online',
    vb: 512,
    mark: (c, box) => (box ? `<rect width="512" height="512" rx="96" fill="${box.fill}"/>` : '') +
      `<path d="M256 104 L460 408 L52 408 Z M256 104 L342 246 L388 176 L460 408" stroke="${c}" stroke-width="20" stroke-linecap="round" stroke-linejoin="miter" stroke-miterlimit="3" fill="none"/>`,
    pal: {
      dark:  { bg0: '#2a2414', bg1: '#0f0d09', fg: '#f0ead9', sub: '#a89f8a', acc: '#d4af37', box: '#1b1a18' },
      light: { bg0: '#fffdf6', bg1: '#ece4cf', fg: '#241f14', sub: '#7a705a', acc: '#b8860b', box: '#1b1a18' }
    }
  },
  nox: {
    name: 'noxpress', tag: 'dark, minimal, privacy-first.', font: 'Inter', site: 'noxpress.tech',
    vb: 64,
    mark: (c, box) => (box ? `<rect width="64" height="64" rx="14" fill="${box.fill}" ${box.stroke ? `stroke="${box.stroke}" stroke-width=".8"` : ''}/>` : '') +
      `<path d="M21 45V27a11 11 0 0 1 22 0v18" fill="none" stroke="${c}" stroke-width="8" stroke-linecap="round"/>`,
    pal: {
      dark:  { bg0: '#2a1e5c', bg1: '#0d0f14', mid: '#15131f', fg: '#f6f4ff', sub: '#b7b2d6', acc: '#6d4aff', box: '#0d0f14', boxS: '#2e3240' },
      light: { bg0: '#dcd3ff', bg1: '#f7f6fb', mid: '#efecfb', fg: '#14121f', sub: '#6a6488', acc: '#6d4aff', box: '#ffffff', boxS: '#dcd7f5' }
    }
  }
};
// place a mark (in its own viewBox) at centre cx,cy with size s
function mark(id, color, cx, cy, s, box) {
  const b = BRAND[id];
  return `<svg x="${cx - s / 2}" y="${cy - s / 2}" width="${s}" height="${s}" viewBox="0 0 ${b.vb} ${b.vb}" overflow="visible">${b.mark(color, box)}</svg>`;
}
// small seeded PRNG so every render is identical
function rng(seed) { let s = seed >>> 0 || 1; return () => (s = (s * 16807) % 2147483647) / 2147483647; }
