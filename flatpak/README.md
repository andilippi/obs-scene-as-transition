# Flathub submission for `com.obsproject.Studio.Plugin.SceneAsTransition`

This directory holds the three files needed to ship the plugin as a
Flathub extension to `com.obsproject.Studio` (the official OBS Studio
Flatpak). Once accepted, Linux users on the Flatpak build of OBS install
the plugin from Flathub instead of compiling from source.

## How it works

OBS Studio's Flatpak declares `com.obsproject.Studio.Plugin` as an
extension point with `subdirectories: true`, so any app with the id
`com.obsproject.Studio.Plugin.<Name>` can hook in without per-plugin
approval from the OBS maintainers — only Flathub review.

At install time the OBS Flatpak picks up plugins from each extension's
`/app/plugins/<Name>/lib/obs-plugins/` and
`/app/plugins/<Name>/share/obs/obs-plugins/` paths. Our existing CMake
install rules already write to those exact relative paths, so no
plugin-side changes are needed. The manifest pins `prefix:
/app/plugins/SceneAsTransition` and lets cmake do the rest.

## Files

- `com.obsproject.Studio.Plugin.SceneAsTransition.yaml` — Flatpak
  manifest. Builds the plugin out-of-tree against the OBS runtime SDK,
  pins the source to a tagged commit.
- `com.obsproject.Studio.Plugin.SceneAsTransition.metainfo.xml` —
  AppStream metadata. Required by Flathub for the store listing.
- `flathub.json` — Flathub-side build config (x86_64 only, skip the
  icon check since extensions don't ship icons).

## Submitting to Flathub

1. Fork [flathub/flathub](https://github.com/flathub/flathub).
2. Create a branch `new-pr` (the bot expects that name).
3. Copy the three files (`.yaml`, `.metainfo.xml`, `flathub.json`) into
   the repo root of your fork.
4. Open a PR against `flathub/flathub`.
5. The Flathub `flatpak-builder-lint` check runs on the PR. Fix any
   warnings (most common: `appstream-cli validate` on the metainfo).
6. A reviewer will run `flatpak-builder` locally and approve.
7. On merge, Flathub auto-creates a dedicated repo at
   `flathub/com.obsproject.Studio.Plugin.SceneAsTransition`. From then
   on, all future updates go via PRs to that repo (just bump `tag` and
   `commit` in the yaml + add a `<release>` to metainfo).

Reference: https://docs.flathub.org/docs/for-app-authors/submission

## Updating the manifest on each release

When tagging a new version of this plugin:

1. Update `tag:` and `commit:` in the yaml to the new tag/sha.
2. Add a `<release version="x.y.z" date="YYYY-MM-DD"/>` entry at the
   top of the `<releases>` block in the metainfo.
3. Open a PR to the dedicated Flathub repo.

The `x-checker-data` block in the yaml lets Flathub's flatpak-external-
data-checker bot auto-open update PRs when new tags appear, so manual
PRs are only needed if the bot misses something.
