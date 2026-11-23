// piece.js: reads window.sense inside its own frame loop and answers.
//
// The piece is the same face scream_face.ino draws on the OLED, big: asleep
// when nobody is there, awake when someone is, and a mouth that opens with the
// loudness of the scream. On the bench page the mouth follows loud (the
// Arduino's volume); on the camera page it follows open (your own mouth).
// Read the object, never the camera.

const canvas = document.querySelector('#piece');
const ctx = canvas.getContext('2d');

function resize() {
  canvas.width = innerWidth * devicePixelRatio;
  canvas.height = innerHeight * devicePixelRatio;
}
addEventListener('resize', resize);
resize();

const COLORS = {
  asleep: '#0b0b0d', nobody: '#0b0b0d',
  awake: '#1d2a3a', here: '#1d2a3a',
  screaming: '#5a0f14', open: '#3a1d2a',
};
const INK = '#7fd3ff';

// A state with no colour above still gets a bright one of its own, picked from
// its name, so a new row in the table is visible the moment it fires.
function colorFor(state) {
  if (COLORS[state]) return COLORS[state];
  let hash = 0;
  for (const ch of state) hash = (hash * 31 + ch.charCodeAt(0)) % 360;
  return `hsl(${hash}, 85%, 50%)`;
}

let mouth = 0;           // smoothed, 0 closed .. 1 full scream
let nextBlink = 3000;

function frame(now) {
  const { state, signals } = window.sense;
  const rows = window.TABLE.states;
  const asleep = state === rows[rows.length - 1].name; // the last row: nobody there

  const target = asleep ? 0 : 0.15 + 0.85 * (signals.loud ?? signals.open ?? 0);
  mouth += (target - mouth) * 0.2;

  ctx.fillStyle = colorFor(state);
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  // The face is laid out on the OLED's 128 x 64 grid, scaled to the window.
  const u = Math.min(canvas.width / 128, canvas.height / 64) * 0.9;
  ctx.save();
  ctx.translate(canvas.width / 2 - 64 * u, canvas.height / 2 - 32 * u);
  ctx.scale(u, u);
  ctx.strokeStyle = ctx.fillStyle = INK;
  ctx.lineWidth = 1.5;
  ctx.lineCap = 'round';

  if (asleep) {
    line(31, 29, 49, 29);
    line(79, 29, 97, 29);
    line(60, 54, 68, 54);
    const z = (now / 400) % 12;
    ctx.font = '8px ui-monospace, Menlo, monospace';
    ctx.globalAlpha = 1 - z / 12;
    ctx.fillText('z', 100 + z / 2, 40 - z * 2);
    ctx.globalAlpha = 1;
  } else {
    // shake harder as it screams harder
    const shake = mouth > 0.4 ? mouth * 4 : 0;
    ctx.translate((Math.random() * 2 - 1) * shake, (Math.random() * 2 - 1) * shake / 2);

    const r = 8 + mouth * 3;
    const pupil = Math.max(1, 4 - mouth * 3);
    const dart = mouth > 0.6 ? 2 : 0;
    const dx = (Math.random() * 2 - 1) * dart, dy = (Math.random() * 2 - 1) * dart / 2;
    const blink = mouth < 0.5 && now > nextBlink;
    if (now > nextBlink + 120) nextBlink = now + 2000 + Math.random() * 3000;
    for (const x of [40, 88]) {
      if (blink) { line(x - r, 29, x + r, 29); continue; }
      circle(x, 29, r, false);
      circle(x + dx, 29 + dy, pupil, true);
    }

    // eyebrows: the inner ends shoot up as it panics
    const lift = mouth * 4, by = 29 - r - 2;
    line(31, by, 47, by - lift);
    line(81, by - lift, 97, by);

    // the mouth, from a little o to a full scream
    const rx = 3 + mouth * 19, ry = 2 + mouth * 6;
    ctx.beginPath();
    ctx.ellipse(64, 54, rx, ry, 0, 0, Math.PI * 2);
    ctx.stroke();
    if (rx > 4 && ry > 3) {
      ctx.beginPath();
      ctx.ellipse(64, 54 + ry - 2, rx / 2, 2, 0, Math.PI, 0);
      ctx.fill();
    }
  }
  ctx.restore();

  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

function line(x1, y1, x2, y2) {
  ctx.beginPath();
  ctx.moveTo(x1, y1);
  ctx.lineTo(x2, y2);
  ctx.stroke();
}

function circle(x, y, r, fill) {
  ctx.beginPath();
  ctx.arc(x, y, r, 0, Math.PI * 2);
  fill ? ctx.fill() : ctx.stroke();
}
