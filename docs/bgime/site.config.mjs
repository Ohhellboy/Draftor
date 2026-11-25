export const baseline = 'f18939f8880e2278c231739d553fbf983c19e0f6';
export const repository = 'https://github.com/Ohhellboy/Draftor';
export const pages = [
  { slug: '', file: 'index.md', title: '认识 bgIME', group: '开始使用', description: '从候选词到你的程序，了解 bgIME 的用途、模块和适用范围。' },
  { slug: 'quickstart', file: 'quickstart.md', title: '快速开始', group: '开始使用', description: '获取 Alpha 3 源码，编译并运行第一个命令行示例。' },
  { slug: 'schemes', file: 'schemes.md', title: '输入方案与示例', group: '开始使用', description: '运行全拼、小鹤双拼和五笔86示例，选择适合的接入入口。' },
  { slug: 'integration', file: 'integration.md', title: '接入自己的程序', group: '开始使用', description: '连接输入事件、候选显示、文本提交与宿主调度。' },
  { slug: 'api', file: 'api.md', title: '接口导览', group: 'API 参考', description: '按任务查找公开头文件，区分推荐入口、底层接口与产品实现。' },
  { slug: 'api/core', file: 'api-core.md', title: '资源、候选与提交', group: 'API 参考', description: 'Core 的资源打开、分步查询、候选生命周期和提交结果。' },
  { slug: 'api/composition', file: 'api-composition.md', title: '拼音与组合输入', group: 'API 参考', description: 'Composition 与 Submission 的参数、顺序、返回值和数据有效期。' },
  { slug: 'api/wubi', file: 'api-wubi.md', title: '五笔接口', group: 'API 参考', description: 'Wubi86Decoder 与 DirectQueryBinding 的使用约定。' },
  { slug: 'learning', file: 'learning.md', title: '选词学习', group: '学习与资源', description: '理解 Alpha 3 的共享记录、选词事件、排序和保存，以及独立接入的边界。' },
  { slug: 'resources', file: 'resources.md', title: '词库与配套资源', group: '学习与资源', description: '区分测试词库、正式资源和个人数据，取得版本匹配的文件。' },
  { slug: 'about', file: 'about.md', title: '版本、许可与参与', group: '项目', description: '查阅模块版本、许可资料、源码及问题反馈入口。' },
];
export const pageUrl = slug => `/bgime/${slug ? slug + '/' : ''}`;
