#!/usr/bin/env python3
"""Create the Chinese rendering test EPUB and its source tree."""

from __future__ import annotations

import argparse
import io
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = ROOT / "test" / "epubs-src" / "test_chinese"
EPUB_PATH = ROOT / "test" / "epubs" / "test_chinese.epub"

BOOK_ID = "urn:uuid:test-chinese-rendering"
BOOK_TITLE = "中文渲染测试书"
BOOK_AUTHOR = "十字墨测试组"
BOOK_LANG = "zh-CN"

CONTAINER_XML = """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
"""

CSS = """body {
  font-family: serif;
  line-height: 1.5;
}

h1, h2 {
  text-align: center;
}

.note {
  font-size: 0.9em;
  margin-top: 0.5em;
  margin-bottom: 0.5em;
}

.indent-2em {
  text-indent: 2em;
}

.mixed {
  text-align: justify;
}
"""

CHAPTERS = [
    ("chapter1.xhtml", "简体长文", "zh-CN"),
    ("chapter2.xhtml", "繁体对照", "zh-TW"),
    ("chapter3.xhtml", "中英混排", "zh-CN"),
    ("chapter4.xhtml", "标点避头尾", "zh-CN"),
    ("chapter5.xhtml", "注音与拼音", "zh-CN"),
    ("chapter6.xhtml", "首行缩进", "zh-CN"),
    ("chapter7.xhtml", "异体选择符", "zh-CN"),
    ("chapter8.xhtml", "GBK编码章", "zh-CN"),
]


def dense_sc_paragraph() -> str:
    unit = (
        "晋太元中，武陵人捕鱼为业。缘溪行，忘路之远近。"
        "忽逢桃花林，夹岸数百步，中无杂树，芳草鲜美，落英缤纷。"
        "渔人甚异之。复前行，欲穷其林。林尽水源，便得一山。"
        "山有小口，仿佛若有光。便舍船，从口入。初极狭，才通人。"
        "复行数十步，豁然开朗。土地平旷，屋舍俨然，有良田美池桑竹之属。"
        "阡陌交通，鸡犬相闻。其中往来种作，男女衣着，悉如外人。"
        "黄发垂髫，并怡然自乐。见渔人，乃大惊，问所从来，具答之。"
        "便要还家，设酒杀鸡作食。村中闻有此人，咸来问讯。"
        "自云先世避秦时乱，率妻子邑人来此绝境，不复出焉，遂与外人间隔。"
        "问今是何世，乃不知有汉，无论魏晋。此人一一为具言所闻，皆叹惋。"
    )
    parts: list[str] = []
    total = 0
    while total < 2100:
        parts.append(unit)
        total += len(unit)
    return "".join(parts)


def xhtml(lang: str, title: str, body: str) -> str:
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        "<!DOCTYPE html>\n"
        f'<html xmlns="http://www.w3.org/1999/xhtml" lang="{lang}" xml:lang="{lang}">\n'
        "<head>\n"
        f"  <title>{title}</title>\n"
        '  <link rel="stylesheet" type="text/css" href="styles/test.css"/>\n'
        "</head>\n"
        "<body>\n"
        f"{body}"
        "</body>\n"
        "</html>\n"
    )


def chapter1_sc() -> str:
    para = dense_sc_paragraph()
    body = (
        f"  <h1>简体长文</h1>\n"
        '  <p class="note" lang="en">Simplified Chinese. Language is zh-CN. '
        "The next paragraph has more than 2000 characters.</p>\n"
        f"  <p>{para}</p>\n"
        "  <p>后段较短，用于对照换页后的简体字形是否仍可读。</p>\n"
    )
    return xhtml("zh-CN", "简体长文", body)


def chapter2_tc() -> str:
    body = (
        "  <h1>繁體對照</h1>\n"
        '  <p class="note" lang="en">Traditional Chinese. Language is zh-TW. '
        "Watch 說, 體, 龍, and 為.</p>\n"
        "  <p>他說這不是一本普通的書。書中寫一條龍的形體，鱗甲分明。</p>\n"
        "  <p>讀者為此一讀再讀。國學、書體、與舊時傳說同頁而列。</p>\n"
        "  <p>開門見山之後，問聞後事，從無到有，於燈下翻頁。</p>\n"
        "  <p>這一段刻意使用繁體字形：說、體、龍、為、國、學、與、這、個、來、時、會、過、還、對、開、關、門、問、聞、後、從、無、於。</p>\n"
    )
    return xhtml("zh-TW", "繁體對照", body)


