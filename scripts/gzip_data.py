# PlatformIO pre-build script: build the LittleFS image from a staged copy of data/ in which
# the web console (www/*.html|js|css) is gzip-compressed (56 KB -> ~13 KB). ESPAsyncWebServer
# serves "x.gz" with "Content-Encoding: gzip" when "x" is requested and absent, and browsers
# decompress transparently. The sources in data/ stay plain text; nothing compressed is committed.
Import("env")
import gzip, os, shutil

COMPRESS = (".html", ".js", ".css")

src = env.subst("$PROJECT_DATA_DIR")
dst = os.path.join(env.subst("$BUILD_DIR"), "data_gz")

if os.path.isdir(src):
    shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(src, dst)
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
