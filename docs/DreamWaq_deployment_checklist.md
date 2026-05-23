# DreamWaq 模型导出部署 - 快速清单

## 🚀 快速开始（5分钟）

### 前提条件
- ✅ DreamWaq_train_go2 项目已训练完成
- ✅ unitree_cpp_deploy 项目已克隆
- ✅ Python 环境已配置（PyTorch, ONNX）

### 执行步骤

#### 1️⃣ 导出 PyTorch 模型
```bash
cd DreamWaq_train_go2

# 编辑 legged_gym/scripts/play.py
# 设置 EXPORT_POLICY = True

python legged_gym/scripts/play.py --task go2
```

**预期输出：** `legged_gym/logs/rough_go2/exported/policies/policy_dwaq.pt`

#### 2️⃣ 自动化部署（推荐）
```bash
cd unitree_cpp_deploy
bash deploy_dreamwaq.sh
```

#### 或者 3️⃣ 手动步骤
```bash
cd unitree_cpp_deploy

# 转换到 ONNX
python convert_dreamwaq_to_onnx.py \
    ../DreamWaq_train_go2/legged_gym/logs/rough_go2/exported/policies/policy_dwaq.pt

# 输出到：logs/go2/go2_dreamwaq/exported/policy.onnx
# 输出到：logs/go2/go2_dreamwaq/params/deploy.yaml
```

#### 4️⃣ 更新 FSM 配置
编辑 `deploy/robots/go2/config/config.yaml`：

```yaml
FSM:
  _:
    Passive:
      id: 1
    FixStand:
      id: 2
    DreamWaq_Velocity:        # ← 新增
      id: 8
      type: RLBase

  DreamWaq_Velocity:          # ← 新增
    transitions:
      Passive: LT + B.on_pressed
    policy_dir: ../../../logs/go2/go2_dreamwaq
    logging: false
    logging_dt: 0.01
```

#### 5️⃣ 编译
```bash
cd deploy/robots/go2
mkdir build && cd build
cmake ..
make -j4
```

#### 6️⃣ 部署到实机
```bash
# 从 unitree_cpp_deploy 目录
scp -r logs/go2/go2_dreamwaq unitree@<robot_ip>:/home/unitree/logs/go2/
```

#### 7️⃣ 在实机上运行
```bash
# 在机器人上执行编译后的程序
./go2 config/config.yaml
```

---

## 📁 最终文件结构

```
unitree_cpp_deploy/
├── logs/
│   └── go2/
│       ├── go2_moe_cts_137k_0.6713/      (已有 MOE 模型)
│       │   ├── exported/
│       │   │   └── policy.onnx
│       │   └── params/
│       │       └── deploy.yaml
│       │
│       └── go2_dreamwaq/                 (新增 DreamWaq 模型)
│           ├── exported/
│           │   └── policy.onnx
│           └── params/
│               └── deploy.yaml
│
├── deploy/
│   └── robots/go2/
│       ├── config/
│       │   └── config.yaml              (已更新)
│       └── build/
│           └── go2                      (编译输出)
│
├── docs/
│   └── DreamWaq_deployment_guide_zh.md  (详细指南)
│
├── convert_dreamwaq_to_onnx.py          (ONNX 转换脚本)
└── deploy_dreamwaq.sh                   (自动化脚本)
```

---

## ⚙️ 关键参数一览

| 参数 | DreamWaq | 单位/说明 |
|-----|---------|---------|
| 观测维度 | 45 | num_observations |
| 历史长度 | 6 | num_obs_hist |
| 动作维度 | 12 | num_actions (Go2 的 12 个关节) |
| 控制周期 | 0.02 | step_dt (50 Hz) |
| 关节刚度 | 28.0 | N·m/rad |
| 关节阻尼 | 0.7 | N·m·s/rad |
| 动作缩放 | 0.25 | action_scale |

---

## ✅ 验证检查

部署完成后，验证以下项目：

- [ ] `policy.onnx` 文件存在且大小 > 1 MB
- [ ] `deploy.yaml` 文件格式正确（可用 YAML 验证工具检查）
- [ ] `config.yaml` 中成功添加了 DreamWaq_Velocity 状态
- [ ] 编译成功，无报错
- [ ] 可通过遥控器切换到 DreamWaq 状态（按 LT + 上/下/左/右）

---

## 🔧 故障排除

| 问题 | 原因 | 解决方案 |
|-----|------|--------|
| ONNX 转换失败 | PyTorch 版本不匹配 | 使用与训练相同的 PyTorch 版本 |
| 模型输出不对 | 观测维度错误 | 检查 deploy.yaml 中 observations 总和 = 270 |
| 编译错误 | C++ 编译器缺失 | 安装 CMake、g++ 等工具 |
| 机器人抖动 | 控制参数不对 | 确保 stiffness/damping 与训练配置一致 |
| 部署不成功 | 文件路径错误 | 检查 policy_dir 路径是否正确 |

---

## 📚 详细文档

- 完整部署指南：[DreamWaq_deployment_guide_zh.md](./docs/DreamWaq_deployment_guide_zh.md)
- DreamWaq 项目说明：[DreamWaq_train_go2/readme.md](../DreamWaq_train_go2/readme.md)
- Unitree C++ 部署文档：[unitree_cpp_deploy/README.md](./README.md)

---

## 💡 常见问题

**Q: 可以用 DreamWaq 替换现有的 MOE 模型吗？**
A: 可以，在 config.yaml 中修改 policy_dir 或同时配置两个模型进行对比。

**Q: DreamWaq 模型与 MOE 模型有什么区别？**
A: DreamWaq 使用 VAE 进行地形想象，对不规则地形的适应能力更强；MOE 是直接策略网络。

**Q: 需要重新训练吗？**
A: 不需要，可以直接使用已训练的模型。若要针对特定场景微调，才需要重新训练。

**Q: 支持其他机器人吗？**
A: 本指南针对 Go2。其他机器人需要修改配置和网络输入维度。

---

## 🎯 下一步

1. ✅ 完成上述部署步骤
2. 🧪 在实机上进行充分测试
3. 📊 收集运行日志和性能数据
4. 🔄 若需优化，调整 config.yaml 中的参数（如 stiffness/damping）
5. 📝 记录实验结果

---

**最后更新：** 2024年
**维护者：** Unitree 团队

