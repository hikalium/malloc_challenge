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
  canvas.height = Math.min(Math.max(r, 3), 8192 / h) * h;
  const ctx = canvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  ctx.scale(r, canvas.height / h);
  ctx.drawImage(backedCanvas, 0, 0);
}

function drawPixelsFromTrace(begin, end, ops, hsegments, endIndex) {
  const pixels = Array.from({length: end - begin}, (e, i) => 0);
  for (let i = 0; i < ops.length; i++) {
    if (i >= endIndex) break;
    const e = ops[i];
    if (e[0] == 'a') {
      for (let i = e[1]; i < e[1] + e[2]; i++) {
        pixels[i - begin] = 4;
      }
    }
    if (e[0] == 'f') {
      for (let i = e[1]; i < e[1] + e[2]; i++) {
        pixels[i - begin] = 2;
      }
    }
    if (e[0] == 'm') {
      for (let i = e[1]; i < e[1] + e[2]; i++) {
        pixels[i - begin] = 2;
      }
    }
    if (e[0] == 'u') {
      for (let i = e[1]; i < e[1] + e[2]; i++) {
        pixels[i - begin] = 0;
      }
    }
  }
  drawPixels(pixels, hsegments);
}

const hsegments = document.querySelector('#hsegments');
hsegments.addEventListener('input', (event) => {
  drawVisualizer();
});

const progress = document.querySelector('#progress');
progress.addEventListener('input', (event) => {
  drawVisualizer();
});

function loadData(input) {
  const ops = input.split('\n')
                  .map(s => s.trim())
                  .filter(e => e.length > 0)
                  .map(s => s.split(' '))
                  .filter(e => e.length == 3)
                  .map(e => [e[0], parseInt(e[1], 10), parseInt(e[2], 10)]);
  let range_begin = Number.POSITIVE_INFINITY;
  let range_end = 0;
  const stat_allocated_now = [0];
  const stat_allocated_acc = [0];
  const stat_mapped_now = [0];
  const stat_freed_acc = [0];
  const stat_allocated_labels = [0];
  let count = 1;
  let allocated_acc = 0;
  let allocated_now = 0;
  let mapped_now = 0;
  let freed_acc = 0;
  for (const e of ops) {
    range_begin = Math.min(range_begin, e[1]);
    range_end = Math.max(range_end, e[1] + e[2]);
    if (e[0] == 'a') {
      allocated_acc += e[2];
      allocated_now += e[2];
    }
    if (e[0] == 'f') {
      freed_acc += e[2];
      allocated_now -= e[2];
    }
    if (e[0] == 'm') {
      mapped_now += e[2];
    }
    if (e[0] == 'u') {
      mapped_now -= e[2];
    }
    stat_allocated_labels.push(count);
    stat_allocated_now.push(allocated_now);
    stat_allocated_acc.push(allocated_acc);
    stat_mapped_now.push(mapped_now);
    stat_freed_acc.push(freed_acc);
    count++;
  }
  console.assert(range_begin <= range_end);
  console.log(`[${range_begin}, ${range_end})`);

  if (window.malloc_trace) {
    window.malloc_trace.chart.destroy();
  }

  window.malloc_trace = {};
  window.malloc_trace.ops = ops;
  window.malloc_trace.range_begin = range_begin;
  window.malloc_trace.range_end = range_end;

  progress.max = ops.length;

  drawVisualizer(256);

  // chart
  const data = {
    labels: stat_allocated_labels,
    datasets: [
      {
        label: 'allocated_now',
        backgroundColor: '#03af7a',
        borderColor: '#03af7a',
        data: stat_allocated_now,
        fill: 'origin',
      },
      {
        label: 'mapped_now',
        backgroundColor: '#4dc4ff',
        borderColor: '#4dc4ff',
        data: stat_mapped_now,
        fill: 'origin',
      }
    ]
  };
  const config = {
    type: 'line',
    data,
    options: {
      animation: false,
      pointRadius: 0,
      showLine: true,
      responsive: true,
      maintainAspectRatio: false,
      plugins: {tooltip: {enabled: false}},
      scales: {
        x: {display: true, title: {display: true, text: 'progress'}},
        y: {
          display: true,
          title: {display: true, text: 'Bytes'},
        }
      }
    }
  };
  const chartCanvas = document.getElementById('chart');
  chartCanvas.height = 200;
  window.malloc_trace.chart =
      new Chart(document.getElementById('chart'), config);
}

function drawVisualizer() {
  const t = window.malloc_trace;
  drawPixelsFromTrace(
      t.range_begin, t.range_end, t.ops, Math.pow(2, hsegments.value),
      progress.value);
}

const handleFileSelect =
    async (evt) => {
  evt.stopPropagation();
  evt.preventDefault();
  var files = evt.dataTransfer.files;
  var output = [];
  for (var i = 0, f; f = files[i]; i++) {
    var r = new FileReader();
    r.onload = ((file) => {
      return async (e) => {
        var a = await file.text();
        loadData(a);
      }
    })(f);
    r.readAsArrayBuffer(f);
  }
}

function handleDragOver(evt) {
  evt.stopPropagation();
  evt.preventDefault();
  evt.dataTransfer.dropEffect = 'copy';  // Explicitly show this is a copy.
}

const input = `
  m 0 400
  a 0 100
  a 100 100
  a 200 100
  a 300 100
  f 0 100
  f 100 100
  f 200 100
  f 300 100
`;

loadData(input);


const dropZone = document.getElementById('fileDropZone');
dropZone.addEventListener('dragover', handleDragOver, false);
dropZone.addEventListener('drop', handleFileSelect, false);

