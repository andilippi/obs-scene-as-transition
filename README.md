# Scene as Transition Plugin for OBS Studio

Use any OBS scene as a transition. Build whatever you want with sources, filters and effects, then use it as the thing that plays between your scenes. The sky's the limit.

Pair it with something like [Exeldro's Move Transition](https://obsproject.com/forum/resources/move-transition.913/) and you can create some genuinely advanced stuff. You can even make transitions dynamic by feeding data from [Streamer.Bot](https://streamer.bot) or [SAMMI](https://sammi.solutions). Put a text source on the transition scene and have it update with the name of the scene or game you're switching to. That sort of thing.

Grab some ready-made examples at [StreamUP](https://streamup.tips) to get started.

## How To Use

1. In the **Scene Transitions** dock, press **+** and select **Scene**. Give it a name.

2. Configure the transition properties:
   - **Scene** - Pick which scene to use as the transition.
   - **Duration** - How long the transition lasts in milliseconds.
   - **Transition Point** - When the actual scene switch happens underneath the transition. Set it as a percentage or a time in milliseconds.
   - **Audio Fade Style** - *Fade Out/Fade In* fades scene A out then scene B in around the transition point. *Cross-fade* blends both simultaneously from start to finish.
   - **Audio Volume** - How loud the transition scene's own audio plays.
   - **Filter To Trigger** - Pick a filter on your transition scene that gets enabled when the transition starts and disabled when it finishes. Useful for triggering animations timed to the transition.

## Good to Know

- **Old plugin detection** - If you've got a legacy `scene-as-transition.dll` from a previous install, the plugin will flag it and help you sort it out so they don't clash.
- **Media sources** - Media sources on your transition scene won't cut off at the transition point. Audio and playback continue properly through the whole transition.
- **Filter lazy-loading** - If your selected filter isn't loaded yet when OBS starts, the plugin retries when the transition actually runs. It still gets triggered.

## Build

**In-tree build:**
1. Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
2. Check out this repository to `frontend/plugins/obs-streamup-scene-as-transition`
3. Add `add_subdirectory(obs-streamup-scene-as-transition)` to `frontend/plugins/CMakeLists.txt`
4. Rebuild OBS Studio

**Stand-alone build (Linux only):**
1. Make sure you have the OBS development packages installed
2. Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

## Support

Built and maintained by Andi. If you're getting use out of this, consider chucking some support his way.

- [**Memberships**](https://andilippi.co.uk/pages/memberships) - Access all products and exclusive perks
- [**PayPal**](https://www.paypal.me/andilippi) - Buy me a beer
- [**Twitch**](https://www.twitch.tv/andilippi) - Come hang out and ask questions
- [**YouTube**](https://www.youtube.com/andilippi) - Tutorials on OBS and streaming
