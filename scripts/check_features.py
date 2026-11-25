#!/usr/bin/env python3
"""Pre-build guard: README.md's feature list matches docs/features.toml (the single source).

The section is generated (scripts/gen_features.py); the website's home page reads the same file
when the site is built. This fails the build (or exits 1 when run directly) when they disagree.
"""
import os
import sys


def check(root):
    sys.path.insert(0, os.path.join(root, "scripts"))
    import gen_features
    return gen_features.run(root, check=True)


def report(errors):
    for e in errors:
        print(f"[check_features] {e}")
    print("[check_features] FAILED" if errors else "[check_features] OK")


try:
    Import("env")  # noqa: F821  (PlatformIO pre-build)
    _errors = check(env.subst("$PROJECT_DIR"))  # noqa: F821
    report(_errors)
    if _errors:
        env.Exit(1)  # noqa: F821
except NameError:
    if __name__ == "__main__":
        _errors = check(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        report(_errors)
        sys.exit(1 if _errors else 0)
