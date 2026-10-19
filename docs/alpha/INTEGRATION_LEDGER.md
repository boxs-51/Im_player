# Alpha integration provenance

**Snapshot of original integration (2026-10-19)**

- Repo: `boxs-51/Im_player`
- main/base: `e9b72924facd816c671c52c567c9e513a24077a4`
- alpha initial integration commit: `20e354758efa9211494f945ace90335549cafc63`
- merge parents in order: `main@e9b72924`, `PR #54@b1b0085f`, `PR #58@01916217`
- tree: `e82ed7bcb24c8a6e8c919679e2156f5499da8ee4`
- alpha-main difference at this snapshot: six exact paths below; no overlap between participating PRs.

| PR | Integrated paths | Historical exact-head CI |
|---|---|---|
| [#54](https://github.com/boxs-51/Im_player/pull/54), diagnostic only | `AudioCaptureManager.cpp`, `AudioOutputWorker.cpp`, `AudioTelemetry.h`, `PlaybackObserver.cpp`, `scripts/ci_sanity.py` | [run 37797445347](https://github.com/boxs-51/Im_player/actions/runs/37797445347), Static + Windows Debug/Release + BRG Gate SUCCESS. Audio diagnosis and physical reproduction unresolved. |
| [#58](https://github.com/boxs-51/Im_player/pull/58), cache optimization | `.github/workflows/brg-ci.yml` | [run 37784907950](https://github.com/boxs-51/Im_player/actions/runs/37784907950), same four jobs SUCCESS. Does not prove cross-branch combined physical E2E. |
| [#60](https://github.com/boxs-51/Im_player/pull/60), evidence-only | **EXCLUDED** from alpha; includes ephemeral probe and mutated lock manifest | [run 37801494518](https://github.com/boxs-51/Im_player/actions/runs/37801494518) success, but its test-only tree must not be shipped. |

This was performed as a source-preserving octopus integration commit on alpha rather than merging into main. Source HEAD ancestry is preserved and GitHub compares verify each parent with behind=0. Subsequent documentation/CI commits are additive and separately traceable.

**Cross-check:** `git diff main 20e354758efa9211494f945ace90335549cafc63 --name-only` lists precisely those six paths. A clean file-union does not itself prove functional semantic compatibility.

**Rollback:** restore alpha ref to pre-integration main SHA or revert to a reviewed content baseline; never force-reset main. Branch/PR closure is distinct from deleting work or closing its bug issue.

**Blocked claims:** `#42 root-cause fixed`, `alpha E2E passed`, `main merge-ready`, `release-ready` all remain NOT PROVEN. Latest alpha check status must be read afresh.
