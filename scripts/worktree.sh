#!/usr/bin/env bash
# 并行开发用的工作树工具。
#
# 一个需求一棵工作树：从主干当前 HEAD 拉临时分支 feature/<需求名>，
# 在旁边的 StoryBoard_<需求名> 目录里独立开发；合并回主干后工作树和临时分支一起删掉。
# 工作树里只写代码，不跑自检、不打包；自检和打包只在主干目录做。
#
# 用法：
#   bash scripts/worktree.sh new    <需求名> [主干分支]   # 建工作树
#   bash scripts/worktree.sh merge  <需求名>              # 合并到主干并删工作树/分支
#   bash scripts/worktree.sh drop   <需求名>              # 丢弃工作树（未合并也删）
#   bash scripts/worktree.sh list                         # 列出工作树
#
# 合并有冲突时脚本会停下并打印冲突文件，不自动解决、也不删工作树。

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAIN_BRANCH="${STORYBOARD_MAIN_BRANCH:-main}"
BASE_DIR="$(dirname "$ROOT")"
REPO_NAME="$(basename "$ROOT")"

usage() {
    sed -n '4,14p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit "${1:-1}"
}

require_name() {
    local name="${1:-}"
    if [ -z "$name" ]; then
        echo "missing requirement name" >&2
        usage 1
    fi
    case "$name" in
    *[!A-Za-z0-9._-]*)
        echo "requirement name may only contain letters, digits, dot, dash and underscore: $name" >&2
        exit 1
        ;;
    esac
    printf '%s' "$name"
}

worktree_path() {
    printf '%s/%s_%s' "$BASE_DIR" "$REPO_NAME" "$1"
}

current_branch() {
    git -C "$ROOT" rev-parse --abbrev-ref HEAD
}

cmd_new() {
    local name main_branch wt
    name="$(require_name "${1:-}")"
    main_branch="${2:-$MAIN_BRANCH}"
    wt="$(worktree_path "$name")"

    if [ -e "$wt" ]; then
        echo "worktree directory already exists: $wt" >&2
        exit 1
    fi
    if git -C "$ROOT" show-ref --verify --quiet "refs/heads/feature/$name"; then
        echo "branch already exists: feature/$name" >&2
        exit 1
    fi

    git -C "$ROOT" worktree add -b "feature/$name" "$wt" "$main_branch"
    echo "worktree: $wt"
    echo "branch:   feature/$name (from $main_branch)"
    echo "note:     code only in the worktree; self-test and packaging run in $ROOT"
}

cmd_merge() {
    local name branch wt
    name="$(require_name "${1:-}")"
    branch="feature/$name"
    wt="$(worktree_path "$name")"

    if ! git -C "$ROOT" show-ref --verify --quiet "refs/heads/$branch"; then
        echo "no such branch: $branch" >&2
        exit 1
    fi
    if [ "$(current_branch)" != "$MAIN_BRANCH" ]; then
        echo "main checkout must be on $MAIN_BRANCH (now on $(current_branch))" >&2
        exit 1
    fi
    if [ -n "$(git -C "$ROOT" status --porcelain)" ]; then
        echo "main checkout has uncommitted changes; commit or stash them first" >&2
        exit 1
    fi

    if ! git -C "$ROOT" merge --no-ff "$branch" -m "合并 $branch：$name"; then
        echo
        echo "merge stopped: resolve conflicts in $ROOT, then run 'bash scripts/worktree.sh merge $name' again" >&2
        git -C "$ROOT" status --short >&2
        exit 1
    fi

    cmd_drop "$name"
    echo "merged into $MAIN_BRANCH and removed $branch"
}

cmd_drop() {
    local name branch wt
    name="$(require_name "${1:-}")"
    branch="feature/$name"
    wt="$(worktree_path "$name")"

    if [ -d "$wt" ]; then
        git -C "$ROOT" worktree remove --force "$wt"
    fi
    git -C "$ROOT" worktree prune
    if git -C "$ROOT" show-ref --verify --quiet "refs/heads/$branch"; then
        if git -C "$ROOT" merge-base --is-ancestor "$branch" "$MAIN_BRANCH"; then
            git -C "$ROOT" branch -D "$branch"
        else
            echo "branch $branch still has unmerged commits; deleting anyway" >&2
            git -C "$ROOT" branch -D "$branch"
        fi
    fi
    echo "dropped $branch"
}

case "${1:-}" in
new) shift; cmd_new "$@" ;;
merge) shift; cmd_merge "$@" ;;
drop) shift; cmd_drop "$@" ;;
list) git -C "$ROOT" worktree list ;;
*) usage 0 ;;
esac
