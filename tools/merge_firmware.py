# -*- coding: utf-8 -*-
"""
MotoNav Stopwatch: ESP32-S3 Factory Firmware Merger
===================================================
自动将 bootloader、partitions、boot_app0 与 主程序固件合并为一个完整的 .bin 文件。
可直接从 0x0000 烧录，方便在 GitHub Releases 发布、或使用 Web Flasher / M5Burner / esptool 一键刷机。
"""

import os
import sys
import glob
import shutil
import hashlib
import argparse
import subprocess

# 强制 UTF-8 编码输出
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding='utf-8')

# 项目默认配置
DEFAULT_ENV = "stopwatch"
CHIP = "esp32s3"
FLASH_MODE = "dio"
FLASH_FREQ = "80m"
FLASH_SIZE = "16MB"

# 固件布局偏移量 (ESP32-S3 16MB 分区标准)
OFFSET_BOOTLOADER = 0x0000
OFFSET_PARTITIONS = 0x8000
OFFSET_BOOT_APP0  = 0xE000
OFFSET_APP        = 0x10000

# 标准 boot_app0 数据 (8192 字节，以防外部依赖缺失)
BOOT_APP0_DEFAULT = (
    b'\x01\x00\x00\x00' + b'\xFF' * 28 + b'\x9A\x98\x43\x47' +
    b'\xFF' * (8192 - 36)
)


def find_platformio_dir():
    """查找 PlatformIO 用户主目录 (~/.platformio)"""
    home = os.path.expanduser("~")
    pio_home = os.path.join(home, ".platformio")
    if os.path.isdir(pio_home):
        return pio_home
    return None


def find_esptool(pio_home):
    """查找可用的 esptool.exe 或 esptool.py 脚本"""
    # 1. 优先使用本地独立的 esptool.exe（免 Python / pyserial 依赖，100% 独立稳定）
    direct_exe_candidates = [
        r"D:\GreenPrograms\M5Burner-v3-beta-win-x64\packages\tool\esptool.exe",
        r"C:\GreenPrograms\M5Burner-v3-beta-win-x64\packages\tool\esptool.exe",
    ]
    # 模糊匹配各驱动盘下的 M5Burner 目录
    for drive in ["D:\\", "C:\\", "E:\\"]:
        m5_burner_search = os.path.join(drive, "GreenPrograms", "*M5Burner*", "packages", "tool", "esptool.exe")
        direct_exe_candidates.extend(glob.glob(m5_burner_search))

    for exe in direct_exe_candidates:
        if os.path.isfile(exe):
            return [exe]

    # 2. 尝试系统 PATH 中的 esptool / esptool.exe
    if shutil.which("esptool.exe"):
        return [shutil.which("esptool.exe")]
    if shutil.which("esptool"):
        return [shutil.which("esptool")]
    if shutil.which("esptool.py"):
        return [shutil.which("esptool.py")]

    # 3. 从 PlatformIO 安装包中查找 (寻找装有 pyserial 的 Python 执行器)
    if pio_home:
        python_candidates = [
            os.path.join(pio_home, "penv", "Scripts", "python.exe"),
            sys.executable,
            "python"
        ]
        chosen_python = None
        for py in python_candidates:
            if shutil.which(py) or (os.path.isfile(py) if isinstance(py, str) else False):
                try:
                    res = subprocess.run([py, "-c", "import serial, esptool"], capture_output=True)
                    if res.returncode == 0:
                        chosen_python = py
                        break
                except Exception:
                    pass

        patterns = [
            os.path.join(pio_home, "packages", "tool-esptoolpy*", "esptool.py"),
            os.path.join(pio_home, "penv", "Scripts", "esptool.py"),
            os.path.join(pio_home, "penv", "bin", "esptool.py")
        ]
        for pat in patterns:
            matches = glob.glob(pat)
            if matches:
                matches.sort(reverse=True)
                py_exec = chosen_python if chosen_python else sys.executable
                return [py_exec, matches[0]]

    return None


