# 第十一章 动作库设计

本章规定 Hexapod 动作库的**命名、标识、分类、参数、目录、配置格式、版本与审核**。动作库是 Motion 层之上的「可调度资产」，不是舵机寄存器操作手册。所有可被 APP、语音、脚本、AI 编排调用的行为，都必须先进入动作库，再经 Motion 层执行。

> **机器人前提（全文一致）**  
> 六足 18 DOF；每腿 J1 / J2 / J3；舵机 MG996R；Home = J1 90° / J2 180° / J3 125°；坐标系 +X 前、+Y 左、+Z 上；腿号 1～6（左前→逆时针→右前）。

---

## 11.1 设计目标

动作库要同时满足五类读者：

1. **固件 / Motion 实现者**：能从 MotionID 一眼判断语义与安全等级。  
2. **APP / 交互设计者**：能按分类摆按钮、摇杆、自动播放列表。  
3. **展厅与儿童场景运营者**：能区分「可公开按钮」与「需确认 / 禁用」。  
4. **AI 代码助手**：能按前缀与参数规范生成调用，而不是发明新的底层写角接口。  
5. **审核与维护者**：能按清单验收版本、兼容性与文档对应关系。

因此，动作库不是「函数名随便起」的代码堆，而是一套**可版本化、可审核、可中断、可参数化**的资产体系。

---

## 11.2 如何命名

### 11.2.1 人类可读名（Display Name）

- 使用简体中文短名，优先动宾或状态名词：`前进`、`三角步态`、`打招呼`、`展厅待机`。  
- 避免内部缩写出现在 UI：不要写 `TROT_V2` 给儿童模式。  
- 同一语义只保留一个主名；别名放在配置的 `aliases` 字段，供语音与搜索。  
- 长度建议：中文 2～8 字；英文副标题可选，仅用于开发面板。

### 11.2.2 工程标识（MotionID）

MotionID 是唯一主键，全大写蛇形，带强制前缀（见 11.3）。规则：

| 规则 | 说明 | 正例 | 反例 |
|------|------|------|------|
| 前缀必填 | 必须属于约定前缀之一 | `MOVE_FORWARD` | `FORWARD` |
| 语义具体 | 动词/姿态/对象清晰 | `EMO_WAVE_RIGHT` | `EMO_DO` |
| 不写硬件 | 不出现通道、板卡、PWM | `POSTURE_HOME` | `PCA_CH3_90` |
| 不写裸角 | 角度进参数或标定表 | `LEG_LIFT` | `J2_TO_120` |
| 版本外置 | 大版本用库版本，不塞进 ID | `MOVE_TRIPOD` | `MOVE_TRIPOD_V3` |
| 稳定不变 | ID 发布后不改语义 | — | 改 ID 却保留旧含义 |

### 11.2.3 文件名与类名

- 配置文件：`snake_case` + 前缀语义，如 `move_forward.json`。  
- 实现模块：可按分类分包，如 `motions/move/`、`motions/emo/`。  
- 禁止用「临时」「test2」「new」作为入库名；试验品放 `sandbox/` 且不得进入正式目录索引。

### 11.2.4 命名决策流程

1. 先判断**分类前缀**（走？姿态？单腿？表演？情绪？行为？组合？进阶？）。  
2. 再写**核心语义词**（FORWARD / SIT / WAVE…）。  
3. 若需区分左右或方向，用后缀：`_LEFT` / `_RIGHT` / `_CW` / `_CCW`。  
4. 若同一语义有强度变体，**不要**拆成多个 ID；用 `intensity` 参数。  
5. 若同一语义有完全不同轨迹策略，才允许并列 ID，并在文档写清互斥与选用条件。

---

## 11.3 MotionID 规范

<a id="motionid-prefixes"></a>

### 11.3.1 前缀一览（强制）

