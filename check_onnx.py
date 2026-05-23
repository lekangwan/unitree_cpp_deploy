#!/usr/bin/env python3
"""
检查 ONNX 模型的输入输出接口
"""

import onnxruntime as ort
import sys
import os

def check_onnx_model(model_path):
    """检查 ONNX 模型的输入输出"""
    try:
        if not os.path.exists(model_path):
            print(f"错误: 模型文件不存在: {model_path}")
            return False

        sess = ort.InferenceSession(model_path)
        print(f"✅ ONNX 模型加载成功: {model_path}")
        print()

        print('📥 ONNX 模型输入:')
        for i, input_info in enumerate(sess.get_inputs()):
            print(f'  {i}: "{input_info.name}" - shape: {input_info.shape} - type: {input_info.type}')

        print()
        print('📤 ONNX 模型输出:')
        for i, output_info in enumerate(sess.get_outputs()):
            print(f'  {i}: "{output_info.name}" - shape: {output_info.shape} - type: {output_info.type}')

        return True

    except Exception as e:
        print(f"❌ 错误: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("用法: python check_onnx.py <model_path>")
        print("示例: python check_onnx.py logs/go2/go2_dreamwaq/exported/policy.onnx")
        sys.exit(1)

    model_path = sys.argv[1]
    success = check_onnx_model(model_path)

    if success:
        print("\n✅ 检查完成")
    else:
        print("\n❌ 检查失败")
        sys.exit(1)