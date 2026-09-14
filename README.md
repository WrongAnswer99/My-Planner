# My Plan

My Plan 是一个面向 Windows 的本地规划日志应用，提供 SFML 图形界面和命令行工具。两者共用同一套独立于 GUI 的存储核心与 JSON 数据格式；CLI 的输入、输出和数据文件均使用 UTF-8，支持直接传入中文。

主要功能包括：

- 创建、编辑、完成归档、恢复和永久删除大目标。
- 为大目标和小目标设置 ISO 8601 截止时间。
- 添加、编辑和删除进度、备注及小目标。
- 将已完成的小目标保留为历史记录，并关联自动生成的进度。
- 在 GUI 中筛选归档状态、排序小目标，并分别滚动目标、子目标和日志列表。
- 通过 GUI 与 CLI 操作同一份本地 JSON 数据。

## 构建准备

需要 Windows、CMake 3.20 或更高版本，以及支持 C++20 的 MinGW `g++`。`cmake`、`g++` 和 `git` 需要已加入 `PATH`。

项目不会提交 `thirdparty/`，首次构建前需要准备 SFML 3.0.0 和 nlohmann/json 3.11.3：

```powershell
git clone --depth 1 --branch 3.0.0 https://github.com/SFML/SFML.git thirdparty/SFML
git clone --depth 1 --branch v3.11.3 https://github.com/nlohmann/json.git thirdparty/json
```

GUI 还需要字体文件 `resources/fonts/default.TTF`。字体目录不会提交到 Git，需要在本地提供；发布打包时脚本会将该字体复制到 `dist/resources/fonts/`。

## 使用 g++ 构建

在项目根目录执行：

```powershell
cmake --fresh -S . -B build -G "MinGW Makefiles" -DCMAKE_CXX_COMPILER=g++
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

`--fresh` 会重新生成 `build/` 中的 CMake 配置，适合该目录以前使用过其他生成器的情况。全量构建会同时生成正式程序和核心测试。最终程序位于：

```text
build/myplan-cli.exe
build/myplan-gui.exe
```

以后代码有变化时，一般只需重新执行构建和测试，不必再次配置：

```powershell
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

`myplan-gui` 使用 GUI/SFML 实现，在 Windows 上启动时不会弹出控制台；`myplan-cli` 本身不依赖或链接 SFML。构建 GUI 后，字体和图片资源会自动复制到 `build/resources/`。

## 打包和使用 dist

执行根目录下的打包脚本：

```powershell
.\package-dist.ps1
```

脚本默认先构建 `myplan-gui` 和 `myplan-cli`，然后刷新 `dist/` 中的程序、`README.md`、资源和 MinGW 运行库。可选参数：

- `-SkipBuild`：使用当前 `build/` 产物，不重新构建。
- `-BuildDirectory PATH`：指定构建目录。
- `-DestinationDirectory PATH`：指定输出目录。

如果 `build/myplan-data.json` 存在且目标目录中还没有数据文件，脚本会将它移动到目标目录；若目标目录已有 `myplan-data.json`，脚本会保留两边的数据并给出警告，不会覆盖现有数据。

发布包可直接运行：

```powershell
.\dist\myplan-gui.exe
.\dist\myplan-cli.exe --pretty target list
```

两个程序默认读写自身所在目录中的 `myplan-data.json`，因此从 `dist/` 运行时会自然共用 `dist/myplan-data.json`。

## GUI 版

GUI 窗口默认为 1280×800，最小尺寸为 980×640，并支持调整大小。从项目根目录启动构建产物：

```powershell
.\build\myplan-gui.exe
```

未指定数据路径时，GUI 和 CLI 都使用自身 exe 所在目录下的 `myplan-data.json`。因此从 `build/` 启动时默认使用 `build/myplan-data.json`，从 `dist/` 启动时默认使用 `dist/myplan-data.json`。

指定数据文件：

```powershell
.\build\myplan-gui.exe --data .\data\my-plan.json
```

GUI 也支持通过 `MYPLAN_DATA` 环境变量指定数据文件；显式传入的 `--data` 优先级更高。

如果希望 GUI 和 CLI 操作同一份数据，两个程序必须传入相同的 `--data` 路径：

```powershell
.\build\myplan-gui.exe --data .\data\my-plan.json
.\build\myplan-cli.exe --data .\data\my-plan.json --pretty target list
```

界面布局与操作：