| 前缀 | 含义 | 典型用途 | 默认 interruptible |
|------|------|----------|--------------------|
| `MOVE_` | 位移 / 步态 / 转向 | 前进、后退、横移、原地转 | true |
| `POSTURE_` | 全身静态或准静态姿态 | Home、蹲、高站、展厅待机 | true（过渡中） |
| `LEG_` | 单腿操作 | 抬腿、点地、伸展调试 | true |
| `DUAL_` | 双腿协同 | 左右前腿挥手、对称拉伸 | true |
| `PERF_` | 表演编排 | 开场、谢幕、节奏秀 | 视条目；默认可中断 |
| `EMO_` | 情绪表达 | 点头、摇头、兴奋抖动 | true |
| `BEH_` | 行为逻辑 | 寻声转向、避障侧移、跟随准备 | true |
| `COMBO_` | 组合动作 | 多段串联/并列的用户或策划组合 | true |
| `ADV_` | 进阶 / 高风险 / 实验毕业项 | 高速、大振幅、特技预备 | **必须显式声明** |

### 11.3.2 前缀选用细则

**`MOVE_`**  
凡「机体在水平面产生位移或朝向变化」的周期步态与转向，一律 `MOVE_`。即使动作里含抬腿，只要主目标是移动，不用 `LEG_`。

**`POSTURE_`**  
目标是到达并保持某一全身形态；过渡时间由 `duration` / `speed` 控制。Home、校准姿态、展厅「雕塑感」待机属于此类。

**`LEG_` / `DUAL_`**  
调试、教学、局部表演用。`LEG_` 必须带 `leg_id`（或配置默认腿）。`DUAL_` 必须声明腿对（如 1+6）。禁止用 `LEG_` 伪装全身步态。

**`PERF_`**  
面向观众时间轴：可含多次 `MOVE_`/`EMO_` 语义，但对外仍是一个可调度 ID。内部应引用子动作，而不是复制粘贴轨迹。

**`EMO_`**  
短时表达，通常不改变长期站位基准；结束后应回到进入前姿态或明确的结束姿态。

**`BEH_`**  
带感知或策略分支的行为。库条目描述的是「行为入口与安全边界」，细节策略可另文；但对外仍只有一个 MotionID。

**`COMBO_`**  
用户收藏组合、策划片单。ID 可稳定（官方组合）或运行时生成（用户组合用独立命名空间，见 11.8）。

**`ADV_`**  
任何超出日常安全包络、需额外确认、或依赖特殊场地的动作。APP 不得在儿童模式展示；展厅模式默认隐藏或需双确认。

### 11.3.3 MotionID 字符集

```text
^[A-Z]+_[A-Z0-9]+(_[A-Z0-9]+)*$
```

- 仅大写字母、数字、下划线。  
- 第一段为前缀（含尾下划线语义上的前缀词）。  
- 禁止连续双下划线、禁止以下划线结尾。

### 11.3.4 保留与废弃

- 废弃 ID：进入 `deprecated` 列表，保留至少两个大版本；调用时告警并映射到替代 ID。  
- 禁止复用已废弃 ID 给新语义。  
- `MOVE_TEST`、`EMO_TMP` 等不得入库正式索引。

---

## 11.4 分类规范

<a id="motion-categories"></a>

### 11.4.1 一级分类（与前缀对齐）

| 分类键 | 前缀 | UI 默认区 | 说明 |
|--------|------|-----------|------|
| locomotion | `MOVE_` | 摇杆 / 方向键 | 连续控制友好 |
| posture | `POSTURE_` | 姿态条 | 一键到位 |
| single_leg | `LEG_` | 调试页 | 需选腿 |
| dual_leg | `DUAL_` | 调试 / 表演 | 需选腿对 |
| performance | `PERF_` | 自动播放 / 展厅 | 偏时间轴 |
| emotion | `EMO_` | 表情面板 | 短触发 |
| behavior | `BEH_` | 智能模式 | 可长生命周期 |
| combo | `COMBO_` | 收藏与编辑器 | 组合 |
| advanced | `ADV_` | 开发者 / 确认门 | 高门槛 |

### 11.4.2 二级标签（多选）

二级标签用于检索与策略，不改变前缀：

- `grounded` / `aerial_hint`：是否假设足端触地策略（本机无飞行，aerial_hint 仅表示大幅抬腿表演）。  
- `cyclic` / `oneshot`：周期步态 vs 一次性动作。  
- `needs_space`：需要净空（展厅模式可提示）。  
- `child_safe` / `exhibit_safe` / `dev_only`：场景可用性。  
- `voice_enabled`：是否进入默认语音词表。  
- `records_well`：是否适合录制分享（轨迹可理解、不过度依赖传感器偶然性）。

### 11.4.3 与章节动作的对应关系