def chapter3_mixed() -> str:
    body = (
        "  <h1>中英混排</h1>\n"
        '  <p class="note" lang="en">Spaces around English words must remain.</p>\n'
        '  <p class="mixed">请在设置里选择 CrossInk 阅读器字体，并指定 LXGW WenKai。</p>\n'
        '  <p class="mixed">产品名 OpenType 与 TrueType 必须带空格写入中文句子。</p>\n'
        '  <p class="mixed">句中插入 English words like "firmware" 与 "Xteink X4" 后，空格仍须可见。</p>\n'
        '  <p class="mixed">路径示例：把 test_chinese.epub 拷到 /.fonts/ 之外的书籍目录。</p>\n'
    )
    return xhtml("zh-CN", "中英混排", body)


def chapter4_punctuation() -> str:
    fw = (
        "\uff0c"  # ，
        "\u3002"  # 。
        "\u3001"  # 、
        "\uff01"  # ！
        "\uff1f"  # ？
        "\uff1b"  # ；
        "\uff1a"  # ：
        "\u300c"  # 「
        "\u300d"  # 」
        "\u300e"  # 『
        "\u300f"  # 』
        "\uff08"  # （
        "\uff09"  # ）
        "\u300a"  # 《
        "\u300b"  # 》
        "\u3010"  # 【
        "\u3011"  # 】
    )
    g5 = (
        "\u2026"  # …
        "\u2014"  # em dash
        "\u301c"  # 〜 wave dash
        "\uff5e"  # ～ fullwidth tilde
        "\u00b7"  # ·
        "\u3005"  # 々
        "\u30fc"  # ー
        "\u3041\u3043\u3045\u3047\u3049\u3063\u3083\u3085\u3087\u308e"
        "\u30a1\u30a3\u30a5\u30a7\u30a9\u30c3\u30e3\u30e5\u30e7\u30ee"
        "\u301d"  # 〝
        "\u301e"  # 〞
        "\uff62"  # ｢
        "\uff63"  # ｣
    )
    dense = "字" * 40
    start_marks = "，。、！？」』）》…—～·々ー〞｣"
    end_marks = "「『（《【〝｢"
    body = (
        "  <h1>标点避头尾</h1>\n"
        '  <p class="note" lang="en">Marks sit at line starts and line ends.</p>\n'
        f"  <p>全角集合：{fw}</p>\n"
        f"  <p>缺口G5集合：{g5}</p>\n"
        f"  <p>{start_marks}这些标记贴在段首，用于逼出避头规则。</p>\n"
        f"  <p>这些标记贴在段末以逼出避尾规则{end_marks}</p>\n"
        f"  <p>{dense}，{dense}。{dense}、{dense}！{dense}？{dense}；{dense}：</p>\n"
        f"  <p>他说：「这不是一本普通的书。」然后，他翻开了第一页……</p>\n"
        f"  <p>波折号两侧：甲{chr(0x2014)}乙。浪线两侧：丙～丁。</p>\n"
        f"  <p>中点：上·下。叠字：人々。长音：エー。引用〝内侧〞与｢半角｣。</p>\n"
        "  <p>ぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮ must not split from the kana cluster.</p>\n"
    )
    return xhtml("zh-CN", "标点避头尾", body)


