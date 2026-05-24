Import("env")
import os
import shutil

def after_build(source, target, env):
    # Define the target release directory in the root of the project
    release_dir = os.path.join(env.get("PROJECT_DIR"), "release")
    os.makedirs(release_dir, exist_ok=True)

    # Get the path of the file that was just built (firmware.bin or littlefs.bin)
    target_path = target[0].get_abspath()
    target_name = os.path.basename(target_path)

    # Copy the main binary
    shutil.copy(target_path, os.path.join(release_dir, target_name))
    print(f"\033[92m[QRPickle Auto-Release] Copied {target_name} to release/\033[0m")

    # If the firmware was just built, snag the bootloader and partition table too
    if target_name == "firmware.bin":
        build_dir = os.path.dirname(target_path)
        for extra in ["bootloader.bin", "partitions.bin"]:
            extra_path = os.path.join(build_dir, extra)
            if os.path.exists(extra_path):
                shutil.copy(extra_path, os.path.join(release_dir, extra))
                print(f"\033[92m[QRPickle Auto-Release] Copied {extra} to release/\033[0m")

# Attach this script to trigger immediately after these files are compiled
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
env.AddPostAction("$BUILD_DIR/littlefs.bin", after_build)
