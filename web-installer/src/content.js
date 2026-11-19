export const dict = {
    zh: {
      hero:'草稿机', positioning:'无打扰口袋中文写作', values:'开源，无网络，无 AI，无广告',
      installTitle:'安装 Draftor 固件', intro:'通过 USB 连接设备，即可在浏览器中安装固件。无需额外下载软件。',
      boardLabel:'适用设备：', change:'更换', hide:'收起', wsName:'微雪集成版', protoName:'Draftor S3 原型版（暂未开放）',
      installBtn:'连接设备并安装', browserHint:'请使用电脑端 Chrome 或 Edge 浏览器',
      previewHint:'网页预览版 · 尚未接入正式烧录程序',
      whatDraftor:'什么是草稿机', whatBgime:'什么是大鹅输入法', help:'安装帮助', advanced:'高级选项',
      dialogs: {
        draftor:{title:'什么是草稿机',body:'<p><strong>Draftor（草稿机）</strong>是一台以中文输入和持续写作为中心的轻量写作设备。</p><p>它强调<strong>无打扰口袋中文写作</strong>：开源、写作时不依赖网络、不接入 AI，也不提供广告。</p><p>这是一台写作工具，不是需要登录账户的云端写作平台。</p>'},
        bgime:{title:'什么是大鹅输入法',body:'<p><strong>bgIME（大鹅输入法）</strong>是 Draftor 使用的开源中文输入法引擎。</p><p>它面向资源有限的设备，支持全拼、双拼和五笔86等输入方案，并将输入法核心与具体设备界面分开。</p><p>项目源码可在 <a href="https://github.com/Ohhellboy/Draftor" target="_blank" rel="noopener noreferrer">GitHub</a> 查看。</p>'},
        help:{title:'安装帮助',body:'<p>准备一台电脑、一条支持数据传输的 USB 线，以及对应的 Draftor 设备。</p><ul><li>请使用支持 Web Serial 的桌面版 Chrome 或 Edge。</li><li>未来正式版本会在写入前核对所选硬件与固件资源。</li><li>TF 卡内的文稿和词库不应因固件更新而被清空；实际写入策略须在烧录程序接入后验证。</li></ul><div class="notice">当前只是网页界面预览，还不能烧写固件。</div>'},
        advanced:{title:'高级选项',body:'<p>此区域预留给固件版本、设备诊断与故障恢复说明。普通用户不需要调整 Flash 参数。</p><p>正式烧录将使用每种硬件对应的受控发布清单。当前尚未提供已验证清单，因此禁止直接写入设备。</p>'},
        install:{title:'烧录功能尚未接入',body:'<p>这是 Draftor Web Installer 的<strong>首版网页界面</strong>，并非可用的固件烧录程序。</p><p>需要先取得微雪集成板的正式固件、分区布局、可信发布清单和恢复流程，才会启用浏览器烧录。</p><div class="notice">未连接设备，也没有读取或写入任何 Flash。</div>'}
      }
    },
    en: {
      hero:'Draftor', positioning:'Distraction-free pocket Chinese writing', values:'Open source. No network. No AI. No ads.',
      installTitle:'Install Draftor Firmware', intro:'Connect your device by USB and install firmware in your browser. No additional app needed.',
      boardLabel:'Device:', change:'Change', hide:'Hide', wsName:'Waveshare integrated board', protoName:'Draftor S3 prototype (not available yet)',
      installBtn:'Connect & Install', browserHint:'Use desktop Chrome or Edge',
      previewHint:'Website preview · Firmware flashing not yet connected',
      whatDraftor:'What is Draftor?', whatBgime:'What is bgIME?', help:'Installation help', advanced:'Advanced',
      dialogs: {
        draftor:{title:'What is Draftor?',body:'<p><strong>Draftor</strong> is a lightweight device focused on Chinese text input and uninterrupted writing.</p><p>Its emphasis is <strong>distraction-free, pocket-sized Chinese writing</strong>: open source, no network required while writing, no AI, and no advertisements.</p><p>It is a writing tool, not an account-based cloud writing platform.</p>'},
        bgime:{title:'What is bgIME?',body:'<p><strong>bgIME</strong> is the open-source Chinese input engine used by Draftor.</p><p>It is designed for resource-constrained devices and supports full Pinyin, double Pinyin, and Wubi 86.</p><p>Explore the source on <a href="https://github.com/Ohhellboy/Draftor" target="_blank" rel="noopener noreferrer">GitHub</a>.</p>'},
        help:{title:'Installation help',body:'<p>You will need a computer, a USB data cable, and a compatible Draftor device.</p><ul><li>Use desktop Chrome or Edge with Web Serial support.</li><li>A future release will verify firmware and device compatibility.</li><li>Document and dictionary data on the microSD card must be kept separate from firmware writes; the final procedure still requires validation.</li></ul><div class="notice">This is a website preview; firmware flashing is not available yet.</div>'},
        advanced:{title:'Advanced',body:'<p>This area is reserved for firmware versions, diagnostics, and recovery instructions. Normal users will not need Flash settings.</p><p>Flashing will require a verified board-specific release manifest. No manifest is available in this preview.</p>'},
        install:{title:'Firmware flashing not available yet',body:'<p>This is the <strong>first Draftor installer website preview</strong>, not a working firmware flasher.</p><p>The installer will be enabled after verified board-specific firmware, partition layout, release manifest, and recovery procedure are available.</p><div class="notice">No device has been connected or written.</div>'}
      }
    }
  };
