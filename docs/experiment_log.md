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
- Field note: target box is mostly stable with occasional drift, FPS is about 14, laser tracking is worse on white background than black background.
- Adjusted defaults for the next test: target detection every 3 frames, laser confirmation 1 frame, laser smoothing 100, and no far-movement penalty from the previous laser position.
- Added compact laser candidate filters for the white-background test: stronger green threshold, higher density, minimum roundness, maximum elongation, and a minimum candidate score.
- White paper test showed the compact filter was too strict, so the green threshold and shape filters were relaxed and the score now penalizes only extra aspect ratio instead of every normal laser dot.
- White paper still missed the green laser, so a second washed-green threshold was added and the allowed spot size was increased for reflected/bloomed laser dots on white paper.
- White paper detection became sensitive but less precise, so laser detection now tries the strict green core threshold first and uses the washed-green threshold only as fallback. Target detection was returned to every 2 frames and target smoothing was raised to 45 for quicker red-box response.
- The strict-first laser pass was worse and dropped FPS to about 12, so laser detection was reverted to one combined `find_blobs` pass while keeping the two green thresholds and the faster target settings.
- FPS still stayed near 12 with the faster red-box settings, so the default overlay was made lighter: only FPS and aim status stay on-screen, while verbose status text and the center guide are hidden by default.
- White paper detection is sensitive but the blue marker can be pulled by the reflected halo, so a small core-refinement pass was added around the detected laser candidate.
- Core refinement only slightly improved accuracy and reduced FPS to about 13, so it is off by default. A lightweight temporal jitter filter now smooths only small laser movements while preserving fast jumps.
- The jitter filter did not reduce jumps enough, so candidate selection now prefers a nearby previous-frame candidate when its score is close to the best candidate. This should reduce jumping between nearby green reflections without extra image scans.
- Static white-paper laser position is still offset, so the default laser center method changed from blob centroid to bounding-rect center. This does not add image processing cost.
- Rect-center did not improve static accuracy and slightly reduced FPS, so the default laser center method was returned to blob centroid. Fixed low exposure/gain were enabled to prevent the white-paper laser center from saturating to white.
- Added a lower-brightness green threshold for the low-exposure test and disabled jitter/sticky filters by default so the raw laser center can be evaluated directly.
- White-paper center still saturated, so camera setup now explicitly switches to manual exposure mode and lowers exposure to 1500 us with gain 1.
- Exposure 1500 kept FPS and target stable but made the blue laser marker insensitive, so exposure was raised to 2500 us while keeping manual exposure mode and gain 1.
- Exposure 2500 restored some sensitivity but laser still dropped during fast/static tests, so exposure was raised to 2800, laser filters were relaxed, target ROI margin was widened, and target refresh is reduced while the laser is active to avoid red-box interference.
- Laser still appeared intermittently, so the allowed blob size was expanded for white-paper bloom and `LASER_LOST_HOLD_FRAMES` was set to 2 to hide very short detection gaps.
- Parameter-only changes did not solve laser/target interference. The frame pipeline now detects laser before refreshing target when a target already exists, and freezes target updates while the laser is active.
- Red-box interference still remained during laser confirmation/lost frames, so target freezing now holds for 8 frames after a recent laser candidate.
- The 8-frame freeze protected the red box but made it feel behind, so the freeze hold was reduced to 3 frames and low-rate target refresh remains enabled while laser is active.
- Red-box response was still a little slow, so target refresh while laser is active was changed from every 6 frames to every 4 frames without changing laser detection.
- Red-box response still felt slow, so target refresh while laser is active was changed to every 3 frames and freeze hold was reduced to 2 frames.
- Added on-screen `FZ` target-freeze indicator and startup prints for camera exposure/gain and target freeze settings.