- 左栏顶部可以输入标题并创建大目标；“刷新”会重新读取数据文件。创建区域下方的“进行中”和“已归档”标签用于切换两类目标。
- 左栏目标卡片显示大目标标题、截止时间和最近 3 条进度/备注。点击卡片可以切换当前大目标。
- 进行中目标的右侧顶部显示“完成并归档”和“编辑”。已归档目标会显示“已完成并归档”状态文字，以及蓝色“恢复”、蓝色“编辑”和红色“永久删除”；恢复后目标回到“进行中”并清除完成/归档时间。永久删除只允许用于已归档目标，并需要连续点击两次确认。标题通过“编辑”修改；截止时间使用单独的“保存截止时间”按钮，输入留空即可清除。
- 右侧中间分为上下两个独立的滚动栏目：上方显示小目标，下方显示进度和备注；小目标默认按截止日期升序，日志保持最新内容在前。“小目标”和“全部日志”标题固定在滚动列表外，不会随内容滚动。
- “小目标”标题右侧可选择按截止日期或按创建日期排序，并可切换升序/降序。按截止日期排序时，没有截止日期的小目标会改用其创建日期作为排序值；排序只影响 GUI 显示，不修改数据文件中的顺序。
- 选择“添加目标”时，大输入框上方会显示小目标截止时间输入框；进度和备注模式下不会显示。截止时间采用 ISO 8601 格式，也可以留空。
- 活跃小目标右侧有“完成”按钮。点击后会生成一条同名进度；小目标保留为已完成状态并显示完成 UTC 时间，其截止时间会被丢弃。
- 每条小目标和日志右侧都有“编辑”和红色“删除”按钮。编辑进行中的小目标时，可同时修改标题和截止时间，把截止时间留空即可清除；已完成的小目标只能修改标题。点击“编辑”后按钮变为“确定”，再次点击即可保存。
- 删除采用二次确认：第一次点击红色“删除”后按钮变为“确定”，再次点击才真正删除。切换到其他操作会取消当前确认状态。
- 右侧底部可选择“添加目标”“添加进度”或“添加备注”，在大输入框中填写内容后点击“提交”。这里的“添加目标”是给当前大目标添加一个小目标。
- 目标或日志很多时，可在左侧目标列表或右侧日志区域使用鼠标滚轮，也可以拖动滚动。

GUI 和 CLI 共享同一套 JSON 数据格式。GUI 打开期间如果通过 CLI 修改了数据，可以点击左栏的“刷新”载入最新内容。当前版本仍应避免两个进程同时写入同一数据文件。

## 基本调用形式

在项目根目录运行：

```powershell
.\build\myplan-cli.exe [--data FILE] [--pretty] COMMAND
```

全局选项：

- `--data FILE`：指定数据文件。省略时使用 exe 所在目录下的 `myplan-data.json`，即构建产物使用 `build/myplan-data.json`，发布包使用 `dist/myplan-data.json`。
- `--pretty`：缩进输出 JSON，便于人阅读；省略时输出单行 JSON，更适合程序处理。
- `--help` 或 `-h`：显示命令帮助。

也可以用环境变量设置数据文件：

```powershell
$env:MYPLAN_DATA = "F:\data\my-plan.json"
.\build\myplan-cli.exe target list
```

命令行中的标题、进度和备注如果包含空格，应放在引号中。

## 1. 新建大目标

只设置标题：

```powershell
.\build\myplan-cli.exe --pretty target create "发布 1.0"
```

创建时同时设置截止时间：

```powershell
.\build\myplan-cli.exe --pretty target create "发布 1.0" --deadline "2026-12-31"
```

成功结果中的 `result.id` 是后续操作使用的大目标 ID，例如：

```json
{
  "action": "target.create",
  "ok": true,
  "result": {
    "id": "target_a65563cb98ed",
    "title": "发布 1.0",
    "created_at": "2026-09-13T09:37:30.614Z",
    "deadline": "2026-12-31",
    "status": "active",
    "completed_at": null,
    "archived_at": null,
    "log": [],
    "subtargets": []
  }
}
```

编辑大目标标题：

```powershell
.\build\myplan-cli.exe target edit target_a65563cb98ed "发布 2.0"
```

完成并归档大目标：

```powershell
.\build\myplan-cli.exe target complete target_a65563cb98ed
```

该命令会把 `status` 设为 `archived`，并用同一个 UTC 时间同时写入 `completed_at` 和 `archived_at`。归档不会删除目标。旧数据中没有这些字段的目标会按 `active` 读取；旧版本产生的 `completed` 状态会在读取时作为已归档目标处理。

恢复已归档目标：

```powershell
.\build\myplan-cli.exe target restore target_a65563cb98ed
```

恢复会把状态改回 `active`，并清除 `completed_at` 和 `archived_at`。

永久删除已归档目标：

```powershell
.\build\myplan-cli.exe target delete target_a65563cb98ed
```

CLI 会拒绝删除仍在进行中的目标。CLI 命令本身不提供交互确认，调用方应在执行前自行确认；GUI 使用两次点击确认。

## 2. 查询目标

列出全部大目标：

```powershell
.\build\myplan-cli.exe --pretty target list
```

查看一个大目标的完整日志和小目标：

```powershell
.\build\myplan-cli.exe --pretty target show target_a65563cb98ed
```

## 3. 设置或清除截止时间

设置截止日期：

```powershell
.\build\myplan-cli.exe target deadline target_a65563cb98ed "2026-12-31"
```

也可以指定时间和时区：

```powershell
.\build\myplan-cli.exe target deadline target_a65563cb98ed "2026-12-31T18:00:00+08:00"
```

清除截止时间：

```powershell
.\build\myplan-cli.exe target deadline target_a65563cb98ed none
```

