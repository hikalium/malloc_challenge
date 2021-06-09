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
        pixels[i - begin] = 1;
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
  const stat_allocated_acc = [0];
  const stat_freed_acc = [0];
  const stat_allocated_labels = [0];
  let count = 1;
  let allocated_acc = 0;
  let freed_acc = 0;
  for (const e of ops) {
    range_begin = Math.min(range_begin, e[1]);
    range_end = Math.max(range_end, e[1] + e[2]);
    if (e[0] == 'a') {
      allocated_acc += e[2];
    }
    if (e[0] == 'f') {
      freed_acc += e[2];
    }
    stat_allocated_labels.push(count);
    stat_allocated_acc.push(allocated_acc);
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
        label: 'allocated_acc',
        backgroundColor: 'rgb(255, 99, 132)',
        borderColor: 'rgb(255, 99, 132)',
        data: stat_allocated_acc,
      },
      {
        label: 'freed_acc',
        backgroundColor: 'rgb(255, 99, 132)',
        borderColor: 'rgb(255, 99, 132)',
        data: stat_freed_acc,
      }
    ]
  };
  const config = {
    type: 'line',
    data,
    options: {
      animation: false,
      showLine: false,
      responsive: true,
      maintainAspectRatio: false,
      plugins: {tooltip: {enabled: false}}
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
  a 41538208 336
  a 41538560 4064
  a 41542640 4064
  a 41546720 48
  a 41546784 208
  a 41547008 48
  a 41547072 312
  a 41547392 48
  a 41547456 312
  a 41547776 48
  a 41547840 208
  a 41548064 48
  a 41548128 312
  a 41548448 48
  a 41548512 312
  a 41548832 48
  a 41548896 208
  a 41549120 48
  a 41549184 312
  a 41549504 48
  a 41549568 312
  a 41549888 1040
  a 41550944 208
  a 41551168 48
  a 41551232 208
  a 41551456 48
  a 41551520 312
  a 41551840 48
  a 41551904 312
  a 41552224 72704
  a 41624944 5
  f 41624944 5
  a 41624976 120
  a 41624944 12
  a 41625104 776
  a 41625888 112
  a 41626016 1336
  a 41627360 216
  a 41627584 432
  a 41628032 104
  a 41628144 88
  a 41628240 120
  a 41628368 168
  a 41628544 104
  a 41628656 80
  a 41628752 192
  a 41628960 12
  a 41628992 171
  a 41629184 12
  a 41629216 181
  f 41628992 171
  a 41629408 30
  a 41629456 6
  a 41629488 51
  f 41629488 51
  a 41629552 472
  a 41630032 4096
  a 41634144 1600
  a 41635760 1024
  f 41635760 1024
  a 41635760 2048
  f 41630032 4096
  f 41629552 472
  a 41630032 5
  a 41629488 56
  a 41630064 168
  a 41630240 51
  a 41630304 104
  a 41630416 45
  a 41630480 72
  a 41630560 42
  a 41630624 56
  a 41630688 51
  a 41630752 56
  a 41630816 51
  f 41630816 51
  a 41630816 54
  a 41630880 72
  a 41630960 51
  f 41630960 51
  a 41630960 54
  f 41630960 54
  a 41630960 51
  f 41630960 51
  a 41630960 51
  f 41630960 51
  a 41630960 48
  a 41631024 72
  a 41631104 42
  f 41631104 42
  a 41631168 57
  a 41631248 72
  a 41631104 51
  f 41631104 51
  a 41631328 57
  f 41631328 57
  a 41631104 51
  f 41631104 51
  a 41631104 51
  f 41631104 51

`;

loadData(input);


const dropZone = document.getElementById('fileDropZone');
dropZone.addEventListener('dragover', handleDragOver, false);
dropZone.addEventListener('drop', handleFileSelect, false);