本手册前十章（步态、姿态、单腿、表演、情绪、行为等）中的「动作条目」，入库时必须满足：

| 手册章节主题（概念） | 入库前缀 | 备注 |
|----------------------|----------|------|
| 基本移动与步态 | `MOVE_` | 三角、波浪、转向等 |
| 站立 / Home / 蹲伏 | `POSTURE_` | 含过渡 |
| 单腿示教与标定辅助 | `LEG_` | 强制 `leg_id` |
| 对称双腿表演 | `DUAL_` | 声明腿对 |
| 展陈节目段 | `PERF_` | 可嵌套引用 |
| 拟人 / 拟宠情绪 | `EMO_` | 短时 |
| 感知驱动行为 | `BEH_` | 写明依赖传感器假设 |
| 用户或策划串联 | `COMBO_` | 子动作列表 |
| 特技与极限 | `ADV_` | 双确认 + 安全包络 |

**对应关系原则**：手册讲解「为什么这样动」；动作库沉淀「如何被调用」。二者通过 MotionID 互链，而不是靠口头别名。

---

## 11.5 参数规范

<a id="motion-parameters"></a>

所有入库动作在调度接口上视为：

```text
play(motion_id, params) → handle
```

下列参数为**跨动作通用约定**。某动作不使用的参数应忽略并打 debug 日志，不得抛致命错误（除标记为 required 的参数缺失）。

### 11.5.1 `speed`（必须支持）

| 项 | 约定 |
|----|------|
| 类型 | 无量纲或枚举归一化，推荐 `0.0～1.0`，另可用档位 `slow/normal/fast` 映射到同一内部标量 |
| 语义 | 轨迹时间缩放：越大越快；**不是** PWM 脉宽 |
| 默认 | `0.5` 或配置 `default_speed` |
| 约束 | 实现必须支持；儿童模式可强制上限（如 ≤ 0.4） |
| 与 duration | 若同时给出，以动作声明的 `timing_policy` 为准：`prefer_speed` 或 `prefer_duration` |

### 11.5.2 `intensity`（强烈建议）

- 范围推荐 `0.0～1.0`。  
- 控制振幅、抬腿高度、摇摆角度包络等「表现强度」，与 `speed` 正交：可以慢而猛，也可以快而轻。  
- `EMO_` / `PERF_` 几乎都应支持；纯位移 `MOVE_` 可将 intensity 映射为步高或步长系数。

### 11.5.3 `leg_id` / `leg_ids`

- `LEG_`：`leg_id` ∈ {1,2,3,4,5,6} 必填（除非配置写死默认且文档标明）。  
- `DUAL_`：`leg_ids` 长度为 2，且应符合对称或文档允许的腿对。  
- 非法腿号必须拒绝执行并返回明确错误，不得静默夹到 1。

### 11.5.4 `duration`

- 单位：毫秒（整数）或秒（浮点），库内统一一种并在 schema 声明。推荐毫秒整数。  
- 用于 oneshot 姿态过渡、表演片段时长。  
- `loop=true` 时，`duration` 可表示「单次循环时长」或「总播放时长」，必须在条目的 `duration_meaning` 字段写清。

### 11.5.5 `loop`

- `false`：播放一次，进入完成态。  
- `true`：循环，直到 stop / 中断 / 达到 `loop_count`。  
- `MOVE_` 连续行走常为 `loop=true`；`EMO_` 默认 `false`。

### 11.5.6 `interruptible`（必须支持语义）

- 动作必须能被更高优先级指令打断（急停永远最高，见第十二、十三章）。  
- 配置可声明 `interruptible: false` **仅用于极短且已论证的原子段**（例如 80ms 内的支撑切换临界），并在审核清单单独勾选。  
- 即便 `interruptible: false`，**急停与安全故障仍必须能打断**。

### 11.5.7 暂停与恢复（必须支持语义）

动作库条目不直接实现驱动，但必须声明：

- `supports_pause: true`（强制期望为 true）  
- 暂停时：冻结逻辑时间，保持当前安全姿态或进入定义的 pause posture。  
- 恢复时：从暂停点继续，而不是从头硬切（除非 `resume_policy: restart`）。

### 11.5.8 其他推荐参数

