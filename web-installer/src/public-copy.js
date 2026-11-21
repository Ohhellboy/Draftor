// SPDX-License-Identifier: Apache-2.0
export const publicCopy = {
  zh: {
    previewHint: 'v0.1.0-alpha.3 · 正式网站 · 真实安装暂未公开开放',
    intro: '此版本提供 USB 只读检查与安装计划。真实写入已锁定；TF 资源可直接下载。',
    locked: '当前正式网站仅提供只读检查与安装计划，不能执行真实写入。请保存回执后关闭连接。',
    freshLocked: '已识别首装状态，但当前正式网站锁定全片擦除与固件写入。',
    identityBound: '当前 USB 设备身份（只保存在本页内存中；关闭即清除）：',
    publicScope: '已验收范围：微雪 ST7305，准确 Alpha 3 app0 同版本兼容更新，以及 FactoryOnly 20260120 准确指纹首装。公开安装仍待审核；app1、其他厂家批次、Windows/Edge 实机、新 TF 卡和境内下载未验收。',
  },
  en: {
    previewHint: 'v0.1.0-alpha.3 · Official site · Hardware installation not yet public',
    intro: 'USB read-only checks and installation plans are available. Hardware writes are locked. TF resources can be downloaded now.',
    locked: 'This public site provides read-only checks and plans only. Hardware writing is locked. Save the receipt and close the connection.',
    freshLocked: 'First-install state recognized. Full erasure and firmware writing are locked on this public site.',
    identityBound: 'Selected USB identity (page memory only; cleared on close):',
    publicScope: 'Qualified scope: Waveshare ST7305, exact Alpha 3 app0 same-version rewrite and the exact FactoryOnly 20260120 fingerprint. Public installation awaits review. app1, other factory batches, Windows/Edge hardware, new TF cards and mainland China downloads remain unverified.',
  },
};
