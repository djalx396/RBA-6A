# RBA-6A v0.3.0 (test build) - by 396

A light vari-mu compressor with two units:
- **El Rey**: modelled on captures of Acustica Audio's El Rey 2. Soft knee, smooth RMS compression, Attack 1–3, Release 1–5.
- **Vintage RCA**: modelled on captures of a sampled RCA BA-6A (Nebula programs). Hard-knee peak limiting, about 3 ms attack, Release Single/Dual (Dual is program-dependent, as in the RCA manual).

## Signal chain
**1 Input** (Drive, Bump) → **2 Compressor** (El Rey or Vintage RCA) → **3 Clipper** → **4 Preamp** (always last) → Make Up, Dry/Wet

- **Bump:** Off / Pre (before the compressor, as in El Rey 2) / Post (after the compressor).
- **Compressor:**
  - Threshold, Attack (El Rey only), Release (El Rey 1–5 or RCA Single/Dual), SC Filter (El Rey up to 250 Hz, RCA 100–500 Hz) and P-P Balance (push-pull trim).
  - **Detector:** Linked / Dual mono. **Mode:** L/R or M/S. **RCA Range:** Normal / +8 (8 dB more drive into the RCA limiter).
- **Clipper:** On/Off, Crush (0–40), Make Up (0–10 dB). It is oversampled, so it doesn't alias or get stuck like the sampled original.
- **Preamp:** On/Off, Drive (−30…+30), Lines Mono/Stereo.
  - **Stereo** uses two slightly different lines, measured from the sampled unit.
  - **3D Bump** (On/Off) detunes the bump on one side. It is only active when the Bump is on and the Preamp is on Stereo.
- **Oversampling:**
  - Live Off/2x/4x/8x (8x only up to 48 kHz, 4x up to 96 kHz).
  - Render OS, and Use render OS: Auto (only while rendering) / Always / Never.
  - The latency is fixed, so switching never shifts the timing.

## What changed in v0.3.0
- **Units renamed: MODERN** (was El Rey) and **VINTAGE** (was Vintage RCA). Same sound.
- **RANGE Normal / +9** now works on both units (was "RCA RANGE" +8, Vintage only):
  Modern = the Threshold knob 9 higher (Threshold 20 at +9 = Threshold 29); Vintage = 9 dB more drive into the limiter.
  Sessions saved with +8 open with +9.
- **Clipper lamp:** the button reads CLIPPER and lights up (amber outline) when the Clipper is on. Its lamp shows how hard it clips:
  dimmed when it isn't clipping, then 10 steps of 0.5 dB of peak reduction, fully lit from about 5 dB.
- **M/S removed.** The compressor always works L/R (the Mode button and parameter are gone; sessions saved in M/S open in L/R).
- **Output meters:** four bars, top to bottom L OUT, L IN, R IN, R OUT, with a PK / AVG switch (peak, or a 300 ms average).
- **Windows 10 and later:** the workflow (`build.yml`) now also builds a 64-bit Windows VST3 (no Visual C++ runtime needed).
  Every run gives two downloads: RBA-6A-macOS-build-N and RBA-6A-Windows-build-N.

## What changed in v0.2.6
- **FILTER: OFF / CLEAN / AIR** - staged ultrasonic band-limiting, the same module as FreeLA2 (`Source/FreeBandLimit.h`,
  Airwindows Hypersonic sections, MIT licence). Works when the plug-in runs at 88.2 kHz or more inside
  (always at 88.2/96 kHz; at 44.1/48 kHz with oversampling on). Order: D > Compressor > C > Clipper > B > Preamp > A.
  - CLEAN (default): a complete 25 kHz filter (14th-order Butterworth) after the Compressor, the Clipper and the Preamp.
  - AIR: an 8th-order 25 kHz Butterworth split in four: D (gentlest) before the Compressor, C after it, B after the
    Clipper, A (most resonant) plus a complete Hypersonic after the Preamp. Each stage is driven a little darker; the air comes back at the end.
  - OFF: no band-limiting (for null tests).
- **PHASE: ANALOG / LINEAR** (`Source/PhaseAlign.h`, also from FreeLA2): ANALOG = analog-style filters (the top octave lags a little),
  LINEAR = a correction filter removes the phase of the ultrasonic filters and the oversampling (pure delay in the audio band).
- **Latency** is now fixed per PHASE, the same in every OVERSAMPLE x FILTER setting: at 96 kHz 32 samples (ANALOG), 120 (LINEAR).
- **Dry/Wet**: the dry signal now goes through the same oversampling and filters as the wet, so they line up exactly
  (it runs only while Dry/Wet is below 100 %).