| 参数 | 用途 |
|------|------|
| `heading` / `direction` | 移动方向（机体坐标或世界坐标，条目需声明） |
| `radius` | 弧线行走 |
| `style` | 步态风格枚举（如 `tripod` / `wave`），仅当同一 ID 允许多风格时使用；否则应拆 ID |
| `enter_check` / `exit_check` | 布尔，默认 true；调试可临时关闭但不得进儿童/展厅包 |
| `priority` | 调度优先级提示 |
| `tags` | 二级标签数组 |

### 11.5.9 参数设计禁忌

- 禁止参数名叫 `pwm`、`pulse`、`channel`、`pca9685`。  
- 禁止把 18 路裸角度数组作为常规 APP 参数暴露（开发者标定页除外，且仍应走 Motion/标定 API）。  
- 禁止用布尔海（`fast`、`faster`、`very_fast`）代替 `speed`。

---

## 11.6 推荐目录结构

<a id="directory-structure"></a>

```text
motions/
  catalog.json              # 总索引：id → 文件、分类、安全标记、版本
  schema/
    motion.schema.json      # 条目 JSON Schema
    params.schema.json
  move/
    move_forward.json
    move_turn_left.json
    ...
  posture/
    posture_home.json
    posture_crouch.json
  leg/
    leg_lift.json
  dual/
    dual_front_wave.json
  perf/
    perf_opening.json
  emo/
    emo_nod.json
  beh/
    beh_idle_look.json
  combo/
    combo_demo_loop.json
  adv/
    README.md               # 进阶区说明与双确认要求
    adv_example.json
  sandbox/                  # 未审核，不进 catalog 正式区
  deprecated/
    mappings.json           # 旧 ID → 新 ID
  i18n/
    zh-CN.json
    en.json
```

说明：

- **正式可调用** = 出现在 `catalog.json` 且 `status: released`。  
- `sandbox/` 可供 AI 试验，但 CI 应阻止误引用。  
- 大二进制轨迹（若有）放 `motions/assets/`，JSON 仅存引用路径。

---

## 11.7 推荐 JSON 格式（设计规范示例）

<a id="json-format"></a>

以下是**设计规范示例配置**，说明字段应如何表达；不是对外 REST 文档口吻，也不包含任何舵机底层写入。

### 11.7.1 单条目示例：`MOVE_FORWARD`

```json
{
  "motion_id": "MOVE_FORWARD",
  "display_name": "前进",
  "aliases": ["往前走", "向前"],
  "category": "locomotion",
  "prefix": "MOVE_",
  "version": "1.2.0",
  "status": "released",
  "tags": ["cyclic", "grounded", "child_safe", "exhibit_safe", "voice_enabled", "records_well"],
  "description": "机体沿 +X 三角步态前进。速度由 speed 缩放步频，intensity 缩放步长与步高。",
  "handbook_ref": "chapters/gait-basics#tripod-forward",
  "params": {
    "speed": { "type": "float", "min": 0.05, "max": 1.0, "default": 0.45 },
    "intensity": { "type": "float", "min": 0.0, "max": 1.0, "default": 0.5 },
    "loop": { "type": "bool", "default": true },
    "duration": { "type": "int_ms", "optional": true, "duration_meaning": "total_play_time" },
    "direction": { "type": "enum", "values": ["body_+X"], "default": "body_+X" }
  },
  "capabilities": {
    "interruptible": true,
    "supports_pause": true,
    "supports_resume": true,
    "supports_speed": true,
    "enter_posture_check": true,
    "exit_posture_check": true,
    "com_check": true,
    "joint_limit_check": true
  },
  "safety": {
    "min_battery_hint": 0.2,
    "needs_space": false,
    "estop_abort": "immediate_freeze_then_safe_stand",
    "child_mode": "allowed_speed_cap_0.4",
    "exhibit_mode": "allowed"
  },
  "timing_policy": "prefer_speed",
  "implementation_hint": "Call Motion layer gait player; do not write joint angles in APP or AI glue code.",
  "ui": {
    "control": ["joystick_forward", "button_hold"],
    "icon": "arrow_up",
    "confirm_level": "none"
  }
}
```

### 11.7.2 姿态示例：`POSTURE_HOME`

