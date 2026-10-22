# SPDX-License-Identifier: MIT
"""Fetch pinned ambientCG (CC0) surface maps. Only the Unreal-required 2K images are extracted.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchAmbientCG.py Grass008 Ground093C Rock029 [--archive ZIP]

Each archive must match its reviewed SHA-256. Its 2K colour, DirectX normal and roughness maps go to
Saved/AssetImport/<asset>, where BuildSurfaceTextures.py imports them. If the download host refuses
the script, download <asset>_2K-JPG.zip from https://ambientcg.com/view?id=<asset> in a browser and
pass it with --archive (one asset at a time).
"""
import argparse
import hashlib
from pathlib import Path
import tempfile
from urllib.error import URLError
from urllib.request import Request, urlopen
from zipfile import BadZipFile, ZipFile

ROLES = ('Color', 'NormalDX', 'Roughness')
# asset -> SHA-256 of its reviewed 2K-JPG archive.
ASSETS = {
    'Grass008': 'e83f37c63c1f5e3e4ee7d66cdb6b32277108c94e19cab7754595c5c60ed750e7',  # fairway and green turf
    'Ground093C': '7f0223582641a1407ff6050029ed46ad3c4e66f21a15ffb51880d092617f9ecf',  # bunker sand
    'Rock029': '88ae30780d19bda3577503df76fda3e2e1c01076f1e76a1d76d6dcd630d48fe2',  # desert rock layer
}
DESTINATION = Path(__file__).resolve().parents[3] / 'Saved/AssetImport'


def url(asset):
    return f'https://ambientcg.com/get?file={asset}_2K-JPG.zip'


def download(asset, archive):
    request = Request(url(asset), headers={'User-Agent': 'UOpenGolf/1.0 (CC0 asset downloader)',
                                           'Accept': 'application/zip,*/*'})
    with urlopen(request, timeout=90) as response:
        Path(archive).write_bytes(response.read())


def extract(asset, archive, destination):
    """Verify the archive, then write its maps. Nothing is written when the checksum differs."""
    archive, destination = Path(archive), Path(destination)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ASSETS[asset]:
        raise ValueError(f'{asset} archive checksum does not match the reviewed version')
    with ZipFile(archive) as source:
        images = {f'{asset}_2K-JPG_{role}.jpg': source.read(f'{asset}_2K-JPG_{role}.jpg') for role in ROLES}
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in images.items():
        (destination / name).write_bytes(data)


def fetch(asset, archive=None, destination=DESTINATION):
    target = Path(destination) / asset
    if archive:
        extract(asset, archive, target)
        return target
    with tempfile.TemporaryDirectory() as temporary:
        downloaded = Path(temporary) / f'{asset}.zip'
        download(asset, downloaded)
        extract(asset, downloaded, target)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('assets', nargs='+', choices=sorted(ASSETS))
    parser.add_argument('--archive', type=Path, help='Use a previously downloaded archive (one asset only)')
    parser.add_argument('--destination', type=Path, default=DESTINATION)
    args = parser.parse_args()
    if args.archive and len(args.assets) != 1:
        parser.error('--archive takes exactly one asset')
    for asset in args.assets:
        try:
            target = fetch(asset, args.archive, args.destination)
        except (URLError, OSError, ValueError, BadZipFile, KeyError) as error:
            parser.exit(1, f'{asset} fetch failed: {error}\n'
                        f'You can download {asset}_2K-JPG.zip from https://ambientcg.com/view?id={asset} '
                        f'and rerun with {asset} --archive /path/to/{asset}_2K-JPG.zip.\n')
        print(f'{asset} 2K maps verified: {target}')


if __name__ == '__main__':
    main()
