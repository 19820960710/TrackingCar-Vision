# Experiment Log

Record useful test results here so the team can compare versions.

## 2026-07-11

- Initialized project structure.
- Added folders for source code, configs, data, models, scripts, and docs.
- Added MaixCam Pro camera preview.
- Added target detection with perspective rectangle center and blob fallback.
- Added green laser detection with target-area search and startup background calibration.
- Added FPS-friendly defaults: no console FPS spam, grid/ROI hidden, target detection every 2 frames.
- Added optional UART `AIM` output format for main-controller integration.
- Added tuning guide, field test checklist, and reference review notes.

## 2026-07-12

- Split laser background calibration into `CAL BASE` and `CAL TARGET`.
- `CAL BASE` learns fixed reflections in the center ROI.
- `CAL TARGET` learns fixed reflections near the detected target after the red target is visible.