```json
{
  "motion_id": "POSTURE_HOME",
  "display_name": "Home 静止",
  "category": "posture",
  "prefix": "POSTURE_",
  "version": "1.0.0",
  "status": "released",
  "tags": ["oneshot", "child_safe", "exhibit_safe"],
  "description": "过渡到 Home：J1=90、J2=180、J3=125（标称，可被分腿标定覆盖）。此姿态为静止参考，非触地站立展陈主姿态时可再映射 POSTURE_STAND。",
  "params": {
    "speed": { "type": "float", "min": 0.05, "max": 1.0, "default": 0.35 },
    "duration": { "type": "int_ms", "optional": true },
    "loop": { "type": "bool", "default": false }
  },
  "capabilities": {
    "interruptible": true,
    "supports_pause": true,
    "supports_resume": true,
    "supports_speed": true,
    "enter_posture_check": true,
    "exit_posture_check": true,
    "com_check": true,
    "joint_limit_check": true
  },
  "ui": {
    "control": ["button"],
    "confirm_level": "none"
  }
}
```

### 11.7.3 单腿示例：`LEG_LIFT`

```json
{
  "motion_id": "LEG_LIFT",
  "display_name": "抬腿",
  "category": "single_leg",
  "prefix": "LEG_",
  "version": "1.0.1",
  "status": "released",
  "tags": ["oneshot", "dev_only"],
  "params": {
    "leg_id": { "type": "int", "min": 1, "max": 6, "required": true },
    "speed": { "type": "float", "default": 0.4 },
    "intensity": { "type": "float", "default": 0.5, "meaning": "lift_height_scale" },
    "loop": { "type": "bool", "default": false }
  },
  "capabilities": {
    "interruptible": true,
    "supports_pause": true,
    "supports_resume": true,
    "supports_speed": true,
    "enter_posture_check": true,
    "com_check": true
  },
  "safety": {
    "notes": "抬腿前检查其余支撑腿与重心投影；儿童模式默认隐藏。"
  },
  "ui": {
    "control": ["button_after_leg_picker"],
    "confirm_level": "soft"
  }
}
```

### 11.7.4 组合示例：`COMBO_DEMO_HELLO`

```json
{
  "motion_id": "COMBO_DEMO_HELLO",
  "display_name": "见面礼",
  "category": "combo",
  "prefix": "COMBO_",
  "version": "1.0.0",
  "status": "released",
  "tags": ["oneshot", "exhibit_safe", "voice_enabled"],
  "sequence": [
    { "motion_id": "POSTURE_STAND", "params": { "speed": 0.4 } },
    { "motion_id": "EMO_WAVE_RIGHT", "params": { "speed": 0.5, "intensity": 0.6 } },
    { "motion_id": "EMO_NOD", "params": { "speed": 0.45 } }
  ],
  "params": {
    "speed": {
      "type": "float",
      "default": 0.5,
      "meaning": "scales_all_children_unless_overridden"
    },
    "loop": { "type": "bool", "default": false }
  },
  "capabilities": {
    "interruptible": true,
    "supports_pause": true,
    "supports_resume": true,
    "supports_speed": true
  }
}
```

### 11.7.5 总索引片段：`catalog.json`

```json
{
  "library_version": "2026.07.0",
  "robot": {
    "dof": 18,
    "legs": 6,
    "home": { "J1": 90, "J2": 180, "J3": 125 },
    "frame": { "+X": "forward", "+Y": "left", "+Z": "up" }
  },
  "motions": [
    {
      "motion_id": "MOVE_FORWARD",
      "path": "move/move_forward.json",
      "category": "locomotion",
      "status": "released"
    },
    {
      "motion_id": "ADV_SPIN_FAST",
      "path": "adv/adv_spin_fast.json",
      "category": "advanced",
      "status": "released",
      "flags": ["needs_double_confirm", "hide_in_child_mode"]
    }
  ]
}
```

---

## 11.8 推荐配置格式（运行侧）

除 JSON 资产外，运行配置建议分层：

1. **出厂库**：随固件/应用发布的 `motions/`。  
2. **现场覆盖**：展厅可覆盖 `speed` 上限、禁用 `ADV_`、替换开场 `PERF_`。  
3. **用户库**：收藏与自定义 `COMBO_`，ID 建议 `COMBO_USER_<uuid>`，不污染官方前缀语义表。  
4. **特性开关**：`child_mode`、`exhibit_mode`、`voice_enabled` 改变可见性与参数夹取，不改变 MotionID。

