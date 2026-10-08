# Known limitations / 已知限制

Alpha remains experimental; prior device passes cover only the recorded finite scenarios. The T06_TF20_R1 final parent passed its authorized finite 20 MHz device scenario: controlled startup/Ready, XH/PY paging and selection, cloud-computing selection, save/reopen, actual TXT export, and CreateNew Inbox/readback. This is not broad hardware or long-duration qualification.

- 16 unchanged Host failures are inherited; they are not erased or converted to PASS.
- The historical 92-byte/internal-memory cause and the full causal chain of the original physical first-Enter report remain open. Later fixes and finite successful tests do not retrospectively resolve every report.
- Large Chinese resource initialization, low-memory extremes, long-duration use, power-loss recovery and untested Inbox limits are not certified by a short smoke test.
- 14px complex glyph readability has a finite resolution limit; representative UI14 samples were accepted, not all Unicode.
- All B2 and mixed font files are retained in this complete local candidate. Specific legacy/converted-data and older font provenance/distribution evidence remain unconfirmed; see RESOURCE_STATUS.
- The complete source/resource/firmware release remains local pending specific distribution-evidence handling, disclosure timing and final publication approval. Acceptance of the patent application is recorded from user-provided evidence; comparison against the formally filed technical text remains pending. There is no earlier subset launch.
- No Wi-Fi, QR, Find Chinese input, full UI localization or new keyboard/performance matrix is added by this release preparation.

Alpha 未冻结。旧 T01 失败仍是历史失败基线；后来同一 B 场景通过不改写旧结果。当前 TF20 的有限设备烟测已通过，TF 细粒度重试／CRC 计数不可观测，不能按零记录；资源许可与专利公开时点分别确认，不用任一软件测试替代。
