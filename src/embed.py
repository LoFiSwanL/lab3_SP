"""Embed readable runtime/viewer sources into the executable at build time."""
import json
from pathlib import Path

with Path("build/assets.h").open("w") as out:
    for name, source in [("python_runtime", "src/runtime.py"), ("html_viewer", "src/viewer.html")]:
        text = Path(source).read_text(encoding="utf-8")
        out.write(f"static const char {name}[] =\n")
        for line in text.splitlines(keepends=True):
            out.write(json.dumps(line, ensure_ascii=True) + "\n")
        out.write(";\n")
