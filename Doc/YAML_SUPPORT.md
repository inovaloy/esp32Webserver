# YAML Configuration for ESP32 Web Server

The ESP32 Web Server project uses YAML format for the linker data configuration, providing enhanced readability and support for variables/anchors.

## Features

### 1. YAML-Only Format
- **Configuration File**: `WebApp/linkerData.yaml`
- **Clean syntax**: Human-readable YAML format
- **No legacy support**: Simplified codebase with single format

### 2. YAML Variables and Anchors
The YAML format supports variables for commonly used values:

```yaml
# HTTP Request Types
httpMethods: &httpMethods
  GET: &httpGET "GET"
  POST: &httpPOST "POST"

# Response Types
responseTypes: &responseTypes
  HTML: &rtnHTML "HTML"
  JSON: &rtnJSON "JSON"
  ASSET: &rtnASSET "ASSET"

# Content Types
contentTypes: &contentTypes
  CSS: &cssType "text/css"
  JAVASCRIPT: &jsType "application/javascript"
  IMAGE_JPG: &jpgType "image/jpg"
```

### 3. Route Configuration
Routes use the variables for consistency:

```yaml
"/Css/style.css":
  rtnType: *rtnASSET
  fileName: "Css/style.css"
  contentType: *cssType
  macro: "STYLE_CSS"
  reqType: *httpGET
```

## Utility Functions

### `readLinkerData(linkerDataPath)`
Reads linker data from YAML format and returns unified data structure.

### `validateLinkerData(linkerData)`
Validates the structure and required fields for each route type.

## Required Structure

### HTML Routes
```yaml
"/path":
  rtnType: *rtnHTML
  fileName: "file.html"
  macro: "MACRO_NAME"
  reqType: *httpGET
```

### Asset Routes
```yaml
"/path/file.ext":
  rtnType: *rtnASSET
  fileName: "path/file.ext"
  contentType: *typeVariable
  macro: "MACRO_NAME"
  reqType: *httpGET
```

### API Routes
```yaml
"/api/endpoint":
  rtnType: *rtnJSON
  reqType: *httpPOST
```

## Dependencies
- **PyYAML**: Install with `sudo apt install python3-yaml`



## Benefits of YAML Format
- **Readability**: More human-readable than JSON
- **Variables**: Reduce duplication with YAML anchors and aliases
- **Comments**: Support for inline and block comments
- **Maintainability**: Easier to modify and understand
- **Type Safety**: Variables help prevent typos in field values
- **Simplified Codebase**: Single format reduces complexity