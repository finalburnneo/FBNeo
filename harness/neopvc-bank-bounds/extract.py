#!/usr/bin/env python3
"""Extract named C/C++ function definitions verbatim from a source file.

The bank-bounds harness must test the code that actually ships, not a
paraphrase of it.  Copying the functions into the harness by hand would let the
copy and the real source drift apart silently, and a test that passes against a
stale copy is worse than no test.  So the harness generates its inputs: this
script pulls the exact bytes of each named function out of the real source file
and writes them to an .inc that the harness #includes.

The same script runs against two source trees:
  - the pristine base commit, via `git show <rev>:<path>` piped to stdin
  - the working tree, read from disk
which is what makes the baseline-vs-patched comparison meaningful.

Output carries the source path, the line range and a SHA-256 of the extracted
text, so a reviewer can re-derive exactly what was compiled.

Usage:
    extract.py --source <path|-> --label <name> [--require F] [--optional F] ...
"""

import argparse
import hashlib
import sys


def strip_scan(text):
    """Yield (index, char, in_code) over text, flagging comment/literal spans.

    Only `in_code` positions may be counted as braces.  Handles // comments,
    /* */ comments, "strings", 'chars' and backslash escapes -- enough for the
    driver sources this runs on, and it fails loudly (unbalanced braces) rather
    than silently mis-extracting if it ever meets something stranger.
    """
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if c == '/' and nxt == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                yield k, text[k], False
            i = j
            continue
        if c == '/' and nxt == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                yield k, text[k], False
            i = j
            continue
        if c in ('"', "'"):
            quote = c
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2
                    continue
                if text[j] == quote:
                    j += 1
                    break
                j += 1
            for k in range(i, min(j, n)):
                yield k, text[k], False
            i = j
            continue
        yield i, c, True
        i += 1


def find_definition(text, name):
    """Return (start, end, first_line, last_line) of `name`'s definition."""
    lines = text.split('\n')
    line_starts = []
    pos = 0
    for ln in lines:
        line_starts.append(pos)
        pos += len(ln) + 1

    cand = None
    for idx, ln in enumerate(lines):
        if ln.startswith((' ', '\t', '#', '/', '*', '}')):
            continue
        # a definition line mentions the name immediately followed by '('
        hit = ln.find(name + '(')
        if hit < 0:
            hit = ln.find(name + ' (')
        if hit < 0:
            continue
        if ln.rstrip().endswith(';'):        # a declaration, not a definition
            continue
        cand = idx
        # keep scanning: later definitions win only if an earlier one was a
        # prototype we failed to spot; in practice these files define once.
        break

    if cand is None:
        return None

    start = line_starts[cand]
    depth = 0
    seen_open = False
    end = None
    for i, c, in_code in strip_scan(text[start:]):
        if not in_code:
            continue
        if c == '{':
            depth += 1
            seen_open = True
        elif c == '}':
            depth -= 1
            if seen_open and depth == 0:
                end = start + i + 1
                break
    if end is None:
        raise SystemExit(f"extract.py: unbalanced braces extracting {name}")

    last_line = text.count('\n', 0, end)
    return start, end, cand + 1, last_line + 1


def find_define(text, name):
    """Return (source_text, first_line, last_line) for `#define name ...`."""
    lines = text.split('\n')
    for idx, ln in enumerate(lines):
        stripped = ln.lstrip()
        if not stripped.startswith('#define'):
            continue
        rest = stripped[len('#define'):].lstrip()
        if not rest.startswith(name):
            continue
        after = rest[len(name):]
        if after and (after[0].isalnum() or after[0] == '_'):
            continue
        end = idx
        while lines[end].rstrip().endswith('\\'):
            end += 1
        return '\n'.join(lines[idx:end + 1]), idx + 1, end + 1
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source', required=True, help="source path, or '-' for stdin")
    ap.add_argument('--label', required=True, help='name recorded in the output header')
    ap.add_argument('--require', action='append', default=[],
                    help='function that MUST be present')
    ap.add_argument('--optional', action='append', default=[],
                    help='function extracted if present (e.g. only in the patched tree)')
    ap.add_argument('--define', action='append', default=[],
                    help='object-like macro to extract if present, emitted before '
                         'the functions (only the patched tree has these)')
    args = ap.parse_args()

    if args.source == '-':
        text = sys.stdin.read()
    else:
        with open(args.source, encoding='utf-8', errors='replace') as fh:
            text = fh.read()

    out = [f'/* GENERATED by extract.py from {args.label} -- do not edit. */\n']

    for name in args.define:
        found = find_define(text, name)
        if found is None:
            out.append(f'/* {name}: absent from {args.label} */\n')
            continue
        body, first_line, last_line = found
        digest = hashlib.sha256(body.encode('utf-8')).hexdigest()[:16]
        out.append(
            f'\n/* --- {name}: {args.label}:{first_line}-{last_line} '
            f'sha256:{digest} --- */\n'
        )
        out.append(body)
        out.append('\n')

    for name in args.require + args.optional:
        found = find_definition(text, name)
        if found is None:
            if name in args.require:
                raise SystemExit(f"extract.py: required function '{name}' not found in {args.label}")
            out.append(f'/* {name}: absent from {args.label} */\n')
            continue
        start, end, first_line, last_line = found
        body = text[start:end]
        digest = hashlib.sha256(body.encode('utf-8')).hexdigest()[:16]
        out.append(
            f'\n/* --- {name}: {args.label}:{first_line}-{last_line} '
            f'sha256:{digest} --- */\n'
        )
        out.append(body)
        out.append('\n')

    sys.stdout.write(''.join(out))


if __name__ == '__main__':
    main()
