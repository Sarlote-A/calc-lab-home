// Džoanna Akselrode 251RDC072
// https://github.com/Sarlote-A/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: AI tool Claude by Anthropic (model Claude Opus 5.5, model ID claude-opus-5-5, used through claude.ai) wrote this entire file; see the AI disclosure directly below. No other AI tools, websites or books were used.
// Link to chat: https://claude.ai/share/e641ea58-17b7-4bbc-967f-aa6ccbc212b2
// ============================================================================
// AI DISCLOSURE
//
// Everything in this file and in this repository was done by AI,
// all code and comments in calc.c, README.md, .gitignore, the testing
// and every git commit. The course's sample tests (calc-tests.zip from
// e-studijas) were used for testing but are not part of this repository.
//
// Tool:   Claude (claude.ai, by Anthropic), working as an agent with a Linux
//         shell, the student's browser and the student's GitHub account.
// Model:  Claude Opus 5.5 (model ID claude-opus-5-5).
// Date:   25 October 2026.
// Conversation history: the claude.ai chat that contains the prompt below.
//
// The prompt, word for word (line breaks added to fit the comment):
//
//   do my graded C lab for the RTU course Programming Languages and push it to
//   GitHub. Where things are: - The assignment ("Graded lab: Arithmetic Parser
//   in C") is open in the browser on e-studijas.rtu.lv. The full task text is
//   on the Graded Lab page, including when the submission editor is open.
//
//   The sample tests are attached there as calc-tests.zip. - My GitHub account
//   (Sarlote-A) is open in another tab. What to do: Read the whole assignment
//   first, Follow the assignment,, use my details, Džoanna Akselrode, student
//   ID 251RDC072. Directly below the required header lines, add a clear note
//   that everything in this file and repository was done by AI, which tool and
//   model wrote it, and my prompt. Put the same disclosure in a README. Test
//   with the given sample tests with both build commands from section 2,
//   including the sanitizer build. Fix everything until all tests pass with no
//   warnings and no sanitizer errors. 6. Commit as you go with commit messages.
//   When done, give me the link to the latest commit
//   (https://github.com/Sarlote-A/calc-lab-home/commit/<hash>) to paste into
//   the submission box.
//
// Each section below also carries a short "AI (Claude Opus 5.5)" note, as section 7 of the assignment asks.
// ============================================================================

// AI (Claude Opus 5.5): this section was written by AI from the prompt above.
// mkdir(), stat() and the dirent functions are POSIX, not ISO C. With
// -std=c17 glibc hides them unless we ask for POSIX explicitly, so this must
// come before the first #include.
#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// The output names use plain ASCII ("dzoanna", not "Džoanna"), like the
// assignment's own example uses "berzina", so the files are easy to type and
// survive any file system or archive tool.
#define FIRST_NAME "dzoanna"
#define LAST_NAME "akselrode"
#define STUDENT_ID "251RDC072"

// ---------------------------------------------------------------------------
// Small memory helpers
// AI (Claude Opus 5.5): written by AI from the prompt in the header.
// ---------------------------------------------------------------------------

// Running out of memory is not something the program can recover from in a
// useful way, so the helpers stop with a message instead of making every caller
// check for NULL.
static void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "calc: out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = xrealloc(NULL, n);
    memcpy(copy, s, n);
    return copy;
}

// Builds a + b + c in a new string; used for paths and file names.
static char *concat3(const char *a, const char *b, const char *c)
{
    size_t la = strlen(a), lb = strlen(b), lc = strlen(c);
    char *s = xrealloc(NULL, la + lb + lc + 1);
    memcpy(s, a, la);
    memcpy(s + la, b, lb);
    memcpy(s + la + lb, c, lc + 1);
    return s;
}

// A growable byte buffer for the results of one file.
// Large files: the capacity doubles whenever it runs out, so appending n bytes
// costs O(n) in total. Growing by a fixed amount would copy the whole buffer
// again and again (O(n^2)), which is what makes naive programs crawl on files
// of several megabytes.
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

static void buffer_reserve(Buffer *b, size_t extra)
{
    if (b->len + extra <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->len + extra)
        cap *= 2;
    b->data = xrealloc(b->data, cap);
    b->cap = cap;
}

