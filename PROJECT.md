
I want you to create a CLI program that takes in a crappy PDF document, and outputs a simple optimized lightweight PDF document. This program will primarily be used to transform old crappy book PDF's into a better state.

Write a C program that, given a pdf as input (e.g. a 300 page book):
1. renders the page using `pdfium` to the maximum fidelity `PaddleOCR-VL-1.6-GGUF` accepts, and feeds it to the `PaddleOCR-VL-1.6-GGUF` to perform OCR using the `llama-server` inference engine
2. parses the text results that come from `llama-server` and stores it in RAM in the style of an AST as structured data storing document title, chapter title, page body text, page foot notes, page numbers, etc.
3.  After all pages are processed, turn the stored document AST into a streamlined simple optimized lightweight PDF using  `typst`.

All the mentioned programs are located in `C:/root/dev/dsh/better-pdf/third_party/`, or more specifically:
- `llama-server`: "C:/root/dev/dsh/better-pdf/third_party/llama-b11968-bin-win-cpu-x64/llama-server.exe"
- `pdfium`: "C:/root/dev/dsh/better-pdf/third_party/pdfium/bin/pdfium.dll"
- `typst`: "C:/root/dev/dsh/better-pdf/third_party/typst/typst.exe"
- `PaddleOCR-VL-1.6-GGUF`: "C:/root/dev/dsh/better-pdf/third_party/PaddleOCR-VL-1.6-GGUF/PaddleOCR-VL-1.6-GGUF.gguf" AND "C:/root/dev/dsh/better-pdf/third_party/PaddleOCR-VL-1.6-GGUF/PaddleOCR-VL-1.6-GGUF-mmproj.gguf" 

There are sample PDF's located in `C:/root/dev/dsh/better-pdf/samples/`

You must comply with the programming style guideline located in "C:/root/dev/dsh/better-pdf/STYLE.md".