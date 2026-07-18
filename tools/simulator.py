#!/usr/bin/env python3
"""Build, run, package, and auto-reload the Cadenza desktop simulator."""

from __future__ import annotations

import argparse
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import sys
import time
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build" / "host"
EXECUTABLE = BUILD_DIR / "cadenza_desktop"
DIST_DIR = PROJECT_ROOT / "dist"

WATCH_ROOTS = (
    PROJECT_ROOT / "desktop",
    PROJECT_ROOT / "lib",
    PROJECT_ROOT / "cmake",
    PROJECT_ROOT / "third_party",
)
WATCH_FILES = (
    PROJECT_ROOT / "CMakeLists.txt",
    PROJECT_ROOT / "CMakePresets.json",
)
WATCH_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".h",
    ".hh",
    ".hpp",
    ".cmake",
    ".json",
}


def log(message: str) -> None:
    print(f"[simulator] {message}", flush=True)


def run_checked(command: list[str]) -> None:
    log("$ " + " ".join(command))
    subprocess.run(command, cwd=PROJECT_ROOT, check=True)


def configure() -> None:
    run_checked(["cmake", "--preset", "host-debug", "-S", str(PROJECT_ROOT)])


def build(*, reconfigure: bool = True) -> None:
    if reconfigure or not (BUILD_DIR / "CMakeCache.txt").exists():
        configure()
    run_checked(
        [
            "cmake",
            "--build",
            str(BUILD_DIR),
            "--target",
            "cadenza_desktop",
            "--parallel",
        ]
    )


def simulator_arguments(args: argparse.Namespace) -> list[str]:
    result = [
        "--profile", args.profile,
        "--scale", str(args.scale),
        "--palette", args.palette,
    ]
    if args.overlay:
        result.append("--overlay")
    if args.device_frame:
        result.append("--device-frame")
    if args.frames is not None:
        result.extend(("--frames", str(args.frames)))
    return result


def launch(args: argparse.Namespace) -> subprocess.Popen[bytes]:
    command = [str(EXECUTABLE), *simulator_arguments(args)]
    log("$ " + " ".join(command))
    return subprocess.Popen(command, cwd=PROJECT_ROOT)


def stop(process: subprocess.Popen[bytes] | None) -> None:
    if process is None or process.poll() is not None:
        return
    process.send_signal(signal.SIGTERM)
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def watched_paths() -> Iterable[Path]:
    yield from WATCH_FILES
    for root in WATCH_ROOTS:
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in WATCH_SUFFIXES:
                yield path


def snapshot() -> dict[Path, tuple[int, int]]:
    state: dict[Path, tuple[int, int]] = {}
    for path in watched_paths():
        try:
            stat = path.stat()
        except FileNotFoundError:
            continue
        state[path] = (stat.st_mtime_ns, stat.st_size)
    return state


def changed_paths(
    before: dict[Path, tuple[int, int]], after: dict[Path, tuple[int, int]]
) -> set[Path]:
    return {
        path
        for path in before.keys() | after.keys()
        if before.get(path) != after.get(path)
    }


def needs_configure(paths: set[Path]) -> bool:
    return any(
        path.name in {"CMakeLists.txt", "CMakePresets.json"}
        or path.suffix == ".cmake"
        for path in paths
    )


def dev(args: argparse.Namespace) -> None:
    build()
    process = launch(args)
    state = snapshot()
    log("正在监听源码；保存后会增量编译并重启模拟器，按 Ctrl-C 停止。")

    try:
        while True:
            time.sleep(args.poll_interval)
            current = snapshot()
            changed = changed_paths(state, current)
            if not changed:
                continue

            # Editors may write through multiple rename/save events. Wait until the
            # tree has stayed unchanged for one debounce interval before building.
            deadline = time.monotonic() + args.debounce
            while time.monotonic() < deadline:
                delay = min(
                    args.poll_interval, max(0.01, deadline - time.monotonic())
                )
                time.sleep(delay)
                newer = snapshot()
                more_changes = changed_paths(current, newer)
                if more_changes:
                    changed |= more_changes
                    current = newer
                    deadline = time.monotonic() + args.debounce

            state = current
            display = ", ".join(
                str(path.relative_to(PROJECT_ROOT))
                for path in sorted(changed)
                if path.exists()
            )
            log(f"检测到修改：{display or '文件被删除'}")

            try:
                build(reconfigure=needs_configure(changed))
            except subprocess.CalledProcessError:
                log("编译失败；当前模拟器保持运行，等待下一次修改。")
                continue

            stop(process)
            process = launch(args)
            log("编译成功，模拟器已重载。")
    except KeyboardInterrupt:
        log("停止开发模式。")
    finally:
        stop(process)


