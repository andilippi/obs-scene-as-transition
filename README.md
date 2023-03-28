# Browser transition for OBS Studio

Plugin for OBS Studio to show a browser source during scene transition


# Build
1. In-tree build
    - Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
    - Check out this repository to plugins/browser-transition
    - Add `add_subdirectory(browser-transition)` to plugins/CMakeLists.txt
    - Rebuild OBS Studio

1. Stand-alone build (Linux only)
    - Verify that you have package with development files for OBS
    - Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

# Donations
https://www.paypal.me/exeldro
