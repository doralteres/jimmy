# Installing Jimmy in Cubase

## macOS

Copy the built `.vst3` bundle to the system VST3 folder:

```bash
cp -R build/Jimmy_artefacts/Release/VST3/Jimmy.vst3 "/Library/Audio/Plug-Ins/VST3/"
```

Or for the current user only:

```bash
cp -R build/Jimmy_artefacts/Release/VST3/Jimmy.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

## Windows

Copy the built `.vst3` folder to:

```
C:\Program Files\Common Files\VST3\
```

## Cubase Setup

1. Open Cubase and go to **Studio > VST Plug-in Manager**
2. If needed, rescan the VST3 folders
3. **Jimmy** should appear under **Instrument** category
4. Add an **Instrument Track** and select **Jimmy** as the instrument
5. Open the plugin GUI — you should see the Jimmy teleprompter window

## Troubleshooting

- If the plugin doesn't appear, check that the `.vst3` bundle is in the correct folder and rescan in the VST Plug-in Manager
- On macOS, you may need to remove the quarantine attribute: `xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Jimmy.vst3"`

### Upgrading to a newer version

When replacing an existing `Jimmy.vst3` with a newer version, Cubase may fail to load the plugin or show an error. This is caused by macOS re-applying the quarantine flag to the replaced bundle. To fix it, run the following command after copying the new version:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Jimmy.vst3
```

Then rescan the VST3 folders in Cubase (**Studio > VST Plug-in Manager**) and reload the plugin.
