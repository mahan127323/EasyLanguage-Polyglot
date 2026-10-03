# Easy language — UE 编辑器汉化插件

> 面向 UE 5.7 / 5.8（向下兼容 5.x）的**零侵入**编辑器本地化插件：不修改引擎源码、不修改任何资产、不写工程 Content，一键把编辑器界面（引擎 + 任何插件）切换成简体中文 / 英文 / 任意语言，**免重启**。任何第三方插件界面上未翻译的英文，都可以用「拾取工具」当场点出来翻译并写入你自己的词典。

所有模块均为 `Type = Editor`，打包 / Cook 时自动被排除，不会进入最终游戏包。

---

## 一、安装

1. 把 `Plugins/Polyglot` 整个文件夹拷贝到工程的 `Plugins/` 目录下（示例：`YourProject/Plugins/Polyglot`）。
2. 用引擎打开工程，在 **Edit → Plugins → 搜 "Easy language"** 中启用（首次会提示重启编辑器）。
3. 重新打开编辑器即可。右上角界面可立刻执行：**Tools → Easy language → Easy language Panel…**。

> 没有工程？也可以直接用来开发/验证：参见文末「开发者构建验证」。

## 二、快速上手

| 操作 | 入口 |
| --- | --- |
| 中英一键切换 | `Ctrl + Alt + L` 或 **Tools → Easy language → Easy language: Toggle Chinese / English** |
| 打开主控面板 | 主编辑器顶部工具栏的 Easy language 图标按钮，或 **Tools → Easy language → Easy language Panel…** |
| 打开词典编辑器 | **Tools → Easy language → Open Dictionary Editor…** |
| 进入可视化拾取模式 | `Ctrl + Alt + P` 或 **Tools → Easy language → Pick Text To Translate…** |

第一次按下 `Ctrl + Alt + L` 会把整个编辑器切到简体中文（引擎自带的官方中文 + 本插件内置词典）；再按一次回到英文。

> 插件管理器列表、Tools 菜单、顶部工具栏按钮、两个常驻窗口标题均使用同一枚蓝色“A ↔ 文”图标，显示名为 **Easy language**；对外展示名与代码内部标识符（`Polyglot` 前缀的模块/类/目录）互不影响。

## 三、主控面板（Easy language Panel）

**主控面板与词典编辑器两个窗口自带界面语言切换**：窗口顶部有 **English / 简体中文** 按钮（高亮当前语言）。切到中文后，这两个窗口的标题、分组、按钮、提示词**立即同步为中文**，而列表里词典词条始终显示**原始数据不被二次翻译**——这正是“查看英文原文”功能不受影响的原因。该界面语言只影响本插件的自身界面，切换即时、无需重启。

- **Translations enabled**：总开关。关闭时**全量还原**所有已被改写的控件并恢复英文，打开时重新应用。
- **Editor language**：六个快捷按钮（English / 简体中文 / 日本語 / 한국어 / Deutsch / Français），点击即时生效，不重启不重载工程。
- **Scan layer options**：扫描周期（秒）与每帧预算（毫秒）用于控制性能；勾选是否翻译静态文本与可编辑文本框（可编辑文本框会干扰输入，默认关）。
- **Dictionary**：统计当前词条数，并提供：
  - **Dictionary editor**：打开词条编辑器；
  - **Pick text to translate**：进入拾取模式；
  - **Reload dictionaries**：热重载磁盘词典；
  - **Restore all widgets**：还原所有被改写的控件文本（排查问题用）。
- **Coverage**：列出本会话中已探测到的专项界面（Niagara / 材质 / 蓝图 / 内容浏览器等）与状态。它们全部采用**运行时模块探测**，模块不存在时不报错、自动跳过。

## 四、词典体系与存放位置

翻译能力来自 **三级词典合并**，后一级覆盖前一级：

| 层级 | 位置 | 说明 |
| --- | --- | --- |
| 内置词典（只读） | `Plugins/Polyglot/Content/Dictionaries/*.json` | 随插件分发的高频词条（通用界面 / Niagara / 材质 / 蓝图 / 内容浏览器 / 第三方插件模板） |
| 工程级用户词典 | `{工程}/Config/Polyglot/` | `user_dictionary.json`，随工程迁移、可入版本库 |
| 追加用户包 | `{工程}/Config/Polyglot/` 下其他 JSON | 可把词典按插件/主题拆包放置 |

