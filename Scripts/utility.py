import re
import yaml
import os
import hashlib
import json

try:
    import rjsmin as _rjsmin
    _HAS_RJSMIN = True
except ImportError:
    _HAS_RJSMIN = False

from common import *

def str2hex(decimal):
    return "0x" + hex(decimal)[2:].zfill(2).upper()


def convertToCamelCase(filename, separator="."):
    name = re.sub(r'[^a-zA-Z0-9]', '_', filename)

    parts = name.split('_')
    camelString = parts[0].lower()
    for i in range(1, len(parts)):
        if parts[i]:
            camelString += parts[i][0].upper() + parts[i][1:].lower()
    return camelString


def minifyCss(cssContent):
    """Minify CSS by removing unnecessary whitespace and comments"""
    # Remove CSS comments
    cssContent = re.sub(r'/\*.*?\*/', '', cssContent, flags=re.DOTALL)
    # Remove unnecessary whitespace around CSS syntax
    cssContent = re.sub(r'\s*([{}:;,])\s*', r'\1', cssContent)
    # Remove multiple whitespace and newlines
    cssContent = re.sub(r'\s+', ' ', cssContent)
    # Remove leading and trailing whitespace
    return cssContent.strip()


def minifyJavaScript(jsContent):
    """Minify JavaScript using rjsmin if available, otherwise fall back to regex."""
    if _HAS_RJSMIN:
        return _rjsmin.jsmin(jsContent)

    # Fallback: simple regex minifier (safe for basic scripts only)
    jsContent = re.sub(r'/\*.*?\*/', '', jsContent, flags=re.DOTALL)
    jsContent = re.sub(r'(?<!:)//(?!/).*?(?=\n|$)', '', jsContent)
    lines = jsContent.split('\n')
    lines = [line.strip() for line in lines if line.strip()]
    jsContent = ' '.join(lines)
    jsContent = re.sub(r'\s+', ' ', jsContent)
    jsContent = re.sub(r'\s*([{}();,])\s*', r'\1', jsContent)
    return jsContent.strip()


# ── Build cache helpers ──────────────────────────────────────────────────────

def _cacheFilePath(buildDir):
    return os.path.join(buildDir, "buildCache.json")


def loadBuildCache(buildDir):
    """Return the persisted {filepath: sha256} dict, or {} if not present."""
    path = _cacheFilePath(buildDir)
    if os.path.exists(path):
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {}


def saveBuildCache(buildDir, cache):
    """Persist the {filepath: sha256} dict to disk."""
    with open(_cacheFilePath(buildDir), 'w', encoding='utf-8') as f:
        json.dump(cache, f, indent=2)


def fileHash(filepath):
    """Return the SHA-256 hex digest of a file's contents."""
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def isFileUnchanged(filepath, cache):
    """Return True if the file's hash matches the cached value."""
    return cache.get(filepath) == fileHash(filepath)


# ─────────────────────────────────────────────────────────────────────────────


def readLinkerData(linkerDataPath):
    """
    Read linker data from YAML file format.
    Returns data in the format that existing scripts expect.
    """
    if not os.path.exists(linkerDataPath):
        raise FileNotFoundError(f"Linker data file not found: {linkerDataPath}")

    try:
        with open(linkerDataPath, 'r', encoding='utf-8') as file:
            # Read YAML file
            rawData = yaml.safe_load(file)

            # Filter out the variable definitions (httpMethods, responseTypes, contentTypes)
            # and keep only the route definitions (those starting with "/")
            linkerData = {}
            for key, value in rawData.items():
                if isinstance(key, str) and key.startswith('/'):
                    linkerData[key] = value

            return linkerData

    except yaml.YAMLError as e:
        raise ValueError(f"Error parsing YAML file {linkerDataPath}: {e}")
    except Exception as e:
        raise RuntimeError(f"Error reading linker data file {linkerDataPath}: {e}")


def validateLinkerData(linkerData):
    """
    Validate the structure and content of linker data.
    Ensures all required fields are present for each route type.
    """

    errors = []

    for route, config in linkerData.items():
        if not isinstance(config, dict):
            errors.append(f"Route '{route}': Configuration must be a dictionary")
            continue

        rtnType = config.get('rtnType')
        if not rtnType:
            errors.append(f"Route '{route}': Missing required field 'rtnType'")
            continue

        if rtnType not in LINKER_YAML_REQUIRED_FIELDS:
            errors.append(f"Route '{route}': Invalid rtnType '{rtnType}'. Must be one of: {list(LINKER_YAML_REQUIRED_FIELDS.keys())}")
            continue

        # Check required fields for this return type
        missing = []
        for field in LINKER_YAML_REQUIRED_FIELDS[rtnType]:
            if field not in config:
                missing.append(field)

        if missing:
            errors.append(f"Route '{route}': Missing required fields for {rtnType}: {missing}")

    if errors:
        raise ValueError("Linker data validation failed:\n" + "\n".join(errors))

    return True
