import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location("flash_macos", Path(__file__).resolve().parents[1] / "flash_macos.py")
flash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flash)


class FlashTests(unittest.TestCase):
    def test_split_configuration_takes_priority(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "firmware/boards/175/MotoNav/src/core/FirmwareConfig.h"
            source.parent.mkdir(parents=True)
            source.write_text('#define FW_VERSION "v7.12.9-junction.11"\n')
            (source.parents[2] / 'MotoNav.ino').write_text('// structured entry point\n')
            out = root / 'firmware/dist/175/panel0'
            out.mkdir(parents=True)
            app = bytes([0xE9]) + b'generic application'
            entries = []
            for name, data in [('new_app_0x10000.bin', app), ('new_full_0x0.bin', bytes(0x10000) + app)]:
                (out / name).write_bytes(data)
                entries.append({'file': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
            (out / 'new_manifest.json').write_text(json.dumps({'firmware': '7.12.9-junction.11', 'board': flash.BOARD, 'files': entries}))
            version, _ = flash.validate_images(root)
            self.assertEqual(version, '7.12.9-junction.11')
            source.unlink()
            (source.parents[2] / 'MotoNav.ino').unlink()
            (root / 'firmware/RELEASE_VERSION').write_text('7.12.9-junction.11\n')
            version, _ = flash.validate_images(root)
            self.assertEqual(version, '7.12.9-junction.11')

    def test_upgrade_and_full_addresses(self):
        upgrade = flash.flash_command("esptool", "port", Path("app.bin"))
        full = flash.flash_command("esptool", "port", Path("full.bin"), True)
        self.assertEqual(upgrade[-2:], ["0x10000", "app.bin"])
        self.assertEqual(full[-2:], ["0x0", "full.bin"])
        self.assertIn("esp32s3", upgrade)
        self.assertNotIn("--erase-all", upgrade)
        self.assertNotIn("--force", upgrade)

    def test_check_never_writes(self):
        with mock.patch.object(flash, "find_tool", return_value="esptool"), \
             mock.patch.object(flash, "validate_images", return_value=("7.9.0", {})), \
             mock.patch.object(flash, "usb_ports", return_value=[]), \
             mock.patch.object(flash.subprocess, "run") as run, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(flash.main(["--check"]), 0)
            run.assert_not_called()

    def test_absent_device_never_writes(self):
        with mock.patch.object(flash, "find_tool", return_value="esptool"), \
             mock.patch.object(flash, "validate_images", return_value=("7.9.0", {})), \
             mock.patch.object(flash, "usb_ports", return_value=[]), \
             mock.patch.object(flash.subprocess, "run") as run, \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(flash.main([]), 1)
            run.assert_not_called()

    def test_multiple_devices_never_guess(self):
        with mock.patch.object(flash, "find_tool", return_value="esptool"), \
             mock.patch.object(flash, "validate_images", return_value=("7.9.0", {})), \
             mock.patch.object(flash, "usb_ports", return_value=["port1", "port2"]), \
             mock.patch.object(flash.sys.stdin, "isatty", return_value=False), \
             mock.patch.object(flash.subprocess, "run") as run, \
             contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(flash.main([]), 1)
            run.assert_not_called()

    def test_valid_and_corrupted_images(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / "firmware/boards/175/MotoNav/MotoNav.ino"
            source.parent.mkdir(parents=True)
            source.write_text('#define FW_VERSION "v7.9.0"\n')
            out = root / "firmware/dist/175/panel0"
            out.mkdir(parents=True)
            app = bytes([0xE9]) + b"test application"
            images = {"test_app_0x10000.bin": app, "test_full_0x0.bin": bytes(0x10000) + app}
            entries = []
            for name, data in images.items():
                (out / name).write_bytes(data)
                entries.append({"file": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
            (out / "test_manifest.json").write_text(json.dumps({"firmware": "7.9.0", "board": flash.BOARD, "files": entries}))
            version, _ = flash.validate_images(root)
            self.assertEqual(version, "7.9.0")
            (out / "test_app_0x10000.bin").write_bytes(b"corrupted")
            with self.assertRaisesRegex(ValueError, "校验失败"):
                flash.validate_images(root)


if __name__ == "__main__":
    unittest.main()
