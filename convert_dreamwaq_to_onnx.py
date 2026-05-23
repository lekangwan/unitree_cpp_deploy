#!/usr/bin/env python3
"""
将 DreamWaq PyTorch 模型转换为 ONNX 格式用于 C++ 部署
"""

import torch
import sys
import os
from pathlib import Path

def convert_dreamwaq_to_onnx(pt_model_path, onnx_output_path):
    """
    转换 DreamWaq policy_dwaq.pt 到 ONNX 格式
    
    Args:
        pt_model_path: policy_dwaq.pt 的路径
        onnx_output_path: 输出 ONNX 模型的路径
    """
    
    print(f"[INFO] 加载 PyTorch 模型: {pt_model_path}")
    
    # 加载 JIT script 模型
    try:
        model = torch.jit.load(pt_model_path, map_location='cpu')
    except Exception as e:
        print(f"[ERROR] 加载模型失败: {e}")
        return False
    
    # 设置为推理模式
    model.eval()
    
    # 创建虚拟输入 - obs_history: (1, 270) = (batch_size=1, num_obs*num_obs_hist=45*6)
    num_obs = 45
    num_obs_hist = 6
    dummy_input = torch.randn(1, num_obs * num_obs_hist)
    
    print(f"[INFO] 虚拟输入形状: {dummy_input.shape}")
    print(f"[INFO] 开始 ONNX 转换...")
    
    try:
        # 转换为 ONNX
        torch.onnx.export(
            model,
            dummy_input,
            onnx_output_path,
            input_names=['obs_history'],
            output_names=['actions'],
            opset_version=12,
            verbose=False
        )
        print(f"[SUCCESS] 模型已转换为 ONNX: {onnx_output_path}")
        return True
    except Exception as e:
        print(f"[ERROR] ONNX 转换失败: {e}")
        return False


def verify_onnx_model(onnx_path, pt_path):
    """
    验证 ONNX 模型的输出与原 PyTorch 模型一致
    """
    print(f"\n[INFO] 验证 ONNX 模型输出...")
    
    try:
        import onnxruntime as ort
    except ImportError:
        print("[WARNING] onnxruntime 未安装，跳过验证")
        print("          安装: pip install onnxruntime")
        return
    
    # 加载模型
    pt_model = torch.jit.load(pt_path, map_location='cpu')
    pt_model.eval()
    
    sess = ort.InferenceSession(onnx_path, providers=['CPUExecutionProvider'])
    
    # 创建测试输入
    test_input = torch.randn(1, 45 * 6)
    test_input_np = test_input.numpy()
    
    # PyTorch 推理
    with torch.no_grad():
        pt_output = pt_model(test_input)
    
    # ONNX 推理
    onnx_output = sess.run(None, {'obs_history': test_input_np})
    
    # 比较输出
    pt_output_np = pt_output.numpy()
    onnx_output = onnx_output[0]
    
    diff = abs(pt_output_np - onnx_output).max()
    print(f"[INFO] 输出差异 (最大值): {diff:.6e}")
    
    if diff < 1e-4:
        print("[SUCCESS] ONNX 模型验证通过！")
    else:
        print(f"[WARNING] 输出差异较大: {diff}")


if __name__ == "__main__":
    # 使用示例
    if len(sys.argv) < 2:
        print("使用方法:")
        print(f"  python {sys.argv[0]} <pt_model_path> [onnx_output_path]")
        print("\n示例:")
        print(f"  python {sys.argv[0]} logs/rough_go2/exported/policies/policy_dwaq.pt")
        print(f"  python {sys.argv[0]} logs/rough_go2/exported/policies/policy_dwaq.pt logs/go2/go2_dreamwaq/exported/policy.onnx")
        sys.exit(1)
    
    pt_path = sys.argv[1]
    onnx_path = sys.argv[2] if len(sys.argv) > 2 else pt_path.replace('.pt', '.onnx')
    
    # 检查输入文件是否存在
    if not os.path.exists(pt_path):
        print(f"[ERROR] 文件不存在: {pt_path}")
        sys.exit(1)
    
    # 创建输出目录
    output_dir = os.path.dirname(onnx_path)
    os.makedirs(output_dir, exist_ok=True)
    
    # 转换模型
    success = convert_dreamwaq_to_onnx(pt_path, onnx_path)
    
    # 验证模型
    if success:
        verify_onnx_model(onnx_path, pt_path)
