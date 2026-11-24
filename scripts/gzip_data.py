# PlatformIO pre-build script: build the LittleFS image from a staged copy of data/ in which
# the web console (www/*.html|js|css|svg) is gzip-compressed (56 KB -> ~13 KB). ESPAsyncWebServer
# serves "x.gz" with "Content-Encoding: gzip" when "x" is requested and absent, and browsers
# decompress transparently. The sources in data/ stay plain text; nothing compressed is committed.
Import("env")
import gzip
import os
import shutil
import sys

sys.path.insert(0, os.path.join(env.subst("$PROJECT_DIR"), "scripts"))
import about_credits  # noqa: E402  (the About tab's software list, from THIRD_PARTY_NOTICES.md)

COMPRESS = (".html", ".js", ".css", ".svg", ".json")

src = env.subst("$PROJECT_DATA_DIR")
dst = os.path.join(env.subst("$BUILD_DIR"), "data_gz")

if os.path.isdir(src):
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(src, dst)
    about = os.path.join(dst, "about.txt")
    if os.path.isfile(about):
        with open(os.path.join(env.subst("$PROJECT_DIR"), "THIRD_PARTY_NOTICES.md"), encoding="utf-8") as f:
            credits = about_credits.credits_section(f.read())
        with open(about, "a", encoding="utf-8") as f:
            f.write(credits)
    before = after = 0
    for root, _, files in os.walk(os.path.join(dst, "www")):
        for name in files:
            if not name.endswith(COMPRESS):
                continue
            path = os.path.join(root, name)
            with open(path, "rb") as f:
                raw = f.read()
            # mtime=0 keeps the image byte-identical between builds of the same sources
            with open(path + ".gz", "wb") as f:
                f.write(gzip.compress(raw, compresslevel=9, mtime=0))
            os.remove(path)
            before += len(raw)
            after += os.path.getsize(path + ".gz")
    env.Replace(PROJECT_DATA_DIR=dst)
    print(f"[gzip_data] web console {before:,} B -> {after:,} B gzipped; filesystem built from {dst}")