def find_boot_app0(pio_home):
    """查找 boot_app0.bin"""
    if pio_home:
        patterns = [
            os.path.join(pio_home, "packages", "framework-arduinoespressif32*", "tools", "partitions", "boot_app0.bin"),
            os.path.join(pio_home, "packages", "framework-espidf*", "components", "app_update", "boot_app0.bin")
        ]
        for pat in patterns:
            matches = glob.glob(pat)
            if matches:
                return matches[0]
    return None


def calc_checksum(file_path):
    """计算文件 SHA256 与 MD5 校验和"""
    sha256 = hashlib.sha256()
    md5 = hashlib.md5()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            sha256.update(chunk)
            md5.update(chunk)
    return sha256.hexdigest(), md5.hexdigest()


def format_size(bytes_size):
    """格式化字节大小"""
    if bytes_size < 1024:
        return f"{bytes_size} B"
    elif bytes_size < 1024 * 1024:
        return f"{bytes_size / 1024:.2f} KB"
    else:
        return f"{bytes_size / (1024 * 1024):.2f} MB"


def main():
    parser = argparse.ArgumentParser(description="MotoNav ESP32-S3 Factory Firmware Merger for M5Burner")
    parser.add_argument("--env", default=DEFAULT_ENV, help=f"PlatformIO build environment (default: {DEFAULT_ENV})")
    parser.add_argument("--build", action="store_true", help="Compile firmware using PlatformIO before merging")
    parser.add_argument("--output-dir", default="release", help="Directory to save merged binaries (default: release)")
    args = parser.parse_args()

    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(project_root)

    print("=" * 70)
    print(" MotoNav Stopwatch: ESP32-S3 Factory Firmware Merger (M5Burner Ready)")
    print("=" * 70)

    # 1. 如需编译，先调用 pio run
    if args.build:
        print("\n[Step 1/4] Compiling project with PlatformIO...")
        pio_cmd = "pio"
        if not shutil.which("pio"):
            pio_home = find_platformio_dir()
            if pio_home:
                candidate = os.path.join(pio_home, "penv", "Scripts", "platformio.exe")
                if os.path.exists(candidate):
                    pio_cmd = candidate

        ret = subprocess.call([pio_cmd, "run", "-e", args.env])
        if ret != 0:
            print(f"[ERROR] PlatformIO compilation failed with code {ret}")
            sys.exit(1)
        print("[Step 1/4] Compilation completed successfully.")
    else:
        print("\n[Step 1/4] Skipping build (using existing compilation artifacts).")

    # 2. 定位各固件段文件
    build_dir = os.path.join(project_root, ".pio", "build", args.env)
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    partitions_path = os.path.join(build_dir, "partitions.bin")
    firmware_path   = os.path.join(build_dir, "firmware.bin")

    missing = []
    if not os.path.exists(bootloader_path): missing.append("bootloader.bin")
    if not os.path.exists(partitions_path): missing.append("partitions.bin")
    if not os.path.exists(firmware_path):   missing.append("firmware.bin")

    if missing:
        print(f"\n[ERROR] Missing build artifacts in {build_dir}:")
        for m in missing:
            print(f"  - {m}")
        print("\nPlease run with '--build' or run 'pio run -e stopwatch' first to generate build files.")
        sys.exit(1)

    pio_home = find_platformio_dir()
    boot_app0_path = find_boot_app0(pio_home)

    temp_boot_app0 = None
    if not boot_app0_path or not os.path.exists(boot_app0_path):
        temp_boot_app0 = os.path.join(build_dir, "boot_app0.bin")
        print(f"[INFO] boot_app0.bin not found in framework, creating fallback at {temp_boot_app0}")
        with open(temp_boot_app0, "wb") as f:
            f.write(BOOT_APP0_DEFAULT)
        boot_app0_path = temp_boot_app0

    esptool_cmd = find_esptool(pio_home)
    if not esptool_cmd:
        print("[ERROR] Could not find esptool! Please install esptool or PlatformIO.")
        sys.exit(1)

    print("\n[Step 2/4] Verified firmware components:")
    print(f"  - [0x{OFFSET_BOOTLOADER:04X}] Bootloader:   {bootloader_path} ({format_size(os.path.getsize(bootloader_path))})")
    print(f"  - [0x{OFFSET_PARTITIONS:04X}] Partitions:   {partitions_path} ({format_size(os.path.getsize(partitions_path))})")
    print(f"  - [0x{OFFSET_BOOT_APP0:04X}] Boot_App0:    {boot_app0_path} ({format_size(os.path.getsize(boot_app0_path))})")
    print(f"  - [0x{OFFSET_APP:04X}] Application:  {firmware_path} ({format_size(os.path.getsize(firmware_path))})")

    # 3. 执行合并操作
    out_dir = os.path.join(project_root, args.output_dir)
    os.makedirs(out_dir, exist_ok=True)

    factory_bin = os.path.join(out_dir, "MotoNav_stopwatch_factory_0x0000.bin")
    app_ota_bin = os.path.join(out_dir, "MotoNav_stopwatch_app_ota_0x10000.bin")

    print("\n[Step 3/4] Merging all segments into a single factory binary...")

    merge_args = esptool_cmd + [
        "--chip", CHIP,
        "merge_bin",
        "-o", factory_bin,
        "--flash_mode", FLASH_MODE,
        "--flash_freq", FLASH_FREQ,
        "--flash_size", FLASH_SIZE,
        f"0x{OFFSET_BOOTLOADER:x}", bootloader_path,
        f"0x{OFFSET_PARTITIONS:x}", partitions_path,
        f"0x{OFFSET_BOOT_APP0:x}", boot_app0_path,
        f"0x{OFFSET_APP:x}", firmware_path
    ]

    ret = subprocess.call(merge_args)
    if ret != 0:
        print(f"[ERROR] esptool merge_bin failed with code {ret}")
        sys.exit(1)

    # 同步复制一份单独的应用层固件 (方便仅做 OTA 升级的用户)
    shutil.copyfile(firmware_path, app_ota_bin)

    # 4. 生成校验和与报告
    print("\n[Step 4/4] Output files and flash information:")
    factory_size = os.path.getsize(factory_bin)
    factory_sha, factory_md5 = calc_checksum(factory_bin)

    app_size = os.path.getsize(app_ota_bin)
    app_sha, app_md5 = calc_checksum(app_ota_bin)

    print("-" * 70)
    print(f"1. 完整工厂固件 (出厂/全量线刷/M5Burner 发布，直接从 0x0000 烧录):")
    print(f"   路径: {os.path.relpath(factory_bin, project_root)}")
    print(f"   大小: {factory_size} 字节 ({format_size(factory_size)})")
    print(f"   MD5 : {factory_md5}")
    print(f"   SHA : {factory_sha[:16]}...{factory_sha[-16:]}")
    print()
    print(f"2. 应用程序固件 (仅主程序，用于 OTA 升级或 0x10000 烧录):")
    print(f"   路径: {os.path.relpath(app_ota_bin, project_root)}")
    print(f"   大小: {app_size} 字节 ({format_size(app_size)})")
    print(f"   MD5 : {app_md5}")
    print("-" * 70)

    print("\n[烧录指南 Flash Instructions]:")
    print("方法 A: M5Burner 官方工具烧录 / 自定义固件导入 (推荐)")
    print("  1. 打开 M5Burner，选择设备型号 Stopwatch。")
    print("  2. 在自定义固件（Burn / Custom）中选择 MotoNav_stopwatch_factory_0x0000.bin")
    print("  3. 烧录起始地址填 0x0000，连接设备串口后点击 Burn 即可一键刷入！\n")

    print("方法 B: 网页一键刷机 (免驱动免安装环境)")
    print("  1. 打开 ESP Web Flasher (如 https://espressif.github.io/esptool-js/ 或 https://web.esphome.io/)")
    print("  2. 连接设备 COM 口，波特率 921600")
    print("  3. 添加文件: MotoNav_stopwatch_factory_0x0000.bin，地址填 0x0000")
    print("  4. 点击 Program 即可完整烧录并开机！\n")

    print("方法 C: 命令行工具 esptool 烧录")
    print(f"  esptool.py --chip {CHIP} write_flash 0x0000 {os.path.relpath(factory_bin, project_root)}\n")
    print("=" * 70)


if __name__ == "__main__":
    main()
