# TrackingCar-Vision

Vision code for the NUEDC tracking car project.

## Project Layout

```text
TrackingCar-Vision/
├─ src/                 # Core vision code
├─ maixcam/             # MaixCam Pro / MaixPy device-side scripts
├─ scripts/             # Small test and helper scripts
├─ configs/             # Camera and algorithm parameters
├─ docs/                # Notes, logs, and collaboration records
├─ data/                # Local datasets, not committed to Git
└─ models/              # Local model weights, not committed to Git
```

## Daily Workflow

Before editing:

```powershell
git pull
```

After editing:

```powershell
git status
git add .
git commit -m "更新视觉代码"
git pull
git push
```

## What To Commit

Commit these files:

- source code
- config examples
- short experiment notes
- small reference images if needed

Do not commit these files:

- large videos
- full datasets
- generated output folders
- model weights such as `.pt`, `.onnx`, `.engine`

Large local files can be stored in `data/` or `models/`, but they are ignored by Git.

## Camera Targets

This repository keeps two camera test paths:

- `scripts/check_camera.py`: PC-side OpenCV preview for a USB camera.
- `maixcam/camera_preview.py`: MaixCam Pro device-side preview using MaixPy.

For MaixCam Pro, copy or open `maixcam/camera_preview.py` in MaixVision and run it on the device.
