"""
esp32-autogen entry-point
Runs the full autogen pipeline in the correct order:
  1. compileHtml
  2. compileAssets
  3. updateWebServer

Equivalent to:  make autogen
"""

import sys

import Scripts.compileHtml as _html
import Scripts.compileAssets as _assets
import Scripts.updateWebServer as _server


def main() -> None:
    print("=" * 60)
    print("Step 1/3 — Compiling HTML")
    print("=" * 60)
    sys.argv = [sys.argv[0]]          # strip any args; each script has its own parser
    _html.main()

    print()
    print("=" * 60)
    print("Step 2/3 — Compiling Assets")
    print("=" * 60)
    sys.argv = [sys.argv[0]]
    _assets.main()

    print()
    print("=" * 60)
    print("Step 3/3 — Updating Web Server")
    print("=" * 60)
    _server.main()

    print()
    print("Autogen pipeline complete.")


if __name__ == "__main__":
    main()
