# SPDX-License-Identifier: MIT
"""Cook a base release and its content-only course, then write a static download catalog."""
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from tempfile import TemporaryDirectory


ATTRIBUTION = "Course layout © OpenStreetMap contributors, available under the Open Database License 1.0"
LAYOUT_FILES = ("source_layout.json", "source.geojson")


def layout_files(source):
    """The derived layout published with each pack, as ODbL requires: the prepared layout, the map extract,
    the author corrections named in the source's recipe.json and the licence note beside them."""
    recipe = json.loads((source / "recipe.json").read_text())
    files = [source / name for name in LAYOUT_FILES]
    if recipe.get("corrections"):
        corrections = Path(recipe["corrections"])
        files += [corrections, corrections.with_name("LICENSE.md")]
    for path in files:
        if not path.is_file():
            raise FileNotFoundError(f"Publishing needs {path}; prepare the site first")
    return files


def publish(files, host, entry, layout=()):
    """Copies a pack and its layout into the static host and lists it in catalog.json.

    A published version is never overwritten. Every platform of one version shares its layout folder.
    """
    folder = host / entry["id"] / entry["version"] / entry["platform"]
    folder.parent.mkdir(parents=True, exist_ok=True)
    if folder.exists():
        raise ValueError("This version already exists. Publish a new course version.")
    layout_folder = folder.parent / "layout"
    if layout:
        entry = {**entry, "layout": f"{entry['id']}/{entry['version']}/layout/", "attribution": ATTRIBUTION}
    with TemporaryDirectory(dir=folder.parent) as stage:
        content = Path(stage) / "files"
        content.mkdir()
        files_list = []
        for path in files:
            name = "Course" + path.suffix
            dest = content / name
            shutil.copyfile(path, dest)
            with dest.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            files_list.append({"name": name, "size": dest.stat().st_size, "sha256": digest})
        entry = {**entry, "files": files_list}
        staged_layout = Path(stage) / "layout"
        if layout and not layout_folder.exists():
            staged_layout.mkdir()
            for path in layout:
                shutil.copyfile(path, staged_layout / path.name)
        catalog = host / "catalog.json"
        data = json.loads(catalog.read_text()) if catalog.is_file() else {"courses": []}
        data["courses"] = [c for c in data["courses"] if (c["id"],c["platform"],c["game_version"]) !=
                           (entry["id"],entry["platform"],entry["game_version"])]
        data["courses"].append(entry)
        temp = Path(stage) / "catalog.json"
        temp.write_text(json.dumps(data, indent=2))
        if staged_layout.exists():
            staged_layout.rename(layout_folder)
        content.rename(folder)
        temp.replace(catalog)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--platform", choices=["Linux", "Win64"], required=True)
    parser.add_argument("--course", default="HunterRanch")
    parser.add_argument("--definition", default="/HunterRanch/Course/DA_HunterRanch.DA_HunterRanch")
    parser.add_argument("--name", default="Hunter Ranch")
    parser.add_argument("--version", required=True)
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--source", type=Path, help="Prepared site folder (default Saved/CourseDesign/<course>)")
    args = parser.parse_args()
    if not all(re.fullmatch(r"[A-Za-z0-9_-]+", x) for x in (args.course,args.version)):
        parser.error("Use letters, digits, underscores or hyphens in the course ID and version.")
    root = Path(__file__).resolve().parents[2]
    project = root / "UOpenGolf.uproject"
    layout = layout_files(args.source or root / "Saved/CourseDesign" / args.course)
    config = configparser.ConfigParser(strict=False, interpolation=None)
    config.read(root / "Config/DefaultGame.ini")
    version = config["/Script/EngineSettings.GeneralProjectSettings"]["ProjectVersion"]
    release = "Courses-" + version
    uat = args.engine / "Engine/Build/BatchFiles" / ("RunUAT.bat" if os.name == "nt" else "RunUAT.sh")
    common = [str(uat), "BuildCookRun", f"-project={project}", "-nop4", "-utf8output", "-unattended",
              f"-platform={args.platform}", "-clientconfig=Development", "-cook", "-stage", "-pak", "-iostore"]
    work = root / "Saved/CoursePackaging"
    registry = work / "Releases"
    subprocess.run(common + ["-build", "-map=/Game/UOpenGolf/Maps/L_SavannaRange", f"-createreleaseversion={release}",
                             f"-createreleaseversionroot={registry}", f"-stagingdirectory={work / 'Base'}"], check=True)
    with TemporaryDirectory(dir=work) as temp:
        subprocess.run(common + [f"-dlcname={args.course}", f"-basedonreleaseversion={release}",
                                 f"-basedonreleaseversionroot={registry}", "-DLCIncludeEngineContent",
                                 f"-stagingdirectory={temp}"], check=True)
        paks = list(Path(temp).rglob("*.pak"))
        if len(paks) != 1:
            raise ValueError(f"Expected one standalone course container, found {paks}")
        base = paks[0]
        publish([base, base.with_suffix(".utoc"), base.with_suffix(".ucas")], args.host.resolve(),
                {"id": args.course, "name": args.name, "definition": args.definition, "version": args.version,
                 "platform": "Windows" if args.platform == "Win64" else "Linux", "game_version": version, "files": []},
                layout)


if __name__ == "__main__":
    main()
