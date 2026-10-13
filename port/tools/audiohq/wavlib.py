"""Small pure-python WAV helpers for the audiohq checks (no numpy needed).

Only what the checks use: read a 16-bit stereo/mono WAV, mix to mono, window RMS,
onset times, a dominant-frequency estimate (FFT), the largest sample-to-sample
jump, and writers for generated test tones.
"""
import array
import cmath
import math
import struct
import sys


class Wav:
    def __init__(self, rate, channels, left, right):
        self.rate = rate
        self.channels = channels
        self.left = left      # list of ints
        self.right = right

    @property
    def frames(self):
        return len(self.left)

    def mono(self):
        return [(a + b) * 0.5 for a, b in zip(self.left, self.right)]

    def seconds(self):
        return self.frames / float(self.rate)


def read_wav(path):
    with open(path, "rb") as f:
        d = f.read()
    if d[:4] != b"RIFF" or d[8:12] != b"WAVE":
        raise ValueError("%s is not a WAV file" % path)
    pos = 12
    fmt = None
    data = None
    while pos + 8 <= len(d):
        tag = d[pos:pos + 4]
        size = struct.unpack("<I", d[pos + 4:pos + 8])[0]
        body = d[pos + 8:pos + 8 + size]
        if tag == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif tag == b"data":
            # the game's own dump patches the size on exit; if it did not, take the rest
            data = d[pos + 8:] if (size == 0 or size > len(d) - pos - 8) else body
            break
        pos += 8 + size + (size & 1)
    if fmt is None or data is None:
        raise ValueError("%s has no fmt/data chunk" % path)
    tag, ch, rate, _, _, bits = fmt
    if tag != 1 or bits != 16:
        raise ValueError("%s: only 16-bit PCM WAV is supported" % path)
    a = array.array("h")
    n = len(data) // 2
    a.frombytes(data[:n * 2])
    if sys.byteorder == "big":
        a.byteswap()
    if ch == 1:
        left = list(a)
        right = left
    else:
        left = list(a[0::ch])
        right = list(a[1::ch])
    return Wav(rate, ch, left, right)


def write_wav(path, rate, samples, channels=1):
    """samples: list of ints (interleaved if channels == 2)."""
    a = array.array("h", [max(-32768, min(32767, int(s))) for s in samples])
    if sys.byteorder == "big":
        a.byteswap()
    data = a.tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt ")
        f.write(struct.pack("<IHHIIHH", 16, 1, channels, rate, rate * channels * 2, channels * 2, 16))
        f.write(b"data" + struct.pack("<I", len(data)) + data)


def rms(x):
    if not x:
        return 0.0
    return math.sqrt(sum(v * v for v in x) / len(x))


def rms_db(x):
    r = rms(x)
    return 20.0 * math.log10(r) if r > 1e-9 else -120.0


def window_rms(x, rate, win_s=0.01):
    n = max(1, int(rate * win_s))
    return [rms(x[i:i + n]) for i in range(0, len(x) - n + 1, n)]


def onsets(x, rate, win_s=0.005, ratio=4.0, floor=300.0, min_gap_s=0.05):
    """Times (s) where the short-window level jumps up by `ratio` and is above `floor`."""
    env = window_rms(x, rate, win_s)
    out = []
    last = -1e9
    for i in range(1, len(env)):
        prev = max(env[i - 1], 1.0)
        if env[i] > floor and env[i] / prev > ratio:
            t = i * win_s
            if t - last >= min_gap_s:
                out.append(t)
                last = t
    return out


def max_jump(x):
    best = 0.0
    for i in range(1, len(x)):
        d = abs(x[i] - x[i - 1])
        if d > best:
            best = d
    return best


def _fft(a):
    n = len(a)
    if n == 1:
        return a
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    length = 2
    while length <= n:
        ang = -2j * math.pi / length
        w_len = cmath.exp(ang)
        for i in range(0, n, length):
            w = 1 + 0j
            half = length // 2
            for k in range(half):
                u = a[i + k]
                v = a[i + k + half] * w
                a[i + k] = u + v
                a[i + k + half] = u - v
                w *= w_len
        length <<= 1
    return a


