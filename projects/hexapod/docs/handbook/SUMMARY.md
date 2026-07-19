# SUMMARY — Hexapod Motion Design Handbook

完整导航大纲，供 GitBook / Docsify / AI 跳转。

---

## 入口

- [手册首页 README](README.md)
- [本导航 SUMMARY](SUMMARY.md)
- [机体总览](../robot-overview.md) · [关节](../joints.md) · [碰撞](../collisions.md)

---

## 第一部 · 思想与合同

- [第一章 运动设计思想](01_Introduction.md)
- [第一章 · 跨领域灵感地图](01_Introduction.md#110-跨领域灵感地图动作从哪里来)
- [第十一章 动作库设计](11_Motion_Library.md)
- [第十一章 · 跨领域前缀策略](11_Motion_Library.md#1112-跨领域扩展前缀与入库策略)
- [第十二章 APP设计](12_APP_Design.md)
- [第十二章 · 控件矩阵](12_APP_Design.md#1212-跨领域控件矩阵产品价值导向)
- [第十三章 AI开发规范（强制）](13_AI_Guidelines.md)
- [第十三章 · 红线](13_AI_Guidelines.md#red-lines)
- [第十三章 · 跨领域禁令补丁](13_AI_Guidelines.md#1320-跨领域实现时的-ai-禁令补丁)

---

## 第二部 · 原子动作

- [第二章 基础移动](02_Basic_Motion.md)
- [第三章 姿态动作](03_Posture.md)
- [第四章 单腿动作](04_Single_Leg.md)
- [第五章 双腿动作](05_Dual_Leg.md)

### 移动速链

- Forward / Backward / Strafe / Turn / Arc / Circle / Square / Eight
- Creep / Dash / Crab / Moonwalk / Dock / Orbit / E-Stop

### 姿态速链

- Stand / Sit / Sleep / Wake / Body Up-Down / Pitch / Roll / Shift
- Look Around / Alert / Photo / Freeze / Power

---

## 第三部 · 表演、情绪与行为

- [第六章 身体表演](06_Performance.md)
- [第七章 情绪表达](07_Emotion.md)
- [第八章 行为模式](08_Behavior.md)
- [第九章 组合动作](09_Combinations.md)

---

## 第四部 · 高级与扩展方言

- [第十章 高级动作](10_Advanced.md)
- [第十四章 人机互动与场景动作](14_Interaction_Extended.md)
- [第十五章 生物行为与游戏动作灵感](15_Bio_Game_Inspiration.md)

### 互动速链

- Handshake / Fist Bump / Follow / Peekaboo / Nod / Shake No
- Pet Response / Soothe / Photo Countdown / Voice Wake / Guide Lead

### 游戏与生物速链

- Boss Enter / Level Up / Low HP / Emote Wheel / Idle Breath
- Mantis Threat / Hermit Hide / Ant Carry / Chameleon Creep

---

## 推荐阅读路径

1. 新人建立观念：[01](01_Introduction.md)
2. 先让它站稳走稳：[03](03_Posture.md) → [02](02_Basic_Motion.md) → Stop/E-Stop
3. 做出第一次可爱：[04](04_Single_Leg.md) Wave → [09](09_Combinations.md) Greeting
4. 做产品包：[12](12_APP_Design.md) + [08](08_Behavior.md)
5. AI 实现前：[13](13_AI_Guidelines.md) + 目标条目
6. 差异化扩展：[14](14_Interaction_Extended.md) + [15](15_Bio_Game_Inspiration.md) + [10](10_Advanced.md)

---

## 章节邻接

| 章 | 上一 | 下一 |
|----|------|------|
| 01 | README | [02](02_Basic_Motion.md) |
| 02 | [01](01_Introduction.md) | [03](03_Posture.md) |
| 03 | [02](02_Basic_Motion.md) | [04](04_Single_Leg.md) |
| 04 | [03](03_Posture.md) | [05](05_Dual_Leg.md) |
| 05 | [04](04_Single_Leg.md) | [06](06_Performance.md) |
| 06 | [05](05_Dual_Leg.md) | [07](07_Emotion.md) |
| 07 | [06](06_Performance.md) | [08](08_Behavior.md) |
| 08 | [07](07_Emotion.md) | [09](09_Combinations.md) |
| 09 | [08](08_Behavior.md) | [10](10_Advanced.md) |
| 10 | [09](09_Combinations.md) | [11](11_Motion_Library.md) |
| 11 | [10](10_Advanced.md) | [12](12_APP_Design.md) |
| 12 | [11](11_Motion_Library.md) | [13](13_AI_Guidelines.md) |
| 13 | [12](12_APP_Design.md) | [14](14_Interaction_Extended.md) |
| 14 | [13](13_AI_Guidelines.md) | [15](15_Bio_Game_Inspiration.md) |
| 15 | [14](14_Interaction_Extended.md) | [README](README.md) |

---

## 导航

- [README](README.md)
- [01](01_Introduction.md) · [02](02_Basic_Motion.md) · [03](03_Posture.md) · [04](04_Single_Leg.md) · [05](05_Dual_Leg.md)
- [06](06_Performance.md) · [07](07_Emotion.md) · [08](08_Behavior.md) · [09](09_Combinations.md) · [10](10_Advanced.md)
- [11](11_Motion_Library.md) · [12](12_APP_Design.md) · [13](13_AI_Guidelines.md) · [14](14_Interaction_Extended.md) · [15](15_Bio_Game_Inspiration.md)
