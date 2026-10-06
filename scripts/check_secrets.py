# PlatformIO pre-build guard: fail if a credential-looking literal is committed in the
# sources that end up in the firmware or the LittleFS image. Credentials belong in NVS,
# entered on the device (see CLAUDE.md "Secrets").
Import("env")
import os, re

PATTERN = re.compile(
    r'(api[_-]?key|appid|passw\w*|passcode|secret\w*|token\w*|ham_?pass\w*)\s*[:=]\s*"([^"\n]{6,})"',
    re.IGNORECASE)
ALLOW = {"unset", "(unset)", "(redacted)", "********"}
DIRS = ("src", "include", "data")
EXTS = (".c", ".cpp", ".h", ".hpp", ".ino", ".js", ".html", ".json", ".txt", ".css")

def scan(project_dir):
    hits = []
    for d in DIRS:
        for root, _, files in os.walk(os.path.join(project_dir, d)):
            for name in files:
                if not name.endswith(EXTS):
                    continue
                path = os.path.join(root, name)
                with open(path, encoding="utf-8", errors="ignore") as f:
                    for n, line in enumerate(f, 1):
                        for m in PATTERN.finditer(line):
                            if m.group(2) not in ALLOW:
                                hits.append(f"{os.path.relpath(path, project_dir)}:{n}: {m.group(1)} = \"<masked>\"")
    return hits

hits = scan(env.subst("$PROJECT_DIR"))
if hits:
    print("[check_secrets] Credential-like literals found (value masked):")
    for h in hits:
        print("  " + h)
    env.Exit("[check_secrets] FAIL: remove credentials from the sources (store them in NVS).")
print("[check_secrets] OK: no credential literals in src/, include/, data/")
