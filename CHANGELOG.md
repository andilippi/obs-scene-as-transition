# Scene As Transition - Changelog

## v1.3.1 (12 Apr '26)
**Patch Focus:** Filter reliability & old plugin detection
- Fixed Filter to Trigger breaking after OBS updates because the filter reference was cached at load time and never refreshed. Filter is now looked up fresh at each transition start and released on stop
- Improved old plugin detection to search beyond the current module directory. Now uses obs_enum_modules() first, then falls back to multi-directory search covering ProgramData and AppData installs
- Old plugin detection dialog now offers one-click removal (rename to .old) or open folder

## v1.3.0 (21 Mar '26)
**Patch Focus:** Transition lifecycle and stability fixes
- Fixed transition locking up OBS when no scene was selected, preventing scene or transition changes until restart
- Fixed filter not triggering when transition point was set to 0%
- Fixed first scene change after opening OBS not working correctly (filter not triggering, audio issues)
- Fixed ref count leak when plugin was destroyed or settings changed mid-transition
- Centralised transition stop logic to prevent inconsistent cleanup across destroy, update, and render paths
- Removed redundant initialisation in plugin create

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
