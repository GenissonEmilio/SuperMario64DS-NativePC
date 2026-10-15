#!/usr/bin/env python3
"""coop_drift.py with the castle classes judged as enemies (Boo 209, MIPS 187: they chase / flee, so the 80 unit band)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coop_drift as D  # noqa: E402

D.ENEMY.update({209, 187})
if __name__ == "__main__":
    D.main()