// ---------------------------------------------------------------------------
// Values and arithmetic
// AI (Claude Opus 5.5): written by AI from the prompt in the header.
// ---------------------------------------------------------------------------

// Integer or float: every value remembers which one it is.
// Integer vs float decision: a value is a float only if it came from a number
// written with a '.', or from an operation that had a float operand. So the
// type is decided per operation from its two operands (apply() below), which
// gives exactly the rule "if either operand is a float, use double", and
// integer-only expressions never lose precision by passing through double.
typedef struct {
    int is_float;
    long long i;
    double d;
} Value;

static Value make_int(long long i)
{
    Value v = {0, i, 0.0};
    return v;
}

static Value make_float(double d)
{
    Value v = {1, 0, d};
    return v;
}

static double as_double(Value v)
{
    return v.is_float ? v.d : (double)v.i;
}

// Signed overflow is undefined behaviour in C, and the sanitizer build would
// report it. The tests never overflow, but doing +, -, * on unsigned numbers
// (where wrap-around is defined) and converting back gives the same answer
// whenever the true result fits, and stays well defined when it does not.
static long long wrap_add(long long a, long long b)
{
    return (long long)((unsigned long long)a + (unsigned long long)b);
}

static long long wrap_sub(long long a, long long b)
{
    return (long long)((unsigned long long)a - (unsigned long long)b);
}

static long long wrap_mul(long long a, long long b)
{
    return (long long)((unsigned long long)a * (unsigned long long)b);
}

// base ** e for e >= 0 by repeated squaring: O(log e) steps, so even
// 1 ** 1000000000000 is instant. b is squared only while bits of e remain,
// so it never grows past the final result (no needless overflow).
// e == 0 returns 1, which also covers the rule 0 ** 0 == 1.
static long long int_pow(long long base, long long e)
{
    unsigned long long result = 1;
    unsigned long long b = (unsigned long long)base;
    while (e > 0) {
        if (e & 1)
            result *= b;
        e >>= 1;
        if (e > 0)
            b *= b;
    }
    return (long long)result;
}

// ---------------------------------------------------------------------------
// Tokenizer
// AI (Claude Opus 5.5): written by AI from the prompt in the header.
// ---------------------------------------------------------------------------

typedef enum {
    T_NUM,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_POW,
    T_LPAREN,
    T_RPAREN,
    T_END, // end of the line
    T_BAD  // a character that cannot start any token
} TokKind;

typedef struct {
    TokKind kind;
    size_t pos; // 1-based position in the whole file
    Value val;  // only for T_NUM
} Token;

// One entry of the ** chain stack, see parse_power().
typedef struct {
    Value v;
    size_t op_pos; // position of the ** in front of this operand
    size_t start;  // position where this operand starts
} PowItem;

typedef struct {
    char *text;     // the whole file, NUL terminated
    size_t cur;     // 0-based index of the next unread character
    size_t end;     // 0-based index of this line's '\n' (or the file length)
    Token tok;      // the one token of lookahead
    size_t err_pos; // position of the first error, set by fail()
    PowItem *stack; // shared stack for ** chains
    size_t sp;
    size_t cap;
} Parser;

