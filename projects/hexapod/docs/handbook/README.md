# Hexapod Motion Design Handbook

**Hexapod Motion Design Handbook**（六足运动设计手册）是一本面向产品、动画、运动控制与 AI 编码模型的**长期可维护设计书**。它不写驱动教程，不写 API 清单，而写：机器人应该怎么玩、有哪些动作、为何好看、如何组合、如何扩展、如何在家庭/展厅/儿童/比赛场景中负责任地释放表现力。

本手册基于仓库内真实机体参数，并主动吸收四足机器人、宠物机器人、工业巡检、动画十二法则、电子游戏状态机与生物行为学中的可用方言，再本地化到 **18 DOF + MG996R** 的约束中。

---

## 机器人摘要

| 项 | 规格 |
|----|------|
| 构型 | 蜘蛛原型六足，左右对称 |
| 自由度 | 18（6×3） |
| 关节 | J1 胯横摆 · J2 大腿俯仰 · J3 膝 |
| 舵机 | Tower Pro MG996R × 18 |
| Home | J1=90° · J2=180° · J3=125°（标定参考，**不是**触地站立） |
| 坐标 | 原点几何中心；**+X 前，+Y 左，+Z 上** |
| 腿号 | 1左前 2左中 3左后 4右后 5右中 6右前 |

机体细节：[robot-overview.md](../robot-overview.md) · [joints.md](../joints.md) · [collisions.md](../collisions.md)

---

## 这本书解决什么问题

1. **动作比算法更先被看见**：观众先判断「像不像活的」，再关心求解器。  
2. **性格是一致性策略**：速度、高度、抬腿、停顿、对中断的反应必须跨动作稳定。  
3. **扩展有方法**：从 Spot 式身体通道、宠物跟随、工业急停、游戏 Emote、昆虫威胁展示吸收灵感，但必须过滤恐惧与危险。  
4. **AI 必须守合同**：只许通过 Motion 层；禁止 PWM/PCA9685 直控；必须支持速度/中断/暂停/恢复。

---

## 读者怎么读

| 读者 | 路径 |
|------|------|
| 产品经理 | [01](01_Introduction.md) → [08](08_Behavior.md) → [09](09_Combinations.md) → [12](12_APP_Design.md) |
| 动画/动作设计 | [01](01_Introduction.md) → [03](03_Posture.md)–[07](07_Emotion.md) → [15](15_Bio_Game_Inspiration.md) |
| 运动控制工程师 | [02](02_Basic_Motion.md) → [10](10_Advanced.md) → [11](11_Motion_Library.md) → [13](13_AI_Guidelines.md) |
| AI 编码模型 | **强制** [13](13_AI_Guidelines.md) + [11](11_Motion_Library.md)，再读目标动作条目 |
| 展厅/教育运营 | [08](08_Behavior.md) → [12](12_APP_Design.md) → [14](14_Interaction_Extended.md) |

完整锚点导航见 [SUMMARY.md](SUMMARY.md)。

---

## 章节文件

| 文件 | 章 |
|------|----|
| [01_Introduction.md](01_Introduction.md) | 第一章 运动设计思想与跨领域灵感地图 |
| [02_Basic_Motion.md](02_Basic_Motion.md) | 第二章 基础移动（含潜行/冲刺/蟹步/对接/环绕等扩展） |
| [03_Posture.md](03_Posture.md) | 第三章 姿态（含环顾/警戒/拍照/定格等扩展） |
| [04_Single_Leg.md](04_Single_Leg.md) | 第四章 单腿（含探针/鼓点/嗅探/敬礼等扩展） |
| [05_Dual_Leg.md](05_Dual_Leg.md) | 第五章 双腿（含抱拳/镜像/护卫等扩展） |
| [06_Performance.md](06_Performance.md) | 第六章 身体表演 |
| [07_Emotion.md](07_Emotion.md) | 第七章 情绪表达 |
| [08_Behavior.md](08_Behavior.md) | 第八章 行为模式 |
| [09_Combinations.md](09_Combinations.md) | 第九章 组合剧本 |
| [10_Advanced.md](10_Advanced.md) | 第十章 高级地形与拟态 |
| [11_Motion_Library.md](11_Motion_Library.md) | 第十一章 动作库合同 |
| [12_APP_Design.md](12_APP_Design.md) | 第十二章 APP 与控件策略 |
| [13_AI_Guidelines.md](13_AI_Guidelines.md) | 第十三章 AI 开发规范（强制） |
| [14_Interaction_Extended.md](14_Interaction_Extended.md) | 第十四章 人机互动与场景动作 |
| [15_Bio_Game_Inspiration.md](15_Bio_Game_Inspiration.md) | 第十五章 生物行为与游戏动作灵感 |
| [SUMMARY.md](SUMMARY.md) | 导航大纲 |

---

## 维护约定

1. 新增动作：先手册条目与 MotionID，再实现；JSON 含 `dialect` / `product_packs` / `child_safe` / `handbook_ref`。  
2. 语义不兼容变更必须升 MAJOR，并同步 APP 风险提示。  
3. 第十三章红线不得静默稀释。  
4. 增删文件时同步本 README 与 SUMMARY。  
5. 不把 PWM/PCA9685 教程写入动作章。  
6. 扩展灵感时回答第一章「五问检查表」。

---

## 导航

- [SUMMARY 完整导航](SUMMARY.md)
- [第一章](01_Introduction.md) · [第十三章 AI红线](13_AI_Guidelines.md) · [第十四章 互动](14_Interaction_Extended.md) · [第十五章 生物与游戏](15_Bio_Game_Inspiration.md)
