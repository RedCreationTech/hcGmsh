#!/usr/bin/env python3
"""GMP-ISE 软件内用户手册构建脚本（Python3 标准库单文件，零依赖）。

流程（doc/ref/软件内用户手册设计.md 地基步）：
  doc/manual/手册目录.json + doc/manual/src/*.md
    -> 内置 Markdown 子集转换（# / ## / ### 标题、段落、- 列表、
       ![图](images/x.png)、[文字](同目录.html)、**粗体**）
    -> HTML（<out>/）
    -> Qt Help 工程（gmp-manual.qhp 项目 + gmp-manual.qhcp 集合，
       namespace gmp-ise.manual，虚拟文件夹 manual/）
    -> qhelpgenerator 打 gmp-manual.qch

qhelpgenerator 查找顺序：--qhelpgenerator 参数 > QT_BIN_DIR 环境变量 >
PATH > Homebrew qttools libexec 常见路径。找不到时只警告并写出空占位
qch（退出码 0）：构建不失败，运行时手册窗口降级为占位提示。

用法：
  python3 tools/build_manual.py [--root 仓库根目录] [--out 输出目录]
      [--qhelpgenerator 可执行文件路径]
"""

import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

PAGE_TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
<!-- Qt QTextDocument CSS 子集实证结论（独立探针，Qt 6.11）：
     body 的 margin/max-width/word-wrap/overflow-wrap 不被正确支持：
     margin:24px 会让 idealWidth 恒等于视口+48px，横向滚动条常驻并
     反吃视口宽度，文本折行宽度比视口窄 48px（右侧大留白事故）。
     左右留白由 HelpBrowser 的 documentMargin 控制（保证严格对称）。
     长拉丁词无需处理：默认 WrapAtWordBoundaryOrAnywhere 可中途折断。
     img 的 max-width:100% Qt 认识：超宽图自动等比收敛并随视口重缩放。 -->
