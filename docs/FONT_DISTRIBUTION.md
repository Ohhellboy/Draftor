# Fonts and generated inputs / 字体与生成输入

Draftor uses LXGW WenKai for the editor and Find, and **MiSans** for the Chinese menus. These font resources keep their own licenses; the project’s Apache-2.0 license does not relicense them.

草稿机在编辑器及查找中使用霞鹜文楷，在中文菜单中使用 **MiSans**。字体资源适用各自许可，不因项目采用Apache-2.0而改变。

## WenKai / 文楷

The UI16 generator uses LXGW WenKai 1.522, source SHA-256 `39ad71264b588165b469e35e6afb162a378dacd1f95348160240ba9038ac3009`. It rasterizes at 16px with a fixed threshold, using UI14 coverage plus ASCII and navigation characters. BuildInputs carries the exact UI16 data, source font, generator and original OFL notice. The generated bitmap is supplied under OFL-1.1, without presenting it as a newly branded WenKai font family. The existing UI14 and TF body font are retained unchanged.

UI16生成器使用上述准确文楷1.522，在16px下按固定阈值生成位图，字符集继承UI14并加入ASCII与导航字符。BuildInputs保留准确数据、原字体、生成器和OFL声明；生成位图继续按OFL-1.1提供，不冒充新的文楷字体家族。既有UI14和TF正文字体保持原样。

Original notice: [LXGW OFL](../integration/product/assets/find_ui16/LXGW_OFL.txt). Generator: [generate_ui16.py](../integration/product/scripts/generate_ui16.py).

## MiSans

The menu uses MiSans VF 4.009 at its real weight axis value 500, source SHA-256 `0ddef90648998900175cfdca9a6f087a2544c182f130b0ad4f7e94a03a115e79`. The retained generator rasterizes native 14/16px glyphs with FreeType BASIC and a threshold of 128, then stores cropped monochrome bitmaps and metrics. It does not edit outlines or synthesize bold. The 14px labels use 437 glyphs; the 16px name font uses 29,568.

菜单使用上述MiSans可变字体的真实500字重，在原生14/16px下生成单色位图及度量，保留原有阈值与裁边方式。生成器没有编辑轮廓或模拟加粗。14px文案表含437个字形，16px名称表含29,568个字形。

Xiaomi’s [official FAQ](https://hyperos.mi.com/font/zh/faq/) permits embedded use with attribution. The [license](https://hyperos.mi.com/font-download/MiSans%E5%AD%97%E4%BD%93%E7%9F%A5%E8%AF%86%E4%BA%A7%E6%9D%83%E8%AE%B8%E5%8F%AF%E5%8D%8F%E8%AE%AE.pdf), section 2, also restricts adaptation and separate distribution of font components while allowing distribution of works such as apps. Keep the [original license](../LICENSES/MiSans_License.pdf) and attribution with any permitted copies.

小米[官方FAQ](https://hyperos.mi.com/font/zh/faq/)允许注明来源后的嵌入使用。许可第2节同时限制改编及单独分发字体组件，并允许分发使用字体创作的App等作品。因此，固件内显示与单独提供生成字形表需要分别判断；随允许分发的副本保留原许可和署名。

**Distribution review remains open for the generated menu table.** The official text does not specifically classify this cropped, packed bitmap format. Firmware embedding has the FAQ as a basis, but the BuildInputs table is independently reusable. Omitting the original TTF does not resolve that distinction. The local review bundle retains the exact table so the software can be built; public upload of that payload requires a documented distribution basis. No original MiSans font file is bundled, and the table is excluded from Git/Source.

**生成菜单表的分发依据仍待确认。** 官方文本没有具体说明这种裁边、打包位图的归类。固件嵌入有FAQ作为依据，而BuildInputs中的字形表可以被独立复用；不附原TTF也不能消除这一差别。本地审阅包保留准确表以保证构建完整，公开上传该载荷前需明确分发依据。MiSans原字体不入包，生成表不进入Git／Source。

Generator: [generate_menu_font.py](../tools/generate_menu_font.py). These records describe the existing assets; they are not a new font conversion or a legal clearance.
