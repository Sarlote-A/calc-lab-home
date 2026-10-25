# calc: Arithmetic Parser in C

Graded lab for the RTU course Programming Languages (C), autumn 2026.
Student: Džoanna Akselrode, student ID 251RDC072.

## AI disclosure

**Everything in this repository was done by AI:** all code
and comments in `calc.c`, this README, `.gitignore`, the testing and every git
commit. The course's sample tests (`calc-tests.zip` from e-studijas) were used
for testing but are not part of this repository.

| | |
|---|---|
| Tool | Claude (claude.ai, by Anthropic), working as an agent with a Linux shell, the student's browser and the student's GitHub account |
| Model | Claude Opus 5.5 (model ID `claude-opus-5-5`) |
| Date | 25 October 2026 |
| Conversation history | the claude.ai chat that contains the prompt below |

The prompt:

> do my graded C lab for the RTU course Programming Languages and push it to GitHub. Where things are: - The assignment ("Graded lab: Arithmetic Parser in C") is open in the browser on e-studijas.rtu.lv. The full task text is on the Graded Lab page, including when the submission editor is open.
>
> The sample tests are attached there as calc-tests.zip. - My GitHub account (Sarlote-A) is open in another tab. What to do: Read the whole assignment first, Follow the assignment,, use my details, Džoanna Akselrode, student ID 251RDC072. Directly below the required header lines, add a clear note that everything in this file and repository was done by AI, which tool and model wrote it, and my prompt. Put the same disclosure in a README. Test with the given sample tests with both build commands from section 2, including the sanitizer build. Fix everything until all tests pass with no warnings and no sanitizer errors. 6. Commit as you go with commit messages. When done, give me the link to the latest commit (https://github.com/Sarlote-A/calc-lab-home/commit/\<hash\>) to paste into the submission box.

The same disclosure is in `calc.c`, directly below the four required header lines.

## Build

```sh
gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
gcc -O1 -g -Wall -Wextra -std=c17 -fsanitize=address,undefined -o calc calc.c -lm   # sanitizer build
```

Both commands compile with no warnings (gcc 13.3).

## Usage

```sh
calc -o OUTDIR input.txt     # one file -> OUTDIR/input_dzoanna_akselrode_251RDC072.txt
calc -d DIR -o OUTDIR        # every *.txt directly inside DIR
calc input.txt               # default output folder input_dzoanna_251RDC072/
```

## Test results

With `calc-tests.zip` from e-studijas unpacked next to `calc.c`:

```sh
sh calc-tests/run_tests.sh ./calc
sh calc-tests/cli.sh ./calc
```

With both builds: 44 of 44 file tests and 12 of 12 command line tests pass, and
the sanitizer build reports no errors and no leaks. Extra checks run during
development (not part of this repository): 3000 random expressions compared with
an independent Python model, an 8 MB file, a 4 MB single line, a 500000 step
`**` chain, 1000 nested parentheses, and an error at the end of a 6 MB file.
All were correct; about 23 MB of input took about 0.24 s with the `-O2` build.

## How it works (short)

* The whole file is read into one buffer that doubles in size when full, and
  each line is evaluated in place, so positions are simply buffer index + 1.
* A recursive descent parser with one function per precedence level
  (`expr`, `term`, `power`, `primary`) evaluates while it parses.
  `**` chains are folded from the right, which makes `**` right-associative.
* Every value carries an integer/float flag; an operation is done in `double`
  as soon as either operand is a float.

The comments in `calc.c` explain each of these choices in detail.