配置合并顺序（后者覆盖前者）：

```text
factory catalog → site overlay → user library → session overrides
```

会话覆盖（例如本次演示把全局 speed×0.8）必须可一键清除，避免「演示结束后机器人一直偏慢」。

---

## 11.9 版本管理

<a id="versioning"></a>

### 11.9.1 双版本号

| 版本 | 作用 |
|------|------|
| `library_version` | 整库发布号，如 `2026.07.0` |
| 条目 `version` | 单动作 SemVer：`MAJOR.MINOR.PATCH` |

### 11.9.2 SemVer 语义（动作条目）

- **PATCH**：文案、标签、默认参数微调，轨迹包络不变。  
- **MINOR**：新增可选参数、扩大安全可用范围、性能优化但不破坏旧调用。  
- **MAJOR**：默认轨迹显著变化、删除参数、改变坐标系假设、结束姿态变更。

### 11.9.3 兼容策略

- APP 与 AI 应请求「能力」而非绑死小版本：`supports_pause` 等。  
- `catalog.json` 记录 `min_runtime_api`。  
- 变更结束姿态或支撑相的 MAJOR 升级，必须同步更新手册对应节与第十二章 UI 风险提示。

### 11.9.4 变更日志

每个正式动作目录旁可维护 `CHANGELOG.md` 片段，或集中在 `motions/CHANGELOG.md`。一条变更至少包含：MotionID、版本、原因、风险、迁移建议。

---

## 11.10 审核清单

<a id="review-checklist"></a>

入库或升级 `status: released` 前，审核者逐项确认：

### A. 标识与文档

- [ ] MotionID 前缀正确，字符集合法  
- [ ] `display_name` / `aliases` 无歧义  
- [ ] `handbook_ref` 指向手册有效条目  
- [ ] 与既有 ID 无语义冲突  

### B. 参数与能力

- [ ] 支持 `speed`  
- [ ] 支持中断（急停必达）  
- [ ] 支持暂停 / 恢复  
- [ ] `LEG_`/`DUAL_` 腿参数完整  
- [ ] 无 PWM/通道/魔术角散落在描述性实现说明之外的「调用方接口」  

### C. 安全

- [ ] 进入 / 退出姿态检查策略已声明  
- [ ] 关节限位与重心检查策略已声明  
- [ ] 儿童 / 展厅标志正确  
- [ ] `ADV_` 具备双确认与隐藏策略  

### D. 体验与场景

- [ ] UI `control` 建议合理（见第十二章）  
- [ ] 语音别名若启用，已加入词表候选  
- [ ] 自动播放列表适用性已评估  

### E. 工程

- [ ] 出现在 `catalog.json`  
- [ ] Schema 校验通过  
- [ ] 版本号与 CHANGELOG 已更新  
- [ ] 废弃映射（若有）已配置  

---

## 11.11 与各章动作的映射实践

<a id="chapter-mapping"></a>

建议在手册每章动作节末增加「入库卡片」四行：

1. **MotionID**  
2. **默认参数**（speed / intensity / loop）  
3. **推荐控件**（按钮 / 摇杆 / 自动播放…）  
4. **场景标志**（child_safe / exhibit_safe / dev_only）

动作库 JSON 用 `handbook_ref` 回指；形成双向链接。AI 实现某动作前，必须同时打开：手册条目 + 库 JSON + 第十三章强制规范。

### 11.11.1 映射表示例（节选）

| 概念动作 | MotionID | 控件倾向 |
|----------|----------|----------|
| 三角步态前进 | `MOVE_FORWARD` | 摇杆前推 / 按住按钮 |
| 原地左转 | `MOVE_TURN_LEFT` | 摇杆左扭 / 按钮 |
| Home | `POSTURE_HOME` | 按钮 |
| 展厅高站 | `POSTURE_EXHIBIT_STAND` | 自动播放首帧 / 按钮 |
| 左前抬腿 | `LEG_LIFT` + leg_id=1 | 调试页 |
| 双前肢挥手 | `DUAL_FRONT_WAVE` | 表情/表演按钮 |
| 开场 30 秒 | `PERF_OPENING` | 自动播放 |
| 点头 | `EMO_NOD` | 按钮 / 语音 |
| 待机张望 | `BEH_IDLE_LOOK` | 智能模式 |
| 见面礼组合 | `COMBO_DEMO_HELLO` | 分享 / 自动播放 |
| 高速自旋（若有） | `ADV_SPIN_FAST` | 确认门 |