// How positions are tracked: the tokenizer never copies a line out of the file.
// It walks the one buffer that holds the whole file, and cur is an index into
// that buffer. The index of a character counts every character before it,
// including all earlier '\n's and comment lines, so the 1-based position the
// rules ask for is simply index + 1, with no line or column bookkeeping that
// could drift.
//
// Why END is reported at the '\n': a line ends at index end, which is where
// its '\n' is. For the last line without a '\n', end is the file length, so
// end + 1 is "file length + 1", exactly as the rules say for a missing operand
// or a missing ')' at the end of the line.
static int fail(Parser *p, size_t pos)
{
    p->err_pos = pos;
    return 0;
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

// Reads a number starting at p->cur. Integers are digits only; floats are
// digits '.' digits. If a '.' is not followed by a digit ("5." or "2. + 5"),
// the number stops before the '.', so the '.' becomes the next token, a T_BAD,
// and the error is reported at the '.'. A number cannot start with '.', so
// ".5" fails at the '.' as well. "1.5e3" is read as 1.5 followed by the bad
// character 'e', so that error lands on the 'e'.
static void lex_number(Parser *p)
{
    size_t start = p->cur, j = p->cur;
    unsigned long long acc = 0; // unsigned: no undefined behaviour on overflow
    while (j < p->end && is_digit(p->text[j])) {
        acc = acc * 10u + (unsigned)(p->text[j] - '0');
        j++;
    }
    if (j + 1 < p->end && p->text[j] == '.' && is_digit(p->text[j + 1])) {
        j++;
        while (j < p->end && is_digit(p->text[j]))
            j++;
        // strtod would happily read past our token (it accepts "1.5e3"), so
        // the character after the token is set to '\0' for the call and put
        // back afterwards. This avoids copying the digits, which matters for
        // numbers with thousands of digits. text[j] always exists because the
        // buffer has one extra byte after the file contents.
        char saved = p->text[j];
        p->text[j] = '\0';
        p->tok.val = make_float(strtod(p->text + start, NULL));
        p->text[j] = saved;
    } else {
        p->tok.val = make_int((long long)acc);
    }
    p->tok.kind = T_NUM;
    p->cur = j;
}

// Moves to the next token. Spaces and tabs are skipped; nothing else is.
static void next_token(Parser *p)
{
    while (p->cur < p->end && (p->text[p->cur] == ' ' || p->text[p->cur] == '\t'))
        p->cur++;
    p->tok.pos = p->cur + 1; // 0-based index -> 1-based position
    if (p->cur >= p->end) {
        p->tok.kind = T_END;
        return;
    }
    char c = p->text[p->cur];
    switch (c) {
    case '+': p->tok.kind = T_PLUS; p->cur++; return;
    case '-': p->tok.kind = T_MINUS; p->cur++; return;
    case '/': p->tok.kind = T_SLASH; p->cur++; return;
    case '(': p->tok.kind = T_LPAREN; p->cur++; return;
    case ')': p->tok.kind = T_RPAREN; p->cur++; return;
    case '*':
        // "**" is one token, so "2 ** ** 3" is two operators in a row and
        // fails at the second "**", while "2 * * 3" fails at the second '*'.
        if (p->cur + 1 < p->end && p->text[p->cur + 1] == '*') {
            p->tok.kind = T_POW;
            p->cur += 2;
        } else {
            p->tok.kind = T_STAR;
            p->cur++;
        }
        return;
    default:
        break;
    }
    if (is_digit(c)) {
        lex_number(p);
        return;
    }
    // Invalid character (including '#' that is not at the start of a line).
    // It is not an error yet: it becomes the current token, and the parser
    // fails at it as soon as it needs a token there. That way the error is the
    // first one the parser meets, and its position is the bad character.
    p->tok.kind = T_BAD;
}

// ---------------------------------------------------------------------------
// Parser and evaluator (recursive descent, evaluates while parsing)
// AI (Claude Opus 5.5): written by AI from the prompt in the header.
// ---------------------------------------------------------------------------
//
// Grammar, one function per precedence level:
//
//   expr    := term  { ('+' | '-') term }
//   term    := power { ('*' | '/') power }
//   power   := primary [ '**' power ]
//   primary := NUMBER | '(' expr ')'
//
// Why this gives the right precedence: a lower level only ever sees complete
// results of the level below it. expr combines whole terms, so in 3 + 2 * 5
// the term "2 * 5" is finished before '+' gets its right operand; * and / bind
// tighter than + and -, and ** tighter than both. Parentheses go back to the
// top level (expr), so anything inside them is complete before it is used.
//
// Why + - * / are left-associative: expr and term are loops that fold each new
// operand into the running result from left to right, so 7 - 2 - 1 is
// (7 - 2) - 1.
//
// Why ** is right-associative: the right side of ** is a whole power, not a
// primary. So 2 ** 3 ** 2 is 2 ** (3 ** 2) = 512. parse_power() does this with
// a loop and a stack instead of calling itself: it collects the chain
// a ** b ** c and then folds it from the right end. The result is the same as
// the recursive version, but a very long chain on one line (the lines can be
// megabytes long) cannot overflow the C call stack.
//
// Why errors are reported where they are:
//  * Syntax errors (invalid character, two operators in a row, missing
//    operand, missing or extra ')') are always reported at the start of the
//    current lookahead token, because that is exactly "the token the parser was
//    looking at when it could not continue". A missing operand at the end of a
//    line is the T_END token, whose position is the line's '\n'.
//  * Division by zero is reported at the start of the right operand: term()
//    remembers the position of the token after '/' before parsing the operand,
//    since the operand is what is wrong, not the '/'.
//  * A negative integer exponent is reported at the start of the exponent for
//    the same reason; with a chain the exponent is everything to the right of
//    that **, so it starts where the next operand starts.
//  * An infinite or NaN float result is reported at the operator, because both
//    operands are valid numbers and it is the operation that breaks.
//  Values are computed as soon as both operands are known, so the first error
//  the parser runs into, reading left to right, is the one that is reported.

static int parse_expr(Parser *p, Value *out);

// Applies one binary operator. op_pos is the operator's position and rhs_pos
// the position where the right operand starts; they are only used for errors.
static int apply(Parser *p, TokKind op, Value a, Value b, size_t op_pos, size_t rhs_pos, Value *out)
{
    if (!a.is_float && !b.is_float) {
        long long x = a.i, y = b.i;
        switch (op) {
        case T_PLUS:  *out = make_int(wrap_add(x, y)); return 1;
        case T_MINUS: *out = make_int(wrap_sub(x, y)); return 1;
        case T_STAR:  *out = make_int(wrap_mul(x, y)); return 1;
        case T_SLASH:
            if (y == 0)
                return fail(p, rhs_pos);
            // C's '/' already truncates toward zero, as the rules want.
            // LLONG_MIN / -1 is the one quotient that does not fit, and is
            // undefined behaviour, so dividing by -1 is done as a negation.
            *out = make_int(y == -1 ? wrap_sub(0, x) : x / y);
            return 1;
        case T_POW:
            if (y < 0)
                return fail(p, rhs_pos);
            *out = make_int(int_pow(x, y));
            return 1;
        default:
            break;
        }
    }

    // At least one operand is a float, so the operation is done in double.
    double x = as_double(a), y = as_double(b), r = 0.0;
    switch (op) {
    case T_PLUS:  r = x + y; break;
    case T_MINUS: r = x - y; break;
    case T_STAR:  r = x * y; break;
    case T_SLASH:
        // Checked before dividing: 1.5 / 0.0 must be a division-by-zero error
        // at the operand, not an "infinite result" error at the operator.
        if (y == 0.0)
            return fail(p, rhs_pos);
        r = x / y;
        break;
    case T_POW:   r = pow(x, y); break;
    default:      break;
    }
    if (!isfinite(r))
        return fail(p, op_pos);
    *out = make_float(r);
    return 1;
}

static int parse_primary(Parser *p, Value *out)
{
    if (p->tok.kind == T_NUM) {
        *out = p->tok.val;
        next_token(p);
        return 1;
    }
    if (p->tok.kind == T_LPAREN) {
        next_token(p);
        if (!parse_expr(p, out))
            return 0;
        // Anything but ')' here is an error at that token. At the end of the
        // line that token is T_END, which gives "missing )" at the '\n'.
        if (p->tok.kind != T_RPAREN)
            return fail(p, p->tok.pos);
        next_token(p);
        return 1;
    }
    // An operand was required but the token is an operator, ')', the end of
    // the line or an invalid character. This one check covers unary minus
    // ("-5" fails at the '-'), two operators in a row, "()" and missing
    // operands.
    return fail(p, p->tok.pos);
}

static void push_pow(Parser *p, Value v, size_t op_pos, size_t start)
{
    if (p->sp == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 64;
        p->stack = xrealloc(p->stack, p->cap * sizeof *p->stack);
    }
    PowItem item = {v, op_pos, start};
    p->stack[p->sp++] = item;
}

static int parse_power(Parser *p, Value *out)
{
    // All ** chains share one stack. A primary in the chain can contain
    // parentheses with their own chain, which is pushed above ours and fully
    // popped again before we continue, so remembering our base is enough.
    size_t base = p->sp;
    size_t start = p->tok.pos;
    size_t op_pos = 0;
    Value v;
    for (;;) {
        if (!parse_primary(p, &v))
            return 0;
        push_pow(p, v, op_pos, start);
        if (p->tok.kind != T_POW)
            break;
        op_pos = p->tok.pos;
        next_token(p);
        start = p->tok.pos;
    }

    // Fold from the right: acc holds the value of everything to the right of
    // stack[i-1], which is exactly the exponent of the ** in front of stack[i].
    Value acc = p->stack[p->sp - 1].v;
    for (size_t i = p->sp - 1; i > base; i--) {
        const PowItem *right = &p->stack[i];
        if (!apply(p, T_POW, p->stack[i - 1].v, acc, right->op_pos, right->start, &acc))
            return 0;
    }
    p->sp = base;
    *out = acc;
    return 1;
}

static int parse_term(Parser *p, Value *out)
{
    Value left, right;
    if (!parse_power(p, &left))
        return 0;
    while (p->tok.kind == T_STAR || p->tok.kind == T_SLASH) {
        TokKind op = p->tok.kind;
        size_t op_pos = p->tok.pos;
        next_token(p);
        size_t rhs_pos = p->tok.pos; // where the right operand starts
        if (!parse_power(p, &right))
            return 0;
        if (!apply(p, op, left, right, op_pos, rhs_pos, &left))
            return 0;
    }
    *out = left;
    return 1;
}

static int parse_expr(Parser *p, Value *out)
{
    Value left, right;
    if (!parse_term(p, &left))
        return 0;
    while (p->tok.kind == T_PLUS || p->tok.kind == T_MINUS) {
        TokKind op = p->tok.kind;
        size_t op_pos = p->tok.pos;
        next_token(p);
        size_t rhs_pos = p->tok.pos;
        if (!parse_term(p, &right))
            return 0;
        if (!apply(p, op, left, right, op_pos, rhs_pos, &left))
            return 0;
    }
    *out = left;
    return 1;
}

// Evaluates the line text[start, end). A complete expression must use up the
// whole line; anything left over (an extra ')', a second number, a '#') is an
// error at that token.
static int eval_line(Parser *p, size_t start, size_t end, Value *out)
{
    p->cur = start;
    p->end = end;
    p->sp = 0;
    next_token(p);
    if (!parse_expr(p, out))
        return 0;
    if (p->tok.kind != T_END)
        return fail(p, p->tok.pos);
    return 1;
}

// Evaluates a whole file held in text[0, len) and puts the output file's
// contents into out: one result per expression, or only "ERROR:<pos>" for the
// first error.
static void eval_text(char *text, size_t len, Buffer *out)
{
    Parser p;
    memset(&p, 0, sizeof p);
    p.text = text;

    size_t start = 0;
    while (start < len) {
        // memchr finds each '\n' once, so the whole file is scanned in O(n)
        // no matter how long the lines are.
        const char *nl = memchr(text + start, '\n', len - start);
        size_t end = nl ? (size_t)(nl - text) : len;

        size_t first = start;
        while (first < end && (text[first] == ' ' || text[first] == '\t'))
            first++;
        // Blank lines and comment lines produce no output, but they are still
        // part of the buffer, so their characters are counted in positions.
        if (first < end && text[first] != '#') {
            Value v;
            if (!eval_line(&p, start, end, &v)) {
                // One error replaces everything: drop the results so far.
                out->len = 0;
                buffer_reserve(out, 32);
                out->len += (size_t)snprintf(out->data, 32, "ERROR:%zu\n", p.err_pos);
                break;
            }
            buffer_reserve(out, 64); // enough for any %lld or %.15g
            int n = v.is_float
                ? snprintf(out->data + out->len, 64, "%.15g\n", v.d)
                : snprintf(out->data + out->len, 64, "%lld\n", v.i);
            out->len += (size_t)n;
        }
        start = end + 1;
    }
    free(p.stack);
}

// ---------------------------------------------------------------------------
// Files and folders
// AI (Claude Opus 5.5): written by AI from the prompt in the header.
// ---------------------------------------------------------------------------

// Reads the whole file into one buffer. The size is not known in advance
// (and ftell does not work on every kind of file), so the buffer doubles when
// full: each byte is copied O(1) times on average, which keeps files of
// several megabytes fast. One spare byte is kept for the final '\0' that
// lex_number() relies on.
static char *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return NULL;
    size_t cap = 1 << 16, len = 0;
    char *data = xrealloc(NULL, cap);
    for (;;) {
        if (len == cap - 1) {
            cap *= 2;
            data = xrealloc(data, cap);
        }
        size_t n = fread(data + len, 1, cap - 1 - len, f);
        len += n;
        if (n == 0)
            break;
    }
    if (ferror(f)) {
        int saved = errno;
        fclose(f);
        free(data);
        errno = saved ? saved : EIO;
        return NULL;
    }
    fclose(f);
    data[len] = '\0';
    *len_out = len;
    return data;
}

