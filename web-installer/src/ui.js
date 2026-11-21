// SPDX-License-Identifier: Apache-2.0
import { dict } from './content.js';
import { PINS } from './pins.js';
import { loadRelease } from './firmware.js';
import { InstallSession } from './session.js';
import { normalizeMac, Stop } from './safety.js';

const copy = {
  zh: {
    previewHint: 'v0.1.0-alpha.3 · 软件验收版 · 实机写入锁定',
    testHint: 'v0.1.0-alpha.3 · 专用实机测试构建 · 仅兼容更新',
    firstHint: 'v0.1.0-alpha.3 · R6 首装专项测试 · 将清除内部 Flash',
    factory: '已识别微雪厂家演示程序：初始化安装',
    intro: '通过 USB 连接设备，先检查兼容性，再确认安装。',
    installHeading: '连接设备并安装', preparing: '正在核验本地发行文件…',
    preflight: '先保存文稿并正常结束传输，完全断电后取出 TF。断开其他开发板，按住 BOOT 接入 USB，约两秒后松开。请保持当前窗口打开。',
    boardConfirm: '我已核对实物是微雪 ESP32-S3-RLCD-4.2（ST7305），不是其他 ESP32-S3 板。',
    macLabel: '本机备份或之前核验记录中的设备 MAC', macHint: '只在本页内存中比对；不上传、不写入回执。请勿凭芯片型号猜测板型。',
    connectRead: '选择串口并只读检查', ready: '四份固件与发行清单已核验，可以连接。',
    unsupported: '当前环境不支持安全的 Web Serial。请用支持 Web Serial 的电脑端浏览器，从 localhost 或 HTTPS 打开本页。当前没有连接设备。',
    backup: '我已私下保存并核验可恢复的完整 Flash（含设置、配对）、原固件，以及 TF 文稿和学习目录备份。',
    tfRemoved: 'TF 卡已在完全断电后取出，重要文稿已保存。',
    approve: '我已阅读下方完整计划，同意执行其中的写入范围。',
    erase: '我单独确认：全新安装将擦除整片 16 MiB 内部 Flash，清除旧固件、设置和配对。',
    currentApp: '可选：准确的旧 App 文件（仅本地比对，不上传）',
    run: '按已核验计划安装', export: '保存本地回执', close: '关闭', stop: '停止并保留回执',
    update: '兼容更新', fresh: '已证明整片空白：全新安装', plan: '完整只读计划',
    updateSummary: '只写活动 App；擦除范围仅为该 App 覆盖的扇区。保留其他 App、分区、OTA、NVS 和配对，不操作 TF。更新中断可能使设备无法启动，须保留可恢复备份。',
    freshSummary: '将清除整片 16 MiB 内部 Flash，包括旧固件、设置、配对及内部文件，再写入准确的四份 Draftor 固件。TF 不受操作。必须保留已验证的完整恢复备份并单独确认清除；传输中断可能无法开机，不自动重试或恢复。',
    locked: '此交付构建只允许真实设备只读计划，真实写入已锁定。后续经授权使用专用测试构建验证兼容更新。',
    freshLocked: '此构建锁定真实首装。仅独立的 R6 首装测试构建可在完整前检、恢复准备和单独清除确认后执行。',
    sim: '软件模拟 · 合成 Flash · 不访问 USB / TF',
    simHint: '以下结果仅来自软件夹具，不能证明真实设备安装或启动通过。',
    tfGuide: '固件校验完成仍需普通启动验收。TF 词库、字体及独立 LearningIndex 必须另行准备；本页不检查 TF，也不宣称整机已可写作。',
    finished: '固件内容已独立校验，等待普通启动与 TF 验收。没有自动复位。',
    phase_CONNECTING: '正在连接 ROM 下载模式…', phase_CHECKING: '正在只读检查设备与实际分区 / OTA…',
    phase_PLAN_READY: '只读计划已就绪，尚未写入。', phase_RECHECKING: '写入前重新核对同一设备与计划…',
    phase_ERASING: '正在清除内部 Flash 并核验全片空白…', phase_WRITING: '正在写入计划指定的区域…', phase_VERIFYING: '正在独立校验 Flash 内容与保护区域…',
    phase_VERIFIED_BOOT_PENDING: 'VERIFIED_BOOT_PENDING · 等待普通启动验收', phase_STOP: 'STOP · 已停止，请保留状态；不重试、不换槽、不改为清空重刷。',
  },
  en: {
    previewHint: 'v0.1.0-alpha.3 · Software qualification · Hardware writes locked',
    testHint: 'v0.1.0-alpha.3 · Dedicated device test build · Compatible updates only',
    firstHint: 'v0.1.0-alpha.3 · R6 first-install experiment · Internal Flash will be cleared',
    factory: 'Recognized Waveshare factory demo: initialization',
    intro: 'Connect by USB, check compatibility, then confirm installation.',
    installHeading: 'Connect & Install', preparing: 'Verifying local release files…',
    preflight: 'Save writing and finish transfers. Fully power off and remove the TF card. Disconnect other boards, hold BOOT while connecting USB, then release after about two seconds. Keep this window open.',
    boardConfirm: 'I checked the physical board: Waveshare ESP32-S3-RLCD-4.2 (ST7305), not another ESP32-S3 board.',
    macLabel: 'Device MAC from your private backup or previously verified record', macHint: 'Compared only in this page’s memory; never uploaded or included in receipts. A chip family does not identify a PCB.',
    connectRead: 'Select port & inspect', ready: 'All four firmware files and release manifests verified. Ready to connect.',
    unsupported: 'Secure Web Serial is unavailable here. Open this page on localhost or HTTPS in a desktop browser with Web Serial support. No device was connected.',
    backup: 'I privately retained and verified recoverable full Flash (settings and pairing), original firmware, TF document and learning-directory backups.',
    tfRemoved: 'The TF card was removed after complete power-off and important writing was saved.',
    approve: 'I reviewed the complete plan below and approve its write ranges.',
    erase: 'I separately confirm: fresh installation erases all 16 MiB of internal Flash, removing old firmware, settings and pairing.',
    currentApp: 'Optional: exact previous App file (local comparison only; never uploaded)',
    run: 'Install the verified plan', export: 'Save local receipt', close: 'Close', stop: 'Stop & keep receipt',
    update: 'Compatible update', fresh: 'Entire Flash proven blank: fresh installation', plan: 'Complete read-only plan',
    updateSummary: 'Write only the active App and erase only its covered sectors. Preserve the other App, partitions, OTA, NVS and pairing. Do not access TF. An interruption may prevent boot; retain recoverable backups.',
    freshSummary: 'Clear all 16 MiB of internal Flash, including firmware, settings, pairing and internal files, then write the four exact Draftor images. TF is untouched. Retain verified full recovery backups and separately approve erasure. An interruption may prevent boot; no automatic retry or recovery.',
    locked: 'This delivery build permits read-only hardware plans. Hardware writing is locked. Use the dedicated test build after authorization for compatible-update qualification.',
    freshLocked: 'First installation is locked in this build. Only the separate R6 experiment permits it after complete checks, recovery preparation and separate erase approval.',
    sim: 'Software simulation · Synthetic Flash · No USB / TF access',
    simHint: 'These are software fixture results, not proof of physical installation or boot.',
    tfGuide: 'Verified firmware still needs a normal boot check. TF dictionaries, body font and the separate LearningIndex must be prepared independently. This page does not inspect TF or certify writing readiness.',
    finished: 'Flash content independently verified; normal boot and TF qualification remain pending. No automatic reset.',
    phase_CONNECTING: 'Connecting to ROM download mode…', phase_CHECKING: 'Inspecting device, actual partitions and OTA read-only…',
    phase_PLAN_READY: 'Read-only plan ready. No write performed.', phase_RECHECKING: 'Rechecking the same device and approved plan…',
    phase_ERASING: 'Clearing internal Flash and verifying it is entirely blank…', phase_WRITING: 'Writing the ranges specified in the plan…', phase_VERIFYING: 'Independently verifying Flash content and protected ranges…',
    phase_VERIFIED_BOOT_PENDING: 'VERIFIED_BOOT_PENDING · Normal boot qualification pending', phase_STOP: 'STOP · Preserve the state. No retry, alternate slot, or erase-and-reinstall fallback.',
  },
};
const errors = {
  BOARD: ['必须人工确认准确的微雪实物板型。', 'Confirm the exact physical Waveshare board.'],
  IDENTITY: ['设备身份缺失或与预期 MAC 不符。核对私人记录与所选串口。', 'Device identity is missing or does not match the expected MAC. Check your private record and selected port.'],
  ASSET: ['发行文件的大小或 SHA-256 不符；禁止写入。', 'Release size or SHA-256 mismatch. Writing is forbidden.'],
  IMAGE: ['ESP32-S3 镜像头或发行标识不符。', 'ESP32-S3 image header or release identity mismatch.'],
  CAPACITY: ['App 超出固定槽位容量。', 'App exceeds the fixed slot capacity.'],
  DOWNLOAD: ['发行文件读取中断。没有自动重试，请检查本地站点文件。', 'Release file loading failed. No automatic retry. Check the local site files.'],
  CHIP: ['不是受支持的 ESP32-S3。', 'Not a supported ESP32-S3.'],
  FLASH: ['未确认是 16 MiB Flash。', '16 MiB Flash could not be confirmed.'],
  SECURITY: ['安全状态未知、受限或已启用 Secure Boot / Flash Encryption。', 'Security state is unknown, restricted, or Secure Boot / Flash Encryption is enabled.'],
  PARTITIONS: ['实际分区表与固定 B 布局字节不符；禁止更新或自动首装。', 'Actual partition bytes differ from the fixed B layout. No update or automatic fresh installation.'],
  OTA: ['OTA 状态 pending、rollback、损坏或不唯一；禁止猜测活动槽。', 'OTA is pending, rollback, corrupt or ambiguous. Never guess the active slot.'],
  UNKNOWN: ['分区空白不足以证明整片为空；该设备状态不支持自动首装。', 'Blank metadata does not prove blank Flash. This state cannot enter fresh installation.'],
  READBACK: ['只读数据与 ROM 独立摘要不一致。', 'Read data does not match the independent ROM digest.'],
  VERIFY: ['写后内容校验不一致，设备可能只有部分 App。请保留现场。', 'Post-write verification failed. The App may be partial. Preserve the device state.'],
  PROTECTED_CHANGED: ['保护区域发生变化；不能宣称安装成功。', 'A protected region changed. Installation cannot be reported as successful.'],
  STALE_PLAN: ['设备状态已变化，原计划失效。', 'Device state changed; the approved plan is stale.'],
  BACKUP: ['必须确认可恢复备份、TF 已取出以及完整写入计划。', 'Confirm recoverable backups, TF removal and the complete write plan.'],
  CURRENT_APP: ['准确旧 App 比对失败，未开始写入。', 'Exact previous App comparison failed. No write started.'],
  USB_TRANSPORT: ['R1 只覆盖 Espressif USB Serial/JTAG（303a:1001）。其他串口路径尚未验收。', 'R1 covers only Espressif USB Serial/JTAG (303a:1001). Other serial paths are unqualified.'],
  STUB: ['设备似乎已运行 stub；请手动重新进入 ROM，保留原现场。', 'A stub appears to be running. Preserve the state and manually re-enter ROM mode.'],
  CONNECTION: ['未能连接 ROM。核对串口、BOOT 操作及占用该串口的应用。', 'Could not connect to ROM. Check the port, BOOT procedure and other serial applications.'],
  DISCONNECTED: ['连接中断；停止后续命令，不自动重连。', 'Disconnected. Further commands stopped; no automatic reconnect.'],
  IO: ['串口读写失败；设备状态可能不完整，请保留回执。', 'Serial I/O failed. Device state may be incomplete; keep the receipt.'],
  TIMEOUT: ['设备应答超时，已停止。', 'Device response timed out. Stopped.'],
  PROTOCOL: ['ROM 应答结构或长度不符，已停止。', 'ROM response structure or length is invalid. Stopped.'],
  ROM_STATUS: ['ROM 返回错误状态，已停止。', 'ROM reported an error. Stopped.'],
  CANCELLED: ['用户已停止；若写入已经开始，App 可能不完整。', 'Stopped by the user. If writing had begun, the App may be incomplete.'],
  ERASE_CONFIRM: ['全片擦除必须单独确认。', 'Full Flash erasure requires separate confirmation.'],
  ERASE_FORBIDDEN: ['兼容更新不接受全片擦除权限。', 'Compatible updates never accept full erase permission.'],
  FRESH_DEVICE_PENDING: ['真实首装已锁定，等待独立空板验收。', 'Real fresh installation is locked pending separate blank-board qualification.'],
  DEVICE_WRITES_LOCKED: ['此构建的真实写入已锁定。', 'Hardware writing is locked in this build.'],
  FACTORY_FINGERPRINT: ['厂家镜像指纹不符；未知旧状态禁止清除。', 'Factory fingerprint mismatch. Unknown old state must not be erased.'],
  ERASE_VERIFY: ['全片擦后校验失败；停止，不写任何固件。', 'Full erase verification failed. Stopped before firmware writes.'],
  INITIALIZATION_VERIFY: ['初始化完整镜像校验失败；停止并保留现场。', 'Complete initialized Flash verification failed. Preserve the state.'],
  BUILD_SCOPE: ['构建权限冲突；停止。', 'Conflicting build permissions. Stopped.'],
};

