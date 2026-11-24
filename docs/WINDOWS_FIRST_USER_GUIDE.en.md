[Current installation steps and troubleshooting](https://draftor.tcssn.org/guide.html)

# Windows first-user guide

Windows 10/11 desktop Chrome and Edge are open for informed beta use. Physical USB remains WINDOWS_DEVICE_FIELD_TEST_PENDING. No Python, command line, extension or developer tools are required.

Save writing and end transfers. Fully power off including the battery; remove and retain TF separately. Connect only the Waveshare ST7305 with a data cable, hold BOOT while plugging in USB and release after about two seconds. Close serial monitors and other installer tabs.

Use Device Manager to identify the USB Serial Device (COMx) / USB JTAG/serial debug unit corresponding to this board. Never identify the board by COM number alone. If absent, check cable, USB port, BOOT mode and port ownership. Use only [Espressif official USB Serial/JTAG driver guidance](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_overview/usb_serial_jtag.html). Do not randomly replace the serial interface driver with a JTAG driver. If Edge policy blocks Web Serial, try desktop Chrome or ask the administrator; never bypass policy.

At https://draftor.tcssn.org/first-install.html choose Connect & Install. Read the Windows beta notice and confirm physical conditions. Select the actual board in the browser picker. After read-only qualification, capture all 16 MiB, save the private ZIP to disk and reselect the actual saved file. An interrupted download is not a backup. Wait for verification against the current original board. Save the exact offline HTML alongside the ZIP before independently confirming installation. First installation erases all internal Flash; reinstallation writes only the qualified active App. Keep power/USB/page connected. Errors stop; do not automatically reflash.

For recovery, close all installer tabs. With the computer offline, press Ctrl+O in desktop Chrome/Edge and open the saved Draftor_Recovery_R11_Offline.html. Select the saved private ZIP, prepare the original board in ROM with TF removed, check the full-Flash overwrite impact and confirm once. Restore returns to backup time, including settings and pairing. Factory packages restore the factory demo. If FILE Web Serial is unavailable, use the other supported browser or obtain support; do not disable safety checks.

After content verification, close the connection, fully power off, insert TF and boot normally. Reconnect the original keyboard channel after restoring Draftor; pair as instructed after first installation. Check display, input, old writing, saving and reading after restart. Content verification is not normal-boot verification.

The first willing recipient should report OS/browser, USB/COM display, read-only plan, actual ZIP/HTML saving and reimport, installation and TF/keyboard/writing results. Do not send a private ZIP, MAC or documents. Physical Windows offline recovery is a separate authorized field validation, not a reason for deliberate interruption. Preserve diagnostics and state on real errors.
