#!/usr/bin/env bash
# 命名检查：按 .clang-tidy 的 readability-identifier-naming 诊断改动文件，只报告不修改。
#
# 用法：
#   bash scripts/naming-check.sh                 # 检查本次暂存的 .cpp
#   bash scripts/naming-check.sh src/ArcItem.cpp # 检查指定文件
#
# 环境变量：TIDY / MINGW_DIR / QT_DIR 可覆盖默认路径；SKIP_NAMING_CHECK=1 直接跳过。
# 说明：clang 不能直接解析 GCC 15 的 libstdc++，因此只把 libstdc++ 与 win32 头按
# -isystem 传入，并补 __GCC_ATOMIC_* 宏与 windows-gnu target（与 .clang-tidy 注释一致）。
set -euo pipefail

if [ "${SKIP_NAMING_CHECK:-0}" = "1" ]; then
    echo "naming-check: skipped (SKIP_NAMING_CHECK=1)"
    exit 0
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

if [ ! -f build/compile_commands.json ]; then
    echo "naming-check: build/compile_commands.json 不存在，先配置构建目录" >&2
    exit 2
fi

TIDY=${TIDY:-/d/Qt/Tools/QtCreator/bin/clang/bin/clang-tidy.exe}
MINGW_DIR=${MINGW_DIR:-/d/Qt/Tools/mingw1510_64}
CXX_INC=$(ls -d "$MINGW_DIR"/lib/gcc/x86_64-w64-mingw32/*/include/c++ 2>/dev/null | head -n 1 || true)
if [ -z "$CXX_INC" ] || [ ! -x "$TIDY" ]; then
    echo "naming-check: 找不到 clang-tidy 或 MinGW 头目录（可用 TIDY / MINGW_DIR 覆盖）" >&2
    exit 2
fi

to_win() { cygpath -m "$1"; }
EXTRA=(
    --extra-arg=--target=x86_64-w64-windows-gnu
    "--extra-arg=-isystem$(to_win "$CXX_INC")"
    "--extra-arg=-isystem$(to_win "$CXX_INC/x86_64-w64-mingw32")"
    "--extra-arg=-isystem$(to_win "$CXX_INC/backward")"
    "--extra-arg=-isystem$(to_win "$MINGW_DIR/x86_64-w64-mingw32/include")"
)
for m in BOOL CHAR CHAR16_T CHAR32_T WCHAR_T SHORT INT LONG LLONG POINTER; do
    EXTRA+=("--extra-arg=-D__GCC_ATOMIC_${m}_LOCK_FREE=2")
done
EXTRA+=("--extra-arg=-D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1")

if [ "$#" -gt 0 ]; then
    FILES=("$@")
else
    mapfile -t FILES < <(git diff --cached --name-only --diff-filter=ACMR -- '*.cpp')
fi

if [ "${#FILES[@]}" -eq 0 ]; then
    echo "naming-check: 没有需要检查的 .cpp"
    exit 0
fi

status=0
for f in "${FILES[@]}"; do
    case "$f" in
        *.cpp) ;;
        *) continue ;;
    esac
    hits=$("$TIDY" -p build --config-file=.clang-tidy "${EXTRA[@]}" "$f" 2>&1 |
        grep -E "warning: .*\[readability-identifier-naming\]" || true)
    if [ -n "$hits" ]; then
        echo "$hits"
        status=1
    fi
done

if [ "$status" -ne 0 ]; then
    echo "naming-check: 命名不合规范，见上方 \`invalid case style\` 条目" >&2
else
    echo "naming-check: 通过（${#FILES[@]} 个文件）"
fi
exit "$status"