static int write_file(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return 0;
    int ok = len == 0 || fwrite(data, 1, len, f) == len;
    if (fclose(f) != 0) // a full disk often only shows up when flushing
        ok = 0;
    return ok;
}

// Creates the output folder, or accepts it if it already exists as a folder.
static int ensure_dir(const char *path)
{
    if (mkdir(path, 0777) == 0)
        return 1;
    if (errno == EEXIST) {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            return 1;
        errno = ENOTDIR;
    }
    return 0;
}

static int ends_with_txt(const char *name)
{
    size_t n = strlen(name);
    return n >= 4 && strcmp(name + n - 4, ".txt") == 0;
}

// "labs/task1.txt" -> "task1". Only a final ".txt" is removed.
static char *file_base(const char *path)
{
    const char *slash = strrchr(path, '/');
    char *base = xstrdup(slash ? slash + 1 : path);
    if (ends_with_txt(base))
        base[strlen(base) - 4] = '\0';
    return base;
}

// "../labs/" -> "labs". Trailing slashes are ignored so that -d labs and
// -d labs/ give the same default output folder.
static char *dir_base(const char *path)
{
    char *copy = xstrdup(path);
    size_t n = strlen(copy);
    while (n > 1 && copy[n - 1] == '/')
        copy[--n] = '\0';
    const char *slash = strrchr(copy, '/');
    char *base = xstrdup(slash && slash[1] != '\0' ? slash + 1 : copy);
    free(copy);
    return base;
}

