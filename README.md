# Nibble

A tiny native Windows companion for Codex usage. Nibble is a glossy mint-green slime at a miniature desk, beside a cyan HUD showing your **remaining** short-window and weekly allowances.

The compact widget is **192 × 156 logical pixels**, 40% smaller in each dimension than the previous design. Windows display scaling still applies. HUD text stays at least 8 logical pixels high. Expression-name badges have been removed; Nibble's face and the live percentages convey its state. Settings and summary dialogs retain their normal readable size.

## Run

Extract the portable package and double-click **Nibble.exe**. Keep the **assets** folder beside the executable. No installer, browser runtime, admin rights, or Windows startup registration is required. Keep Codex installed and signed in. Windows 10/11, x64.

Drag the monitor to move Nibble. Right-click Nibble or its green tray icon for:

- **Hide / Show Nibble** — monitoring continues in the tray.
- **Refresh now** — otherwise checks run every two minutes.
- **Usage summary** — remaining percentages, reset times, reset-credit count, last update, and connection status.
- **Expression thresholds...** — adjust the character's three expression thresholds; popup thresholds stay unchanged.
- **Quiet mode** — suppresses all notification popups and sounds. Suppressed alerts are not replayed later.
- **Always on top** — enabled by default.
- **Exit Nibble** — exits the companion and stops its own usage helper.

Closing the window hides it. To stop monitoring completely, use **Exit Nibble**. Launch Nibble manually whenever you need it; it does not run while your PC is asleep or after you exit it. Windows may place the tray icon in the overflow menu.

## Usage and alerts

Percentages mean **remaining**, matching Codex: 27% used becomes 73% remaining. Data updates every two minutes, so rapid usage can outpace the displayed reading. Countdown text updates locally between checks. The actual window duration comes from Codex.

Warnings trigger at **25%, 10%, and 5% remaining** for each allowance. A check crossing several thresholds sends the most urgent warning once. New reset windows rearm warnings. Reset notifications require fresh data showing an increased allowance and a later reset timestamp, rather than the local countdown simply reaching zero.

Reset-credit notifications appear when a positive credit count is first observed or increases. Redeem credits in Codex; Nibble never redeems them. Unknown credit counts show as unavailable, not zero.

Alerts are silent Windows tray notifications. Windows notification settings and Do Not Disturb can suppress them. Quiet mode keeps the visible meters updated. The report is on demand; no scheduled daily report or history charts are included.

A **STALE / check connection** label indicates unconfirmed readings. Right-click → Usage summary explains the error. Missing sign-in, network failures, unsupported responses, and timeouts never replace cached readings with zero. Resume from sleep requests a fresh reading.

## Expressions and animation

Nibble reacts to whichever allowance has less remaining. Default expression thresholds are:

| Remaining | Expression |
| --- | --- |
| Above 50% | Happy / idle |
| Above 25%, up to 50% | Moderate / attentive |
| Above 10%, up to 25% | Low / worried |
| 10% or below | Critical / distressed |

Use **Expression thresholds...** to change them. Values must satisfy `0 <= Critical < Worried < Attentive <= 100`. Existing settings and notification history are preserved when upgrading.

Happy Nibble gently breathes and blinks. A confirmed reset triggers a four-second happy bounce unless another allowance is still low. A usage request selects the attentive **Checking usage** state. After ten minutes without Windows keyboard or mouse input, Nibble rests. Low and critical expressions take priority over checking, resting and celebration. Stale readings retain their explicit status and never produce a false healthy state. Quiet mode also suppresses reset celebrations.

The desk and HUD remain fixed. The character sprites are aligned to a common baseline. PNGs are decoded once at the current display scale; no images are generated during use. The HUD is cached separately and rebuilt only when displayed content changes. Unchanged animation ticks do not send a new composite to Windows.

## Memory and data

The native executable uses Win32/GDI and Windows Imaging Component, without a browser engine, 3D engine or resident monitoring service. Per-pixel alpha preserves the soft transparent artwork edges. Animation runs at up to 8 FPS and pauses when hidden or locked. A hidden Codex helper runs briefly for each check and then exits with its owned subprocesses. Idle memory and check-time peaks are documented in **VALIDATION.md**.

Settings, cached usage and notification state are stored in `%LOCALAPPDATA%\Nibble\state.json`; a bounded `events.log` holds connection status messages. Nibble does not read or copy credential files. Its Codex child uses Codex's own existing authentication and normal local state. It requests account rate limits only: no model conversations, API billing, reset redemption, or messages to other people.

## Troubleshooting

Open Codex and confirm that you are signed in, then choose **Refresh now**. Update Codex if the usage interface is unsupported. Nibble locates `codex.exe` on PATH or inside the Codex desktop installation. If needed, set `NIBBLE_CODEX_PATH` to the full path to your installed `codex.exe` before launching.

For a disconnected visual demo, run `Nibble.exe --demo`. The monitor clearly says DEMO. This mode has no account connection and does not save settings.

## Build and tests

Source is included in `src/`. Obtain the Windows x64 portable [Zig 0.13.0 compiler](https://ziglang.org/download/), extract it, then run from PowerShell:

```powershell
./build.ps1 -Zig 'C:\path\to\zig.exe'
```

This builds Nibble, the logic tests, an isolated fixture server and connection tests, and runs both test suites. `probe.exe` performs one real read-only Codex query and reports the result, sampled full-process-tree memory peak, and helper cleanup status. It requires the same signed-in Codex environment as Nibble.

The app also accepts `NIBBLE_DATA_DIR` for an isolated test settings directory. `Nibble.exe --export-previews` writes transparent PNGs of all eight UI states into `previews/` beside the executable, using clearly marked demo readings. Additional happy-state previews show 100%, 125%, 150% and 200% DPI scaling in the corresponding subfolders. These are renders of the app itself, not desktop captures. These diagnostics are not needed for normal use.

The art was prepared with the built-in image generation tool. Assets and the exact final prompts are documented in `assets/ARTWORK.md`.

Usage protocol: [OpenAI's Codex app-server documentation](https://learn.chatgpt.com/docs/app-server#6-rate-limits-chatgpt). JSON parsing uses nlohmann/json 3.11.3; its MIT license is included in `src/JSON-LICENSE.txt`. This portable build is unsigned.

## License

Nibble is source-available under the [Nibble Source-Available License](LICENSE).
You may use, modify and share it for free, including for internal business use.
Selling Nibble or modified versions, charging for access, offering paid hosted
versions, or including its code in a paid product or service requires prior
written permission from the applicable copyright holders. See LICENSE for the
full conditions.

Third-party components retain their own licenses; see
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).

This custom license is a draft prepared for the project's stated policy and has
not been reviewed by a lawyer. See [GitHub setup and review notes](GITHUB-LICENSE-GUIDE.md)
before publishing or relying on the restrictions.
