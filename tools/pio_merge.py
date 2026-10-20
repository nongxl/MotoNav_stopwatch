import os
import subprocess

Import("env")

def merge_bin_action(source, target, env):
    project_dir = env.get("PROJECT_DIR")
    script_path = os.path.join(project_dir, "tools", "merge_firmware.py")
    env_name = env.get("PIOENV")
    
    cmd = ["python", script_path, "--env", env_name]
    print(f"\n[PlatformIO Hook] Triggering firmware merge: {' '.join(cmd)}")
    subprocess.call(cmd)

# 添加自定义构建目标: pio run -e stopwatch -t mergebin
env.AddCustomTarget(
    name="mergebin",
    dependencies=["$BUILD_DIR/${PROGNAME}.bin"],
    actions=[merge_bin_action],
    title="Merge Factory Binary",
    description="Merges bootloader, partitions, boot_app0, and app into release/MotoNav_stopwatch_factory_0x0000.bin"
)
