# Software Goals
- Value safety, performance, developer experience. In that order.
- Make the design simple and elegant.
- Achieve 100% MC/DC coverage and 100% branch coverage in your test suite without gaming the coverage metric.
# Programming Rules
- Compile C with `clang -Wall -Wextra -pedantic -std=c23 -fsanitize=address,undefined -Og -g3 ...` during development.
- Use local `git` repository for version control. Prefer atomic commits with descriptive commit messages. Prefer rebase.
- Use very simple and explicit imperative control flow to achieve clarity. Avoid recursion and minimize abstractions.
- Use explicitly-sized types like `uint64_t` and avoid architecture-specific types like `size_t`
- All memory must be statically allocated at startup. No memory may be dynamically requested from the OS (or freed and reallocated) after initialization.
- Untangle lifetimes with usage of memory arenas.
- Centralize control flow. When splitting a large function, try to keep all switch/if statements in the "parent" function, and move non-branchy logic fragments to helper functions. Divide responsibility. All control flow should be handled by _one_ function, the rest shouldn't care about control flow at all.
* Centralize state manipulation. Let the parent function keep all relevant state in local variables, and use helpers to compute what needs to change, rather than applying the change directly. Keep leaf functions pure.
- Handle errors. Immediately crash at unrecoverable states due to programming mistakes so the problem can be easily debugged, preferably emitting an ERROR log detailing the state and problem.
- When analyzing performance, perform back-of-the-envelope calculations with respect to the four resources (network, disk, memory, CPU) and their two main characteristics (bandwidth, latency). Use calculations to be “roughly right” and land within 90% of the theoretical maximum.
- When naming things, use `snake_case` for function, variable, and file names. Give descriptive, meaningful names and avoid abbreviations for the most part.
- Write descriptive commit messages that inform and delight the reader after every meaningful change. 
- Minimize third party dependencies, and vendor any dependency you use into the source tree under `./third_party/` directory.
- Don't use proprietary build systems, prefer concise .bat or .sh build scripts.
# Testing, Validation, Soundness, Observability
- Do regression testing. Whenever you encounter an interesting bug, add a test case that would've caught that bug.
- Create debugger-friendly assertion macros and use them all over the code. Use both static and runtime assertions. Assert all function arguments and return values, all pre/postconditions and invariants. Use blatantly true assertions instead of comments for documentation. Split compound assertions, preffering `assert(a); assert(b);` over `assert(a and b);`. Use single-line `if` to assert an implication: `if (a) assert(b)`.
- Create LOG and ERROR macros and use them to inform the user with meaningful event descriptions with useful information.