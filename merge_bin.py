Import("env")

def merge_firmware(source, target, env):
    """Post-build: merge bootloader + partitions + firmware into one .bin"""
    import os

    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    output = os.path.join(project_dir, "FlightTracker.bin")

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware   = os.path.join(build_dir, "firmware.bin")

    # Use the esptool bundled with PlatformIO's toolchain
    esptool_path = os.path.join(
        env.PioPlatform().get_package_dir("tool-esptoolpy"),
        "esptool.py"
    )

    cmd = (
        f'python3 "{esptool_path}" '
        f'--chip esp32s3 merge_bin -o "{output}" '
        f'--flash_mode dio --flash_size 8MB '
        f'0x0000 "{bootloader}" '
        f'0x8000 "{partitions}" '
        f'0x10000 "{firmware}"'
    )

    print(f"\n*** Merging into {output} ***")
    ret = env.Execute(cmd)
    if ret == 0:
        print(f"*** FlightTracker.bin ready! Copy to SD card for M5Launcher ***")

env.AddPostAction("$BUILD_DIR/firmware.bin", merge_firmware)
