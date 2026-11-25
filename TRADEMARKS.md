# 名称与标识使用政策

# Name and Branding Policy

**Draftor / bgIME**

## 中文

### 1. 适用范围

Draftor（草稿机）是开源写作设备项目。

bgIME（大鹅输入法，Big Goose Input Method Engine）是面向嵌入式系统的中文输入引擎，可独立集成至其他软件及硬件产品。

本政策适用于上述项目名称、项目使用的品牌标识，以及对官方发行版本、网站和维护身份的表述。

本政策仅涉及项目有权管理的名称及品牌使用，不主张第三方标识的权利，也不声明未经确认的商标注册状态。

### 2. 软件许可与品牌使用

Draftor 和 bgIME 的自有源码采用 Apache License 2.0。具有独立许可证的模块、第三方库、字体、词库及其他材料继续适用各自的许可条件。

根据 Apache License 2.0 第六条，软件许可本身不授予许可方商号、商标、服务标记或产品名称的一般使用权，但合理说明作品来源及保留 NOTICE 内容所需的使用除外。

本政策不修改软件许可证，不撤回已经授予的软件使用权利，也不限制适用许可证允许的研究、修改、移植、集成、再分发及商业使用。

### 3. 无需另行授权的使用

以下行为在准确说明项目来源且不造成官方身份混淆的前提下，无需取得额外的品牌使用授权：

- 研究、评论、教学、技术讨论及其他合法的描述性使用。
- 正常的源码 Fork、私人修改、代码贡献和兼容性开发。
- 按适用许可证保留版权、归属声明及原有技术标识。
- 如实说明第三方软件或硬件基于 Draftor 开发、使用 bgIME 引擎或兼容特定项目版本。
- 在符合相关分发许可的前提下，原样镜像和转发官方发行包，并明确其原始来源与实际分发渠道。
- 依据公开资料自行组装 Draftor 设备，并如实说明其为个人或第三方组装设备。

允许使用以下事实性文字标注：

- 基于 Draftor 开发
- 使用 bgIME 输入引擎
- Based on Draftor
- Powered by bgIME
- Built with bgIME

上述标注不代表官方认证、质量保证、合作关系或商业背书。

经修改的引擎应根据实际情况标明其修改版或衍生版身份。

### 4. 技术标识与兼容性

源码中的 HanIME 命名及相关技术标识属于现有实现和接口兼容体系。

包括但不限于：

- `hanime`
- `hanime_extension`
- `hanime::wubi`
- `HanIME::Core`
- `HanIME::Extension`
- 对应头文件、目录及构建目标

下游开发者可以依据适用许可证保留这些技术标识，不需要因独立发行名称或产品品牌的变化而重命名现有 API。

正常的 GitHub Fork 仓库名称、代码路径和技术标识，不应仅因包含项目名称而被视为官方身份声明。

### 5. 第三方发行与产品命名

独立发布的修改版软件、固件和衍生产品应采用能够与官方发行明确区分的名称、标识及版本说明。

第三方发行者应准确标明：

- 上游项目及所使用的版本。
- 自身发行或修改身份。
- 实际维护者及技术支持渠道。
- 适用许可证要求披露的修改及归属信息。

不得将独立维护的修改版冒充官方 Draftor 或 bgIME 发行版本。

对于平台移植、编译适配和第三方应用集成，允许保留必要的技术标识及事实性来源说明，不要求为了品牌区分而破坏接口兼容性。

### 6. 硬件销售及非官方发行渠道

个人 DIY、第三方硬件开发及商业集成均可按照适用许可进行。

第三方组装、销售或分发设备时，应清楚说明实际制造者、销售者和售后支持主体，不得使购买者误认为第三方设备由原项目生产、销售、认证或担保。

非官方固件发行、下载镜像、烧录网站及相关服务应明确自身身份。

不得使用容易造成来源混淆的页面标题、域名、图标、版本说明或官方认证表述。

第三方发行者对其自身修改、产品及服务承担相应责任。本政策不排除适用法律规定的责任。

### 7. 品牌标识与官方背书

项目名称及品牌标识不得用于虚构官方身份、授权合作、认证关系或项目方背书。

除本政策明确允许的合理指称、适用法律允许的使用及已有有效授权外，将项目名称或标识作为第三方独立产品的主要品牌、商业标识或认证徽章使用，应事先取得相关权利人的明确许可。

本政策所允许的 Powered by bgIME 等文字说明，不自动包含将项目徽标用作第三方商业品牌或认证标识的授权。

### 8. 官方项目来源

- 源码仓库：https://github.com/Ohhellboy/Draftor
- 官方发行：https://github.com/Ohhellboy/Draftor/releases
- Draftor 网站：https://draftor.tcssn.org/
- bgIME 开发者文档：https://draftor.tcssn.org/bgime/

官方发行身份以项目维护者在上述渠道明确发布的信息为准。

项目仓库中的实验分支、第三方 Fork、非官方构建及自行修改版本，不因包含原有名称或代码而自动成为官方发行版本。

有关名称及标识使用的问题，可通过项目 GitHub Issues 联系维护者。涉及不适宜公开的信息时，应请求适当的联系渠道。

