import os

# Configuration
WEB_APP_DIR               = "webApp"
HTML_DIR                  = os.path.join(WEB_APP_DIR, "html")
ASSETS_DIR                = os.path.join(WEB_APP_DIR, "assets")
LINKER_DATA_FILE          = os.path.join(WEB_APP_DIR, "linkerData.yaml")

AUTOGEN_DEST_DIR          = "src/autoGen"
BUILD_DIR                 = ".temp/autoGen"

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