def chapter5_ruby() -> str:
    body = (
        "  <h1>注音与拼音</h1>\n"
        '  <p class="note" lang="en">Ruby uses rt for Bopomofo and pinyin.</p>\n'
        "  <p>"
        "<ruby>汉<rt>ㄏㄢˋ</rt></ruby>"
        "<ruby>字<rt>ㄗˋ</rt></ruby>"
        "<ruby>注<rt>ㄓㄨˋ</rt></ruby>"
        "<ruby>音<rt>ㄧㄣ</rt></ruby>"
        "。</p>\n"
        "  <p>"
        "<ruby>汉<rt>hàn</rt></ruby>"
        "<ruby>字<rt>zì</rt></ruby>"
        "<ruby>拼<rt>pīn</rt></ruby>"
        "<ruby>音<rt>yīn</rt></ruby>"
        "。</p>\n"
        "  <p>"
        "<ruby>臺<rt>ㄊㄞˊ</rt></ruby>"
        "<ruby>灣<rt>ㄨㄢ</rt></ruby>"
        "与"
        "<ruby>北<rt>běi</rt></ruby>"
        "<ruby>京<rt>jīng</rt></ruby>"
        "同页。</p>\n"
    )
    return xhtml("zh-CN", "注音与拼音", body)


def chapter6_indent() -> str:
    sample = (
        "春日既暮，庭中新叶初齐。这段文字用于比较首行缩进。"
        "前两句之后仍有足够长度，方便看出缩进宽窄。"
        "请在阅读器中打开与关闭 Embedded Style，并对照三节。"
    )
    body = (
        "  <h1>首行缩进</h1>\n"
        "  <h2>CSS text-indent 2em</h2>\n"
        f'  <p class="indent-2em">{sample}</p>\n'
        "  <h2>No CSS indent</h2>\n"
        f"  <p>{sample}</p>\n"
        "  <h2>Publisher style off</h2>\n"
        '  <p class="note" lang="en">Turn Embedded Style OFF. '
        "The CSS indent in the next paragraph must vanish.</p>\n"
        f'  <p class="indent-2em">{sample}</p>\n'
    )
    return xhtml("zh-CN", "首行缩进", body)


def chapter7_vs() -> str:
    fe_run = "".join("汉" + chr(cp) for cp in range(0xFE00, 0xFE10))
    ivs_run = (
        "葛"
        + "\U000e0100"
        + "辺"
        + "\U000e0101"
        + "辻"
        + "\U000e0102"
        + "漢"
        + "\U000e0100"
    )
    body = (
        "  <h1>异体选择符</h1>\n"
        '  <p class="note" lang="en">Each Han base keeps its variation selector.</p>\n'
        f"  <p>FE00 to FE0F: {fe_run}</p>\n"
        f"  <p>E0100 range: {ivs_run}</p>\n"
        "  <p>对照无选择符：葛辺辻漢。</p>\n"
    )
    return xhtml("zh-CN", "异体选择符", body)


def chapter8_gbk() -> bytes:
    text = """<?xml version="1.0" encoding="GBK"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN" xml:lang="zh-CN">
<head>
  <title>GBK编码章</title>
  <link rel="stylesheet" type="text/css" href="styles/test.css"/>
</head>
<body>
  <h1>GBK编码章</h1>
  <p class="note" lang="en">Declared GBK. Bytes are GBK. This chapter fails today (gap G6).</p>
  <p>这是一章以GBK编码的正文。解析器今日应失败。</p>
  <p>请先读完前面各章。本章放在书末，以免挡住其余测试。</p>
</body>
</html>
"""
    return text.encode("gbk")


def content_opf() -> str:
    items = [
        '    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>',
        '    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>',
        '    <item id="style" href="styles/test.css" media-type="text/css"/>',
    ]
    refs = []
    for index, (filename, _title, _lang) in enumerate(CHAPTERS, start=1):
        item_id = f"chapter{index}"
        items.append(
            f'    <item id="{item_id}" href="{filename}" media-type="application/xhtml+xml"/>'
        )
        refs.append(f'    <itemref idref="{item_id}"/>')
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="bookid" version="3.0" xml:lang="{BOOK_LANG}">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:identifier id="bookid">{BOOK_ID}</dc:identifier>
    <dc:title>{BOOK_TITLE}</dc:title>
    <dc:creator>{BOOK_AUTHOR}</dc:creator>
    <dc:language>{BOOK_LANG}</dc:language>
  </metadata>
  <manifest>
{chr(10).join(items)}
  </manifest>
  <spine toc="ncx">
{chr(10).join(refs)}
  </spine>
