# SPDX-License-Identifier: MIT
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile

import FetchAmbientCG as fetch


class AmbientCGDownloadTests(unittest.TestCase):
    def test_every_asset_is_pinned(self):
        self.assertEqual(set(fetch.ASSETS), {'Grass008', 'Ground093C', 'Rock029'})
        for asset, sha256 in fetch.ASSETS.items():
            self.assertEqual(len(sha256), 64, asset)
            int(sha256, 16)

    def test_download_identifies_client_and_writes_response(self):
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / 'source.zip'
            with patch.object(fetch, 'urlopen', return_value=io.BytesIO(b'archive')) as request:
                fetch.download('Ground093C', target)
            self.assertEqual(target.read_bytes(), b'archive')
            sent = request.call_args.args[0]
            self.assertEqual(sent.full_url, 'https://ambientcg.com/get?file=Ground093C_2K-JPG.zip')
            self.assertIn('UOpenGolf', sent.get_header('User-agent'))
            self.assertEqual(request.call_args.kwargs['timeout'], 90)

    def test_wrong_checksum_preserves_existing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / 'source.zip'
            archive.write_bytes(b'wrong archive')
            existing = root / 'Ground093C_2K-JPG_Color.jpg'
            existing.write_bytes(b'original')
            with self.assertRaisesRegex(ValueError, 'checksum'):
                fetch.extract('Ground093C', archive, root)
            self.assertEqual(existing.read_bytes(), b'original')

    def test_a_verified_archive_yields_only_the_three_maps(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / 'source.zip'
            with ZipFile(archive, 'w') as zip_file:
                for role in ('Color', 'NormalDX', 'NormalGL', 'Roughness', 'Displacement'):
                    zip_file.writestr(f'Test_2K-JPG_{role}.jpg', role)
            import hashlib
            with patch.dict(fetch.ASSETS, {'Test': hashlib.sha256(archive.read_bytes()).hexdigest()}):
                fetch.fetch('Test', archive, root / 'out')
            self.assertEqual(sorted(p.name for p in (root / 'out/Test').iterdir()),
                             ['Test_2K-JPG_Color.jpg', 'Test_2K-JPG_NormalDX.jpg', 'Test_2K-JPG_Roughness.jpg'])


if __name__ == '__main__':
    unittest.main()
