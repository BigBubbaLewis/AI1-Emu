# Authentic screenshot inventory

Seventeen 1280×720 PNGs were converted without layout edits from BMPs captured by the isolated v0.5.0 Windows executable. It uses the frozen shared `library_ui`, `ui_kit`, `app_data` and `game_session` source. Capture hashes are recorded in PROVENANCE.json. Original build evidence is retained by the maintainer. SDL's dummy/software driver produced the frames.

The four populated catalog entries are explicitly labeled UI Preview, use original simple title cards, and are empty filename markers rather than ROMs. This avoids commercial games and unlicensed cover collections in staged media.

The preview source generates a background scene and uses synthetic serialization/no-op cheat callbacks. Its sample cheat, state thumbnails, battery value and demonstration messages are fixtures. These captures demonstrate UI rendering and screens, not gameplay, cheat correctness, rewind behavior, hardware performance or actual user play history. Keep these captions attached if any are published.

| Requested view | Capture | Status |
| --- | --- | --- |
| Home / white theme | `01-home.png`, `02-home-shelf.png` | Captured, preview/demo catalog |
| Populated library / custom covers | `03-library.png` | Captured, four labeled sample entries |
| Gameplay settings | `04-settings-gameplay.png` | Captured, preview only |
| Theme customization | `05-settings-themes.png` | Captured, preview only |
| Controller settings | `06-settings-controls.png` | Captured, diagnostic/mapping reference, not verified remapping |
| Integrated update screen | `07-settings-updates.png` | Captured, no real update installed |
| Map / About | `08-settings-map.png`, `09-settings-about.png` | Captured, demo-local data |
| Black theme | `10-home-black.png` | Captured, preview only |
| New quick menu | `11-menu-resume.png` | Captured, synthetic background |
| Save-slot management | `12-menu-save.png`, `16-menu-load.png` | Captured, synthetic states/thumbnails |
| Speed / rewind settings | `13-menu-speed.png` | Captured, not a performance benchmark |
| Cheat management | `14-menu-cheats.png` | Captured, sample code not executed by a real core |
| Display options / overlay | `15-menu-display.png`, `17-game-overlay.png` | Captured, synthetic scene |

## Manual captures needed after integration

Use the final Switch candidate and record its hash. Capture white Home and a populated real library with authorized artwork, the in-game quick menu over an actual game, a verified normal/state save reload, theme changes after restart, cheats with a demonstrated supported game/code, and rewind if it actually works. Capture docked controls/DS virtual touch separately from handheld touch.

If remapping is not implemented, omit a remapping screenshot and claim. For public imagery, prefer legally redistributable homebrew and owner-cleared custom covers. Keep private commercial cover caches out of the release package. Actual performance measurements need their own benchmark report, independent of these screenshots.
