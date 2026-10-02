#!/usr/bin/env python3
"""Local KOReader Sync (KOSync) server for simulator previews.

The simulator sends KOReader Sync requests through the host's curl, so pointing
the firmware at this server exercises the real sync client, progress mapping,
and result screens without a KOReader account or a second device.

Typical use, from the consuming firmware repo:

    python3 ../crossink-simulator/tools/mock_kosync_server.py --seed-sd ./fs_
    pio run -e simulator -t run_simulator

Open a book, then open KOReader Sync from the reader menu. Every GET returns
the scenario chosen on the command line, so the Apply/Upload comparison screen
appears each time. Uploads are accepted and logged.

Standard library only. Binds to 127.0.0.1 by default.
"""

import argparse
import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CREDENTIALS_PATH = os.path.join(".crosspoint", "koreader.json")
# Must match CONFIG_VERSION in the firmware's KOReaderCredentialStore.cpp so the
# seeded file is not treated as a pre-v2 config and migrated.
CREDENTIALS_CONFIG_VERSION = 2


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="127.0.0.1", help="Address to bind (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=17200, help="Port to listen on (default: 17200)")
    parser.add_argument(
        "--percentage",
        type=float,
        default=0.62,
        help="Remote book progress, 0.0-1.0 or 0-100 (default: 0.62)",
    )
    parser.add_argument(
        "--chapter",
        type=int,
        default=5,
        help="1-based spine item the remote position starts at (default: 5). "
        "Must exist in the book or the firmware reports a mapping failure.",
    )
    parser.add_argument(
        "--device",
        default="Kobo Libra 2",
        help='Remote device name shown on the result screen (default: "Kobo Libra 2"). Pass "" to omit it.',
    )
    parser.add_argument(
        "--no-progress",
        action="store_true",
        help="Report that the server has no progress for the book (shows the upload prompt)",
    )
    parser.add_argument(
        "--echo",
        action="store_true",
        help="Return the last uploaded progress for a book instead of the scenario, once one exists",
    )
    parser.add_argument(
        "--reject-auth",
        action="store_true",
        help="Answer 401 to every request to preview authentication failures",
    )
    parser.add_argument(
        "--seed-sd",
        metavar="SD_ROOT",
        help="Write KOReader Sync credentials pointing at this server into SD_ROOT/.crosspoint/koreader.json "
        "(an existing file is kept as koreader.json.bak), then start the server",
    )
    args = parser.parse_args()
    if args.percentage > 1.0:
        args.percentage /= 100.0
    if not 0.0 <= args.percentage <= 1.0:
        parser.error("--percentage must be between 0 and 1 (or 0 and 100)")
    if args.chapter < 1:
        parser.error("--chapter must be 1 or greater")
    return args


def seed_credentials(sd_root, base_url):
    if not os.path.isdir(sd_root):
        sys.exit(f"--seed-sd: {sd_root} is not a directory")
    path = os.path.join(sd_root, CREDENTIALS_PATH)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if os.path.exists(path):
        backup = path + ".bak"
        if os.path.exists(backup):
            print(f"Keeping existing backup {backup}")
        else:
            os.replace(path, backup)
            print(f"Backed up existing credentials to {backup}")
    config = {
        "cfgVersion": CREDENTIALS_CONFIG_VERSION,
        "username": "simulator",
        # The firmware accepts a plain password when password_obf is absent and
        # re-saves it in obfuscated form on first load.
        "password": "simulator",
        "serverUrl": base_url,
        "matchMethod": 0,  # Filename
        "sendMetadata": False,
        "syncBehavior": 0,  # Ask every time, so the comparison screen always appears
    }
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(config, handle, indent=2)
    print(f"Seeded {path} -> {base_url}")


def make_handler(args):
    uploads = {}

    class Handler(BaseHTTPRequestHandler):
        server_version = "MockKOSync/1.0"

        def log_message(self, fmt, *log_args):
            sys.stderr.write("[kosync] " + (fmt % log_args) + "\n")

        def send_json(self, status, payload):
            body = json.dumps(payload).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def authorized(self):
            if args.reject_auth:
                self.send_json(401, {"code": 2001, "message": "Unauthorized"})
                return False
            return True

        def read_json_body(self):
            length = int(self.headers.get("Content-Length") or 0)
            raw = self.rfile.read(length) if length > 0 else b""
            try:
                return json.loads(raw or b"{}")
            except json.JSONDecodeError:
                return None

        def scenario_progress(self, document):
            return {
                "document": document,
                "progress": f"/body/DocFragment[{args.chapter}]/body",
                "percentage": args.percentage,
                "device": args.device,
                "device_id": "mock-kosync-device",
                "timestamp": int(time.time()),
            }

        def do_GET(self):
            if not self.authorized():
                return
            path = self.path.split("?", 1)[0]
            if path == "/users/auth":
                self.send_json(200, {"authorized": "OK"})
                return
            prefix = "/syncs/progress/"
            if path.startswith(prefix):
                document = path[len(prefix) :]
                if args.echo and document in uploads:
                    self.send_json(200, uploads[document])
                elif args.no_progress:
                    self.send_json(200, {})
                else:
                    self.send_json(200, self.scenario_progress(document))
                return
            self.send_json(404, {"message": "Not found"})

        def do_POST(self):
            if not self.authorized():
                return
            if self.path.split("?", 1)[0] == "/users/create":
                self.read_json_body()
                self.send_json(201, {"username": "simulator"})
                return
            self.send_json(404, {"message": "Not found"})

        def do_PUT(self):
            if not self.authorized():
                return
            if self.path.split("?", 1)[0] != "/syncs/progress":
                self.send_json(404, {"message": "Not found"})
                return
            payload = self.read_json_body()
            if not isinstance(payload, dict) or not payload.get("document"):
                self.send_json(400, {"message": "Invalid progress payload"})
                return
            payload["timestamp"] = int(time.time())
            uploads[payload["document"]] = payload
            sys.stderr.write(
                "[kosync] stored upload: document=%s percentage=%s progress=%s\n"
                % (payload.get("document"), payload.get("percentage"), payload.get("progress"))
            )
            self.send_json(200, {"document": payload["document"], "timestamp": payload["timestamp"]})

    return Handler


def main():
    args = parse_args()
    base_url = f"http://{args.host}:{args.port}"
    if args.seed_sd:
        seed_credentials(args.seed_sd, base_url)
    server = ThreadingHTTPServer((args.host, args.port), make_handler(args))
    mode = "no remote progress" if args.no_progress else f"{args.percentage * 100:.1f}% at spine item {args.chapter}"
    print(f"Mock KOReader Sync server on {base_url} ({mode}). Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
