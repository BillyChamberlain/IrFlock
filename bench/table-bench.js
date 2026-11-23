// bench/table-bench.js: the State Table for bench pairs, in code.
//
// bench.html loads this file instead of table.js, so a bench pair's rows live
// here. Same format as table.js: signals are the numbers sense-serial.js writes
// (each 0 to 1, named in its SIGNALS), states are checked top to bottom and the
// first true row wins.
//
// hold is in seconds: how long a rule must stay true before the state changes.
// It is a tradeoff you choose per row. A short hold feels instant but flickers
// when a reading wobbles near its line; a long hold is calm but slow.
// Keep "asleep" last, so it catches everything the other rows miss.

window.TABLE = {
<<<<<<< HEAD
  // Our build: bench/scream_face/scream_face.ino on the Nano. present comes
  // from the distance (someone within 150 cm, the same line the sketch wakes up
  // at), loud from the volume the sketch is screaming at.
  signals: ['present', 'loud'],

  states: [
    // Up close the sketch screams at nearly full volume: the face shakes and
    // the mouth is wide open.
    { name: 'screaming', when: s => s.present > 0.5 && s.loud > 0.75, hold: 0.3 },
    // Someone in range: awake and starting to scream, quietly at first.
    { name: 'awake',     when: s => s.present > 0.5,                  hold: 0.3 },
    // Nobody for a second: the sketch stops screaming and the face sleeps.
    { name: 'asleep',    when: s => s.present < 0.2,                  hold: 1 },
=======
  // AAAAAH: the face that names you. Same numbers as bench/name_it/name_it.ino.
  //   near    = (200 - cm) / 190, so cm 15 reads 0.97
  //   present = 1 when cm is under 150
  signals: ['present', 'near'],

  states: [
    { name: 'monster',  when: s => s.near > 0.97,    hold: 1 },   // under 15 cm, 1 s
    { name: 'stranger', when: s => s.present > 0.5,  hold: 0.5 }, // under 150 cm, 0.5 s
    { name: 'alone',    when: s => s.present < 0.2,  hold: 5 },   // nobody, 5 s
>>>>>>> main
  ],
};
