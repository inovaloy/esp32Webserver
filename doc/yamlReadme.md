# ESP32 Web Server - YAML Configuration

This project now uses **YAML format exclusively** for linker data configuration, providing a clean and maintainable setup.

## Quick Start

1. **Install Dependencies:**
   ```bash
   sudo apt install python3-yaml
   ```

2. **Configuration File:**
   Edit `webApp/linkerData.yaml` to define your routes:
   ```yaml
   # Variables for consistency
   httpMethods: &httpMethods
     GET: &httpGET "GET"
     POST: &httpPOST "POST"

   responseTypes: &responseTypes
     HTML: &rtnHTML "HTML"
     ASSET: &rtnASSET "ASSET"
     JSON: &rtnJSON "JSON"

   # Route definitions
   "/":
     rtnType: *rtnHTML
     fileName: "index.html"
     macro: "INDEX_HTML"
     reqType: *httpGET
   ```

3. **Build Scripts:**
   ```bash
   # Compile HTML files
   python3 scripts/compileHtml.py

   # Compile assets (CSS, JS, images)
   python3 scripts/compileAssets.py

   # Generate web server code
   python3 scripts/updateWebServer.py
   ```

4. **Test Configuration:**
   ```bash
   python3 scripts/testYamlSupport.py
   ```

## Benefits of YAML Format

✅ **Human Readable** - Clean, intuitive syntax
✅ **Variables Support** - Reduce duplication with anchors & aliases
✅ **Comments** - Document your configuration inline
✅ **Type Safety** - Variables prevent typos
✅ **Maintainable** - Easier to modify and understand

## Migration from JSON

If you have an existing `linkerData.json`:
1. The format structure remains the same
2. Convert manually or use any JSON-to-YAML converter
3. Add variable definitions for consistency
4. Remove the old JSON file

For detailed documentation, see `doc/yamlSupport.md`.