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

| Path                                                                     | What                       |
| ------------------------------------------------------------------------ | -------------------------- |
| `third_party/pdfium/bin/pdfium.dll` (+ `lib/pdfium.dll.lib`, `include/`) | PDF rendering              |
| `third_party/llama-b11968-bin-win-cpu-x64/llama-server.exe`              | inference server           |
| `third_party/PaddleOCR-VL-1.6-GGUF/*.gguf`                               | model and vision projector |
| `third_party/typst/typst.exe`                                            | typesetting                |

The build needs clang (tested with 23.x) with the MSVC linker available.

## Build

Two modes, one binary path:

```bat
scripts\build.bat debug      rem -Og -g3, assertions stop in the debugger
scripts\build.bat release    rem -O2, no debug info, assertions compile away
```

`debug` is the default and the mode to develop in. A failed `ASSERT` logs its
file, line, and expression and then executes `int3`; with the generated
`build\better-pdf.pdb` loaded, the debugger stops at that instruction with the
failing expression in the caller's frame. `release` is the mode to run books in:
it writes no PDB, carries no debug sections, and compiles every `ASSERT` to a
compile-time check that emits no code, so assertions cost no branches and no
memory traffic.

Both modes copy `pdfium.dll` next to the exe. Neither needs a sanitizer
runtime. The release image is roughly 228 KB; the debug image is 652 KB plus
6.8 MB of PDB.

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

## Watching progress

Before any page is read, the run logs how many pages the input holds and which
range it is going to process. On a console it then keeps one line at the bottom
of the screen and rewrites it in place: once for the start, after every page,
and every second while the OCR server is still working on the current page. The
line names the page in flight, so a 45-second page never looks frozen:

```
[######..................] 3/12 pages, 25% | page 4: 17s | 10.0 s/page | ETA 1m 30s
```

When the output is redirected (a file, a pipe, CI), in-place rewriting would
only fill the file with carriage returns, so the run falls back to plain log
lines at the start, every ten pages, and for the last page:

```
[########................] 25/120 pages, 20% | 8.4 s/page | ETA 13m 18s
```

The seconds per page are the running mean over every page finished so far:
cached pages cost almost nothing and pull the mean down, so a resumed run
estimates optimistically until the newly recognized pages dominate. The ETA is
that mean multiplied by the pages that are left, rounded up.

Log lines written while the bar is up clear it first and redraw it afterwards,
so a warning about a blank page does not end up welded to the bar.

## Interrupting a run

Ctrl+C (or closing the console window) stops the run immediately, but not
abruptly: the handler prints a line, terminates the OCR server and any typst
child, and exits with code `130`. The children are also members of a job
object created with kill-on-job-close, so even a `TerminateProcess` from Task
Manager, a failed assertion, or a crash takes them down with the parent. No
`llama-server.exe` or `typst.exe` survives the parent by any exit path.

Cached pages are written to a temporary file and renamed into place, so an
interrupt can never leave a truncated page that a later run would read as
complete. Because every finished page is already cached, restarting the run
resumes where it stopped.

A long run also survives the model dying: if llama-server exits while a page is
being recognized, the pipeline restarts it once and retries that page. The
pages recognized before the crash are already cached, so nothing is repeated.

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