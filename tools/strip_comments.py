import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# File type patterns and comment regexes
C_STYLE_EXT = {".c", ".h"}
PY_EXT = {".py"}
TXT_EXT = {".txt", ".md"}

# Regex to remove C/C++ style comments safely (/* ... */ and // ...)
C_BLOCK = re.compile(r"/\*.*?\*/", re.DOTALL)
C_LINE = re.compile(r"(^|\s)//.*?$", re.MULTILINE)

# Python comments: lines starting with #, but avoid shebang and encoding lines
PY_LINE = re.compile(r"(^|\s)#.*?$", re.MULTILINE)

# Simple function to strip comments with care

def strip_c_comments(text: str) -> str:
    # Remove block comments first
    no_block = C_BLOCK.sub("", text)
    # Then line comments
    no_line = C_LINE.sub(lambda m: ("\n" if m.group(0).strip().startswith("//") and m.group(1) == "" else m.group(1)), no_block)
    return no_line


def strip_py_comments(text: str, path: Path) -> str:
    lines = text.splitlines(True)
    out = []
    for i, line in enumerate(lines):
        if i == 0 and line.startswith("#!"):
            out.append(line)
            continue
        if "coding:" in line and line.strip().startswith("#"):
            out.append(line)
            continue
        # Remove inline comments only if not inside string literal (naive: skip lines with quotes)
        if "#" in line:
            # If line is entirely comment, drop
            stripped = line.lstrip()
            if stripped.startswith("#"):
                continue
            # Try to remove trailing comment parts
            # crude heuristic: if number of quotes is even, assume not inside string
            if line.count('"') % 2 == 0 and line.count("'") % 2 == 0:
                idx = line.find("#")
                if idx != -1:
                    line = line[:idx].rstrip() + ("\n" if line.endswith("\n") else "")
        out.append(line)
    return "".join(out)


def process_file(path: Path, dry_run: bool = True) -> None:
    ext = path.suffix.lower()
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        return
    original = text
    if ext in C_STYLE_EXT:
        text = strip_c_comments(text)
    elif ext in PY_EXT:
        text = strip_py_comments(text, path)
    else:
        return
    if text != original:
        if dry_run:
            print(f"CHANGED: {path}")
        else:
            path.write_text(text, encoding="utf-8")


def main():
    dry_run = True
    if len(sys.argv) > 1 and sys.argv[1] == "--apply":
        dry_run = False
    for root, dirs, files in os.walk(ROOT):
        # Skip binary-ish directories
        skip = {".git", "build", "dist", "node_modules", "__pycache__"}
        dirs[:] = [d for d in dirs if d not in skip]
        for f in files:
            p = Path(root) / f
            process_file(p, dry_run=dry_run)

if __name__ == "__main__":
    main()
