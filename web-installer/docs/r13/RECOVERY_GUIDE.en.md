# Restore the original device

1. Close other installers and serial monitors. Save writing, power off completely and remove TF.
2. Keep USB connected, hold PWR to power off, hold BOOT, briefly press PWR to power on, and release BOOT after about two seconds.
3. In Chrome or Edge press Ctrl+O (Command+O on macOS) and open the saved Draftor_Recovery_R13_Offline.html.
4. Choose Restore original device and select the full private ZIP saved from this board. Existing v1 and new raw-original v2 packages are accepted.
5. Confirm the physical board and removed TF, select its USB port and connect. Damaged current firmware or partitions do not prevent ROM identity validation.
6. Confirm the full overwrite once. Keep power, USB and the page open until erasure, writing and content verification finish.
7. Close the connection, fully power off, insert the original TF and start normally.

Restore returns the saved original firmware and data, including other firmware, a factory demo or blank content. Normal boot depends on the original image. Preserve TF separately.

A pre-write failure has not modified Flash. Correct its stated cause before starting again. Stop and retain the receipt after an interrupted write. Use the R13 tool for new raw-original packages. Keep private ZIP and protection JSON files local.