def dominant_freq(x, rate, start=0, size=8192, lo=20.0, hi=None):
    """Strongest frequency (Hz) in x[start:start+size], parabolic-interpolated."""
    seg = list(x[start:start + size])
    if len(seg) < size:
        seg += [0.0] * (size - len(seg))
    n = size
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    buf = [complex(seg[i] * win[i], 0.0) for i in range(n)]
    spec = _fft(buf)
    hi = hi or rate / 2.0
    k0 = max(1, int(lo * n / rate))
    k1 = min(n // 2 - 1, int(hi * n / rate))
    best, bk = -1.0, k0
    mag = [abs(spec[k]) for k in range(n // 2)]
    for k in range(k0, k1 + 1):
        if mag[k] > best:
            best, bk = mag[k], k
    if 1 <= bk < n // 2 - 1:
        a, b, c = mag[bk - 1], mag[bk], mag[bk + 1]
        den = a - 2 * b + c
        off = 0.5 * (a - c) / den if den != 0 else 0.0
    else:
        off = 0.0
    return (bk + off) * rate / n


def tone(freq, rate, seconds, amp=12000.0, chirp_to=None):
    n = int(rate * seconds)
    out = []
    ph = 0.0
    for i in range(n):
        f = freq if chirp_to is None else freq + (chirp_to - freq) * i / n
        ph += 2 * math.pi * f / rate
        out.append(amp * math.sin(ph))
    return out


# ---- tick-resolution analysis (the game's sequencer ticks at 192 Hz) --------

TICK_HZ = 192.0


def tick_envelope(x, rate, ticks=None):
    """RMS of each 192 Hz tick window of x (list of numbers). Window k covers
    the samples between tick k and tick k+1, at the file's own rate, so the same
    tick index means the same moment of game time in a 32768 Hz and a 48000 Hz
    dump."""
    n = int(len(x) * TICK_HZ / rate)
    if ticks is not None:
        n = min(n, ticks)
    out = []
    for k in range(n):
        a = int(math.ceil(k * rate / TICK_HZ))
        b = int(math.ceil((k + 1) * rate / TICK_HZ))
        seg = x[a:b]
        out.append(rms(seg))
    return out


def to_db(v, floor=-90.0):
    return 20.0 * math.log10(v / 32768.0) if v > 1e-9 else floor


def best_lag(a, b, max_lag=4):
    """Lag (in ticks, -max_lag..max_lag) at which envelope b best matches a,
    with the normalised correlation there. Positive lag: b is later than a."""
    best = (0, -2.0)
    for lag in range(-max_lag, max_lag + 1):
        if lag >= 0:
            xa, xb = a[:len(a) - lag] if lag else a, b[lag:]
        else:
            xa, xb = a[-lag:], b[:len(b) + lag]
        n = min(len(xa), len(xb))
        if n < 10:
            continue
        xa, xb = xa[:n], xb[:n]
        ma, mb = sum(xa) / n, sum(xb) / n
        num = sum((p - ma) * (q - mb) for p, q in zip(xa, xb))
        da = math.sqrt(sum((p - ma) ** 2 for p in xa))
        db = math.sqrt(sum((q - mb) ** 2 for q in xb))
        c = num / (da * db) if da > 0 and db > 0 else 0.0
        if c > best[1]:
            best = (lag, c)
    return best


def env_onsets(env, rise_db=6.0, floor_db=-60.0, refractory=3):
    """Tick indices where the tick-RMS envelope jumps up by rise_db or more
    (looking back over the last two ticks) and lands above floor_db."""
    db = [to_db(v) for v in env]
    out = []
    last = -999
    for k in range(2, len(db)):
        if db[k] > floor_db and db[k] - min(db[k - 1], db[k - 2]) >= rise_db and k - last > refractory:
            out.append(k)
            last = k
    return out


def match_onsets(a, b, tol=1):
    """Fraction of onsets in a that have one in b within +-tol ticks, and the
    reverse, as (a_in_b, b_in_a, count_a, count_b)."""
    def frac(p, q):
        if not p:
            return 1.0
        qs = sorted(q)
        hit = 0
        import bisect
        for t in p:
            i = bisect.bisect_left(qs, t)
            near = [abs(qs[j] - t) for j in (i - 1, i) if 0 <= j < len(qs)]
            if near and min(near) <= tol:
                hit += 1
        return hit / float(len(p))
    return frac(a, b), frac(b, a), len(a), len(b)


def count_jumps(x, threshold):
    n = 0
    for i in range(1, len(x)):
        if abs(x[i] - x[i - 1]) > threshold:
            n += 1
    return n
