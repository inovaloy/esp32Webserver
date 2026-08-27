import os
import json

# ── Build version ─────────────────────────────────────────────────────────────
# Stored in .temp/AutoGen/version.json so it survives between runs.
# Patch is auto-incremented every autogen run; bump MAJOR/MINOR manually.
_VERSION_FILE = os.path.join(".temp", "AutoGen", "version.json")

def _loadVersion():
    if os.path.exists(_VERSION_FILE):
        with open(_VERSION_FILE, 'r') as f:
            return json.load(f)
    return {"major": 1, "minor": 0, "patch": 0}

def _saveVersion(v):
    os.makedirs(os.path.dirname(_VERSION_FILE), exist_ok=True)
    with open(_VERSION_FILE, 'w') as f:
        json.dump(v, f)

def getBuildVersion(bump=False):
    """Return version string like '1.0.4'. If bump=True, increments patch first."""
    v = _loadVersion()
    if bump:
        v["patch"] += 1
        _saveVersion(v)
    return f"{v['major']}.{v['minor']}.{v['patch']}"

# ─────────────────────────────────────────────────────────────────────────────

# Configuration
WEB_APP_DIR               = "WebApp"
HTML_DIR                  = os.path.join(WEB_APP_DIR, "html")
ASSETS_DIR                = os.path.join(WEB_APP_DIR, "assets")
LINKER_DATA_FILE          = os.path.join(WEB_APP_DIR, "linkerData.yaml")

AUTOGEN_DEST_DIR          = "Src/AutoGen"
BUILD_DIR                 = ".temp/AutoGen"

AUTOGEN_ASSET_INFO_FILE   = "autoGenAssetsInfo.json"
AUTOGEN_HTML_INFO_FILE    = "autoGenHtmlInfo.json"

AUTOGEN_ASSETS_H          = "autoGenAssets.h"
AUTOGEN_HTML_H            = "autoGenHtmlData.h"
AUTOGEN_WEBSERVER_H       = "autoGenWebServer.h"
AUTOGEN_WEBSERVER_CPP     = "autoGenWebServer.cpp"


# Supported file types
SUPPORTED_ASSET_EXTENSIONS = {
    '.css'  : 'text/css',
    '.js'   : 'application/javascript',
    '.png'  : 'image/png',
    '.jpg'  : 'image/jpeg',
    '.jpeg' : 'image/jpeg',
    '.gif'  : 'image/gif',
    '.svg'  : 'image/svg+xml',
    '.ico'  : 'image/x-icon',
    '.woff' : 'font/woff',
    '.woff2': 'font/woff2',
    '.ttf'  : 'font/ttf'
}

TEXT_ASSET_EXTENSIONS = ['.css', '.js', '.svg']


LINKER_YAML_REQUIRED_FIELDS = {
    'HTML' : ['rtnType', 'fileName', 'macro', 'reqType'],
    'ASSET': ['rtnType', 'fileName', 'contentType', 'macro', 'reqType'],
    'JSON' : ['rtnType', 'reqType']
}
