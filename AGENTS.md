# Agent Instructions

This repo is the desktop half of Creation Remote (Creation-Suite issue #65). It is a submodule of the `Creation Suite` umbrella repo — follow the umbrella `AGENTS.md` for suite-wide policy (branch ownership, path rules, build configuration, LLVM/vcpkg cautions, VFS storage boundary) in addition to anything below.

## Scope

This app has no standing open project and no creative-editing UI. Keep it that way: its only job is discovering available projects and depositing received assets into them via brief, self-contained VFS transactions. Feature creep toward "another creative app" belongs in Station/Movie/Texture/etc., not here.

## Build Output

Build via the umbrella repo's `.\scripts\Build-Suite.ps1 -Configuration Debug -Targets remote`, or standalone with `-S . -B build`. Do not create alternate/scratch build folders. Single-core builds only, per the umbrella `AGENTS.md`'s Single-Core Build Rule.

## Storage

Any local persistence this app needs (pairing state, settings) goes through the suite VFS service (`SuiteVfsJsonStore`), never a raw local file — per the umbrella `AGENTS.md`'s Storage Boundary Rule.
