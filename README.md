# better-pdf

Turns a scanned or bloated book PDF into a small, clean, reflowed PDF.

Pages are rendered with **pdfium**, read by **PaddleOCR-VL-1.6** through
**llama-server**, assembled into a document AST in memory, and typeset with
**typst**.
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

| Path                                                                     | What                       |
| ------------------------------------------------------------------------ | -------------------------- |
| `third_party/pdfium/bin/pdfium.dll` (+ `lib/pdfium.dll.lib`, `include/`) | PDF rendering              |
| `third_party/llama-b11968-bin-win-cpu-x64/llama-server.exe`              | inference server           |
| `third_party/PaddleOCR-VL-1.6-GGUF/*.gguf`                               | model and vision projector |
| `third_party/typst/typst.exe`                                            | typesetting                |

The build needs clang (tested with 23.x) with the MSVC linker available.

- NOTE: You need to download and extract the PadlleOCR model into the specified third_party directory.
- NOTE 2: Vendored llama.cpp build is CPU only because I'm on a weak laptop, should be relatively easy to modify it to use GPU inference.

## Build

Two build modes:

```bat
scripts\build.bat debug      rem -Og -g3, assertions stop in the debugger
scripts\build.bat release    rem -O2, no debug info, assertions compile away
```
## Use
```bat
build\better-pdf.exe samples\clean.pdf out.pdf
build\better-pdf.exe samples\bloated.pdf out.pdf --pages 1-40
build\better-pdf.exe book.pdf out.pdf --title "The Book" --body-pt 11
```

Options:

| Option                    | Meaning                                               |
| ------------------------- | ----------------------------------------------------- |
| `--pages <first>-<last>`  | 1-based inclusive range; default is the whole book    |
| `--work-dir <dir>`        | work and cache directory; default is `<output>.work`  |
| `--third-party-dir <dir>` | where pdfium, llama-server, the model, and typst live |
| `--server-url <url>`      | reuse an already running llama-server                 |
| `--port <n>`              | port for the spawned server; default 18080            |
| `--max-pixels <n>`        | override the encoder's pixel budget                   |
| `--body-pt <n>`           | body font size; default 10.5                          |
| `--title <text>`          | override the document title                           |
| `--no-outline`            | omit the table of contents                            |
| `--help`                  | usage                                                 |

Paths may contain spaces and non-ASCII characters: every file is opened with
wide Win32 calls, and child processes receive wide command lines.

Exit codes: `0` success, `1` bad arguments or unreadable input, `2` a pipeline
step failed, `3` an internal invariant was violated (the assertion aborts),
`130` the run was interrupted.

The OCR server is started only when at least one requested page is not cached,
and it is stopped again when the run ends. If something else already answers on
the port, the run refuses to start a second server instead of silently talking
to a server it does not own; pass `--server-url` to reuse that one on purpose.
Its output goes to `<work>/llama-server.log`, the typst output to
`<work>/typst.log`, and the generated markup to `<work>/book.typ`, so a failed
run can be inspected without rerunning the model.
## Tests

```bat
scripts\check.bat [debug|release]   rem build the program and run the unit tests
scripts\test.bat [debug|release]    rem build and run the unit tests
scripts\coverage.bat                rem MC/DC and branch coverage report
scripts\e2e.bat [debug|release]     rem real model on small page ranges (slow)
```

The unit tests cover memory, logging, CLI parsing, base64, PNG encoding, image
analysis, JSON, HTTP framing, GGUF metadata, text classification, the AST,
geometry, typst emission, caching, pipeline planning, and progress accounting.
The Win32 adapter, pdfium binding, HTTP client, child processes, and the
orchestrator are exercised by the end-to-end run instead; they contain no
decision logic beyond error reporting.

Every file the coverage report measures reaches 100% of regions, functions,
lines, branches, and MC/DC conditions: the exclusions in `scripts\coverage.bat`
are the end-to-end-only glue and `asserts.c`, whose only code path is a
deliberate trap. `scripts\coverage.bat` prints the branch and MC/DC summaries.
The interactive `llvm-cov show` view is one command away:

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