// The default output folder: <input_base>_<name>_<studentid>.
static char *default_out_dir(const char *base)
{
    return concat3(base, "_" FIRST_NAME "_", STUDENT_ID);
}

// Evaluates one input file into out_dir/<base>_<name>_<lastname>_<id>.txt.
// Returns 1 on success. An ERROR result is a success: only a file that cannot
// be read or written is a failure.
static int process_file(const char *in_path, const char *out_dir, const char *base)
{
    size_t len = 0;
    char *text = read_file(in_path, &len);
    if (text == NULL) {
        fprintf(stderr, "calc: cannot read '%s': %s\n", in_path, strerror(errno));
        return 0;
    }
    Buffer out = {NULL, 0, 0};
    eval_text(text, len, &out);
    free(text);

    char *dir_slash = concat3(out_dir, "/", base);
    char *out_path = concat3(dir_slash, "_" FIRST_NAME "_" LAST_NAME "_" STUDENT_ID, ".txt");
    free(dir_slash);
    int ok = write_file(out_path, out.data, out.len);
    if (!ok)
        fprintf(stderr, "calc: cannot write '%s': %s\n", out_path, strerror(errno));
    free(out_path);
    free(out.data);
    return ok;
}

// calc [-o OUTDIR] input.txt
static int run_file(const char *input, const char *out_opt)
{
    // Check the input first, so a missing file does not leave an empty
    // output folder behind.
    struct stat st;
    if (stat(input, &st) != 0) {
        fprintf(stderr, "calc: cannot open '%s': %s\n", input, strerror(errno));
        return 1;
    }
    if (S_ISDIR(st.st_mode)) {
        fprintf(stderr, "calc: '%s' is a folder; use -d to evaluate a folder\n", input);
        return 1;
    }
    char *base = file_base(input);
    char *out_dir = out_opt ? xstrdup(out_opt) : default_out_dir(base);
    int status = 0;
    if (!ensure_dir(out_dir)) {
        fprintf(stderr, "calc: cannot create output folder '%s': %s\n", out_dir, strerror(errno));
        status = 1;
    } else if (!process_file(input, out_dir, base)) {
        status = 1;
    }
    free(out_dir);
    free(base);
    return status;
}

