# Scene As Transition - Changelog

## v1.2.2 (19 Dec '25)
**Patch Focus:** Code quality, security fixes, CI/CD improvements, and Flatpak support

### New Features
- Added Flatpak build support for Linux users running OBS via Flatpak
- Added Flatpak manifest (`com.obsproject.Studio.Plugin.SceneAsTransition.json`)
- Added Flatpak build job to CI/CD pipeline

### Code Fixes
- Fixed potential division by zero when transition point is set to 0% or 100%
- Fixed potential crash when transition scene is not set (NULL pointer dereference)
- Fixed command injection vulnerability in file manager open functions (macOS/Linux)
- Fixed render path logging causing potential performance issues (now logs only once per filter change)
- Removed redundant code and duplicate initializations
- Added consistent NULL safety checks throughout
- Added `static` keyword to internal functions for better encapsulation
- Extracted duplicate filter validation logic into helper function
- Removed deprecated `register` keyword from audio processing
- Added platform-specific file extensions for old plugin detection (`.so` for macOS/Linux)

### CI/CD Fixes
- Fixed missing `$` in `CODESIGN_IDENT_USER` variable (macOS notarization)
- Fixed wrong variable name in Ubuntu packaging (`build_args` → `package_args`)
- Fixed undefined `$ProductName` variable in Windows build script
- Fixed inconsistent Zsh version requirements across build scripts
- Removed hardcoded Xcode version selection (uses runner default)
- Removed duplicate zsh installation in packaging step
- Removed dead code (empty `unwanted_formulas` loop, unused `$Columns` variable)
- Added TODO comments to disabled format checks

---

## v1.2.1 (28 Oct '25)
**Patch Focus:** Old plugin detection and branding update
- Added automatic detection of old scene-as-transition.dll file
- Displays warning popup with option to open plugins folder and highlight old file for removal
- Updated plugin log name to "StreamUP Scene as Transition"
- Improved compatibility checks to prevent conflicts with legacy versions

---

## v1.2.0 (26 Oct '25)
**Patch Focus:** OBS 32 compatibility & filter reliability
- Fixed cases where the transition filter failed to load by adding lazy-loading and null-safety guards
- Updated to work with OBS 32

---

## v1.1.1 (26 Jun '24)
**Patch Focus:** Audio output fix
- Addressed an issue preventing audio from outputting correctly after OBS startup

## v1.1.0 (16 Apr '24)
**Patch Focus:** Volume and audio controls, localization expansion
- Implemented volume adjustment over transition scene
- Added audio fade control for both source and destination scenes
- Expanded locale support: Danish, Dutch, German, and Japanese translations
- Corrected locale file references

## v1.0.3 (10 Apr '24)
**Patch Focus:** Audio mixing and media source improvements
- Integrated transition scene audio into the final audio mix
- Established localization framework for community translations
- Added Spanish locale support
- Fixed issue where media sources were cutting off at transition point

## v1.0.2 (04 Apr '24)
**Patch Focus:** Settings stability fixes
- Resolved filter trigger option resetting to defaults when properties opened
- Fixed preview transition feature to function multiple times without additional configuration changes

## v1.0.0 (30 Mar '24)
**Initial Release**
Core functionality allowing users to designate a scene as a transition effect, configure duration, set scene change timing, and trigger filters during transitions.