export function startUI({ requestDriver, simulation = false, scenario, expectedMac = '', hardwareWritesEnabled = false, hardwareFreshWritesEnabled = false }) {
  Object.assign(dict.zh, copy.zh); Object.assign(dict.en, copy.en);
  dict.zh.dialogs.help = { title: '安装帮助', body: '<p>使用电脑端 Web Serial 浏览器、USB 数据线和准确的微雪 ST7305 设备。R1 只覆盖 USB Serial/JTAG；按住 BOOT 接入 USB，保持 ROM 下载模式。</p><p>写入前须在本机核验可恢复的完整 Flash／配对备份，并单独备份 TF 文稿与学习目录。网页不创建、收集或上传私人备份。</p><p>兼容更新只写已确认的活动 App。未知分区、OTA pending／rollback、安全状态不明均停止。真实首装尚未验收，本轮锁定。</p><p>TF 须另备：Alpha 2.1 资源包中的 <code>HANIME/IME/B_SHORT_TEXT_20261018/</code> 词库及 <code>HANIME/IME/R1_API080/matched28.hbf1</code> 字体；Alpha 3 独立包中的 <code>WAVESHARE_R7_L1_AREV1_RES_20261112/relations.bgr</code> 和 <code>relations.hnv</code>。不要格式化 TF，不覆盖不匹配的原文件。</p><p><a href="docs/TF_GUIDE.html" target="_blank" rel="noopener">准确下载、路径及 SHA-256 清单</a> · <a href="docs/INSTALL.zh-CN.md" target="_blank" rel="noopener">发行安装指南</a></p><div class="notice">固件校验成功 ≠ 整机已具备全部写作资源。普通启动、键盘配对、保存状态仍需实机验收。</div>' };
  dict.en.dialogs.help = { title: 'Installation help', body: '<p>Use desktop Web Serial, a USB data cable and the exact Waveshare ST7305 board. R1 covers USB Serial/JTAG only. Hold BOOT while connecting USB and remain in ROM download mode.</p><p>Verify recoverable full Flash/settings/pairing backups and separate TF document and learning backups privately before a write. This page never creates, collects or uploads private backups.</p><p>Compatible updates write only the confirmed active App. Unknown partitions, pending/rollback OTA or unknown security stop installation. Physical fresh installation is locked and unqualified.</p><p>Prepare TF separately: dictionaries under <code>HANIME/IME/B_SHORT_TEXT_20261018/</code> and <code>HANIME/IME/R1_API080/matched28.hbf1</code> from Alpha 2.1 Resources; both <code>WAVESHARE_R7_L1_AREV1_RES_20261112/relations.bgr</code> and <code>relations.hnv</code> from the separate Alpha 3 LearningIndex. Never format TF or overwrite mismatching originals.</p><p><a href="docs/TF_GUIDE.html" target="_blank" rel="noopener">Exact downloads, paths and SHA-256 list</a> · <a href="docs/INSTALL.md" target="_blank" rel="noopener">Release installation guide</a></p><div class="notice">Verified firmware does not certify writing readiness. Normal boot, keyboard pairing and saved writing still need physical qualification.</div>' };
  for (const lang of ['zh', 'en']) {
    dict[lang].dialogs.advanced = { title: lang === 'zh' ? '高级选项' : 'Advanced', body: `<p>${lang === 'zh' ? '锁定发行' : 'Pinned release'}: <strong>${PINS.version}</strong><br>WAVESHARE_RLCD42 · ST7305 · 16 MiB</p><p>App: 4,021,152 bytes<br><code>${PINS.files['app.bin'].sha256}</code></p><p>${lang === 'zh' ? '固定 B 分区；只接受唯一活动槽。无强刷、改槽或全片擦除开关。设备信息仅在本页内存中比对；回执不含 MAC 或私有 Flash 内容。' : 'Fixed B layout; one unambiguous active slot only. No force, slot override or full erase switch. Device identity stays in page memory; receipts exclude MAC and private Flash content.'}</p><p><a href="simulation.html?scenario=app0">${lang === 'zh' ? '运行软件模拟（不连接设备）' : 'Run software simulation (no device)'}</a> · <a href="simulation.html?scenario=app1">app1</a> · <a href="simulation.html?scenario=fresh">fresh</a></p><p><a href="docs/CAPABILITY_AUDIT.md">${lang === 'zh' ? '底层能力审计' : 'Capability audit'}</a> · <a href="docs/SOURCE.html">${lang === 'zh' ? '源码与许可' : 'Source and licenses'}</a></p>` };
  }
  let language = 'zh', modalKey, release, session, busy = false, chooserOpen = false, lastPhase = '', errorCode = '', dialogEpoch = 0;
  const $ = id => document.getElementById(id), t = key => dict[language][key] || key;
  const dialog = $('info-dialog');
  const workflow = document.createElement('dialog'); workflow.id = 'install-dialog'; workflow.setAttribute('aria-labelledby', 'install-heading');
  workflow.innerHTML = `<div class="dialog-inner">
    <div class="dialog-title"><h3 id="install-heading" data-i18n="installHeading"></h3><button class="close" id="install-close" type="button" aria-label="关闭">×</button></div>
    <div class="dialog-body">
      <p id="simulation-notice" class="notice" hidden></p>
      <p id="workflow-status" role="status" aria-live="polite"></p><p id="workflow-error" role="alert" hidden></p>
      <section id="connect-section">
        <p data-i18n="preflight"></p>
        <label class="confirm"><input type="checkbox" id="physical-board"><span data-i18n="boardConfirm"></span></label>
        <label class="field" for="expected-mac" data-i18n="macLabel"></label>
        <input id="expected-mac" type="text" placeholder="aa:bb:cc:dd:ee:ff" autocomplete="off" spellcheck="false" maxlength="17" aria-describedby="mac-hint">
        <p class="field-hint" id="mac-hint" data-i18n="macHint"></p>
        <button id="connect-read" class="install-primary" type="button" data-i18n="connectRead" disabled></button>
      </section>
      <section id="plan-section" hidden>
        <h4 id="plan-title"></h4><p id="plan-summary"></p>
        <details id="plan-details" open><summary data-i18n="plan"></summary><pre id="plan-json"></pre></details>
        <p id="write-lock" class="notice" hidden></p>
        <div id="confirm-section">
          <label class="confirm"><input type="checkbox" id="backup-confirmed"><span data-i18n="backup"></span></label>
          <label class="confirm"><input type="checkbox" id="tf-removed"><span data-i18n="tfRemoved"></span></label>
          <label class="confirm"><input type="checkbox" id="plan-confirmed"><span data-i18n="approve"></span></label>
          <label class="confirm notice" id="erase-label" hidden><input type="checkbox" id="erase-confirmed"><span data-i18n="erase"></span></label>
          <label class="field" id="current-app-label"><span data-i18n="currentApp"></span><input type="file" id="current-app" accept=".bin"></label>
          <button id="execute-plan" class="install-primary" type="button" data-i18n="run" disabled></button>
        </div>
        <progress id="write-progress" max="1" value="0" aria-label="Installation progress" hidden></progress>
        <p data-i18n="tfGuide"></p>
      </section>
      <p id="finished-note" class="notice" hidden></p>
      <div class="workflow-actions"><button type="button" id="save-receipt" data-i18n="export" disabled></button><button type="button" id="stop-close" data-i18n="close"></button></div>
    </div></div>`;
  document.body.append(workflow);
  const supported = simulation || (isSecureContext && !!navigator.serial?.requestPort);
  function updateLanguage() {
    document.documentElement.lang = language === 'zh' ? 'zh-CN' : 'en';
    document.title = language === 'zh' ? 'Draftor 草稿机 · 固件安装' : 'Draftor · Firmware Installer';
    document.querySelectorAll('[data-i18n]').forEach(el => { el.textContent = t(el.dataset.i18n); });
    document.querySelector('[data-i18n="previewHint"]').textContent = simulation ? `${t('sim')} · ${scenario}` : t(hardwareFreshWritesEnabled ? 'firstHint' : hardwareWritesEnabled ? 'testHint' : 'previewHint');
    $('board-name').textContent = language === 'zh' ? '微雪 ESP32-S3-RLCD-4.2' : 'Waveshare ESP32-S3-RLCD-4.2';
    $('board-change').textContent = t(chooserOpen ? 'hide' : 'change');
    $('lang-toggle').setAttribute('aria-label', language === 'zh' ? 'Switch language to English' : '切换为中文');
    for (const id of ['dialog-close', 'install-close']) $(id).setAttribute('aria-label', t('close'));
    $('simulation-notice').textContent = `${t('sim')} · ${scenario || ''}. ${t('simHint')}`;
    $('simulation-notice').hidden = !simulation;
    $('stop-close').textContent = t(busy ? 'stop' : 'close');
    $('workflow-status').textContent = t(lastPhase);
    if (errorCode) $('workflow-error').textContent = `${errorCode}: ${(errors[errorCode] || errors.IO)[language === 'zh' ? 0 : 1]}`;
    if (session?.plan) renderPlan();
    if (modalKey && dialog.open) renderInfo();
    if (session?.state === 'VERIFIED_BOOT_PENDING') $('finished-note').textContent = `${simulation ? t('simHint') + ' ' : ''}${t('finished')}`;
  }
  function renderInfo() { const c = dict[language].dialogs[modalKey]; $('dialog-heading').textContent = c.title; $('dialog-body').innerHTML = c.body; }
  function state(phase, receipt = {}) {
    lastPhase = `phase_${phase}`;
    if (phase === 'STOP' || phase === 'VERIFIED_BOOT_PENDING') $('plan-details').open = false;
    if (phase === 'STOP' && receipt.error) errorCode = receipt.error;
    $('workflow-error').hidden = !errorCode;
    if (receipt.progress !== undefined) { $('write-progress').hidden = false; $('write-progress').value = receipt.progress; }
    $('save-receipt').disabled = !session;
    updateLanguage();
  }
  function renderPlan() {
    const p = session.plan;
    $('plan-section').hidden = false;
    $('plan-title').textContent = `${t(p.initialState === 'waveshare-factory-20260120' ? 'factory' : p.mode === 'fresh' ? 'fresh' : 'update')} · ${p.appSlot} · ${p.appOffset}`;
    $('plan-summary').textContent = t(p.mode === 'fresh' ? 'freshSummary' : 'updateSummary');
    $('plan-json').textContent = JSON.stringify(p, null, 2);
    $('erase-label').hidden = p.mode !== 'fresh';
    $('current-app-label').hidden = p.mode === 'fresh';
    $('write-lock').hidden = simulation || p.hardwareWriteAllowed;
    $('write-lock').textContent = t(p.mode === 'fresh' ? 'freshLocked' : 'locked');
    enableExecution();
  }
  function enableExecution() {
    const p = session?.plan;
    $('execute-plan').disabled = busy || session?.state !== 'PLAN_READY' || !p ||
      (!simulation && !p.hardwareWriteAllowed) ||
      !['backup-confirmed', 'tf-removed', 'plan-confirmed'].every(id => $(id).checked) ||
      (p.mode === 'fresh' && !$('erase-confirmed').checked);
  }
  function fail(error) {
    errorCode = error instanceof Stop ? error.code : error?.name === 'NotFoundError' ? 'CONNECTION' : 'IO';
    if (session && session.state !== 'STOP') session.cancel(errorCode);
    lastPhase = 'phase_STOP'; $('workflow-error').hidden = false; updateLanguage();
  }
  function saveReceipt() {
    if (!session) return;
    const url = URL.createObjectURL(new Blob([JSON.stringify(session.receipt(), null, 2) + '\n'], { type: 'application/json' }));
    const link = document.createElement('a'); link.href = url; link.download = `Draftor-${simulation ? 'SIMULATION' : 'LOCAL'}-receipt.json`; link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  async function closeWorkflow() {
    if (busy) { session?.cancel(); return; }
    dialogEpoch++; // Discard any still-pending asset load from this dialog.
    try { await session?.close(); } catch (error) { fail(error); return; }
    $('expected-mac').value = ''; $('current-app').value = ''; workflow.close(); $('install-btn').focus();
  }
  $('install-btn').addEventListener('click', async () => {
    if (workflow.open || busy) return;
    const epoch = ++dialogEpoch;
    release = undefined;
    session = undefined; errorCode = ''; lastPhase = supported ? 'preparing' : 'unsupported';
    $('workflow-error').hidden = true; $('plan-section').hidden = true; $('finished-note').hidden = true;
    $('connect-section').hidden = !supported; $('connect-read').disabled = true; $('save-receipt').disabled = true;
    $('expected-mac').value = expectedMac;
    for (const id of ['physical-board', 'backup-confirmed', 'tf-removed', 'plan-confirmed', 'erase-confirmed']) $(id).checked = false;
    $('current-app').value = ''; $('write-progress').hidden = true;
    $('plan-details').open = true;
    updateLanguage(); workflow.showModal();
    if (!supported) return;
    try {
      const loaded = await loadRelease(new URL('../firmware/', import.meta.url));
      if (!workflow.open || epoch !== dialogEpoch) return;
      release = loaded;
      lastPhase = 'ready'; $('connect-read').disabled = false; updateLanguage();
    } catch (error) { if (workflow.open && epoch === dialogEpoch) fail(error); }
  });
  $('connect-read').addEventListener('click', async () => {
    if (busy || !release) return;
    try {
      if (!$('physical-board').checked) throw new Stop('BOARD');
      normalizeMac($('expected-mac').value);
    } catch (error) { fail(error); return; }
    busy = true; $('connect-read').disabled = true; errorCode = '';
    try {
      // requestPort is called synchronously from this user click before awaiting.
      const driver = await requestDriver(release);
      session = new InstallSession(driver, release, { hardwareWritesEnabled, hardwareFreshWritesEnabled, onState: state });
      await session.prepare({ boardConfirmed: $('physical-board').checked, expectedMac: $('expected-mac').value });
      $('connect-section').hidden = true;
    } catch (error) { fail(error); }
    finally { busy = false; updateLanguage(); enableExecution(); }
  });
  $('execute-plan').addEventListener('click', async () => {
    if (busy || $('execute-plan').disabled) return;
    busy = true; errorCode = ''; $('confirm-section').hidden = true; enableExecution();
    try {
      const file = $('current-app').files[0];
      if (file && (file.size < 24 || file.size > PINS.slotCapacity)) throw new Stop('CURRENT_APP');
      const currentApp = file ? new Uint8Array(await file.arrayBuffer()) : undefined;
      await session.execute({ backupConfirmed: $('backup-confirmed').checked, tfRemoved: $('tf-removed').checked, planConfirmed: $('plan-confirmed').checked,
        eraseConfirmed: $('erase-confirmed').checked, currentApp });
      $('finished-note').hidden = false;
    } catch (error) { fail(error); }
    finally { busy = false; $('current-app').value = ''; updateLanguage(); enableExecution(); }
  });
  for (const id of ['backup-confirmed', 'tf-removed', 'plan-confirmed', 'erase-confirmed']) $(id).addEventListener('change', enableExecution);
  $('save-receipt').addEventListener('click', saveReceipt);
  $('stop-close').addEventListener('click', closeWorkflow); $('install-close').addEventListener('click', closeWorkflow);
  workflow.addEventListener('cancel', event => { event.preventDefault(); closeWorkflow(); });
  workflow.addEventListener('close', () => { $('confirm-section').hidden = false; });
  $('lang-toggle').addEventListener('click', () => { language = language === 'zh' ? 'en' : 'zh'; updateLanguage(); });
  $('board-change').addEventListener('click', () => { chooserOpen = !chooserOpen; $('board-chooser').hidden = !chooserOpen; $('board-change').setAttribute('aria-expanded', String(chooserOpen)); updateLanguage(); });
  document.querySelectorAll('[data-modal]').forEach(el => el.addEventListener('click', () => { modalKey = el.dataset.modal; renderInfo(); dialog.showModal(); }));
  $('dialog-close').addEventListener('click', () => dialog.close());
  dialog.addEventListener('click', event => { if (event.target === dialog) dialog.close(); });
  addEventListener('beforeunload', event => { if (busy) { event.preventDefault(); event.returnValue = ''; } });
  updateLanguage();
}
