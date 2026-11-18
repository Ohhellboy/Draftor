# Alpha 3 external build inputs / 构建输入

The [input list](../EXTERNAL_INPUTS.json) records the exact repository-relative destination, size and SHA-256 for each of the 11 non-Git build inputs. All 11 were compared byte for byte with the publicly available [Alpha 2.1 BuildInputs ZIP](https://github.com/Ohhellboy/Draftor/releases/download/v0.1.0-alpha.2.1/Draftor_v0.1.0-alpha.2.1_BuildInputs.zip). The ZIP is 470,993,697 bytes and its SHA-256 is `666edd63bcda953c71754e858526743d2f709cb0c784c65fc9a7071b4f9bf49e`. Extract it and use the command in [BUILD.md](BUILD.md).

11 项编译输入含字形表、五笔表、字体配套资料和固定 TinyUSB 组件。核对压缩包后，使用包内 `BuildInputs/inputs/`。这些材料沿用历史资产与各自的来源、署名和使用条件。本批不另上传独立可复用的 MiSans 表、原字体或许可仍有未决项的构建包。[字体分发说明](FONT_DISTRIBUTION.md) · [资源许可记录](RESOURCE_STATUS.md) · [第三方声明](../THIRD_PARTY.md)

TF 运行资源使用[单独清单](../release/alpha3/TF_RESOURCES.json)。其中两份 format 301 索引由三种输入方案的通用词条位置生成，匹配本 App 的关系资源 pin。用户文稿、学习记录和蓝牙配对均不作为编译或安装输入。
