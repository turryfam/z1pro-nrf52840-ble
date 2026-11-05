"""Build the Z1 PRO ZMK application at flash address 0x34000."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=repo.parent)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=repo / "build")
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    sdk = args.sdk.resolve()
    board = repo / "firmware"
    zephyr = workspace / "zephyr"
    zmk = workspace / "zmk" / "app"
    modules = [workspace / "modules" / name for name in (
        "hal/cmsis", "hal/nordic", "debug/segger", "crypto/tinycrypt",
        "crypto/mbedtls", "lib/gui/lvgl",
    )]
    extra_modules = [board, workspace / "modules/custom/paw3395"]
    for directory in [sdk, zephyr, zmk, *modules, *extra_modules]:
        if not directory.is_dir():
            parser.error(f"Missing dependency directory: {directory}")

    env = os.environ.copy()
    env.update({
        "ZEPHYR_BASE": str(zephyr),
        "ZEPHYR_SDK_INSTALL_DIR": str(sdk),
        "ZEPHYR_TOOLCHAIN_VARIANT": "zephyr",
        "PATH": str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", ""),
    })
    cmake = shutil.which("cmake", path=env["PATH"])
    if not cmake:
        parser.error("Install the Python build requirements before running this script")

    build_dir = args.build_dir.resolve()
    configure = [
        cmake, "-G", "Ninja", "-S", str(zmk), "-B", str(build_dir),
        "-DBOARD=zaopin_z1pro/nrf52840",
        f"-DBOARD_ROOT={board}",
        f"-DZMK_CONFIG={board / 'config'}",
        f"-DEXTRA_DTC_OVERLAY_FILE={board / 'config/secondary-slot.overlay'}",
        "-DZEPHYR_MODULES=" + ";".join(map(str, modules)),
        "-DZMK_EXTRA_MODULES=" + ";".join(map(str, extra_modules)),
        f"-DPython3_EXECUTABLE={sys.executable}",
        f"-DCMAKE_PREFIX_PATH={zephyr / 'share/zephyr-package/cmake'}",
        "-DBUILD_VERSION=z1pro-port-20261104",
    ]
    subprocess.run(configure, env=env, check=True)
    subprocess.run([cmake, "--build", str(build_dir), "--parallel", "4"], env=env, check=True)
    print(build_dir / "zephyr/zmk.bin")


if __name__ == "__main__":
    main()