---

## 11.12 反模式速查

| 反模式 | 为什么有害 | 正确做法 |
|--------|------------|----------|
| 把通道号写进 MotionID | 硬件换接线即全库失效 | ID 只表达语义 |
| APP 直调角度表 | 绕过限位与中断框架 | 只调 MotionID |
| 同一前进拆 10 个 ID | 词表与 UI 爆炸 | 用 speed/intensity |
| 正式区塞 sandbox | 展厅误触高风险动作 | 索引隔离 |
| 改语义不改 MAJOR | 远程编排 silently 变危险 | 严守 SemVer |

---

## 11.13 本章小结

动作库是 Hexapod 的「可调度行为合同」：前缀定分类，参数定表现，能力定安全，版本定兼容。后续 APP（第十二章）与 AI 开发（第十三章）都只消费这份合同，而不重新发明底层关节写入。

---

## 导航

- [手册首页 README](./README.md)  
- [完整导航 SUMMARY](./SUMMARY.md)  
- 上一章：第十章（行为与策略，见手册前部章节）  
- [下一章：第十二章 APP设计](./12_APP_Design.md)


---

## 11.12 跨领域扩展前缀与入库策略

当动作灵感来自四足、宠物、工业、游戏或生物行为时，不要另起一套命名宇宙。继续使用既有前缀，用**二级标签**表达方言：

| 方言标签 `dialect` | 含义 | 示例 MotionID |
|--------------------|------|----------------|
| `quadruped` | 四足巡航/身体通道 | `MOVE_FORWARD`（气质偏 Spot） |
| `pet` | 陪伴与服从 | `POSTURE_SIT`, `LEG_WAVE` |
| `industrial` | 巡检/对接/急停伦理 | `MOVE_DOCK_APPROACH`, `MOVE_ESTOP` |
| `cinematic` | 动画预备/收势夸张 | `PERF_BOW` |
| `game` | 状态机/Emote/BOSS | `GAME_BOSS_ENTER`（若新增可用 `PERF_` 或 `COMBO_`） |
| `ethology` | 生物行为可读性 | `ADV_MIMIC_CRAB`, `POSTURE_LOW_PROFILE` |
| `hci` | 对话回合身体反馈 | `HCI_NOD_YES`（建议归 `PERF_`/`EMO_`） |

入库五问（与第一章呼应）：

1. 1秒意图是否可读？
2. 方言是什么？是否与产品默认性格冲突？
3. 儿童/家庭/展厅/比赛默认是否允许？
4. 可中断点与失败姿态？
5. 70%能否用 Combo 实现？能则优先 Combo。

推荐在 JSON 增加字段：

```json
{
  "motion_id": "MOVE_CRAB_WALK",
  "dialect": ["ethology", "cinematic"],
  "product_packs": ["exhibit", "creator"],
  "child_safe": false,
  "exhibit_safe": true,
  "handbook_ref": "02_Basic_Motion.md#蟹步"
}
```

### 扩展动作与章节映射（维护表）

| 章节 | 前缀主用 | 扩展重点 |
|------|----------|----------|
| 02 | MOVE_ | 潜行/冲刺/蟹步/对接/环绕 |
| 03 | POSTURE_ | 环顾/警戒/拍照/定格 |
| 04–05 | LEG_/DUAL_ | 探针/鼓点/抱拳/镜像 |
| 06–07 | PERF_/EMO_ | 演出与情绪强度 |
| 08 | BEH_ | 陪伴/夜巡/归巢 |
| 09 | COMBO_ | 剧本 |
| 10 | ADV_ | 地形与拟态 |
| 14 | PERF_/EMO_/BEH_ | HCI互动 |
| 15 | PERF_/COMBO_/ADV_ | 游戏与生物方言 |

---

## 相关章节

- [第一章 设计思想](01_Introduction.md)
- [第十二章 APP设计](12_APP_Design.md)
- [第十三章 AI规范](13_AI_Guidelines.md)
- [第十四章 互动扩展](14_Interaction_Extended.md)
- [第十五章 生物与游戏](15_Bio_Game_Inspiration.md)
- [README](README.md) · [SUMMARY](SUMMARY.md)
