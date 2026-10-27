#!/usr/bin/env python3
"""mp3_to_sound.py: turn an MP3 into sound.h, the clip sound_alarm.ino plays.

The Nano can't decode an MP3, so this does it ahead of time: it decodes the
file, mixes it to mono, resamples it, trims the silence off both ends, turns it
up as loud as it will go and stores it as 8-bit samples in the Nano's flash.

    python3 mp3_to_sound.py song.mp3
    python3 mp3_to_sound.py song.mp3 --start 12.5     # skip to the good part
    python3 mp3_to_sound.py song.mp3 --rate 6000      # longer clip, more muffled

The Nano has room for about 26 KB of sound: roughly 3 seconds at 8000 samples
a second. A longer MP3 is cut to fit, and the Nano loops whatever is left.
Decoding uses afconvert (built into macOS) or ffmpeg if that's installed.
"""

import argparse
import array
import os
import shutil
import subprocess
import sys
import tempfile
import wave

MAX_BYTES = 26000       # flash left over after the sketch itself


def decode(path, rate):
    """Decode any audio file to mono 16-bit samples at `rate`."""
    tmp = tempfile.mkdtemp()
    out = os.path.join(tmp, 'clip.wav')
    try:
        if shutil.which('afconvert'):
            cmd = ['afconvert', '-f', 'WAVE', '-d', f'LEI16@{rate}', '-c', '1', path, out]
        elif shutil.which('ffmpeg'):
            cmd = ['ffmpeg', '-loglevel', 'error', '-y', '-i', path,
                   '-ac', '1', '-ar', str(rate), '-sample_fmt', 's16', out]
        else:
            sys.exit('Needs afconvert (macOS) or ffmpeg to decode the MP3.')
        subprocess.run(cmd, check=True)
        with wave.open(out, 'rb') as w:
            samples = array.array('h', w.readframes(w.getnframes()))
        if sys.byteorder == 'big':
            samples.byteswap()
        return samples
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def trim(samples, threshold=600):
    """Drop the quiet stretch at each end, so the loop has no dead air."""
    loud = [i for i, s in enumerate(samples) if abs(s) > threshold]
    if not loud:
        sys.exit('The clip is silent.')
    return samples[loud[0]:loud[-1] + 1]


def to_8bit(samples, rate):
    # Turn it up so the loudest 0.5% clips: a tiny speaker needs all the
    # level it can get, and a little clipping is hard to hear on one.
    peaks = sorted(abs(s) for s in samples)
    peak = max(peaks[int(len(peaks) * 0.995)], 1)
    gain = 127 / peak
    # A short fade at each end, so the jump back to the start doesn't click.
    fade = max(1, rate // 100)
    n = len(samples)
    out = bytearray(n)
    for i, s in enumerate(samples):
        edge = min(1.0, i / fade, (n - 1 - i) / fade)
        v = round(s * gain * edge)
        out[i] = max(-128, min(127, v)) + 128
    return out


def write_header(path, data, rate, source):
    with open(path, 'w') as f:
        f.write(f'// sound.h: made by mp3_to_sound.py from {os.path.basename(source)}.\n')
        f.write(f'// {len(data)} samples, 8-bit, {rate} per second '
                f'({len(data) / rate:.2f} s). Run the script again to change it.\n\n')
        f.write('#include <avr/pgmspace.h>\n\n')
        f.write(f'const uint16_t SOUND_RATE = {rate};\n')
        f.write(f'const uint16_t SOUND_LEN = {len(data)};\n')
        f.write('const uint8_t SOUND[] PROGMEM = {\n')
        for i in range(0, len(data), 16):
            f.write('  ' + ', '.join(str(b) for b in data[i:i + 16]) + ',\n')
        f.write('};\n')


def main():
    p = argparse.ArgumentParser(description='Turn an MP3 into sound.h for sound_alarm.ino.')
    p.add_argument('mp3', help='the MP3 (or WAV, M4A, AIFF) to convert')
    p.add_argument('--rate', type=int, default=8000,
                   help='samples per second (default 8000; lower is longer but muffled)')
    p.add_argument('--start', type=float, default=0.0,
                   help='seconds to skip at the start of the MP3')
    p.add_argument('--seconds', type=float,
                   help='how much to keep (default: as much as fits)')
    p.add_argument('--out', default=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sound.h'),
                   help='where to write sound.h (default: next to this script)')
    args = p.parse_args()

    samples = decode(args.mp3, args.rate)
    samples = samples[int(args.start * args.rate):]
    samples = trim(samples)
    keep = MAX_BYTES if args.seconds is None else min(MAX_BYTES, int(args.seconds * args.rate))
    if len(samples) > keep:
        print(f'Clip is {len(samples) / args.rate:.1f} s; keeping the first {keep / args.rate:.1f} s to fit the Nano.')
        samples = samples[:keep]

    data = to_8bit(samples, args.rate)
    write_header(args.out, data, args.rate, args.mp3)
    print(f'Wrote {args.out}: {len(data) / args.rate:.2f} s at {args.rate} Hz, {len(data)} bytes.')


if __name__ == '__main__':
    main()
