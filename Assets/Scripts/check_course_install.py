# SPDX-License-Identifier: MIT
"""Exercise the public catalog, offline play and removal using the packaged game."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
from threading import Thread


def run(game, profile, base_url, action, report):
    report.mkdir(parents=True, exist_ok=True)
    args = [str(game), "-nullrhi", "-nosound", "-unattended", "-nopause", "-nosplash", "-stdout",
            f"-UserDir={profile}", "-SaveToUserDir", f"-ini:Game:[CourseDownloads]:BaseUrl={base_url}",
            f"-CourseTestAction={action}", "-ExecCmds=Automation RunTests UOpenGolf.Course.PackagedDownload",
            "-TestExit=Automation Test Queue Empty", f"-ReportExportPath={report}", f"-abslog={report/'game.log'}"]
    with (report / "stdout.log").open("wb") as log:
        result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, timeout=150)
    path = report / "index.json"
    # -TestExit makes Unreal exit with 1 on Linux even after a clean run (see ue.sh), so the report decides.
    if result.returncode > 1 or not path.exists():
        raise RuntimeError(f"Test {action} exited {result.returncode}. See {report}.")
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if data["succeeded"] + data.get("succeededWithWarnings", 0) != 1 or data["failed"]:
        raise RuntimeError(f"Course {action} failed. See {report}.")
    print(f"{action} passed", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("game", type=Path)
    parser.add_argument("catalog_folder", type=Path)
    parser.add_argument("reports", type=Path)
    args = parser.parse_args()
    reports = args.reports.resolve()
    reports.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory() as temp:
        profile = Path(temp)
        with ThreadingHTTPServer(("127.0.0.1", 0), partial(SimpleHTTPRequestHandler, directory=args.catalog_folder.resolve())) as http:
            base = f"http://127.0.0.1:{http.server_port}"
            server = Thread(target=http.serve_forever, daemon=True)
            server.start()
            try:
                for action in ("absent", "install"):
                    run(args.game.resolve(), profile, base, action, reports / action)
            finally:
                http.shutdown()
                server.join()
        for action in ("play", "remove", "absent"):
            run(args.game.resolve(), profile, base, action, reports / ("offline-" + action))


if __name__ == "__main__":
    main()