- **Developer: 396.** The plug-in now shows 396 as its maker (COMPANY_NAME, bundle id com.396.rba6a). The plug-in codes stay
  the same, so existing sessions still find it.
- Null vs El Rey 2 (drum loop, C08): FILTER OFF -21.2 dB, CLEAN LINEAR -21.3, AIR ANALOG -21.3, CLEAN ANALOG -17.8 (top-octave phase).
- CPU (El Rey + Clipper + Preamp, stereo 96 kHz, one core): OFF 1x/2x/4x 6.9/12.8/24.3 %, AIR 8.1/15.8/30.0 %, CLEAN 8.8/16.7/31.6 %.

## What changed in v0.2.5
- Fix: the plug-in now loads on older macOS / Intel Macs (minimum macOS 11.0). Up to v0.2.4 the build silently required the newest macOS, because the minimum-macOS setting came after `project()` in CMakeLists.txt and was ignored. Same fix as FreeLA2 v0.3.1.
- The workflow also passes the minimum macOS and checks it after the build (fails if it is above 11.0).
- Everything from v0.2.4 (GUI) is included. No change to the sound.

## What changed in v0.2.4
GUI only, from the design canvas. No change to the sound; existing sessions load as before.
- Version number shown under the RBA-6A title (taken from the build).
- Compressor knobs in hardware style: scale numbers around each dial, value and name underneath. Names shortened to THR, ATT, REL, SC HPF, P-P.
  - ATT 1/2/3 and REL 1-5 with SLOW under the left end and FAST under the right end (El Rey).
  - THR 0-10-20-30-40, SC HPF OFF-125-250 (El Rey) / OFF-300-500 (RCA), P-P -100-0-+100.
- Power, Detector (LINK ON / LINK OFF), Clipper and Preamp are now single lit buttons: amber LED when on, dark when off. Click to toggle.

## What changed in v0.2.3
- El Rey Attack and Release knobs are now marked SLOW at the left end of their travel and FAST at the right, like the hardware. The 1-3 and 1-5 numbers stay.
- The host's parameter display adds "slow" / "fast" near the ends of each knob.
- No change to the sound.

## What changed in v0.2.2
- **RCA Range: Normal / +8** (in the Compressor section, active on the Vintage RCA unit). +8 drives the limiter 8 dB harder at any Threshold setting. At the top of the knob that's the heavier range v0.2 had.

## What changed in v0.2.1
- **Threshold now means the same on both units.** The Vintage RCA's Threshold scale is matched to El Rey: at any setting, both give about the same average gain reduction on drums and full mixes. Switching units keeps the amount of compression and changes only the character.

## What changed since v0.1
- El Rey attack and release now match El Rey 2 at every setting (the default attack was about 2× too fast).
- New Vintage RCA unit, Clipper, Preamp, Bump Off/Pre/Post and 3D Bump.
- New GUI. The Preamp Position switch and De-Bumpify are gone.
- Parameters changed: remove any v0.1 instance in your projects and insert RBA-6A again.

## Updating the GitHub repo (web only)
The build workflow is already in the repo, so you only replace the code.
1. Open the RBA-6A repository on github.com and click **Add file > Upload files**.
2. Drag in `CMakeLists.txt`, `README.md` and the `Source`, `tests` and `tools` folders from this zip. Files with the same names are replaced.
3. Click **Commit changes**.
4. Open **Actions**. "Build RBA-6A" runs by itself (about 10–15 minutes). When it shows a green tick, click it and download **RBA-6A-macOS** under **Artifacts**.

`build.yml` is included in case you ever need to set the workflow up again: on the Actions tab, use **set up a workflow yourself** and paste it in.

## Installing
1. Unzip the download. You get `RBA-6A.vst3` and `RBA-6A.component`.
2. Quit REAPER.
3. In Finder press **Cmd+Shift+G**, go to `~/Library/Audio/Plug-Ins/VST3`, and drag `RBA-6A.vst3` there. Choose **Replace**.
4. Press **Cmd+Shift+G** again, go to `~/Library/Audio/Plug-Ins/Components`, and drag `RBA-6A.component` there. Choose **Replace**.
5. If macOS blocks it, open **Terminal** once and paste:
   `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/RBA-6A.vst3 ~/Library/Audio/Plug-Ins/Components/RBA-6A.component`
6. Start REAPER. If it doesn't pick up the new version, go to **Preferences > Plug-ins > VST** and click **Re-scan**.
