# Contributing to AI1 Emu

Small, focused changes help protect working gameplay and saves. Discuss new cores and large UI changes in an issue before implementing them.

## Report a problem

Include the app version, console model, launch method, game region/revision and core. Describe the expected result, what happened and how to reproduce it. State whether the result came from a real Switch or the Windows UI preview. For audio issues, say whether gameplay also slowed down.

Attach relevant logs after removing personal information. Share a mod or patch's public project link when relevant. Keep ROMs, proprietary firmware, console keys and copyrighted cover collections out of attachments and commits.

## Work on the source

Read [COLLABORATING.md](COLLABORATING.md) for builds and preview instructions. Frontend rendering lives in `source/library_ui.cpp`, touch handling in `source/library_touch.cpp`, library/save paths in `source/platform_library.cpp`, and artwork preparation in `tools/`.

Keep core changes incremental. Preserve ordinary saves, state handling, audio, input and return-to-library behavior. Test through normal entry points. Report compilation separately from physical Switch gameplay. Avoid unrelated changes and keep SDK installations and generated binaries out of commits.

In each pull request, describe the problem, resulting behavior, tests and any save compatibility changes. Preserve all third-party notices and license boundaries. Repository commits do not automatically publish app updates. Maintainers build and verify release artifacts separately.
