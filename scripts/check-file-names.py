#!/usr/bin/env python3
"""文件命名体检：同一目录里的文件是否出现「同义词对 / 兜底名 / 名不符实」。

规则来自 StoryBoard 的文件命名约定（README「文件命名」一节）：
  1. 同目录两个 stem 相似度过高（> 0.8）视为同义词对，应合并或改名；
  2. stem 里带 Helpers / Utils / Common / Misc 的兜底名一律提示；
  3. 文件里只有一个类，但类名与 stem 不一致，说明文件名没跟上职责；
  4. 同目录两个文件的注释首句雷同，通常是复制粘贴出来的重复模块；
  5. 白名单（真领域对，如 Star / Star4）不报。

默认只打印报告，退出码 0；加 --strict 时只要有 u 级提示就返回 1。
"""

from __future__ import annotations

import argparse
import difflib
import os
import re
import sys
from pathlib import Path

SIMILARITY_LIMIT = 0.8
COMMENT_LIMIT = 1.0
# 同族图元（items/shapes）共用「XX 图元：…」句式，是有意的统一说明，不做雷同检查。
COMMENT_SKIP_DIRS = {"shapes"}
FILLER_WORDS = ("Helpers", "Utils", "Common", "Misc")

# 真领域对：语义不同，名字相近是正常的。
WHITELIST_PAIRS = {
    ("Star", "Star4"),
    ("ShapeMeta", "ShapeMetaBlock"),
}

CLASS_PATTERN = re.compile(r"\b(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:final\s*)?(?::|;|{)")


def first_comment_sentence(text: str) -> str:
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("//"):
            sentence = stripped.lstrip("/").strip()
            if sentence:
                return sentence.split("。")[0][:40]
    return ""


def collect(root: Path) -> list[Path]:
    files = []
    for path in root.rglob("*"):
        if path.suffix not in {".h", ".cpp"}:
            continue
        if ".skills" in path.parts:
            continue
        files.append(path)
    return sorted(files)


def distinctive_parts(first: str, second: str) -> tuple[str, str]:
    """剥掉公共前缀与后缀，只留下各自的「职责词」。"""
    prefix = len(os.path.commonprefix([first, second]))
    suffix = 0
    limit = min(len(first), len(second)) - prefix
    while suffix < limit and first[len(first) - 1 - suffix] == second[len(second) - 1 - suffix]:
        suffix += 1
    return first[prefix:len(first) - suffix], second[prefix:len(second) - suffix]


def check_pairs(files: list[Path], root: Path) -> list[str]:
    findings = []
    by_dir: dict[Path, list[Path]] = {}
    for path in files:
        by_dir.setdefault(path.parent, []).append(path)

    for directory, siblings in sorted(by_dir.items()):
        for left in range(len(siblings)):
            for right in range(left + 1, len(siblings)):
                first, second = siblings[left], siblings[right]
                if first.stem == second.stem:
                    continue
                if (first.stem, second.stem) in WHITELIST_PAIRS or (second.stem, first.stem) in WHITELIST_PAIRS:
                    continue
                # 同族文件（PropertiesPanelXxxCard、SettingsDialogXxxPage）靠公共前后缀区分职责，
                # 只有「职责词」本身撞车才算同义词对。
                left_word, right_word = distinctive_parts(first.stem, second.stem)
                if not left_word or not right_word:
                    continue
                ratio = difflib.SequenceMatcher(None, left_word, right_word).ratio()
                if ratio > SIMILARITY_LIMIT:
                    findings.append(
                        f"[同义词对] {first.relative_to(root)} / {second.relative_to(root)} 职责词 {left_word}/{right_word} 相似度 {ratio:.2f}"
                    )
    return findings


def check_stem(files: list[Path], root: Path) -> list[str]:
    findings = []
    for path in files:
        for word in FILLER_WORDS:
            if path.stem.endswith(word) or f"{word}." in path.name:
                findings.append(f"[兜底名] {path.relative_to(root)} 使用了 {word}，应改成具体职责名")
    return findings


def check_class_name(files: list[Path], root: Path) -> list[str]:
    findings = []
    for path in files:
        if path.suffix != ".h":
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        names = [name for name in CLASS_PATTERN.findall(text) if not name.startswith("Q")]
        unique = sorted(set(names))
        if len(unique) != 1:
            continue
        only = unique[0]
        if only == path.stem:
            continue
        # 允许「领域 + 职责」的文件名包含类名（如 rows/ValueRows.h 放多个行类）。
        if only.lower() in path.stem.lower():
            continue
        findings.append(f"[名不符实] {path.relative_to(root)} 只声明 {only}，与文件名不一致")
    return findings


def check_comment(files: list[Path], root: Path) -> list[str]:
    findings = []
    by_dir: dict[Path, list[tuple[Path, str]]] = {}
    for path in files:
        sentence = first_comment_sentence(path.read_text(encoding="utf-8", errors="ignore"))
        if sentence:
            by_dir.setdefault(path.parent, []).append((path, sentence))

    for directory, entries in sorted(by_dir.items()):
        for left in range(len(entries)):
            for right in range(left + 1, len(entries)):
                first, second = entries[left], entries[right]
                if directory.name in COMMENT_SKIP_DIRS:
                    continue
                ratio = difflib.SequenceMatcher(None, first[1], second[1]).ratio()
                if ratio >= COMMENT_LIMIT:
                    findings.append(
                        f"[注释雷同] {first[0].relative_to(root)} / {second[0].relative_to(root)} 首句相似度 {ratio:.2f}"
                    )
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description="StoryBoard 文件命名体检")
    parser.add_argument("--root", default=Path(__file__).resolve().parents[1], type=Path, help="仓库根目录")
    parser.add_argument("--strict", action="store_true", help="有提示时返回非 0")
    parser.add_argument("--quiet", action="store_true", help="只打印统计")
    args = parser.parse_args()

    root: Path = args.root.resolve()
    files = collect(root / "src")
    findings: list[str] = []
    findings += check_stem(files, root)
    findings += check_class_name(files, root)
    findings += check_pairs(files, root)
    findings += check_comment(files, root)

    if not args.quiet:
        for line in findings:
            print(line)

    print(f"\n扫描 {len(files)} 个文件，命名提示 {len(findings)} 条")
    return 1 if findings and args.strict else 0


if __name__ == "__main__":
    sys.exit(main())