def sdl_library() -> Path:
    output = subprocess.check_output(["otool", "-L", str(EXECUTABLE)], text=True)
    for line in output.splitlines()[1:]:
        dependency = line.strip().split(" ", 1)[0]
        if Path(dependency).name.startswith("libSDL3"):
            return Path(dependency)
    raise RuntimeError("cannot find the SDL3 dynamic library used by the simulator")


def package() -> Path:
    if sys.platform != "darwin":
        raise RuntimeError("the .app packager currently supports macOS only")

    build()
    app = DIST_DIR / "Cadenza Simulator.app"
    if app.exists():
        shutil.rmtree(app)
    macos_dir = app / "Contents" / "MacOS"
    frameworks_dir = app / "Contents" / "Frameworks"
    macos_dir.mkdir(parents=True)
    frameworks_dir.mkdir(parents=True)

    bundled_executable = macos_dir / "cadenza_desktop"
    shutil.copy2(EXECUTABLE, bundled_executable)
    bundled_executable.chmod(0o755)

    sdl = sdl_library()
    bundled_sdl = frameworks_dir / sdl.name
    shutil.copy2(sdl, bundled_sdl)
    bundled_sdl.chmod(0o755)
    run_checked(
        [
            "install_name_tool",
            "-change",
            str(sdl),
            f"@executable_path/../Frameworks/{sdl.name}",
            str(bundled_executable),
        ]
    )

    info = {
        "CFBundleDevelopmentRegion": "zh_CN",
        "CFBundleDisplayName": "Cadenza Simulator",
        "CFBundleExecutable": "cadenza_desktop",
        "CFBundleIdentifier": "os.cadenza.simulator",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "Cadenza Simulator",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "0.1.0",
        "CFBundleVersion": "1",
        "LSMinimumSystemVersion": "12.0",
        "NSHighResolutionCapable": True,
    }
    with (app / "Contents" / "Info.plist").open("wb") as plist:
        plistlib.dump(info, plist)

    run_checked(["codesign", "--force", "--deep", "--sign", "-", str(app)])
    DIST_DIR.mkdir(exist_ok=True)
    archive_base = DIST_DIR / "cadenza-simulator-macos"
    archive = Path(shutil.make_archive(str(archive_base), "zip", DIST_DIR, app.name))
    log(f"打包完成：{archive}")
    return archive


def add_simulator_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--profile", choices=("t-embed", "sharp"), default="t-embed")
    parser.add_argument("--scale", type=int, choices=range(1, 5), default=2)
    parser.add_argument("--palette", choices=("reflective", "pure"),
                        default="reflective")
    parser.add_argument("--overlay", action="store_true")
    parser.add_argument("--device-frame", action="store_true")
    parser.add_argument("--frames", type=int, help=argparse.SUPPRESS)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("build", help="配置并增量编译桌面模拟器")

    run_parser = commands.add_parser("run", help="编译并运行桌面模拟器")
    add_simulator_options(run_parser)

    dev_parser = commands.add_parser("dev", help="监听源码，编译成功后自动重载")
    add_simulator_options(dev_parser)
    dev_parser.add_argument("--poll-interval", type=float, default=0.25, help=argparse.SUPPRESS)
    dev_parser.add_argument("--debounce", type=float, default=0.2, help=argparse.SUPPRESS)

    commands.add_parser("package", help="生成包含 SDL3 的 macOS .app 和 zip")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "build":
            build()
        elif args.command == "run":
            build()
            return launch(args).wait()
        elif args.command == "dev":
            dev(args)
        elif args.command == "package":
            package()
    except (FileNotFoundError, RuntimeError, subprocess.CalledProcessError) as error:
        log(f"失败：{error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