- 插件配置：`{工程}/Config/Polyglot/settings.json`（开关、当前语言、扫描参数，自管，不写引擎 ini）。
- 导出 / 缓存：`{工程}/Saved/Polyglot/`。

词典文件为 **UTF-8 JSON**，结构：

```json
{
  "culture": "zh-Hans",
  "entries": [
    {
      "ns": "", "key": "",
      "source": "Play",
      "culture": "zh-Hans",
      "plugin": "Engine_Common",
      "scope": "General",
      "comment": "",
      "translation": "播放"
    }
  ]
}
```

CSV 支持与 **UE 官方同列名** 的格式互导（`Namespace,Key,Source,Translation` 四列即可，另接受 `Culture,Plugin` 追加列）：

```csv
Namespace,Key,Source,Translation,Culture,Plugin
,,"Play","播放","zh-Hans",Manual
```

## 五、可视化拾取：汉化任何第三方插件

这是把任何「官方没汉化的英文界面」变成中文的推荐方式：

1. `Ctrl + Alt + P`（或菜单）进入拾取模式，右下角出现小悬浮窗。
2. 把鼠标移到任意英文界面文字上（Niagara 模块名、材质节点、右键菜单项、内容浏览器文件名、任意插件的按钮/标签……），**悬停**即可看到“命中”预览；点击后该文字被读取为原文。
3. 在弹出面板里填写「译文」并保存。
4. 词条写入你的工程用户词典并**立即生效**（无需重启、无需重载）。

细节：
- 保存后可继续点下一个；面板左上角的 **Stop / Esc** 退出拾取模式。
- 命中内容来自显示层控件（`STextBlock` / `SEditableText(Box)` / `SMultiLineEditableText(Box)` / `SInlineEditableTextBlock` / `SRichTextBlock` 等），不受 FText 本地化系统限制，因此能处理 `FText::FromString` / `FName` 派生的动态文本。
- **自动识别引擎本地化文本**：如果点中的文字是走引擎本地化系统的文本（插件用 `LOCTEXT` / `NSLOCTEXT` 编写），保存时它的 **Key（和 Namespace）会自动填入词条**。这类词条会同时进入“本地化覆盖层”——即使控件被第三方刷新重建、面板重开、甚至重启编辑器，译文依然保留。拾取面板会显示 `Key: …` 提示确认已识别。
- **英文界面下拾取自动归入简体中文**：若在编辑器显示英文的状态下保存，词条会自动写入 `zh-Hans`（面板会提示），之后切到简体中文即可看到译文，不会产生永远不被命中的英文词条。
- 内容浏览器里的文件夹名 / 文件名翻译只在显示层替换：**磁盘路径与资产名不会被修改**，不影响工程迁移与打包。

## 六、词典编辑器

**Tools → Easy language → Open Dictionary Editor…**

- 顶部输入框按原文/译文搜索；勾选 “User entries only” 只看你自己添加的词条；每一行是 **原文 → 译文（语言/key/来源插件）**，灰色为内置词条、绿色为用户词条。
- **Add（批量粘贴）**：粘贴多条词条即可一次写入，例如
  `Blueprint/蓝图, simulation/模拟`（条目间用**英文逗号或换行**分隔，每条内部用第一个 `/` 分隔原文/译文）。粘贴窗口会先给出“可添加 N 条、跳过 M 条”的统计预览，确认后再提交；含 `,` 或 `/` 的歧义文本建议改走单条 Edit 或 CSV。
- **Edit（单条）**：对选中行打开单条编辑窗口，保存即写入用户词典（写入会自动刷新界面）。
- **Delete**：删除用户词条（内置词条不能删除，可用「把该原文覆盖成你自己的译文」来实现个性化）。
- **Import TXT**：选择一个 `.txt` 文本文件，按与批量 Add 相同的 `原文/译文`（逗号或换行分隔）格式批量导入用户词典。
- **Import JSON / Import CSV**：把外部词典合并进用户词典（自动跳过空译文行）。
- **Export JSON / Export CSV**：把当前筛选出的行导出为文件，可分享或回灌。
- **Reload**：重新读取磁盘上的所有词典。