截止时间使用 ISO 8601 格式，支持 `YYYY-MM-DD` 或日期时间形式。

## 4. 写入进度

```powershell
.\build\myplan-cli.exe progress add target_a65563cb98ed "完成 CLI 原型"
```

生成的日志类型为 `progress`，`created_at` 会自动记录当前 UTC 时间。

## 5. 写入备注

```powershell
.\build\myplan-cli.exe note add target_a65563cb98ed "GUI 稍后再接入"
```

生成的日志类型为 `note`，`created_at` 同样会自动记录当前 UTC 时间。

## 6. 添加小目标

```powershell
.\build\myplan-cli.exe --pretty subtarget add target_a65563cb98ed "补充测试"
```

添加时设置截止时间：

```powershell
.\build\myplan-cli.exe --pretty subtarget add target_a65563cb98ed "补充测试" --deadline "2026-12-20"
```

成功结果中的 `result.id` 是小目标 ID，例如 `subtarget_510130c88dcf`。

## 7. 把小目标变为进度

直接使用小目标标题作为进度内容：

```powershell
.\build\myplan-cli.exe subtarget promote target_a65563cb98ed subtarget_510130c88dcf
```

也可以在末尾传入新的进度文字：

```powershell
.\build\myplan-cli.exe subtarget promote target_a65563cb98ed subtarget_510130c88dcf "测试已全部通过"
```

`subtarget complete` 是 `subtarget promote` 的等价别名，也支持可选的进度文字。

转化后会发生两件事：

- 大目标的 `log` 中新增一条 `progress`。
- 小目标保留在 `subtargets` 中，但 `status` 变为 `promoted`，通过 `progress_id` 指向生成的进度，并清除原来的 `deadline`。

保留小目标记录是为了保存历史关系，并防止同一个小目标被重复转化。

## 8. 编辑或删除小目标与日志

编辑日志（进度和备注使用同一个命令）：

```powershell
.\build\myplan-cli.exe log edit target_a65563cb98ed log_a640119e4709 "修改后的内容"
```

删除日志：

```powershell
.\build\myplan-cli.exe log delete target_a65563cb98ed log_a640119e4709
```

编辑小目标：

```powershell
.\build\myplan-cli.exe subtarget edit target_a65563cb98ed subtarget_510130c88dcf "修改后的小目标"
```

编辑标题并设置或清除截止时间：

```powershell
.\build\myplan-cli.exe subtarget edit target_a65563cb98ed subtarget_510130c88dcf "修改后的小目标" --deadline "2026-12-25"
.\build\myplan-cli.exe subtarget edit target_a65563cb98ed subtarget_510130c88dcf "修改后的小目标" --deadline none
```

编辑时省略 `--deadline` 会保留原截止时间；传入 `none` 才会清除。已完成的小目标仍可改标题，但不能重新设置截止时间。

删除小目标：

```powershell
.\build\myplan-cli.exe subtarget delete target_a65563cb98ed subtarget_510130c88dcf
```

删除已完成小目标时，其生成的历史进度仍会保留，但不再引用已删除的小目标。删除由小目标生成的进度时，小目标本身及其完成时间也会保留，但会清除 `progress_id`。

## PowerShell 连续调用示例

下面的脚本会自动从 JSON 结果中取得 ID，不需要手工复制：

```powershell
$cli = ".\build\myplan-cli.exe"
$data = ".\data\my-plan.json"

$target = & $cli --data $data target create "发布 1.0" --deadline "2026-12-31" |
    ConvertFrom-Json
$targetId = $target.result.id

& $cli --data $data progress add $targetId "完成 CLI 原型"
& $cli --data $data note add $targetId "GUI 稍后再接入"

$subtarget = & $cli --data $data subtarget add $targetId "补充测试" |
    ConvertFrom-Json
$subtargetId = $subtarget.result.id

& $cli --data $data subtarget promote $targetId $subtargetId
& $cli --data $data --pretty target show $targetId
```

父目录不存在时 CLI 会自动创建，例如上面的 `data/`。

## 输出和退出码

成功结果写到标准输出：

```json
{"action":"progress.add","ok":true,"result":{"id":"log_a640119e4709"}}
```

错误结果写到标准错误：

```json
{"error":{"code":"target_not_found","message":"target not found: target_missing"},"ok":false}
```

退出码：

- `0`：命令成功或显示帮助。
- `1`：数据、文件、目标 ID 等运行时错误。
- `2`：命令拼写或参数数量错误。

调用方应先检查退出码或 `ok`，再读取 `result`。

## 数据文件

数据以格式化 JSON 保存，当前格式版本为 `1`。数据文件不存在时会被视为空计划，并在第一次写入时自动创建其父目录和文件。标题、日志文字等内容不允许为空；截止时间会校验日期、时间和 UTC 偏移是否合法。

日志及完成时间采用 UTC RFC 3339 格式，例如 `2026-09-13T09:37:30.683Z`。数据写入时会先生成临时文件并短暂备份旧文件，成功替换后删除备份，以减少写入中断造成的数据损坏。

当前版本按顺序处理 CLI 操作；不要让多个进程同时修改同一个数据文件。