### 9. 政策边界

本政策不构成软件许可证的附加限制，不改变 Apache License 2.0 已授予的著作权或专利许可。

本政策不撤销已有有效授权，不限制适用法律允许的合理使用，也不改变第三方材料的权利归属。

项目名称及标识的实际法律保护范围，以相关权利、有效授权及适用法律为准。

---

## English

### 1. Scope

Draftor (草稿机) is an open-source writing-device project.

bgIME (大鹅输入法, Big Goose Input Method Engine) is an embeddable Chinese input method engine that can be integrated into independent software and hardware products.

This policy addresses the use of these project names, associated branding, and representations of official release, website and maintainer identity.

It applies only to names and branding that the project is entitled to manage. It does not claim rights over third-party marks or assert unverified trademark registration.

### 2. Software licensing and branding

Project-owned Draftor and bgIME source code is licensed under Apache License 2.0. Independently licensed modules, third-party libraries, fonts, dictionaries and other materials retain their applicable licensing terms.

Section 6 of Apache License 2.0 does not grant general permission to use the licensor's trade names, trademarks, service marks or product names, except for reasonable and customary descriptions of the origin of the work and reproduction of NOTICE content.

This policy does not modify the software licenses, revoke existing permissions, or restrict research, modification, porting, integration, redistribution or commercial use permitted by the applicable licenses.

### 3. Uses not requiring separate permission

The following uses do not require additional branding permission when they accurately identify the source and do not create confusion about official affiliation:

- Research, commentary, education, technical discussion and other lawful descriptive uses.
- Ordinary source repository forks, private modifications, contributions and compatibility development.
- Retention of copyright, attribution notices and technical identifiers under applicable licenses.
- Accurate statements that independent software or hardware is based on Draftor, uses bgIME, or is compatible with a specified version.
- Unmodified mirroring and redistribution of official releases where permitted by the applicable licenses, with accurate identification of the original source and actual distribution channel.
- Independently assembled Draftor devices accurately identified as personal or third-party builds.

Permitted factual text references include:

- Based on Draftor
- Powered by bgIME
- Built with bgIME
- Uses the bgIME input method engine

These statements do not imply certification, warranty, partnership or endorsement.

Modified engines should be accurately identified as modified or derivative versions where relevant.

### 4. Technical identifiers and compatibility

Existing HanIME identifiers, including namespaces, headers, source paths and build targets such as `HanIME::Core` and `HanIME::Extension`, serve technical and compatibility purposes.

Downstream developers may retain these identifiers under the applicable licenses. Independent product branding does not require renaming existing APIs or breaking technical compatibility.

The presence of upstream names in ordinary GitHub forks, source paths or technical identifiers does not by itself constitute a claim of official affiliation.

### 5. Independent distributions and products

Independently distributed modified software, firmware and derivative products should use public-facing names, branding and version descriptions that clearly distinguish them from official releases.

Distributors should accurately identify the upstream project and version, their own distribution or modification identity, the responsible maintainer and support channel, and modifications and attribution required by applicable licenses.

Independently maintained derivatives must not be represented as official Draftor or bgIME releases.

Platform adaptations, build configurations and third-party integrations may retain necessary technical identifiers and accurate descriptive references.

### 6. Hardware and unofficial distribution channels

Personal DIY assembly, third-party hardware development and commercial integration remain permitted under the applicable licenses.

Third-party hardware assembly, sale or distribution should clearly identify the actual manufacturer, seller and support provider. Independent devices must not be represented as manufactured, sold, certified or warranted by the upstream project.

Unofficial firmware distributions, mirrors, download sites, web installers and related services should clearly disclose their independent status.

Names, icons, domains, release descriptions and certification claims must not misrepresent their origin or affiliation.

Downstream distributors are responsible for their own modifications, products and services, subject to applicable law.

### 7. Branding and endorsement

Project names and branding must not be used to falsely claim official status, authorization, partnership, certification or endorsement.

Except for uses expressly permitted by this policy, applicable law or existing valid permissions, use of project names or branding as the primary brand, commercial identity or certification badge of an independent product requires explicit permission from the relevant rights holder.

Permission to use factual text references such as Powered by bgIME does not automatically grant permission to use project logos as independent commercial branding or certification marks.

### 8. Official project sources

- Source repository: https://github.com/Ohhellboy/Draftor
- Official releases: https://github.com/Ohhellboy/Draftor/releases
- Draftor website: https://draftor.tcssn.org/
- bgIME developer documentation: https://draftor.tcssn.org/bgime/

Official release status is established by explicit publication by project maintainers through these channels.

Experimental branches, third-party forks, unofficial builds and independently modified versions do not acquire official release status merely by containing upstream names or code.

Questions concerning name and branding use may be directed to the project maintainers through GitHub Issues. Request an appropriate contact channel for matters unsuitable for public discussion.

### 9. Policy boundaries

This policy is not an additional software license restriction and does not modify the copyright or patent licenses granted under Apache License 2.0.

It does not revoke existing valid permissions, restrict lawful uses, or alter third-party ownership.

The legal protection available for project names and branding remains subject to the relevant rights, valid permissions and applicable law.
