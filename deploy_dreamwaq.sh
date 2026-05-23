#!/bin/bash

# DreamWaq 模型导出和部署自动化脚本
# 用法: ./deploy_dreamwaq.sh [options]

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
DREAMWAQ_PATH="${SCRIPT_DIR}/../DreamWaq_train_go2"
DEPLOY_PATH="${SCRIPT_DIR}"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

show_usage() {
    cat << EOF
DreamWaq 模型导出部署脚本

使用方法:
    $0 [options]

选项:
    -h, --help           显示此帮助信息
    -s, --skip-convert   跳过 ONNX 转换（已存在的 ONNX）
    --pt-path <path>     指定 policy_dwaq.pt 的路径
    --output-dir <path>  指定输出目录（默认：logs/go2/go2_dreamwaq）

示例:
    # 完整流程：导出 PT 模型 -> 转换 ONNX -> 生成部署配置
    $0

    # 跳过 ONNX 转换
    $0 --skip-convert

    # 指定自定义路径
    $0 --pt-path /path/to/policy_dwaq.pt --output-dir custom_model

EOF
}

# 默认值
SKIP_CONVERT=false
PT_PATH=""
OUTPUT_DIR="logs/go2/go2_dreamwaq"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -s|--skip-convert)
            SKIP_CONVERT=true
            shift
            ;;
        --pt-path)
            PT_PATH="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        *)
            log_error "未知选项: $1"
            show_usage
            exit 1
            ;;
    esac
done

# ============================================================
# Step 1: 检查环境
# ============================================================

log_info "=== 步骤 1: 检查环境 ==="

if [ ! -d "$DREAMWAQ_PATH" ]; then
    log_error "DreamWaq 项目不存在: $DREAMWAQ_PATH"
    exit 1
fi

if [ ! -d "$DEPLOY_PATH" ]; then
    log_error "部署目录不存在: $DEPLOY_PATH"
    exit 1
fi

log_info "✓ DreamWaq 项目路径: $DREAMWAQ_PATH"
log_info "✓ 部署目录: $DEPLOY_PATH"

# ============================================================
# Step 2: 查找或输入 policy_dwaq.pt 路径
# ============================================================

log_info "=== 步骤 2: 定位 policy_dwaq.pt ==="

# 如果没有指定，尝试自动查找
if [ -z "$PT_PATH" ]; then
    POSSIBLE_PATHS=(
        "$DREAMWAQ_PATH/legged_gym/logs/rough_go2/exported/policies/policy_dwaq.pt"
        "$DREAMWAQ_PATH/logs/rough_go2/exported/policies/policy_dwaq.pt"
    )
    
    for path in "${POSSIBLE_PATHS[@]}"; do
        if [ -f "$path" ]; then
            PT_PATH="$path"
            log_info "✓ 找到模型: $PT_PATH"
            break
        fi
    done
fi

if [ -z "$PT_PATH" ]; then
    log_error "未找到 policy_dwaq.pt，请指定 --pt-path"
    log_info "尝试查找位置："
    find "$DREAMWAQ_PATH" -name "policy_dwaq.pt" -type f 2>/dev/null || true
    exit 1
fi

if [ ! -f "$PT_PATH" ]; then
    log_error "模型文件不存在: $PT_PATH"
    exit 1
fi

log_info "✓ 模型文件路径: $PT_PATH"

# ============================================================
# Step 3: 准备输出目录
# ============================================================

log_info "=== 步骤 3: 准备输出目录 ==="

EXPORT_DIR="${DEPLOY_PATH}/${OUTPUT_DIR}/exported"
PARAMS_DIR="${DEPLOY_PATH}/${OUTPUT_DIR}/params"

mkdir -p "$EXPORT_DIR"
mkdir -p "$PARAMS_DIR"

log_info "✓ 输出目录已创建"
log_info "  导出目录: $EXPORT_DIR"
log_info "  参数目录: $PARAMS_DIR"

# ============================================================
# Step 4: ONNX 转换
# ============================================================

if [ "$SKIP_CONVERT" = false ]; then
    log_info "=== 步骤 4: 转换为 ONNX ==="
    
    ONNX_PATH="${EXPORT_DIR}/policy.onnx"
    
    python "${DEPLOY_PATH}/convert_dreamwaq_to_onnx.py" "$PT_PATH" "$ONNX_PATH"
    
    if [ ! -f "$ONNX_PATH" ]; then
        log_error "ONNX 转换失败"
        exit 1
    fi
    
    log_info "✓ ONNX 模型已生成: $ONNX_PATH"
else
    log_info "=== 步骤 4: 跳过 ONNX 转换 ==="
    
    ONNX_PATH="${EXPORT_DIR}/policy.onnx"
    if [ ! -f "$ONNX_PATH" ]; then
        log_warning "未找到已有的 ONNX 模型: $ONNX_PATH"
        log_info "请确保该文件存在，或不使用 --skip-convert 选项"
        exit 1
    fi
    
    log_info "✓ 使用已有的 ONNX 模型: $ONNX_PATH"
fi

# ============================================================
# Step 5: 验证部署配置
# ============================================================

log_info "=== 步骤 5: 验证部署配置 ==="

DEPLOY_YAML="${PARAMS_DIR}/deploy.yaml"

if [ ! -f "$DEPLOY_YAML" ]; then
    log_warning "未找到 deploy.yaml: $DEPLOY_YAML"
    log_info "请手动创建或复制参考配置"
else
    log_info "✓ deploy.yaml 已就位: $DEPLOY_YAML"
fi

# ============================================================
# Step 6: 显示配置检查清单
# ============================================================

log_info "=== 步骤 6: 配置检查清单 ==="

cat << 'EOF'
请确保以下步骤已完成：

□ ONNX 模型已生成: logs/go2/go2_dreamwaq/exported/policy.onnx
□ deploy.yaml 已配置: logs/go2/go2_dreamwaq/params/deploy.yaml
□ config.yaml 已更新: 添加 DreamWaq_Velocity 状态

配置示例（在 deploy/robots/go2/config/config.yaml 中添加）:

FSM:
  _:
    DreamWaq_Velocity:
      id: 8
      type: RLBase

  DreamWaq_Velocity:
    transitions:
      Passive: LT + B.on_pressed
    policy_dir: ../../../logs/go2/go2_dreamwaq
    logging: false
    logging_dt: 0.01

EOF

log_info "✓ 部署准备完成！"
log_info ""
log_info "下一步:"
log_info "  1. 更新 config.yaml （参考上面的配置示例）"
log_info "  2. 编译: cd deploy/robots/go2 && mkdir build && cd build && cmake .. && make"
log_info "  3. 部署到实机: scp -r logs/go2/go2_dreamwaq unitree@<robot_ip>:/home/unitree/logs/go2/"
log_info ""
log_info "更多信息见: docs/DreamWaq_deployment_guide_zh.md"

