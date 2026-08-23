# Test: Chinese Rendering

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

You can also pack the source tree with the same zip steps as the other fixtures:

```sh
cd test/epubs-src/test_chinese
rm -f ../../epubs/test_chinese.epub
zip -X0 ../../epubs/test_chinese.epub mimetype
zip -Xr9D ../../epubs/test_chinese.epub META-INF OEBPS
```

## Checks

epubcheck is not required.
The generator parses every UTF-8 XML file.
The GBK chapter is decoded as GBK.

Copy `test/epubs/test_chinese.epub` to the device or to the simulator book folder.
