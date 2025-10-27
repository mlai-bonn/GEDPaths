#!/usr/bin/env python3
"""
Extract default option assignments from GEDLIB .ipp files under libGraph/external/gedlib/src/methods/
This is a heuristic extractor: it looks for functions named *_set_default_options_ and collects simple assignments
and calls to set_option-like patterns. It writes Methods.defaults.json and Methods.defaults.csv in repo root.
"""
from __future__ import annotations
import re
import os
import json
import csv

ROOT = os.path.join(os.path.dirname(__file__), '..')
METHODS_DIR = os.path.normpath(os.path.join(ROOT, 'libGraph', 'external', 'gedlib', 'src', 'methods'))
OUT_JSON = os.path.normpath(os.path.join(ROOT, 'Methods.defaults.json'))
OUT_CSV = os.path.normpath(os.path.join(ROOT, 'Methods.defaults.csv'))

assign_re = re.compile(r"\b([a-zA-Z_][a-zA-Z0-9_:<>]*)\s*=\s*([^;]+);")
option_set_re = re.compile(r"(?:this->|\b)([a-zA-Z0-9_]+)_\s*=\s*([^;]+);")
function_re = re.compile(r"([a-zA-Z0-9_:<>]+)::([a-zA-Z0-9_]+)_set_default_options_\s*\(")
setdef_re = re.compile(r"\b([a-zA-Z0-9_]+)_set_default_options_\s*\(")

def find_ipp_files():
    if not os.path.isdir(METHODS_DIR):
        print('Methods directory not found:', METHODS_DIR)
        return []
    files = []
    for entry in os.listdir(METHODS_DIR):
        if entry.endswith('.ipp'):
            files.append(os.path.join(METHODS_DIR, entry))
    return files


def extract_defaults_from_file(path: str) -> dict:
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        txt = f.read()
    results = {}
    # Find implementations of set_default functions
    # Strategy: find occurrences of "ls_set_default_options_()" or "ged_set_default_options_()" etc
    # We'll search for "set_default_options_" and take the surrounding block (braces)
    for m in re.finditer(r"([a-zA-Z0-9_:<>]+)::([a-zA-Z0-9_]+)_set_default_options_\s*\(.*?\)\s*\{", txt):
        class_name = m.group(1).split('::')[-1]
        func_start = m.end()
        # extract the brace-balanced block
        depth = 1
        i = func_start
        while i < len(txt) and depth > 0:
            if txt[i] == '{':
                depth += 1
            elif txt[i] == '}':
                depth -= 1
            i += 1
        block = txt[func_start:i]
        defaults = {}
        # heuristics: find lines like variable_ = value; or option_name = value;
        for line in block.splitlines():
            line = line.strip()
            # skip comments
            if line.startswith('//') or line.startswith('/*') or not line:
                continue
            # match typical assignments
            for am in option_set_re.finditer(line):
                key = am.group(1)
                val = am.group(2).strip()
                defaults[key] = val
            # also try generic assigns
            for am in assign_re.finditer(line):
                key = am.group(1)
                val = am.group(2).strip()
                # filter out function declarations (heuristic)
                if '(' in val or 'new ' in val:
                    continue
                # avoid too generic matches
                if key.endswith(')'):
                    continue
                defaults[key] = val
        if defaults:
            results[class_name] = defaults
    return results


def main():
    files = find_ipp_files()
    all_defaults = {}
    for p in files:
        try:
            d = extract_defaults_from_file(p)
            for k,v in d.items():
                if k not in all_defaults:
                    all_defaults[k] = {}
                # merge
                all_defaults[k].update(v)
        except Exception as e:
            print('Error parsing', p, e)
    # write JSON
    with open(OUT_JSON, 'w', encoding='utf-8') as jf:
        json.dump(all_defaults, jf, indent=2)
    # write CSV (flat: class,key,value)
    with open(OUT_CSV, 'w', encoding='utf-8', newline='') as cf:
        writer = csv.writer(cf)
        writer.writerow(['class','option','value'])
        for cls, opts in all_defaults.items():
            for k,v in opts.items():
                writer.writerow([cls,k,v])
    print('Wrote', OUT_JSON, 'and', OUT_CSV)

if __name__ == '__main__':
    main()

