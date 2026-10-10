# 1.3.4 (2026-10-11)
- Updated emu2413 to 1.6.4 and emu8950 to 1.2.3.
  - No float/double arithmetic in the per-sample path, for CPUs without an FPU.
  - Faster and more accurate rate converter.
  - Fixed the OPLL stereo output resampling L and R at different positions.
  - The save state format has changed.

# 1.3.3 (2026-09-29)
- Added `KSSPLAY_calc_silent_accurate`. It works like `KSSPLAY_calc_silent`
  but also clocks OPL, OPLL and PSG, so a state saved after a silent run
  matches real playback. `KSSPLAY_calc_silent` is unchanged.

# 1.3.2 (2026-09-28)
- Fixed a buffer overflow in the MGSDRV `@m` text handler. It could overwrite
  the volume table, which turned the output into noise until the process
  restarted.
- Fixed a buffer overflow in the MGS/BGM/MPK converters when the song is
  larger than the driver layout allows.
- Updated emu2413 to 1.6.2.

# 1.3.1 (2026-09-07)
- Updated emu2413 to 1.6.1.
- Updated emu8950 to 1.2.1.

# 1.3.0 (2026-07-22)
- Fixed the reset function to fully clear runtime state.
- Added save/load state functionality.

# 1.2.1 (2024-08-28)
- Update emu2149 to fix [issue 5](https://github.com/digital-sound-antiques/emu2149/issues/5).

# 1.2.0 (2023-07-16)
- Fix problem where playback rate is slightly faster Thanks to @gcielniak

# 1.1.4 (2022-11-26)
- Update emu2149 to fix [issue 4](https://github.com/digital-sound-antiques/emu2149/issues/4).

# 1.1.3 (2022-11-24)
- Update emu2149 to fix [issue 3](https://github.com/digital-sound-antiques/emu2149/issues/3).

# 1.1.2 (2022-09-21)
- Update emu2413 (again) to fix [this issue](https://github.com/digital-sound-antiques/emu2413/issues/12).

# 1.1.1 (2022-09-19)
- Add non-blocking version of KSS to VGM functionality.
- Update emu2413 to fix [this issue](https://github.com/digital-sound-antiques/emu2413/issues/12).

# 1.1.0 (2022-09-18)
- Add KSS to VGM functionality.
- Fix incorrect prototype definition.

# 1.0.0 (2022-09-14)
- Follow Semantic Versioning
- Update minimum cmake version to 3.0.