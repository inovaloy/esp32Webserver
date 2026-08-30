import re
import yaml
import os
import rjsmin
import rcssmin

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
    """Minify CSS using rcssmin — handles @media, variables, and all valid CSS."""
    return rcssmin.cssmin(cssContent)


def minifyJavaScript(jsContent):
    """Minify JavaScript using rjsmin — template-literal and regex safe."""
    return rjsmin.jsmin(jsContent)


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
