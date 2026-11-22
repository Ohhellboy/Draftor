# Draftor private packages and offline recovery

Device operations support macOS desktop Google Chrome and the original Waveshare ESP32-S3-RLCD-4.2 / ST7305 with 16 MiB Flash. Tested baseline: macOS 15.5 / arm64, Chrome 157.0.8103.50. This does not qualify every version combination.

## Save before installation

1. Save writing and finish transfers. Fully power off, including the battery, and remove TF. Connect only this board. Hold BOOT while plugging in USB, release after about two seconds and close serial monitors. Select USB JTAG/serial debug unit.
2. Use Connect & Install on the official homepage. Inspection selects the exact supported factory first installation or Alpha 3 app0 same-version reinstallation. Unknown states stop; there is no force option.
3. Without old files, read complete Flash read-only. ROM slow reading takes time: keep the page, USB and power connected. Save the generated private ZIP, wait for the browser to finish, then reselect the actual saved file. Its complete contents must match this board's current Flash.
4. Download the offline HTML, confirm saving finished, and keep it with your private ZIP. The page verifies the exact tool digest. Resolve download failures before installing. Saving the tool does not replace package reimport.
5. Read the plan and impact and confirm backup/TF conditions. First installation separately requires erasing all internal Flash; reinstallation writes only the active App. After verification and port release, follow the normal boot checks.

## Restore a board that cannot boot

1. Close other installation/recovery pages. In Chrome choose File → Open File (⌘O) and open your saved Draftor_Recovery_R10_Offline.html. It works without network, Python, terminal, Codex, extensions, developer tools or security setting changes.
2. Choose Restore original board and select the private ZIP without extracting it. The package contains original-board identity, an exact 16 MiB image, hashes and instructions. Stop on identity or security failure; never edit the package or substitute a board.
3. Save writing, finish transfers, fully power off and remove TF; enter ROM as above. Confirm physical conditions and select USB JTAG/serial debug unit. Current partitions may be damaged, but original identity, capacity and security checks remain mandatory.
4. Confirm the full overwrite: all 16 MiB, settings, pairing and internal writing return to backup time. Later internal changes are lost. TF is separate and is never accessed. Click Restore once and keep power and USB connected.
5. After Contents verified, close the connection, clear the private session and save a sanitized receipt. Fully power off, insert the original TF and boot normally with BOOT released. Reconnect the keyboard on its original channel without resetting pairing. Check old writing, saving new writing and reading after restart. A factory package restores the factory demo, not Draftor.
6. Communication, writing or verification errors stop the session. There is no automatic retry, slot switch, reset or fallback to first installation. Retain the original package and receipt. Partial writing may prevent boot. After the specific cause is resolved, a user-initiated new session still requires all checks and consent; no project test quota applies.

## Privacy and evidence

The private ZIP contains MAC, settings, pairing and possibly internal documents. Never upload or publish it. Files stay local; there is no account or upload service. Hashes and MAC prevent accidental corruption and mistakes, not malicious forgery. Back up TF writing and learning directories separately; downloading TF resources is not a writing backup.

Closing clears page references, file inputs and object URLs, not guaranteed physical memory erasure. Diagnostics exclude private images, MAC, filenames and private hashes. Counters represent program control points, not independent wire capture. Content verification does not prove normal boot, keyboard or writing health.

Draftor full recovery inherits R9 physical and independent-user qualification. Factory recovery has software coverage and shares the qualified writer; no separate factory-package restore hardware test is claimed. Only exact Alpha 3 app0 or FactoryOnly_20260120 original states are accepted. The private package remains the strict four-entry draftor-private-recovery/1 ZIP. Keep the tool separately.