批量写入的合并规则与 JSON/CSV 导入一致：同原文+语言的新词条覆盖旧词条，译文为空的条目会删除对应词条；解析统计（可添加/跳过）展示在提交按钮上，避免误导入。

## 七、未翻译词条扫描导出（命令行）

插件自带 `PolyglotExport` commandlet：遍历**引擎本体 + 引擎插件 + 工程插件**编译出的全部 `.locres`，对每个语言目标做 `en` 与中文（`zh` / `zh-Hans` / `zh-CN` / `zh-Hant`）差集，导出**所有尚未中文化**的 `Namespace,Key,Source` 到 CSV，供人工补译后回灌。

在启用插件的前提下，对任意工程执行（Windows 命令行）：

```
"<引擎>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "<工程>.uproject" ^
    -run=PolyglotExport -Output="D:/tmp/untranslated.csv" -unattended -nop4 -nosplash
```

- 缺省输出到 `{工程}/Saved/Polyglot/untranslated.csv`。
- 之后工作流：用 Excel/WPS 打开 CSV → 填好 `Translation` 列 → 词典编辑器 **Import CSV** 回灌 → 全部即时生效。
- 日志类别：`LogPolyglot`，开启 Verbose 可看全量细节。

## 八、备份 / 迁移 / 卸载

- **迁移（跟随工程）**：整个 `{工程}/Config/Polyglot/` 目录（设置 + 用户词典）即可；放进版本库团队共享。
- **升级引擎**：词典格式与引擎版本无关，直接把 `{工程}/Config/Polyglot/` 与插件拷贝到新版本工程即可，详见下节维护手册。
- **卸载**：在插件管理器停用/删除插件；删除 `{工程}/Config/Polyglot/`（如需同时清空设置与词典）。卸载不影响任何资产与工程配置；编辑器重启后一切回到英文默认状态（插件的所有修改都在内存中，进程退出即消失）。

## 九、已知局限（设计取舍）

1. **窗口创建时一次性烘焙文本**且没有刷新路径的极少数第三方自定义控件，切换语言后需重开对应标签页/窗口。
2. GPU 自绘 / 非 Slate 文本控件（少见）无法替换。
3. 拾取模式读取的是**显示层当前文本**；若某控件正处于被 Easy language 翻译后的状态，应先切回英文再拾取（或直接编辑其 Source）。
4. 文化切换基于引擎当前文化（`FInternationalization`）。若目标语言文化包在系统/引擎上不存在，会给出提示而不是崩溃。
5. 为保证编辑器不卡顿，扫描按分片 + 帧预算执行；超大窗口的首次全量翻译可能有几百毫秒延迟是正常现象，可调大预算。
6. **富文本标记安全处理**：`SRichTextBlock` 的译文若自身包含 `<` / `>`（例如想把某段富文本翻译成含代码尖括号的文字），会被安全跳过并在 `LogPolyglot`（Verbose）记录原因；普通中文译文不受影响。
7. **逐控件覆盖清单**（扫描层）：显示类 `STextBlock` / `SRichTextBlock` / `SInlineEditableTextBlock`（默认开启），输入类 `SEditableText` / `SEditableTextBox` / `SMultiLineEditableText` / `SMultiLineEditableTextBox`（默认关闭，需在面板勾选“翻译可编辑文本框”）。自定义自绘、非上述 Slate 控件派生、或 GPU 渲染的文字不在逐控件覆盖范围（可经拾取写入 ns/key 词条走本地化覆盖层，见第五节）。

## 十、排查：翻译没有生效？

打开 **Output Log**，看类别 `LogPolyglot`（默认过滤栏输入 `LogPolyglot` 即可），按下面的顺序对照：

