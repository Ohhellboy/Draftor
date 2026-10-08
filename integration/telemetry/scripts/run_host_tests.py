#!/usr/bin/env python3
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
(ROOT/'_tmp').mkdir(exist_ok=True)
sys.path[:0]=[str(ROOT/'host'),str(ROOT/'tests')]
suite=unittest.TestSuite()
for folder in ('codec','reconstruction','privacy','overload','collector','preintegration','hardening','analyzer'):
    path=ROOT/'tests'/folder
    if path.exists(): suite.addTests(unittest.defaultTestLoader.discover(str(path),pattern='test_*.py'))
result=unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(0 if result.wasSuccessful() else 1)