</package>
"""


def nav_xhtml() -> str:
    links = "\n".join(
        f'      <li><a href="{filename}">{title}</a></li>'
        for filename, title, _lang in CHAPTERS
    )
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="{BOOK_LANG}" xml:lang="{BOOK_LANG}">
<head>
  <title>{BOOK_TITLE}</title>
  <link rel="stylesheet" type="text/css" href="styles/test.css"/>
</head>
<body>
  <nav epub:type="toc" id="toc">
    <h1>目录</h1>
    <ol>
{links}
    </ol>
  </nav>
</body>
</html>
"""


def toc_ncx() -> str:
    points = []
    for index, (filename, title, _lang) in enumerate(CHAPTERS, start=1):
        points.append(
            "    <navPoint id="
            f'"navPoint-{index}" playOrder="{index}">\n'
            f"      <navLabel><text>{title}</text></navLabel>\n"
            f'      <content src="{filename}"/>\n'
            "    </navPoint>"
        )
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="{BOOK_ID}"/>
    <meta name="dtb:depth" content="1"/>
    <meta name="dtb:totalPageCount" content="0"/>
    <meta name="dtb:maxPageNumber" content="0"/>
  </head>
  <docTitle>
    <text>{BOOK_TITLE}</text>
  </docTitle>
  <navMap>
{chr(10).join(points)}
  </navMap>
</ncx>
"""


def readme() -> str:
    return """# Test: Chinese Rendering

This fixture is a synthetic Chinese EPUB.
Use it to check Simplified and Traditional text on the simulator and on the device.

## Chapters

1. Simplified Chinese prose. The chapter language is zh-CN. One paragraph has more than 2000 characters.
2. Traditional Chinese prose. The chapter language is zh-TW. The text uses Traditional forms such as 說, 體, 龍, and 為.
3. Mixed Chinese and English. Spaces around English words and product names must remain.
4. Punctuation torture. Full-width marks and the G5 set sit at line starts and line ends.
5. Ruby annotation. The markup uses ruby and rt for Bopomofo and pinyin.
6. First-line indent. One block uses text-indent 2em. One block has no CSS. One block is for Embedded Style OFF.
7. Variation selectors. Han characters are followed by U+FE00 to U+FE0F and by U+E0100.
8. GBK chapter. The XML declaration is GBK and the bytes are GBK. This chapter fails today (gap G6). It is last on purpose.

The OPF language is zh-CN.
The book title and the author are Chinese.
The NCX and the nav use Chinese chapter titles.

## Build

From the repository root:

```sh
python3 scripts/generate_chinese_test_epub.py
```

This command writes the source tree and the packed EPUB.
It is the only supported build path for this fixture.

This whole directory is generated output, unlike the other fixtures in `test/epubs-src/`.
The generator deletes every file here and writes it again on each run.
Do not edit the chapter XHTML, the OPF, the NCX, or this README by hand.
Make content changes in `scripts/generate_chinese_test_epub.py` and run the command again.
The generator stamps a fixed zip timestamp, so a rebuild without content changes gives the same bytes.
The `zip` steps that the other fixtures use record the current time and give a different file each run.

## Checks

epubcheck is not required.
The generator parses every UTF-8 XML file.
The GBK chapter is decoded as GBK.

