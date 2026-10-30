"""Run standard unit regressions with JDK 17 (no APK or device needed)."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
source = root / 'app/src/main/java/com/example/motonav'
output = root / '.local/tests-bin'
output.mkdir(parents=True, exist_ok=True)
suffix = '.exe' if os.name == 'nt' else ''
java_home = os.environ.get('JAVA_HOME')
def java_tool(name):
    return str(Path(java_home) / 'bin' / (name + suffix)) if java_home else name

src_names = ['AmapBuildings', 'RasterPacket', 'BleSession', 'CrossPacket', 'CoverPacket']
test_names = ['RasterCompressionDeltaTest', 'CrossPacketTest', 'CoverPacketTest']

subprocess.run([
    java_tool('javac'), '-encoding', 'UTF-8', '-d', str(output),
    *[str(source / (name + '.java')) for name in src_names],
    *[str(root / 'tests' / (test + '.java')) for test in test_names]
], check=True)

for test in test_names:
    print(f"Running {test}...")
    subprocess.run([java_tool('java'), '-cp', str(output), 'com.example.motonav.' + test], check=True)
print("All standard Java unit tests passed!")
