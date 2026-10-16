#!/usr/bin/env python3
"""Build the standalone diagram page from the template and the model's netlist JSON.

Usage: python3 tools/build_page.py web/page.html build/netlist.json build/site/index.html

The template holds the page head (title, styles) followed by <main>...; the netlist JSON
is injected at __DATA__. The output is one self-contained HTML file with no server needed.
"""
import json
import pathlib
import sys

template_path, data_path, out_path = map(pathlib.Path, sys.argv[1:4])
template = template_path.read_text()
data = data_path.read_text()
json.loads(data)  # fail early on a broken export
if "</script" in data:
    sys.exit("netlist JSON contains '</script', refusing to inline it")

head, sep, body = template.partition("<main>")
if not sep:
    sys.exit("template has no <main> element")

page = (
    "<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, viewport-fit=cover\">\n"
    + head
    + "</head>\n<body>\n"
    + sep
    + body.replace("__DATA__", data)
    + "\n</body>\n</html>\n"
)
out_path.parent.mkdir(parents=True, exist_ok=True)
out_path.write_text(page)
print(f"wrote {out_path} ({len(page) // 1024} KB)")