static int compare_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// calc -d DIR [-o OUTDIR]
static int run_dir(const char *dir, const char *out_opt)
{
    DIR *d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "calc: cannot open folder '%s': %s\n", dir, strerror(errno));
        return 1;
    }

    // All names are collected before anything is written. If OUTDIR is the
    // input folder itself, our own result files must not be picked up as new
    // inputs while we are still reading the folder.
    char **names = NULL;
    size_t count = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *name = e->d_name;
        // Like the shell pattern *.txt: hidden files are skipped.
        if (name[0] == '.' || !ends_with_txt(name))
            continue;
        // Only regular files; a subfolder called "x.txt" is ignored too.
        char *path = concat3(dir, "/", name);
        struct stat st;
        int is_file = stat(path, &st) == 0 && S_ISREG(st.st_mode);
        free(path);
        if (!is_file)
            continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            names = xrealloc(names, cap * sizeof *names);
        }
        names[count++] = xstrdup(name);
    }
    closedir(d);
    // readdir order is arbitrary; sorting makes runs (and error messages)
    // repeatable.
    if (count > 1)
        qsort(names, count, sizeof *names, compare_names);

    int status = 0;
    char *dbase = dir_base(dir);
    char *out_dir = out_opt ? xstrdup(out_opt) : default_out_dir(dbase);
    if (!ensure_dir(out_dir)) {
        fprintf(stderr, "calc: cannot create output folder '%s': %s\n", out_dir, strerror(errno));
        status = 1;
    } else {
        for (size_t i = 0; i < count; i++) {
            char *path = concat3(dir, "/", names[i]);
            char *base = file_base(names[i]);
            // Keep going after a failed file, so one unreadable file does not
            // hide the results of the others; the exit code still reports it.
            if (!process_file(path, out_dir, base))
                status = 1;
            free(base);
            free(path);
        }
    }
    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
    free(out_dir);
    free(dbase);
    return status;
}

