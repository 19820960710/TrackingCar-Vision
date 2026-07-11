# Field Test Checklist

Use this checklist before changing code again.

## Before Running

- MaixCam Pro is connected.
- MaixVision can run `maixcam/main.py`.
- The target board is visible in the camera image.
- The green laser is off before starting the program.

## Startup

- `laser: CAL x/25` appears.
- The laser remains off during calibration.
- Calibration finishes and changes to `laser: LOST`.
- FPS is not obviously frozen.

## Target Test

- Red outline follows the tilted target.
- Target center is near the true center of the target.
- Target does not jump when the board is still.
- Target can recover if the board moves slowly.

## Laser Test

- With laser off, screen should show `laser: LOST`.
- With laser on, the blue marker should sit on the real laser dot.
- The marker should disappear when the laser is turned off.
- The marker should not jump to green reflections.

## Aim Test

- `aim dx` changes when the laser moves left/right.
- `aim dy` changes when the laser moves up/down.
- If the motor moves the wrong direction later, flip the sign in the controller, not in the vision test.

## FPS Test

- Record the displayed FPS.
- If FPS is low, test `DETECT_EVERY_N_FRAMES = 3`.
- If FPS is still low, hide status text or lower resolution.

## Record Results

After each useful test, add one short entry to `docs/experiment_log.md`:

```text
Lighting:
Distance:
Target angle:
FPS:
Target result:
Laser result:
Next change:
```
