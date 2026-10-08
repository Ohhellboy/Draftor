#!/usr/bin/env python3
"""Separate entry point so existing collector/analyze conventions remain unchanged."""
from draftor_telemetry.analyzer.cli import main

if __name__ == '__main__':
    raise SystemExit(main())
