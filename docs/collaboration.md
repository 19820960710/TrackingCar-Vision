# Collaboration Notes

## Pull Teammate Updates

```powershell
cd "D:\Documents\电赛\TrackingCar-Vision"
git pull
```

If Git says local files would be overwritten, stop and check:

```powershell
git status
```

## Upload Your Changes

```powershell
git status
git add .
git commit -m "Update vision code"
git pull
git push
```

## Good Habits

- Pull before editing.
- Commit small and clear changes.
- Do not upload large videos, datasets, generated folders, or model weights.
- Put local videos and pictures in `data/`.
- Put local model files in `models/`.
- Record important test results in `docs/experiment_log.md`.
