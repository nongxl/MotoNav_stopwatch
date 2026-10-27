"""Verify input integrity, atomic downloads, and offline behavior without external services."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

MODULE = Path(__file__).resolve().parents[1] / 'provider_download.py'
loader = importlib.util.spec_from_file_location('provider_download', MODULE)
provider = importlib.util.module_from_spec(loader)
loader.loader.exec_module(provider)


class ProviderDownloadTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.data = b'synthetic-compatible-input'
        self.spec = {'url': 'https://example.invalid/provider.apk', 'fileName': 'provider.apk',
                     'bytes': len(self.data), 'sha256': hashlib.sha256(self.data).hexdigest()}
        self.root_patch = patch.object(provider, 'ROOT', self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)
        self.spec_patch = patch.object(provider, 'read_spec', return_value=self.spec)
        self.spec_patch.start()
        self.addCleanup(self.spec_patch.stop)

    def test_verified_cache_works_offline_without_network(self):
        target = self.root / 'inputs/provider.apk'
        target.parent.mkdir()
        target.write_bytes(self.data)
        with patch.object(provider, 'urlopen') as network:
            self.assertEqual(provider.resolve_provider(offline=True), target.resolve())
            network.assert_not_called()

    def test_missing_cache_offline_does_not_attempt_download(self):
        with patch.object(provider, 'urlopen') as network:
            with self.assertRaisesRegex(ValueError, 'Offline mode'):
                provider.resolve_provider(offline=True)
            network.assert_not_called()

    def test_successful_download_is_verified_and_atomically_cached(self):
        with patch.object(provider, 'urlopen', return_value=io.BytesIO(self.data)) as network:
            result = provider.resolve_provider()
            self.assertEqual(result.read_bytes(), self.data)
            self.assertEqual(result.name, 'provider.apk')
            network.assert_called_once()
        self.assertEqual(list(result.parent.glob('*.part')), [])

    def test_invalid_download_is_not_cached_and_partial_files_are_removed(self):
        with patch.object(provider, 'urlopen', side_effect=lambda *a, **k: io.BytesIO(b'x' * len(self.data))), \
                patch.object(provider.time, 'sleep'):
            with self.assertRaisesRegex(ValueError, 'SHA256 mismatch'):
                provider.resolve_provider()
        self.assertFalse((self.root / 'inputs/provider.apk').exists())
        self.assertEqual(list((self.root / 'inputs').glob('*.part')), [])

    def test_invalid_existing_input_is_rejected_without_network(self):
        path = self.root / 'custom.apk'
        path.write_bytes(b'x' * len(self.data))
        with patch.object(provider, 'urlopen') as network:
            with self.assertRaisesRegex(ValueError, 'SHA256 mismatch'):
                provider.resolve_provider(path)
            network.assert_not_called()


if __name__ == '__main__':
    unittest.main()