body {{ font-family: "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", sans-serif;
       line-height: 1.65; color: #333; }}
h1 {{ font-size: 22px; color: #2f6fed; }}
h2 {{ font-size: 17px; margin-top: 26px; color: #1f3c88; }}
h3 {{ font-size: 15px; margin-top: 20px; color: #2f4f8f; }}
img {{ max-width: 100%; height: auto; }}
a {{ color: #2f6fed; text-decoration: none; }}
ul {{ padding-left: 22px; }}
</style>
</head>
<body>
{body}
</body>
</html>
"""

INDEX_TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
<!-- 同 PAGE_TEMPLATE：body 外边距交由 HelpBrowser documentMargin 控制。
     Qt 对 body margin 的误用会让目录页出现横向滚动条。 -->
body {{ font-family: "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", sans-serif;
       line-height: 1.65; color: #333; }}
h1 {{ font-size: 22px; color: #2f6fed; }}
a {{ color: #2f6fed; text-decoration: none; }}
ul {{ padding-left: 22px; line-height: 1.9; }}
</style>
</head>
<body>
<h1>{title}</h1>
<p>{intro}</p>
<ul>
{items}
</ul>
</body>
</html>
"""


def warn(message):
    print(f"[manual] WARNING: {message}", file=sys.stderr)


# ---------------------------------------------------------------- Markdown 子集

def inline_markup(text):
    """转义后套用行内子集：**粗体**、![图](src)、[文字](href)。"""
    out = html.escape(text, quote=False)
    out = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", out)
    out = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)\)", r'<img src="\2" alt="\1"/>', out)
    out = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", r'<a href="\2">\1</a>', out)
    return out


def render_markdown(md_text):
    """把 Markdown 子集转成 HTML。返回 (html_body, [(小节标题, 锚点)])。

    锚点按 ch 设计文档规范递增编号（s1、s2…），同目录页间链接用
    <a href="其他章.html"> 直接引用。
    """
    lines = md_text.splitlines()
    blocks = []
    sections = []
    h2_count = 0
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if not line.strip():
            i += 1
            continue
        if line.startswith("### "):
            blocks.append(f"<h3>{inline_markup(line[4:].strip())}</h3>")
            i += 1
            continue
        if line.startswith("## "):
            h2_count += 1
            anchor = f"s{h2_count}"
            sections.append((line[3:].strip(), anchor))
            blocks.append(
                f'<h2 id="{anchor}">{inline_markup(line[3:].strip())}</h2>')
            i += 1
            continue
        if line.startswith("# "):
            blocks.append(f"<h1>{inline_markup(line[2:].strip())}</h1>")
            i += 1
            continue
        if line.lstrip().startswith("- "):
            items = []
            while i < len(lines) and lines[i].lstrip().startswith("- "):
                items.append(lines[i].lstrip()[2:].strip())
                i += 1
            blocks.append(
                "<ul>" +
                "".join(f"<li>{inline_markup(item)}</li>" for item in items) +
                "</ul>")
            continue
        paragraph = [line.strip()]
        i += 1
        while (i < len(lines) and lines[i].strip() and
               not re.match(r"\s*(#{1,3}\s|-\s)", lines[i])):
            paragraph.append(lines[i].strip())
            i += 1
        blocks.append(f"<p>{inline_markup(' '.join(paragraph))}</p>")
    return "\n".join(blocks), sections


# ---------------------------------------------------------------- qhelpgenerator

def find_qhelpgenerator(explicit=None):
    if explicit:
        return explicit
    candidates = []
    env_dir = os.environ.get("QT_BIN_DIR", "").strip()
    if env_dir:
        env_path = Path(env_dir)
        candidates.append(env_path if env_path.is_file() else env_path / "qhelpgenerator")
    found = shutil.which("qhelpgenerator") or shutil.which("qhelpgenerator.exe")
    if found:
        candidates.append(Path(found))
    # Homebrew qttools 把 Qt 工具放在 libexec 下（不在 PATH）。
    for prefix in ("/usr/local/opt/qttools", "/opt/homebrew/opt/qttools"):
        candidates.append(Path(prefix) / "share/qt/libexec/qhelpgenerator")
    for candidate in candidates:
        if candidate.is_file() and os.access(str(candidate), os.X_OK):
            return str(candidate)
    return None


def write_qhp(out_dir, catalog, chapters):
    namespace = catalog["namespace"]
    toc_sections = []
    keyword_entries = []
    for chapter in chapters:
        title = html.escape(chapter["title"], quote=True)
        file_name = chapter["html"]
        subs = "".join(
            f'        <section title="{html.escape(sub, quote=True)}" '
            f'ref="{file_name}#{anchor}"/>\n'
            for sub, anchor in chapter["sections"])
        toc_sections.append(
            f'      <section title="{title}" ref="{file_name}">\n{subs}'
            f'      </section>')
        for kw in chapter["keywords"]:
            keyword_entries.append(
                f'      <keyword name="{html.escape(kw, quote=True)}" '
                f'ref="{file_name}"/>')
    image_files = sorted(
        p.relative_to(out_dir).as_posix()
        for p in (out_dir / "images").glob("**/*") if p.is_file()
    ) if (out_dir / "images").is_dir() else []
    html_files = ["index.html"] + [c["html"] for c in chapters]
    file_entries = "\n".join(f"      <file>{name}</file>"
                             for name in html_files + image_files)
    toc = "\n".join(toc_sections)
    kws = "\n".join(keyword_entries)
    qhp = f"""<?xml version="1.0" encoding="UTF-8"?>
<QtHelpProject version="1.0">
  <namespace>{namespace}</namespace>
  <virtualFolder>{catalog.get("virtualFolder", "manual")}</virtualFolder>
  <customFilter name="gmp-ise">
    <filterAttribute>gmp-ise</filterAttribute>
  </customFilter>
  <filterSection>
    <filterAttribute>gmp-ise</filterAttribute>
    <toc>
{toc}
    </toc>
    <keywords>
{kws}
    </keywords>
    <files>
{file_entries}
    </files>
  </filterSection>
</QtHelpProject>
"""
    (out_dir / "gmp-manual.qhp").write_text(qhp, encoding="utf-8")


def write_qhcp(out_dir, catalog, namespace):
    title = html.escape(catalog["title"], quote=True)
    home = f"qthelp://{namespace}/{catalog.get('virtualFolder', 'manual')}/index.html"
    qhcp = f"""<?xml version="1.0" encoding="UTF-8"?>
<QHelpCollectionProject version="1.0">
  <assistant>
    <title>{title}</title>
    <cacheDirectory>gmp-ise</cacheDirectory>
    <homePage>{home}</homePage>
    <startPage>{home}</startPage>
  </assistant>
  <docFiles>
    <generate>
      <file>
        <input>gmp-manual.qhp</input>
        <output>gmp-manual.qch</output>
      </file>
    </generate>
  </docFiles>
</QHelpCollectionProject>
"""
    (out_dir / "gmp-manual.qhcp").write_text(qhcp, encoding="utf-8")


def write_placeholder_qch(out_dir, reason):
    warn(f"{reason}；写出空占位 gmp-manual.qch（手册窗口将显示降级提示）")
    (out_dir / "gmp-manual.qch").write_bytes(b"")


# ---------------------------------------------------------------- 主流程

def main():
    parser = argparse.ArgumentParser(description="构建 GMP-ISE 软件内用户手册 qch")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parent.parent),
                        help="仓库根目录（默认：脚本所在仓库）")
    parser.add_argument("--out", default=None,
                        help="输出目录（默认：<root>/build/manual）")
    parser.add_argument("--qhelpgenerator", default=None, help="qhelpgenerator 路径")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out_dir = Path(args.out).resolve() if args.out else root / "build" / "manual"
    manual_dir = root / "doc" / "manual"
    catalog_path = manual_dir / "手册目录.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    namespace = catalog["namespace"]
    title = catalog["title"]

    out_dir.mkdir(parents=True, exist_ok=True)

    # 截图资产：复制 images/ 目录结构（md 中以 images/x.png 引用）。
    src_images = manual_dir / "images"
    if src_images.is_dir():
        for image in sorted(src_images.iterdir()):
            if image.is_file() and not image.name.startswith("."):
                target_dir = out_dir / "images"
                target_dir.mkdir(parents=True, exist_ok=True)
                shutil.copy2(image, target_dir / image.name)

    # 章节 md -> HTML
    chapters = []
    for entry in catalog["chapters"]:
        md_path = manual_dir / "src" / entry["file"]
        md_text = md_path.read_text(encoding="utf-8")
        body, sections = render_markdown(md_text)
        html_name = re.sub(r"\.md$", ".html", entry["file"])
        (out_dir / html_name).write_text(
            PAGE_TEMPLATE.format(title=entry["title"], body=body),
            encoding="utf-8")
        chapters.append({"id": entry["id"], "title": entry["title"],
                         "html": html_name, "sections": sections,
                         "keywords": entry.get("keywords", [])})
        print(f"[manual] {md_path.name} -> {html_name}")

    # 目录首页
    items = "\n".join(
        f'<li><a href="{c["html"]}">{html.escape(c["title"])}</a></li>'
        for c in chapters)
    (out_dir / "index.html").write_text(
        INDEX_TEMPLATE.format(title=title, intro="目录", items=items),
        encoding="utf-8")

    # Qt Help 工程
    write_qhp(out_dir, catalog, chapters)
    write_qhcp(out_dir, catalog, namespace)

    generator = find_qhelpgenerator(args.qhelpgenerator)
    if not generator:
        write_placeholder_qch(out_dir, "未找到 qhelpgenerator"
                              "（--qhelpgenerator / QT_BIN_DIR / PATH）")
        return 0
    print(f"[manual] qhelpgenerator: {generator}")
    result = subprocess.run([generator, "gmp-manual.qhcp"],
                            cwd=str(out_dir), capture_output=True, text=True)
    if result.returncode != 0 or not (out_dir / "gmp-manual.qch").is_file():
        write_placeholder_qch(
            out_dir,
            f"qhelpgenerator 失败（exit {result.returncode}）: "
            f"{(result.stderr or result.stdout).strip()[:400]}")
        return 0
    qch_size = (out_dir / "gmp-manual.qch").stat().st_size
    print(f"[manual] gmp-manual.qch 生成成功（{qch_size} 字节）: "
          f"{out_dir / 'gmp-manual.qch'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
