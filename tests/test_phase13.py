#!/usr/bin/env python3
"""
Phase 13 tests: data export and history management.
"""
import sys
import os
import re

passed = 0
failed = 0

def check(label, condition):
    global passed, failed
    if condition:
        passed += 1
        print(f"  PASS  {label}")
    else:
        failed += 1
        print(f"  FAIL  {label}")

BASE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32', 'data')

# =====================================================================
#  HTML structure
# =====================================================================
print("\n== HTML structure ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("export-csv-btn exists", 'id="export-csv-btn"' in html)
check("data-count span exists", 'id="data-count"' in html)
check("chart-title-row wrapper", 'chart-title-row' in html)
check("chart-actions wrapper", 'chart-actions' in html)
check("export button says CSV", '>CSV<' in html)

# =====================================================================
#  CSS styles
# =====================================================================
print("\n== CSS styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".btn-export style", '.btn-export' in css)
check(".btn-export has border", 'border' in css.split('.btn-export')[1].split('}')[0])
check(".btn-export:active feedback", '.btn-export:active' in css)
check(".data-count style", '.data-count' in css)
check(".chart-title-row style", '.chart-title-row' in css)
check(".chart-actions style", '.chart-actions' in css)
check("data-count uses tabular-nums", 'tabular-nums' in css.split('.data-count')[1].split('}')[0])

# =====================================================================
#  JavaScript functions
# =====================================================================
print("\n== JavaScript functions ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("updateDataCount function", 'function updateDataCount' in js)
check("exportCsv function", 'function exportCsv' in js)
check("export-csv-btn handler", "export-csv-btn" in js)
check("updateDataCount called in loadHistory", 'updateDataCount()' in js)

# CSV generation
check("CSV header row", "'timestamp,temperature_c,humidity_pct'" in js)
check("Blob creation with text/csv", "text/csv" in js)
check("createObjectURL for download", 'createObjectURL' in js)
check("revokeObjectURL cleanup", 'revokeObjectURL' in js)
check("filename includes env name", 'envName' in js)
check("filename includes date", 'dateStr' in js)
check("filename has .csv extension", ".csv'" in js)

# Data count
check("data count shows pts", "' pts'" in js or '" pts"' in js)
check("data count checks length", 'chartData.length' in js)

# Timestamp formatting
check("ISO-style timestamp in CSV", 'getFullYear' in js)
check("padded month", "getMonth()+1" in js)
check("padded hours", "getHours" in js)

# Safety
check("handles empty chartData", "chartData.length === 0" in js)
check("sanitizes env name for filename", "replace(" in js.split('exportCsv')[1].split('var rows')[0])

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
