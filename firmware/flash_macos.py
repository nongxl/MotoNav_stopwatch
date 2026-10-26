#!/usr/bin/env python3
"""Flash the repository's verified 1.75-inch image with the local Arduino esptool."""
import argparse
import glob
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
BOARD = "Waveshare ESP32-S3-Touch-AMOLED-1.75"


def usb_ports():
    patterns = ("/dev/cu.usbmodem*", "/dev/cu.usbserial*", "/dev/cu.SLAB_USBtoUART*", "/dev/cu.wchusbserial*")
    return sorted({port for pattern in patterns for port in glob.glob(pattern)})


def find_tool():
    base = Path.home() / "Library/Arduino15/packages/esp32/tools/esptool_py"
    versions = sorted(base.glob("*/esptool"), key=lambda p: tuple(int(n) for n in re.findall(r"\d+", p.parent.name)), reverse=True)
    for candidate in versions:
        if os.access(candidate, os.X_OK):
            return str(candidate)
    candidate = shutil.which("esptool")
    if candidate:
        return candidate
    raise ValueError("未找到 esptool。请先安装 Arduino ESP32 Core 3.3.10。")


def validate_images(root=ROOT):
    source = root / "firmware/boards/175/MotoNav/src/core/FirmwareConfig.h"
    if not source.exists():
        source = root / "firmware/boards/175/MotoNav/MotoNav.ino"
    if source.exists():
        match = re.search(r'^#define FW_VERSION "v([^\"]+)"', source.read_text(), re.MULTILINE)
        if not match:
            raise ValueError("无法读取当前固件版本。")
        version = match.group(1)
    else:
        version = (root / 'firmware/RELEASE_VERSION').read_text().strip()
        if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9][A-Za-z0-9.-]*)?', version):
            raise ValueError("固件版本标记无效。")
    folder = root / "firmware/dist/175/panel0"
    manifests = sorted(folder.glob("*_manifest.json"))
    for path in manifests:
        manifest = json.loads(path.read_text())
        if manifest.get("firmware") != version or manifest.get("board") != BOARD:
            continue
        images = {}
        for item in manifest.get("files", []):
            name = item["file"]
            if Path(name).name != name:
                raise ValueError("固件清单包含无效路径。")
            image = folder / name
            data = image.read_bytes()
            if len(data) != item["bytes"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
                raise ValueError("固件校验失败：" + name)
            if name.endswith("_app_0x10000.bin"):
                images["upgrade"] = image
            elif name.endswith("_full_0x0.bin"):
                images["full"] = image
        if set(images) != {"upgrade", "full"}:
            raise ValueError("缺少完整镜像或应用升级镜像。")
        app, full = images["upgrade"].read_bytes(), images["full"].read_bytes()
        if not app or app[0] != 0xE9 or full[0x10000:0x10000 + len(app)] != app:
            raise ValueError("固件格式或应用偏移校验失败。")
        return version, images
    raise ValueError("缺少与源码版本匹配的固件及 SHA-256 清单，请先构建固件。")


def flash_command(tool, port, image, full=False):
    return [tool, "--chip", "esp32s3", "--port", port, "--baud", "460800",
            "--before", "default-reset", "--after", "hard-reset", "write-flash",
            "--flash-size", "16MB", "0x0" if full else "0x10000", str(image)]


def main(argv=None):
    parser = argparse.ArgumentParser(description="Mac 一键烧录 MotoNav 1.75 英寸圆屏固件（默认应用升级）")
    parser.add_argument("--check", action="store_true", help="仅检查工具、固件和串口，不烧录")
    parser.add_argument("--port", help="指定实际 USB 串口，例如 /dev/cu.usbmodem…")
    parser.add_argument("--full", action="store_true", help="首次完整安装，替换引导与分区布局")
    args = parser.parse_args(argv)
    try:
        tool = find_tool()
        version, images = validate_images()
        ports = usb_ports()
        print("MotoNav v" + version + " · 微雪 1.75 英寸圆屏", flush=True)
        print("工具：" + tool, flush=True)
        print("固件 SHA-256 与应用偏移校验通过。", flush=True)
        if args.check:
            print("USB 串口：" + ("、".join(ports) if ports else "未检测到圆屏，请连接 USB 数据线。"))
            print("检查完成，未执行烧录。")
            return 0
        port = args.port
        if not port:
            if not ports:
                raise ValueError("未检测到圆屏 USB 串口。请用数据线连接圆屏，关闭串口监视器后重新双击。")
            if len(ports) == 1:
                port = ports[0]
            else:
                if not sys.stdin.isatty():
                    raise ValueError("检测到多个 USB 串口，请使用 --port 明确选择圆屏。")
                for i, value in enumerate(ports, 1):
                    print(f"{i}. {value}")
                choice = input("请选择圆屏的串口编号：").strip()
                if not choice.isdigit() or not 1 <= int(choice) <= len(ports):
                    raise ValueError("无效的串口编号，未烧录。")
                port = ports[int(choice) - 1]
        if port not in ports:
            raise ValueError("指定的 USB 串口不存在或不属于当前可用 USB 设备：" + port)
        if args.full:
            print("首次完整安装将写入引导程序、分区表和应用，可能影响原设置。请先备份。")
            if input("输入 INSTALL 确认完整安装：").strip() != "INSTALL":
                raise ValueError("已取消完整安装。")
        else:
            print("应用升级：写入 0x10000，不执行整片擦除；要求现有 16 MB / app3M_fat9M_16MB 分区。")
        image = images["full" if args.full else "upgrade"]
        command = flash_command(tool, port, image, args.full)
        print("串口：" + port)
        print("开始烧录：" + shlex.join(command), flush=True)
        subprocess.run(command, check=True)
        print("烧录完成。圆屏将重启，请确认屏幕显示 v" + version + "。")
        return 0
    except (ValueError, OSError, KeyError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print("烧录未完成：" + str(error), file=sys.stderr)
        print("若无法进入下载模式，请按板卡说明使用 BOOT/RESET 后重试；本脚本不会自动擦除全部闪存。", file=sys.stderr)
        return 1
    except (KeyboardInterrupt, EOFError):
        print("操作已取消。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
