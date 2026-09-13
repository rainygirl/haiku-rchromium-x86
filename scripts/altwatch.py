"""Watch paint activity and memory while scheduled navigations alternate.

Samples every second and reports, per navigation: how long until the first
new frame, how long until frames stop arriving, and the system memory floor
between loads. A leak across repeated loads shows in the floor, not in the
peak during any one load.
"""
import re
import subprocess
import sys
import time

log = sys.argv[1]
marks = [int(x) for x in sys.argv[2].split(",")]
total = int(sys.argv[3])


def paints():
    try:
        with open(log, "rb") as f:
            return f.read().count(b"PresentCanvas")
    except OSError:
        return 0


def used_mb():
    out = subprocess.run(["sysinfo", "-mem"], capture_output=True,
                         text=True).stdout
    m = re.search(r"used/max\s+(\d+)", out)
    return int(m.group(1)) / 1048576.0 if m else -1.0


start = time.time()
segments = []
idx = 0
prev = paints()
seg_start = 0.0
first_new = None
last_new = None

print("%-8s %-9s %-9s %-8s %-9s" %
      ("nav_at", "first_s", "settle_s", "frames", "used_MB"))
while True:
    t = time.time() - start
    if t > total:
        break
    time.sleep(1.0)
    t = time.time() - start
    now = paints()
    if now != prev:
        if first_new is None:
            first_new = t - seg_start
        last_new = t - seg_start
        prev = now
    if idx < len(marks) and t >= marks[idx]:
        # Report the segment that just ended, then start the next.
        print("%-8s %-9s %-9s %-8d %-9.1f" %
              ("t+%ds" % (marks[idx - 1] if idx else 0),
               ("%.1f" % first_new) if first_new is not None else "none",
               ("%.1f" % last_new) if last_new is not None else "none",
               now - (segments[-1] if segments else 0), used_mb()))
        sys.stdout.flush()
        segments.append(now)
        seg_start = t
        first_new = None
        last_new = None
        idx += 1

print("%-8s %-9s %-9s %-8d %-9.1f" %
      ("t+%ds" % (marks[-1] if marks else 0),
       ("%.1f" % first_new) if first_new is not None else "none",
       ("%.1f" % last_new) if last_new is not None else "none",
       paints() - (segments[-1] if segments else 0), used_mb()))
