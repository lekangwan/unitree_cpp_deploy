# DreamWaq 模型导出和部署指南

## 概述

本指南说明如何将 DreamWaq_train_go2 项目训练的 Go2 模型导出到 unitree_cpp_deploy 中进行实机部署。

## 关键参数对照

| 参数 | 值 | 说明 |
|-----|-----|-----|
| num_observations | 45 | 每个时步的观测维度 |
| num_obs_hist | 6 | 观测历史长度 |
| num_actions | 12 | 12个关节的控制 |
| action_scale | 0.25 | 动作缩放因子 |
| stiffness | 28.0 N·m/rad | 关节刚度 |
| damping | 0.7 N·m·s/rad | 关节阻尼 |
| step_dt | 0.02 s | 控制周期 (50 Hz) |

## 步骤 1：导出 PyTorch 模型

### 方式 A：使用现有的导出脚本

在 DreamWaq_train_go2 目录中编辑 `legged_gym/scripts/play.py`：

```python
EXPORT_POLICY = True  # 设置为 True
RECORD_FRAMES = False
MOVE_CAMERA = False
```

然后运行：

```bash
cd DreamWaq_train_go2
python legged_gym/scripts/play.py --task go2
```

这会生成：
```
legged_gym/logs/rough_go2/exported/policies/policy_dwaq.pt
```

### 方式 B：手动导出（如果上面不成功）

```python
from legged_gym.utils.helpers import PolicyExporterDWAQ
import torch

# 加载已训练的模型
# ... (加载训练配置和模型的代码)

actor_critic = ppo_runner.alg.actor_critic
exporter = PolicyExporterDWAQ(actor_critic, num_obs=45, num_obs_hist=6)
exporter.export('legged_gym/logs/rough_go2/exported/policies')
```

## 步骤 2：转换为 ONNX 格式

使用提供的转换脚本：

```bash
cd unitree_cpp_deploy

# 基础用法
python convert_dreamwaq_to_onnx.py \
    ../DreamWaq_train_go2/legged_gym/logs/rough_go2/exported/policies/policy_dwaq.pt

# 指定输出路径
python convert_dreamwaq_to_onnx.py \
    ../DreamWaq_train_go2/legged_gym/logs/rough_go2/exported/policies/policy_dwaq.pt \
    logs/go2/go2_dreamwaq/exported/policy.onnx
```

**输出结果应该为：**
- `logs/go2/go2_dreamwaq/exported/policy.onnx`
- 输出差异应 < 1e-4

## 步骤 3：配置部署参数

编辑 `deploy/robots/go2/config/config.yaml`，添加 DreamWaq 策略状态：

```yaml
FSM:
  _: # enabled fsms
    Passive:
      id: 1
    FixStand:
      id: 2
    DreamWaq_Velocity:  # 新增
      id: 8
      type: RLBase
      
  DreamWaq_Velocity:    # 新增
    transitions: 
      Passive: LT + B.on_pressed
    policy_dir: ../../../logs/go2/go2_dreamwaq
    logging: false
    logging_dt: 0.01
```

## 步骤 4：目录结构

确保以下目录结构正确：

```
unitree_cpp_deploy/
├── logs/
│   └── go2/
│       └── go2_dreamwaq/
│           ├── exported/
│           │   └── policy.onnx          # ONNX 模型
│           └── params/
│               └── deploy.yaml          # 部署配置
├── deploy/
│   └── robots/
│       └── go2/
│           ├── CMakeLists.txt
│           ├── main.cpp
│           └── config/
│               └── config.yaml          # FSM 配置
```

## 步骤 5：编译和部署

```bash
# 编译 Go2 可执行文件
cd unitree_cpp_deploy/deploy/robots/go2
mkdir build && cd build
cmake ..
make -j4

# 复制到实机（替换 <robot_ip> 为机器人IP）
scp -r ../../../logs/go2/go2_dreamwaq unitree@<robot_ip>:/home/unitree/logs/go2/
```

## 常见问题解决

### 问题 1：ONNX 转换失败

**原因：** PyTorch 模型包含不支持的操作

**解决方案：**
1. 确保 PyTorch 版本与训练时一致
2. 更新 opset_version：
   ```python
   torch.onnx.export(..., opset_version=14)
   ```

### 问题 2：模型输出维度不匹配

**检查项：**
- 输入维度：应为 (1, 270) = (batch_size, 45*6)
- 输出维度：应为 (1, 12)
- 确保 deploy.yaml 中的 observations 总维度为 45 * 6

### 问题 3：机器人不按预期运动

**诊断步骤：**

1. **检查观测归一化参数**
   - 训练配置中的 observation scale 必须与 deploy.yaml 一致
   - 检查 scale 和 offset 参数

2. **检查动作缩放**
   ```yaml
   actions:
     scale: [0.25, 0.25, ...]      # 必须与训练时的 action_scale 一致
     offset: [0.1, 0.8, -1.5, ...]  # 必须与 default_joint_angles 一致
   ```

3. **验证关节映射**
   ```yaml
   joint_ids_map: [3,4,5,0,1,2,9,10,11,6,7,8]  # Go2 的标准映射
   ```

4. **检查控制参数**
   - stiffness: 28.0 (来自 train_cfg)
   - damping: 0.7 (来自 train_cfg)
   - 若改变这些参数，模型性能会严重下降

### 问题 4：与 MOE 模型对比测试

```bash
# 在同一个 config.yaml 中同时配置两个模型
# 可以通过遥控器切换测试对比性能

# MOE (已有)
Velocity_Up:
  policy_dir: ../../../logs/go2/go2_moe_cts_137k_0.6713

# DreamWaq (新增)
DreamWaq_Velocity:
  policy_dir: ../../../logs/go2/go2_dreamwaq
```

## 附录：观测维度详解

DreamWaq Go2 模型的 45 维观测向量构成：

```
base_ang_vel:          3  (roll, pitch, yaw 角速度)
projected_gravity:     3  (重力向量在机体坐标系)
velocity_commands:     3  (期望速度命令)
joint_pos_rel:        12  (相对关节位置，相对default_pos)
joint_vel_rel:        12  (相对关节速度)
last_action:          12  (上一步的动作)
                       ---
总计:                 45
```

每个观测有 6 个时步的历史，所以网络输入维度 = 45 × 6 = 270

## 性能预期

基于 readme.md 中的描述：

- **平地行走：** 稳定流畅
- **不规则地形：** 能够应对
- **楼梯（15cm）：** 经过充分训练后可以上楼梯
- **推力干扰：** 由于训练中的随机推力，抗干扰能力较强

## 注意事项

⚠️ **重要提示：**

1. **不建议修改网络结构** - 已经固化在 ONNX 模型中
2. **观测和动作维度不能改** - 除非重新训练
3. **如果要改变动作数量（非12个关节）** - 需要重新训练整个模型
4. **实机测试前** - 建议先在 isaac gym 中充分验证

## 进阶：从头开始训练新模型

如果需要针对特定场景微调，可以：

1. 修改 `go2_config.py` 中的奖励权重
2. 修改 terrain 配置
3. 重新运行训练：
   ```bash
   python legged_gym/scripts/train.py --task go2
   ```

更多细节见 DreamWaq_train_go2/readme.md

