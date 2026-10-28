# better-pdf

Turns a scanned or bloated book PDF into a small, clean, reflowed PDF.

Pages are rendered with **pdfium**, read by **PaddleOCR-VL-1.6** through
**llama-server**, assembled into a document AST in memory, and typeset with
**typst**. The output is text, not page images: a 300-page scan becomes a few
hundred kilobytes instead of hundreds of megabytes.

## How a run works

1. Open the PDF with pdfium and read every page's size.
2. Pick the output page size from the median input page (see `geometry.c`).
3. Render each requested page at the pixel budget the vision encoder declares
   in its GGUF metadata (`clip.vision.image_max_pixels`), grayscale when the
   page has no colour.
4. POST the page to llama-server's chat endpoint as a base64 PNG and read the
   OCR text back, caching each page as it arrives.
5. Build the AST: paragraphs, headings, footnotes, printed page numbers,
   running heads, chapters, title.
6. Emit typst markup and compile it to the output PDF.

A page that is blank, or almost solid ink, is recognized as such from the
raster and costs no OCR time.

## Requirements

Everything is vendored under `third_party/`, pinned and committed:

| Path | What |
| --- | --- |
| `third_party/pdfium/bin/pdfium.dll` (+ `lib/pdfium.dll.lib`, `include/`) | PDF rendering |
| `third_party/llama-b11968-bin-win-cpu-x64/llama-server.exe` | inference server |
| `third_party/PaddleOCR-VL-1.6-GGUF/*.gguf` | model and vision projector |
| `third_party/typst/typst.exe` | typesetting |

The build needs clang (tested with 23.x) with the MSVC linker available.

## Build

```bat
scripts\build.bat
```

This writes `build\better-pdf.exe`, copies `pdfium.dll` next to it, and copies
the AddressSanitizer runtime DLL that the debug build links against.

## Use

```bat
build\better-pdf.exe samples\clean.pdf out.pdf
build\better-pdf.exe samples\bloated.pdf out.pdf --pages 1-40
build\better-pdf.exe book.pdf out.pdf --title "The Book" --body-pt 11
```

Options:

| Option | Meaning |
| --- | --- |
| `--pages <first>-<last>` | 1-based inclusive range; default is the whole book |
| `--work-dir <dir>` | work and cache directory; default is `<output>.work` |
| `--third-party-dir <dir>` | where pdfium, llama-server, the model, and typst live |
| `--server-url <url>` | reuse an already running llama-server |
| `--port <n>` | port for the spawned server; default 18080 |
| `--max-pixels <n>` | override the encoder's pixel budget |
| `--body-pt <n>` | body font size; default 10.5 |
| `--title <text>` | override the document title |
| `--no-outline` | omit the table of contents |
| `--help` | usage |

Exit codes: `0` success, `1` bad arguments or unreadable input, `2` a pipeline
step failed, `3` an internal invariant was violated (the assertion aborts).

The OCR server is started only when at least one requested page is not cached,
and it is stopped again when the run ends. Its output goes to
`<work>/llama-server.log`, the typst output to `<work>/typst.log`, and the
generated markup to `<work>/book.typ`, so a failed run can be inspected without
rerunning the model.

## The work directory and the cache

Each recognized page is cached as `<work>/pages/<key>/NNNNN.txt`. The directory
name is a hash of the pipeline version, the input path and size, the page count,
and the pixel budget. Change any of those and the run uses a different
directory, so stale text can never be mixed into a new document. Old
directories are inert; delete them whenever you like.

Deleting one page's text file forces just that page to be recognized again. The
manifest next to the pages records what the directory was built from.

## What the AST contains

Every page is kept as ordered blocks, and the text of the whole book lives in a
single pool that blocks address by offset.

- **Page numbers** are recognized only at the very start or end of a page, in
  digits or roman numerals. They are recorded, not printed: typst numbers the
  output itself.
- **Footnotes** are numbered lines in the last thirty percent of a page, or
  numbered lines whose marker appears as a superscript anywhere on the page. A
  reference with a matching note becomes a real typst footnote, so notes are
  renumbered and placed at the bottom of the page they belong to.
- **Headings** are short lines that do not read as prose: chapter and section
  markers, all-caps lines, numbered headings, or a short first line that is
  followed by a paragraph not starting in lowercase. A parenthetical subtitle
  directly below a heading is merged into it.
- **Running heads** are the page furniture repeated at the top of three or more
  pages. Every occurrence after the first is marked and left out of the output.
- **Chapters** start at level-one headings. A book with fewer than two chapters
  has no outline and is presented as one chapter named after the file.

## Output

The output page uses the median input page's size, normalized to portrait, with
margins of 13% of the width and 10% of the height, clamped. Body text is
Libertinus Serif, justified, with a 0.62 em leading and a 1.05 em paragraph
spacing. Paragraph breaks are kept; hyphens at hard line breaks are rejoined.
The outline is emitted when the book has at least two chapters, and each chapter
after the first starts on a new page.

## Memory

All memory is allocated once, at startup, from a single block: the document
arena (blocks, pages, chapters, and text pool) and one scratch arena (raster,
PNG, base64, request, response, typst source, and the current page's text).
Nothing grows or moves during the run. The plan is computed from the page count
and pixel budget; if it would exceed 2 GiB the program refuses to start.

At the default PaddleOCR-VL budget of 1,003,520 pixels, the scratch side is
about 60 MB, dominated by the typst source buffer.

## Tests

```bat
scripts\test.bat        rem unit tests under ASan and UBSan
scripts\coverage.bat    rem MC/DC and branch coverage report
scripts\e2e.bat         rem real model on small page ranges (slow)
```

The unit tests cover memory, logging, CLI parsing, base64, PNG encoding, image
analysis, JSON, HTTP framing, GGUF metadata, text classification, the AST,
geometry, typst emission, caching, and pipeline planning. The Win32 adapter,
pdfium binding, HTTP client, child processes, and the orchestrator are exercised
by the end-to-end run instead; they contain no decision logic beyond error
reporting.

Product files currently reach 94.8% of lines, 88.0% of branches, and 68.8%
of MC/DC conditions; `arena.c`, `base64.c`, `geometry.c`, and `log.c` are fully
covered, and `png.c` has no uncovered line. What is left is mostly defensive checks
in the JSON, HTTP, GGUF, and text parsers that no well-formed or plausible
malformed input can reach, plus the Win32, pdfium, socket, and process glue
that only the end-to-end run exercises. `asserts.c` is excluded because its
only code path is a deliberate abort.

`scripts\coverage.bat` prints branch and MC/DC summaries. The interactive
`llvm-cov show` view is one command away:

```bat
llvm-cov show build\unit_tests_cov.exe -instr-profile=build\coverage.profdata ^
  -ignore-filename-regex="platform_win32\.c" -show-branches=count
```

## Limitations

- **Images and figures are dropped.** The output is text; a book that relies on
  plates, diagrams, or tables will lose them. Tables and display math are kept
  as their OCR text, not as reconstructed structures.
- **OCR quality sets the ceiling.** The model occasionally misreads ligatures,
  drops a hyphenated word, or joins two columns; the AST heuristics are
  deliberately conservative and prefer a paragraph to a wrong heading.
- **The heuristics are tuned for books.** A heavily designed layout (sidebars,
  pull quotes, multi-column captions) will be treated as prose.
- **OCR is slow on CPU.** Expect tens of seconds per page; that is why the cache
  exists.
- **Rotated pages** are rendered using the page's rotation, but text that is
  rotated inside the page is not detected.