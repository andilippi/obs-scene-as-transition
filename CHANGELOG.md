# Scene As Transition - Changelog

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
