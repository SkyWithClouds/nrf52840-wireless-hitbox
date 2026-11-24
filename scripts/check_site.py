"""Compare every public frontend asset with the local source over HTTPS."""
import sys
import time
from pathlib import Path
from urllib.parse import urljoin
from urllib.request import urlopen

base, directory = sys.argv[1:]
for item in sorted(Path(directory).iterdir()):
    with urlopen(urljoin(base, item.name) + "?verify=" + str(time.time_ns()), timeout=15) as page:
        if page.read() != item.read_bytes():
            sys.exit(f"{item.name} has not reached Pages yet")
        if item.suffix == ".mjs" and "javascript" not in page.headers.get("Content-Type", ""):
            sys.exit(f"{item.name} has the wrong module MIME type")
print("Verified the deployed HTML, CSS and JavaScript files")