1. 引擎整体没变中文：请先在主控面板切到**简体中文**——扫描层只在非英文文化下工作（英文时引擎数据天然是英文，无需改写）。
2. 日志每 5 秒出现 `Slate scanner paused: dictionary culture 'zh-Hans' differs from the engine language 'en'`：插件词典语言与引擎当前显示语言不一致，属于**等待切换状态**，把编辑器切到简体中文后会自动恢复扫描。
3. 保存词条后没立即变化：启动参数带 `-LogCmds="LogPolyglot Verbose"`（或运行时把 `LogPolyglot` 级别调到 Verbose），重新保存一次，日志会给出：
   - `no translation for '...' under culture 'zh-Hans'` —— 控件上**可见的这段文字**在词典里没有对应词条。最常见原因是可见文字与词条源文本有细微差异（空格 / 分隔符 / 前后缀 / 动态拼接）。用拾取模式直接点这段文字做词条最可靠，它读的就是控件可见文本。
   - `Slate scanner patched STextBlock: '...' -> '...'` —— 命中并已改写。若界面仍显示英文，说明第三方随后又把文本刷回英文（每帧重建 / 面板缓存），见“已知局限”第 1 条，重开对应窗口/标签即可。
   - `translation exists but the widget rejected it` —— 富文本（`SRichTextBlock`）译文里含 `<` / `>` 等标记被安全跳过，见“已知局限”第 6 条。
   - 扫描完成时一行 `Slate scanner pass complete: patched N text widget(s).` —— 本轮共改写了 N 处。
4. 以上都没有对应条目：该控件类型不在逐控件覆盖范围内（见“已知局限”第 7 条），或控件是自绘/非 Slate 渲染，无法从显示层改写。

## 十一、版本升级 / 维护手册（面向维护者）

架构原则：**随引擎升级只换词典、不改代码；API 变更只改一个文件。**

1. **词典升级（最常见）**：新引擎界面英文文本若有变化，直接用内置的导出 commandlet 生成差异 CSV → 翻译 → 回灌即可增量补齐。
2. **代码兼容层**：所有引擎版本差异集中在 `Source/PolyglotCore/Public/PolyglotCompat.h`（文化切换 API、控件类型字符串常量、可选模块探测、宏别名）。升级引擎后如遇编译错误，**先改这一个文件**。
3. **专项适配器全部运行时探测**：`PolyglotAdapters` 模块对 Niagara / 材质 / 蓝图 / 内容浏览器模块用 `IsModuleLoaded` + 运行时加载探测，目标模块在任何版本缺失都会自动跳过，不会导致崩溃。
4. **控件改写类型清单**集中在 Compat 层用 `SWidget::GetType()` 字符串匹配，不依赖具体 Slate 类型名做编译期绑定，降低跨版本漂移风险。
5. **日志开关**：`LogPolyglot` 默认 Verbose 级关闭；排查时在命令行加 `-LogCmds="LogPolyglot Verbose"` 观察全量命中。

## 十二、源码结构与开发者构建验证

```
Plugins/Polyglot/
├── Polyglot.uplugin                全部模块 Type=Editor，FriendlyName=Easy language，Icon=Resources/Icon128.png
├── Resources/Icon128.png           插件图标（插件管理器/Tools 菜单/顶部工具栏共用）
├── Content/Dictionaries/*.json     内置词典
├── Source/PolyglotCore             词典、注入层、文化层、Slate 扫描层、补丁器、总控 Subsystem、兼容层
├── Source/PolyglotUI               双语界面助手(UiText)、主控面板、词典编辑器(批量 Add/Import TXT)、拾取模式、快捷键、工具栏按钮与菜单
├── Source/PolyglotAdapters         Niagara/材质/蓝图/内容浏览器专项显示层适配（运行时探测）
├── Source/PolyglotCommandlet       未翻译词条导出 commandlet
└── Docs/README.md                  本文档
```

无需手工建工程即可编译验证整个插件（Epic 安装版自带 RunUAT）：

```
"<引擎>/Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin ^
    -Plugin="<本插件>/Polyglot.uplugin" -Package="<输出目录>" -TargetPlatforms=Win64
```

> 注意：插件未改动任何引擎文件，因此可同时安装多个引擎版本共享同一份 `Plugins/Polyglot`（每个版本各自编译各自的二进制）。
