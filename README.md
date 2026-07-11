# TrackingCar-Vision

Vision code for the NUEDC tracking car / self-aiming project.

The current runnable device program is:

```text
D:\Documents\电赛\TrackingCar-Vision\maixcam\main.py
```

Open this file in MaixVision and run it on MaixCam Pro.

## Current Status

- Camera preview works on MaixCam Pro.
- Target detection uses perspective rectangle detection first, with blob fallback.
- Green laser detection is enabled and uses target-area search plus startup background calibration.
- The screen shows target center, laser center, aiming error, FPS, and state text.
- UART output is available but disabled by default.

## Project Layout

```text
TrackingCar-Vision/
+-- maixcam/   MaixCam Pro / MaixPy device-side scripts
+-- docs/      run notes, tuning notes, UART protocol, test checklist
+-- scripts/   PC-side helper scripts
+-- configs/   example configuration files
+-- src/       PC-side placeholder modules
+-- data/      local images/videos, ignored by Git
+-- models/    local model files, ignored by Git
```

## Quick Run

1. Connect MaixCam Pro.
2. Open MaixVision.
3. Open `maixcam/main.py`.
4. Click Run.
5. Keep the green laser off while `laser: CAL x/25` is shown.
6. After it changes to `laser: LOST`, point the green laser at the target.

For a first camera-only check, run:

```text
maixcam/camera_preview.py
```

## Main Documents

- `docs/maixcam_quickstart.md`: how to run and what each overlay means
- `docs/tuning_guide.md`: what to adjust when target, laser, or FPS is bad
- `docs/test_checklist.md`: field test checklist
- `docs/uart_protocol.md`: UART output format for the main controller
- `docs/reference_review.md`: notes from the reference folder you provided

## Daily Git Workflow

Before editing:

```powershell
git pull
```

After editing:

```powershell
git status
git add .
git commit -m "Update vision code"
git pull
git push
```

Do not commit large videos, full datasets, generated output folders, or model weights.
