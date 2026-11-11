# RBA-6A v0.1 (test build)

A light vari-mu compressor modelled on captures of Acustica Audio's El Rey 2 (mono) and the RCA BA-6A instruction book.

## What's in v0.1
- Input preamp > push > compressor > pull (Preamp: **Before**), or push > preamp > compressor > pull (Preamp: **Inside**)
- Controls: Input, Threshold, Attack (1–3), Release (1–5), Filter (sidechain HPF 0–250 Hz), Make Up, De-Bumpify, Balance, Dry/Wet, Power
- Stereo detector: **Linked** (one detector for L+R) or **Dual mono**; Mode: L/R or M/S
- Oversampling: Off/2x/4x/8x live (8x only up to 48 kHz, 4x up to 96 kHz, 2x at 192 kHz)
- Render oversampling: a separate factor that is used while REAPER renders. **Use render OS**: Auto (only while rendering), Always, Never
- Latency is fixed whatever oversampling you choose, so switching never shifts the timing

## Known limitation (fix planned for v0.2)
- Attack at the default setting is about 2× faster than El Rey 2, and the slow Release settings are 10–20% fast.
- Static curve, EQ and harmonics are close; on a drum loop the level matches El Rey 2 within 2 dB 95% of the time.
- Use this build to check install, the GUI, oversampling, render switching and CPU, not for a final A/B against El Rey 2.

## Uploading to GitHub (web only, no terminal)
1. On github.com, click **+ > New repository**. Name it `rba6a`, set it to **Private**, and click **Create repository**.
2. Click **uploading an existing file**. Drag in everything from this zip **except** `build.yml` at the top level. That means: `CMakeLists.txt`, `README.md`, and the `Source`, `tests` and `tools` folders. Click **Commit changes**.
3. GitHub skips the hidden `.github` folder, so create the build file by hand:
   - Click **Add file > Create new file**.
   - In the name box, type `.github/workflows/build.yml` (the slashes make the folders).
   - Open `build.yml` from the zip in TextEdit, copy all of it, and paste it into the page.
   - Click **Commit changes**.
4. Click the **Actions** tab. The "Build RBA-6A" run starts on its own and takes about 10–15 minutes.
5. When it shows a green tick, click the run, then scroll to **Artifacts** and download **RBA-6A-macOS**.

## Installing
1. Unzip the download (and the zip inside it, if there is one). You'll get `RBA-6A.vst3` and `RBA-6A.component`.
2. In Finder, press **Cmd+Shift+G** and go to `~/Library/Audio/Plug-Ins/VST3`. Drag `RBA-6A.vst3` there.
3. Press **Cmd+Shift+G** again, go to `~/Library/Audio/Plug-Ins/Components`, and drag `RBA-6A.component` there.
4. The build is unsigned, so macOS may block it the first time. If REAPER doesn't list it, open **Terminal** once and paste:
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/RBA-6A.vst3 ~/Library/Audio/Plug-Ins/Components/RBA-6A.component`
5. In REAPER, open **Preferences > Plug-ins > VST**, click **Re-scan**, then find **RBA-6A** under Chris.
