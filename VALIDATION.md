# Compact Nibble validation — 27 October 2026

## Regular-weight text update

REMAINING, allowance labels and percentages now use regular-weight Segoe UI. Font sizes, colours, spacing, widget dimensions, countdowns and status text are unchanged. Rebuilt the executable and regenerated all previews. Inspected the normal and critical compact previews, plus 100% and 200% DPI examples: text fits without clipping. This font-weight-only update does not change monitoring or animation logic; no new tests or resource profiling were needed.

## Implemented changes

- Widget dimensions reduced by 40% per axis, from 320 x 260 to **192 x 156 logical pixels**.
- Window sizing and renderer share layout constants and the same 3/5 scale.
- Character, desk, HUD, cached artwork and alpha buffers scale together. HUD fonts have a minimum height of 8 logical pixels, with expanded text bounds to avoid vertical clipping.
- Removed the top expression badge from every state. The HUD heading is now REMAINING so it fits at the minimum readable font size.
- Settings and summary windows retain their normal DPI scale. Animation, monitoring, alert thresholds, expressions, countdowns, connection status and settings format are preserved.

## Verification

**53 existing logic checks and all 9 connection scenarios passed.** No changes were made to the monitoring calculations, data source, alert state or expression thresholds.

Exported and inspected all eight compact states for transparency, clipping and removal of the top badge. The shortened heading fits cleanly. Checked the actual renderer at four DPI scales:

| Windows scale | Output pixels |
| --- | --- |
| 100% | 192 x 156 |
| 125% | 240 x 195 |
| 150% | 288 x 234 |
| 200% | 384 x 312 |

Inspected the running desktop demo at the current Windows scale. Its logical bounds were 192 x 156, matching the renderer.

A normal-mode compatibility check loaded a copy of the previous settings, preserved the saved position at x=100/y=100, retained quiet mode and the 50/25/10 expression thresholds, fetched usage from the isolated fixture server, and saved its state successfully. The existing screen-clamping and DPI-change handlers use the new smaller dimensions.

Desktop drag automation did not produce a verifiable position change. Manual dragging remains an acceptance check; the native drag handler is unchanged. Moving between physical monitors with different display scales was not forced; rendering at the four DPI settings above was checked through the app's export path.

## Resource measurements

| Compact build scenario | Working set | Private memory |
| --- | ---: | ---: |
| Desktop demo, 25-second sample | 14.54 MiB | 4.04 MiB |
| Normal mode after fixture usage fetch | 14.79 MiB | Not sampled |

**Under-50 MB idle target passed.** Demo handle count was unchanged at 221 during the sample. Measured CPU change rounded to 0.000% of one logical core over that short sample; this is not a long-term CPU or memory soak test.

Usage checks still run in a temporary Codex helper and can exceed the idle-memory target during the fetch. The helper implementation is unchanged; the full-tree fetch peak was not remeasured for this layout update.

## Delivery

The portable ZIP includes the rebuilt executable, unchanged artwork, updated source, build instructions, existing test utilities, compact previews and DPI previews. Keep the assets folder alongside Nibble.exe. Existing settings under %LOCALAPPDATA%\Nibble remain compatible. No new startup registrations or size controls were added.