// ---------------------------------------------------------------------------
// Command line
// AI (Claude Opus 5.5): written by AI from the prompt in the header.
// ---------------------------------------------------------------------------

static void usage(void)
{
    fprintf(stderr,
            "usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n"
            "  calc -o OUTDIR input.txt   evaluate one file\n"
            "  calc -d DIR -o OUTDIR      evaluate every *.txt file directly inside DIR\n");
}

int main(int argc, char **argv)
{
    const char *dir = NULL, *out = NULL, *input = NULL;

    if (argc < 2) {
        usage();
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int is_dir = strcmp(a, "-d") == 0 || strcmp(a, "--dir") == 0;
        int is_out = strcmp(a, "-o") == 0 || strcmp(a, "--output-dir") == 0;
        if (is_dir || is_out) {
            if (i + 1 >= argc) {
                fprintf(stderr, "calc: option '%s' needs a folder name\n", a);
                usage();
                return 1;
            }
            const char **slot = is_dir ? &dir : &out;
            if (*slot != NULL) {
                fprintf(stderr, "calc: option '%s' given more than once\n", a);
                return 1;
            }
            *slot = argv[++i];
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "calc: unknown option '%s'\n", a);
            usage();
            return 1;
        } else {
            if (input != NULL) {
                fprintf(stderr, "calc: only one input file can be given\n");
                usage();
                return 1;
            }
            input = a;
        }
    }

    if (dir != NULL && input != NULL) {
        fprintf(stderr, "calc: give either -d DIR or an input file, not both\n");
        usage();
        return 1;
    }
    if (dir != NULL)
        return run_dir(dir, out);
    if (input != NULL)
        return run_file(input, out);
    fprintf(stderr, "calc: no input file or folder given\n");
    usage();
    return 1;
}
