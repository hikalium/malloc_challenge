// Pixels value:
// 0: not allocated nor mapped (light)
// 1: not allocated nor mapped (dark)
// 2: mapped but not allocated
// 3: mapped but not allocated
// 4: mapped and allocated
const colorMap = [
  [0xc8, 0xc8, 0xcb],  // grey
  [0x84, 0x91, 0x9e],  // dark grey
  [0x4d, 0xc4, 0xff],  // skyblue
  [0xff, 0x4b, 0x00],  // red
  [0x03, 0xaf, 0x7a],
];
// c.f. https://oku.edu.mie-u.ac.jp/~okumura/stat/colors.html

function drawPixels(pixels, hsegments) {
  // https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API/Tutorial/Pixel_manipulation_with_canvas
  console.assert(hsegments > 0);
  console.assert(pixels.length > 0);
  const w = hsegments;
  const h = Math.ceil(pixels.length / w);
  if (h > 512) return;
  const backedCanvas = document.querySelector('#backedCanvas');
  backedCanvas.width = w;
  backedCanvas.height = h;
  const backedContext = backedCanvas.getContext('2d');
  const backedImageData = backedContext.getImageData(0, 0, w, h);
  for (var i = 0; i < pixels.length; i++) {
    let r = colorMap[pixels[i]][0];
    let g = colorMap[pixels[i]][1];
    let b = colorMap[pixels[i]][2];
    backedImageData.data[i * 4 + 0] = r;
    backedImageData.data[i * 4 + 1] = g;
    backedImageData.data[i * 4 + 2] = b;
    backedImageData.data[i * 4 + 3] = 0xff;
  }
  backedContext.putImageData(backedImageData, 0, 0);

  const canvas = document.querySelector('#mainCanvas');
  const r = window.innerWidth / w;
  canvas.width = window.innerWidth;
  canvas.height = r * h;
  const ctx = canvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  ctx.scale(r, r);
  ctx.drawImage(backedCanvas, 0, 0);
}

function drawPixelsFromTrace(begin, end, ops, hsegments) {
  const pixels = Array.from({length: end - begin}, (e, i) => ((i / 4096) & 1));
  for (const e of ops) {
    if (e[0] == 'a') {
      for (let i = e[1]; i < e[2]; i++) {
        pixels[i] = 4;
      }
    }
  }
  drawPixels(pixels, hsegments);
}

// r <begin_addr> <end_addr>: range
// a <begin_addr> <end_addr>: alloc

const input = `
r 0 16384
a 128 2048
`;
const ops = input.split('\n')
                .map(s => s.trim())
                .filter(e => e.length > 0)
                .map(s => s.split(' '))
                .filter(e => e.length == 3)
                .map(e => [e[0], parseInt(e[1]), parseInt(e[2])]);
const range = ops.shift();
console.assert(range[0] == 'r');
const range_begin = range[1];
const range_end = range[2];
console.assert(range_begin <= range_end);

const hsegments = document.querySelector('#hsegments');
hsegments.addEventListener(
    'input',
    (event) => {drawPixelsFromTrace(
        range_begin, range_end, ops, Math.pow(2, hsegments.value))});

drawPixelsFromTrace(range_begin, range_end, ops, 32)

