#!/usr/bin/env python3
"""Pre-build guard: the flash layout in README.md and docs/RELEASING.md matches partitions.csv.

The tables are generated (scripts/gen_flash_docs.py); this fails the build (or exits 1 when run
directly) when they are out of date or a flash command in the docs uses a different address.
"""
import os
import sys


def check(root):
    sys.path.insert(0, os.path.join(root, "scripts"))
    import gen_flash_docs
    return gen_flash_docs.run(root, check=True)


def report(errors):
    for e in errors:
        print(f"[check_flash_docs] {e}")
    print("[check_flash_docs] FAILED" if errors else "[check_flash_docs] OK")


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