Copy `test/epubs/test_chinese.epub` to the device or to the simulator book folder.
"""


def write_tree() -> dict[str, bytes]:
    files: dict[str, bytes] = {
        "mimetype": b"application/epub+zip\n",
        "META-INF/container.xml": CONTAINER_XML.encode("utf-8"),
        "OEBPS/styles/test.css": CSS.encode("utf-8"),
        "OEBPS/content.opf": content_opf().encode("utf-8"),
        "OEBPS/nav.xhtml": nav_xhtml().encode("utf-8"),
        "OEBPS/toc.ncx": toc_ncx().encode("utf-8"),
        "OEBPS/chapter1.xhtml": chapter1_sc().encode("utf-8"),
        "OEBPS/chapter2.xhtml": chapter2_tc().encode("utf-8"),
        "OEBPS/chapter3.xhtml": chapter3_mixed().encode("utf-8"),
        "OEBPS/chapter4.xhtml": chapter4_punctuation().encode("utf-8"),
        "OEBPS/chapter5.xhtml": chapter5_ruby().encode("utf-8"),
        "OEBPS/chapter6.xhtml": chapter6_indent().encode("utf-8"),
        "OEBPS/chapter7.xhtml": chapter7_vs().encode("utf-8"),
        "OEBPS/chapter8.xhtml": chapter8_gbk(),
        "README.md": readme().encode("utf-8"),
    }

    if SRC_DIR.exists():
        for old in SRC_DIR.rglob("*"):
            if old.is_file():
                old.unlink()

    for rel, data in files.items():
        path = SRC_DIR / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return files


ZIP_DATE_TIME = (1980, 1, 1, 0, 0, 0)
ZIP_FILE_ATTR = 0o100644 << 16


def zip_entry(rel: str, compress_type: int) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(rel, date_time=ZIP_DATE_TIME)
    info.compress_type = compress_type
    info.external_attr = ZIP_FILE_ATTR
    info.create_system = 3
    return info


def pack_epub(files: dict[str, bytes]) -> None:
    EPUB_PATH.parent.mkdir(parents=True, exist_ok=True)
    if EPUB_PATH.exists():
        EPUB_PATH.unlink()
    with zipfile.ZipFile(EPUB_PATH, "w") as zf:
        zf.writestr(zip_entry("mimetype", zipfile.ZIP_STORED), files["mimetype"])
        for rel, data in files.items():
            if rel in {"mimetype", "README.md"}:
                continue
            zf.writestr(zip_entry(rel, zipfile.ZIP_DEFLATED), data)


def validate_utf8_xml(files: dict[str, bytes]) -> None:
    errors: list[str] = []
    for rel, data in files.items():
        if rel == "OEBPS/chapter8.xhtml":
            try:
                decoded = data.decode("gbk")
            except UnicodeDecodeError as exc:
                errors.append(f"{rel}: GBK decode failed: {exc}")
                continue
            if 'encoding="GBK"' not in decoded:
                errors.append(f"{rel}: missing GBK declaration")
            continue
        if not rel.endswith((".xml", ".xhtml", ".opf", ".ncx")):
            continue
        # Decode to prove the bytes are UTF-8; ET.parse below reads the raw bytes.
        try:
            data.decode("utf-8")
        except UnicodeDecodeError as exc:
            errors.append(f"{rel}: UTF-8 decode failed: {exc}")
            continue
        try:
            ET.parse(io.BytesIO(data))
        except ET.ParseError as exc:
            errors.append(f"{rel}: XML is not well-formed: {exc}")
    if files["mimetype"] != b"application/epub+zip\n":
        errors.append("mimetype is not application/epub+zip")
    sc = files["OEBPS/chapter1.xhtml"].decode("utf-8")
    start = sc.find("<p>") + 3
    end = sc.find("</p>", start)
    sc_len = len(sc[start:end])
    if sc_len < 2000:
        errors.append(f"chapter1 dense paragraph is {sc_len} characters")
    if errors:
        raise SystemExit("Validation failed:\n" + "\n".join(errors))


def validate_zip() -> None:
    with zipfile.ZipFile(EPUB_PATH) as zf:
        names = zf.namelist()
        if names[0] != "mimetype":
            raise SystemExit("mimetype is not the first zip entry")
        info = zf.getinfo("mimetype")
        if info.compress_type != zipfile.ZIP_STORED:
            raise SystemExit("mimetype is compressed")
        packed = set(names)
        required = {
            "mimetype",
            "META-INF/container.xml",
            "OEBPS/content.opf",
            "OEBPS/nav.xhtml",
            "OEBPS/toc.ncx",
            "OEBPS/styles/test.css",
        }
        required.update(f"OEBPS/{name}" for name, _title, _lang in CHAPTERS)
        missing = sorted(required - packed)
        if missing:
            raise SystemExit("packed EPUB lacks: " + ", ".join(missing))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    files = write_tree()
    validate_utf8_xml(files)
    pack_epub(files)
    validate_zip()
    size = EPUB_PATH.stat().st_size
    print(f"Wrote {SRC_DIR.relative_to(ROOT)}")
    print(f"Wrote {EPUB_PATH.relative_to(ROOT)} ({size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
