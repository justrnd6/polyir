// The nina_* runtime ABI bound in the VM (NinaRuntime.hpp). Each binding follows the LLVM piece of the same name: the hand-written IR of
// PolyirRuntimeText.cpp for printing and number text, runtime/NinaText.c (compiled into PolyirRuntimeStrings.cpp) for the text, console and
// file-system natives, and the math IR (round = floor(x + 0.5)). Strings are byte strings, indexes count bytes (a slice or a character cut
// out of a UTF-8 sequence decodes to U+FFFD), and the case and space rules are ASCII only, because that is what the C runtime does.
#include "vm/NinaRuntime.hpp"

#include <cmath>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace polyir::vm {

  namespace {

    using std::atan2;
    using std::ceil;
    using std::cos;
    using std::fabs;
    using std::fclose;
    using std::ferror;
    using std::fgetc;
    using std::floor;
    using std::fopen;
    using std::fputc;
    using std::fread;
    using std::fwrite;
    using std::pow;
    using std::rename;
    using std::sin;
    using std::sqrt;
    using std::ungetc;

    using NinaFn = Outcome (*)(Machine&, const Vector<Value>&);
    using Table = Map<String, NinaFn>;

    // ── arguments and results ──

    const String& textArg(const Vector<Value>& args, size_t i) {
      return TextOf(ArgumentAt(args, i));
    }

    int64_t wholeArg(const Vector<Value>& args, size_t i) {
      return IntOf(ArgumentAt(args, i));
    }

    double realArg(const Vector<Value>& args, size_t i) {
      return FloatOf(ArgumentAt(args, i));
    }

    // A text result carries a fresh identity, taken once the text is known, as the host layer's results do.
    Outcome textResult(Machine& m, String s) {
      return ValueOutcome(StrValue(m.heap, move(s)));
    }

    // The ABI's whole results are all i64 (statuses, handles, bytes, positions).
    Outcome wholeResult(int64_t v) {
      return ValueOutcome(I64Value(v));
    }

    Outcome realResult(double v) {
      return ValueOutcome(FloatValue(v, 64));
    }

    // ── printing (PolyirRuntimeText.cpp) ──

    Outcome printText(Machine& m, const Vector<Value>& args) {
      *m.out << textArg(args, 0);

      return VoidOutcome();
    }

    Outcome printlnText(Machine& m, const Vector<Value>& args) {
      *m.out << textArg(args, 0) << "\n";

      return VoidOutcome();
    }

    // The IR writes standard error unbuffered (write(2)) while stdout is stdio-buffered. The VM flushes its own stream first, so the
    // two keep their order when both are the terminal.
    Outcome eprintText(Machine& m, const Vector<Value>& args) {
      m.out->flush();
      cerr << textArg(args, 0);

      return VoidOutcome();
    }

    Outcome eprintlnText(Machine& m, const Vector<Value>& args) {
      m.out->flush();
      cerr << textArg(args, 0) << "\n";

      return VoidOutcome();
    }

    // `exit((i32) fptosi(code))`: a code outside the i32 range (or nan) is poison in the IR, 0 here.
    int exitStatusOf(double code) {
      return fabs(code) < 2147483648.0 ? static_cast<int>(code) : 0;
    }

    Outcome processExit(Machine& m, const Vector<Value>& args) {
      m.out->flush();
      cout.flush();
      exit(exitStatusOf(realArg(args, 0)));
    }

    // ── number text (PolyirRuntimeText.cpp) ──

    Outcome textFromBool(Machine& m, const Vector<Value>& args) {
      return textResult(m, Truthy(ArgumentAt(args, 0)) ? "true" : "false");
    }

    Outcome textFromI64(Machine& m, const Vector<Value>& args) {
      return textResult(m, to_string(wholeArg(args, 0)));
    }

    // A u64 above the largest i64 is carried as a negative number: its digits are the unsigned reading's.
    Outcome textFromU64(Machine& m, const Vector<Value>& args) {
      return textResult(m, to_string(static_cast<uint64_t>(wholeArg(args, 0))));
    }

    // All 39 digits, as the LLVM piece prints them: the payload is 128 bits wide (VmValues.hpp).
    Outcome textFromI128(Machine& m, const Vector<Value>& args) {
      return textResult(m, WideText(WideOf(ArgumentAt(args, 0)), true));
    }

    Outcome textFromU128(Machine& m, const Vector<Value>& args) {
      return textResult(m, WideText(WideOf(ArgumentAt(args, 0)), false));
    }

    // The Double text every Nina route shares (java21's NinaRt.f64, cpp17's nina_f64, the llvm route's NinaText.c): an integral value below
    // 2^53 prints as an integer (`3`, or `3.0` in the literal form println uses), anything else as the VM's floatText: the shortest digits
    // that read back exactly (`0.30000000000000004`, `1e+20`, `9007199254740992.0`), `NaN`, `Infinity`.
    // 2^53: every integral double below it is exact, so it prints as an integer.
    constexpr double LARGEST_EXACT_INTEGER = 9007199254740992.0;

    bool isExactInteger(double v) {
      return floor(v) == v && fabs(v) < LARGEST_EXACT_INTEGER;
    }

    String doubleText(double v) {
      if (isExactInteger(v)) {
        return to_string(static_cast<long long>(v));
      }

      return FloatText(v);
    }

    // The literal form println uses keeps a whole number reading as a Double (`3.0`).
    String doubleLiteralText(double v) {
      if (isExactInteger(v)) {
        return to_string(static_cast<long long>(v)) + ".0";
      }

      return FloatText(v);
    }

    Outcome textFromF64(Machine& m, const Vector<Value>& args) {
      return textResult(m, doubleText(realArg(args, 0)));
    }

    Outcome textFromF64Lit(Machine& m, const Vector<Value>& args) {
      return textResult(m, doubleLiteralText(realArg(args, 0)));
    }

    // ── the String natives (NinaText.c) ──

    bool isSpace(char c) {
      return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    }

    Outcome textTrim(Machine& m, const Vector<Value>& args) {
      const String& s = textArg(args, 0);
      size_t from = 0, to = s.size();

      while (from < to && isSpace(s[from])) {
        from++;
      }

      while (to > from && isSpace(s[to - 1])) {
        to--;
      }

      return textResult(m, s.substr(from, to - from));
    }

    // ASCII letters only; every other byte (UTF-8 sequences among them) stays as it is.
    char mappedCase(char c, bool upper) {
      if (upper) {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c;
      }

      return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c;
    }

    String mapped(const String& s, bool upper) {
      String out = s;

      for (char& c : out) {
        c = mappedCase(c, upper);
      }

      return out;
    }

    Outcome textUpper(Machine& m, const Vector<Value>& args) {
      return textResult(m, mapped(textArg(args, 0), true));
    }

    Outcome textLower(Machine& m, const Vector<Value>& args) {
      return textResult(m, mapped(textArg(args, 0), false));
    }

    // ── UTF-8 decoding of cut bytes ──

    // How many bytes the sequence that `lead` starts holds; 0 for a byte that cannot start one.
    size_t sequenceSize(unsigned char lead) {
      if (lead < 0x80) {
        return 1;
      }

      if (lead < 0xC2) {
        return 0;
      }

      if (lead < 0xE0) {
        return 2;
      }

      if (lead < 0xF0) {
        return 3;
      }

      if (lead < 0xF5) {
        return 4;
      }

      return 0;
    }

    // The lowest byte that may come right after `lead`: after E0 and F0 it starts higher, so that no character has an overlong form.
    unsigned char lowestSecondByte(unsigned char lead) {
      if (lead == 0xE0) {
        return 0xA0;
      }

      if (lead == 0xF0) {
        return 0x90;
      }

      return 0x80;
    }

    // The highest byte that may come right after `lead`: after ED it stops below the surrogates, after F4 at U+10FFFF.
    unsigned char highestSecondByte(unsigned char lead) {
      if (lead == 0xED) {
        return 0x9F;
      }

      if (lead == 0xF4) {
        return 0x8F;
      }

      return 0xBF;
    }

    // Whether `byte` may follow the bytes of a sequence `lead` starts. `second` says it is the byte right after the lead, whose range
    // depends on the lead. Every later byte is a plain continuation byte, 0x80..0xBF.
    bool isContinuationByte(unsigned char lead, bool second, unsigned char byte) {
      if (!second) {
        return byte >= 0x80 && byte <= 0xBF;
      }

      return byte >= lowestSecondByte(lead) && byte <= highestSecondByte(lead);
    }

    // Whether the byte `length` bytes after `at` (before `end`) carries on the sequence that starts at `at`.
    bool sequenceGoesOn(const String& s, size_t at, size_t length, size_t end) {
      const unsigned char lead = static_cast<unsigned char>(s[at]);

      if (length >= sequenceSize(lead) || at + length >= end) {
        return false;
      }

      return isContinuationByte(lead, length == 1, static_cast<unsigned char>(s[at + length]));
    }

    // How many bytes from `at` (before `end`) belong to the sequence there: a whole character, or the maximal invalid subpart.
    size_t partLength(const String& s, size_t at, size_t end) {
      size_t length = 1;

      while (sequenceGoesOn(s, at, length, end)) {
        length++;
      }

      return length;
    }

    // Whether the sequence at `at` (before `end`) is a whole character rather than an invalid subpart.
    bool whole(const String& s, size_t at, size_t end) {
      return partLength(s, at, end) == sequenceSize(static_cast<unsigned char>(s[at]));
    }

    // Adds the whole characters from `run` up to `at` and one U+FFFD for the invalid subpart at `at`; where the next run starts.
    size_t replaced(String& out, const String& s, size_t run, size_t at, size_t end) {
      out.append(s, run, at - run);
      out += "\xEF\xBF\xBD";

      return at + partLength(s, at, end);
    }

    // Adds the bytes [from, to) of `s` to `out` up to the last invalid subpart, each one replaced; where the rest starts.
    size_t replacedUpTo(String& out, const String& s, size_t from, size_t to) {
      size_t run = from;

      for (size_t at = from; at < to; at += partLength(s, at, to)) {
        if (!whole(s, at, to)) {
          run = replaced(out, s, run, at, to);
        }
      }

      return run;
    }

    // The bytes [from, to) of `s` as UTF-8 text: a whole character stays as it is. Every maximal invalid subpart (a sequence cut at
    // either end, a stray continuation byte, an invalid lead) becomes one U+FFFD. That is how TextDecoder and NinaText.c read them.
    String decoded(const String& s, size_t from, size_t to) {
      String out;
      const size_t rest = replacedUpTo(out, s, from, to);

      return out.append(s, rest, to - rest);
    }

    // Both ends are clamped into [0, size] and a `from` past `to` is `to` (so a negative `to` gives ""); the bytes between are decoded.
    Outcome textSlice(Machine& m, const Vector<Value>& args) {
      const String& s = textArg(args, 0);
      const int64_t size = static_cast<int64_t>(s.size());
      const int64_t to = min<int64_t>(max<int64_t>(wholeArg(args, 2), 0), size);
      const int64_t from = min<int64_t>(max<int64_t>(wholeArg(args, 1), 0), to);

      return textResult(m, decoded(s, static_cast<size_t>(from), static_cast<size_t>(to)));
    }

    bool inside(const String& s, int64_t i) {
      return i >= 0 && i < static_cast<int64_t>(s.size());
    }

    // The byte at `i` decoded: an ASCII character, U+FFFD for any other byte, "" outside the text.
    Outcome textCharAt(Machine& m, const Vector<Value>& args) {
      const String& s = textArg(args, 0);
      const int64_t i = wholeArg(args, 1);

      return textResult(m, inside(s, i) ? decoded(s, static_cast<size_t>(i), static_cast<size_t>(i) + 1) : String());
    }

    // The unsigned byte at `i` (0..255), -1 outside the text.
    Outcome textCodeAt(Machine&, const Vector<Value>& args) {
      const String& s = textArg(args, 0);
      const int64_t i = wholeArg(args, 1);

      return wholeResult(inside(s, i) ? static_cast<unsigned char>(s[static_cast<size_t>(i)]) : -1);
    }

    // The first match at or after `from` (a negative `from` is 0), -1 when there is none. It is -1 past the end too, where polyir's
    // own Find clamps. An empty needle is found at `from` itself while `from` is inside the text, which is what String::find gives.
    Outcome textIndexOfFrom(Machine&, const Vector<Value>& args) {
      const String& s = textArg(args, 0);
      const size_t from = static_cast<size_t>(max<int64_t>(wholeArg(args, 2), 0));
      const size_t at = s.find(textArg(args, 1), from);

      return wholeResult(at == String::npos ? -1 : static_cast<int64_t>(at));
    }

    bool isAlnum(char c) {
      return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    // 1 when every byte is an ASCII letter or digit, 0 otherwise and for "".
    Outcome textIsAlnum(Machine&, const Vector<Value>& args) {
      const String& s = textArg(args, 0);

      return wholeResult(!s.empty() && all_of(s.begin(), s.end(), isAlnum) ? 1 : 0);
    }

    // One raw byte, not UTF-8 (unlike the VM's own charText): the IR writes `(char) code` into a 1-byte string, so a code of 0 gives "".
    Outcome textFromCode(Machine& m, const Vector<Value>& args) {
      const char c = static_cast<char>(wholeArg(args, 0));

      return textResult(m, c == 0 ? String() : String(1, c));
    }

    // ── the Console natives (NinaText.c) ──

    // The characters up to a newline or the end of the stream; the newline is dropped. `dropCr` also drops a carriage return before that
    // newline (the file reader does, the console reader keeps it). "" at the end, and for no stream at all.
    String lineFrom(FILE* f, bool dropCr) {
      String line;
      int c = f == nullptr ? EOF : fgetc(f);

      for (; c != EOF && c != '\n'; c = fgetc(f)) {
        line += static_cast<char>(c);
      }

      if (dropCr && c == '\n' && !line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      return line;
    }

    // Flushed first so a prompt printed without a newline is on the terminal before the program waits, as stdio does for the IR.
    Outcome consoleReadLine(Machine& m, const Vector<Value>&) {
      m.out->flush();

      return textResult(m, lineFrom(stdin, false));
    }

    // strtod: leading spaces skipped, trailing text ignored, 0 when there is no number; `inf` and `nan` read.
    Outcome textParseF64(Machine&, const Vector<Value>& args) {
      return realResult(strtod(textArg(args, 0).c_str(), nullptr));
    }

    // strtol: leading spaces, an optional sign, the digits up to the first other character ("3.7" is 3, "abc" is 0), saturating on overflow.
    Outcome textParseI64(Machine&, const Vector<Value>& args) {
      return wholeResult(strtoll(textArg(args, 0).c_str(), nullptr, 10));
    }

    // ── Math (PolyirRuntimeStrings.cpp's math IR) ──

    Outcome mathSqrt(Machine&, const Vector<Value>& args) {
      return realResult(sqrt(realArg(args, 0)));
    }

    Outcome mathFloor(Machine&, const Vector<Value>& args) {
      return realResult(floor(realArg(args, 0)));
    }

    Outcome mathCeil(Machine&, const Vector<Value>& args) {
      return realResult(ceil(realArg(args, 0)));
    }

    // Math.round rounds halves up, floor(x + 0.5), as the IR and the Java, Kotlin and JavaScript targets do (round(-2.5) is -2).
    Outcome mathRound(Machine&, const Vector<Value>& args) {
      return realResult(floor(realArg(args, 0) + 0.5));
    }

    Outcome mathSin(Machine&, const Vector<Value>& args) {
      return realResult(sin(realArg(args, 0)));
    }

    Outcome mathCos(Machine&, const Vector<Value>& args) {
      return realResult(cos(realArg(args, 0)));
    }

    Outcome mathPow(Machine&, const Vector<Value>& args) {
      return realResult(pow(realArg(args, 0), realArg(args, 1)));
    }

    Outcome mathAtan2(Machine&, const Vector<Value>& args) {
      return realResult(atan2(realArg(args, 0), realArg(args, 1)));
    }

    // ── FileHost (NinaText.c): scalar only, never throwing — a mutating call answers 0 ok / 1 failed, a probing one its "missing" value ──

    bool statOf(const String& path, struct stat& st) {
      return stat(path.c_str(), &st) == 0;
    }

    bool isDir(const String& path) {
      struct stat st;

      return statOf(path, st) && S_ISDIR(st.st_mode);
    }

    bool isFile(const String& path) {
      struct stat st;

      return statOf(path, st) && S_ISREG(st.st_mode);
    }

    Outcome fsExists(Machine&, const Vector<Value>& args) {
      struct stat st;

      return wholeResult(statOf(textArg(args, 0), st) ? 1 : 0);
    }

    Outcome fsIsDir(Machine&, const Vector<Value>& args) {
      return wholeResult(isDir(textArg(args, 0)) ? 1 : 0);
    }

    Outcome fsIsFile(Machine&, const Vector<Value>& args) {
      return wholeResult(isFile(textArg(args, 0)) ? 1 : 0);
    }

    // `mkdir` for one directory: fine when it already is one.
    bool madeDir(const String& path) {
      return mkdir(path.c_str(), 0777) == 0 || isDir(path);
    }

    // `mkdir -p` for the directories ABOVE a path: the parents a file needs before it can be created.
    void makeParents(const String& path) {
      for (size_t i = 1; i < path.size(); i++) {
        if (path[i] == '/') {
          madeDir(path.substr(0, i));
        }
      }
    }

    Outcome fsCreateDir(Machine&, const Vector<Value>& args) {
      const String& path = textArg(args, 0);
      makeParents(path);

      return wholeResult(madeDir(path) ? 0 : 1);
    }

    // Reads 4 KiB at a time until fread gives nothing more.
    String readWhole(FILE* file) {
      String out;
      char chunk[4096];

      for (size_t got = fread(chunk, 1, sizeof chunk, file); got > 0; got = fread(chunk, 1, sizeof chunk, file)) {
        out.append(chunk, got);
      }

      return out;
    }

    // The whole file; "" when it cannot be opened (and for a directory, whose fread gives nothing).
    Outcome fsReadText(Machine& m, const Vector<Value>& args) {
      FILE* f = fopen(textArg(args, 0).c_str(), "rb");

      if (f == nullptr) {
        return textResult(m, "");
      }

      String out = readWhole(f);
      fclose(f);

      return textResult(m, move(out));
    }

    // Writes the whole text through a fresh stream in `mode` ("wb" truncates, "ab" appends), creating the parents first.
    int64_t put(const String& path, const String& content, const char* mode) {
      makeParents(path);
      FILE* f = fopen(path.c_str(), mode);

      if (f == nullptr) {
        return 1;
      }

      const bool ok = fwrite(content.data(), 1, content.size(), f) == content.size();

      return fclose(f) == 0 && ok ? 0 : 1;
    }

    Outcome fsWriteText(Machine&, const Vector<Value>& args) {
      return wholeResult(put(textArg(args, 0), textArg(args, 1), "wb"));
    }

    Outcome fsAppend(Machine&, const Vector<Value>& args) {
      return wholeResult(put(textArg(args, 0), textArg(args, 1), "ab"));
    }

    // One file or one empty directory (a symlink itself, not its target); a missing path is already gone.
    int64_t deleted(const String& path) {
      struct stat st;

      if (lstat(path.c_str(), &st) != 0) {
        return 0;
      }

      if (S_ISDIR(st.st_mode)) {
        return rmdir(path.c_str()) == 0 ? 0 : 1;
      }

      return unlink(path.c_str()) == 0 ? 0 : 1;
    }

    Outcome fsDelete(Machine&, const Vector<Value>& args) {
      return wholeResult(deleted(textArg(args, 0)));
    }

    bool isDot(const char* name) {
      return name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0));
    }

    // The entry names of an open directory without "." and "..", in byte order (strcmp's, which String's compare shares).
    Vector<String> entryNames(DIR* dir) {
      Vector<String> names;

      for (dirent* e = readdir(dir); e != nullptr; e = readdir(dir)) {
        if (!isDot(e->d_name)) {
          names.push_back(e->d_name);
        }
      }

      sort(names.begin(), names.end());

      return names;
    }

    String joinedLines(const Vector<String>& names) {
      String out;

      for (size_t i = 0; i < names.size(); i++) {
        out += (i == 0 ? "" : "\n") + names[i];
      }

      return out;
    }

    // The names joined with "\n" (no trailing one); "" when the directory cannot be opened.
    Outcome fsList(Machine& m, const Vector<Value>& args) {
      DIR* dir = opendir(textArg(args, 0).c_str());

      if (dir == nullptr) {
        return textResult(m, "");
      }

      const Vector<String> names = entryNames(dir);
      closedir(dir);

      return textResult(m, joinedLines(names));
    }

    bool removedTree(const String& path);

    bool removedChild(const String& child) {
      struct stat st;

      if (lstat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return removedTree(child);
      }

      return unlink(child.c_str()) == 0;
    }

    // Removes the tree under `path` (symlinks are unlinked, not followed), then the directory itself.
    bool removedTree(const String& path) {
      DIR* dir = opendir(path.c_str());

      if (dir == nullptr) {
        return false;
      }

      bool ok = true;

      for (const String& name : entryNames(dir)) {
        ok = removedChild(path + "/" + name) && ok;
      }

      closedir(dir);

      return rmdir(path.c_str()) == 0 && ok;
    }

    Outcome fsRemoveDir(Machine&, const Vector<Value>& args) {
      const String& path = textArg(args, 0);
      struct stat st;

      if (lstat(path.c_str(), &st) != 0) {
        return wholeResult(0);
      }

      if (!S_ISDIR(st.st_mode)) {
        return wholeResult(unlink(path.c_str()) == 0 ? 0 : 1);
      }

      return wholeResult(removedTree(path) ? 0 : 1);
    }

    // realpath(3): absolute, `.`/`..` and symlinks resolved; "" when the path does not exist (or cannot be resolved).
    Outcome fsCanonical(Machine& m, const Vector<Value>& args) {
      char resolvedPath[4096];
      const char* resolved = realpath(textArg(args, 0).c_str(), resolvedPath);

      return textResult(m, resolved == nullptr ? "" : resolvedPath);
    }

    // The modification time in milliseconds.
    int64_t modifiedMillis(const struct stat& st) {
#ifdef __APPLE__
      return static_cast<int64_t>(st.st_mtimespec.tv_sec) * 1000 + st.st_mtimespec.tv_nsec / 1000000;
#else
      return static_cast<int64_t>(st.st_mtim.tv_sec) * 1000 + st.st_mtim.tv_nsec / 1000000;
#endif
    }

    Outcome fsModified(Machine&, const Vector<Value>& args) {
      struct stat st;

      return wholeResult(statOf(textArg(args, 0), st) ? modifiedMillis(st) : 0);
    }

    Outcome fsSize(Machine&, const Vector<Value>& args) {
      struct stat st;

      return wholeResult(statOf(textArg(args, 0), st) ? static_cast<int64_t>(st.st_size) : 0);
    }

    // The next piece of `in`, at most the chunk's size; 0 at the end (or on an error).
    size_t readChunk(FILE* in, Vector<char>& chunk) {
      return fread(chunk.data(), 1, chunk.size(), in);
    }

    // Everything left in `in` goes to `out` in 64 KiB pieces; whether all of it arrived.
    bool pumped(FILE* in, FILE* out) {
      Vector<char> chunk(65536);
      bool isComplete = true;

      for (size_t got = readChunk(in, chunk); got > 0; got = readChunk(in, chunk)) {
        isComplete = fwrite(chunk.data(), 1, got, out) == got && isComplete;
      }

      return isComplete && !ferror(in);
    }

    int64_t copiedInto(FILE* in, const String& to) {
      makeParents(to);
      FILE* out = fopen(to.c_str(), "wb");

      if (out == nullptr) {
        return 1;
      }

      const bool ok = pumped(in, out);

      return fclose(out) == 0 && ok ? 0 : 1;
    }

    // 1 unless `from` is a regular file that opens; the parents of `to` are made.
    int64_t copied(const String& from, const String& to) {
      if (!isFile(from)) {
        return 1;
      }

      FILE* in = fopen(from.c_str(), "rb");

      if (in == nullptr) {
        return 1;
      }

      const int64_t status = copiedInto(in, to);
      fclose(in);

      return status;
    }

    Outcome fsCopy(Machine&, const Vector<Value>& args) {
      return wholeResult(copied(textArg(args, 0), textArg(args, 1)));
    }

    // A rename, or (when the OS cannot rename, e.g. across devices) a copy and a delete of a file.
    int64_t moved(const String& from, const String& to) {
      makeParents(to);

      if (rename(from.c_str(), to.c_str()) == 0) {
        return 0;
      }

      if (!isFile(from) || isDir(to)) {
        return 1;
      }

      if (copied(from, to) != 0) {
        return 1;
      }

      return deleted(from);
    }

    Outcome fsMove(Machine&, const Vector<Value>& args) {
      return wholeResult(moved(textArg(args, 0), textArg(args, 1)));
    }

    // ── FileHost streams ──
    // The open streams by handle (1, 2, 3, …): a slot goes null when closed and is never reused, 0 is never a handle — the C runtime's registry,
    // so FileSink/FileSource see the same handle values on both routes. Process-wide, as that registry is.
    Vector<FILE*> g_streams;

    int64_t registered(FILE* f) {
      g_streams.push_back(f);

      return static_cast<int64_t>(g_streams.size());
    }

    FILE* streamOf(int64_t handle) {
      if (handle < 1 || handle > static_cast<int64_t>(g_streams.size())) {
        return nullptr;
      }

      return g_streams[static_cast<size_t>(handle - 1)];
    }

    // Closes the handle's stream (fine when already closed) and answers the close's status.
    int64_t released(int64_t handle) {
      FILE* f = streamOf(handle);

      if (f == nullptr) {
        return 0;
      }

      g_streams[static_cast<size_t>(handle - 1)] = nullptr;

      return fclose(f) == 0 ? 0 : 1;
    }

    // The fopen mode of a file handle's mode: 0 reads, 1 truncates and writes, anything else appends.
    const char* modeText(int64_t mode) {
      if (mode == 0) {
        return "rb";
      }

      if (mode == 1) {
        return "wb";
      }

      return "ab";
    }

    // Mode 0 reads, 1 truncates, anything else appends (the parents made first). The answer is 0 when the file cannot be opened,
    // or is a directory opened to read.
    int64_t opened(const String& path, int64_t mode) {
      if (mode != 0) {
        makeParents(path);
      }

      FILE* f = fopen(path.c_str(), modeText(mode));

      if (f == nullptr) {
        return 0;
      }

      if (mode == 0 && isDir(path)) {
        fclose(f);
        return 0;
      }

      return registered(f);
    }

    Outcome fsOpen(Machine&, const Vector<Value>& args) {
      return wholeResult(opened(textArg(args, 0), wholeArg(args, 1)));
    }

    // 1 for a closed or unknown handle; a short write closes the handle and answers 1, as the C runtime does.
    Outcome fsWrite(Machine&, const Vector<Value>& args) {
      FILE* f = streamOf(wholeArg(args, 0));

      if (f == nullptr) {
        return wholeResult(1);
      }

      const String& s = textArg(args, 1);

      if (fwrite(s.data(), 1, s.size(), f) == s.size()) {
        return wholeResult(0);
      }

      released(wholeArg(args, 0));

      return wholeResult(1);
    }

    Outcome fsWriteByte(Machine&, const Vector<Value>& args) {
      FILE* f = streamOf(wholeArg(args, 0));

      if (f == nullptr) {
        return wholeResult(1);
      }

      if (fputc(static_cast<int>(wholeArg(args, 1) & 255), f) != EOF) {
        return wholeResult(0);
      }

      released(wholeArg(args, 0));

      return wholeResult(1);
    }

    Outcome fsClose(Machine&, const Vector<Value>& args) {
      return wholeResult(released(wholeArg(args, 0)));
    }

    // The next line without its newline (a carriage return before it dropped too); "" at the end and for a closed handle.
    Outcome fsReadLine(Machine& m, const Vector<Value>& args) {
      return textResult(m, lineFrom(streamOf(wholeArg(args, 0)), true));
    }

    // 1 at the end of the stream (peeked, then given back) and for a closed handle.
    Outcome fsAtEnd(Machine&, const Vector<Value>& args) {
      FILE* f = streamOf(wholeArg(args, 0));

      if (f == nullptr) {
        return wholeResult(1);
      }

      const int c = fgetc(f);

      if (c == EOF) {
        return wholeResult(1);
      }

      ungetc(c, f);

      return wholeResult(0);
    }

    // The next byte (0..255), -1 at the end and for a closed handle.
    Outcome fsReadByte(Machine&, const Vector<Value>& args) {
      FILE* f = streamOf(wholeArg(args, 0));
      const int c = f == nullptr ? EOF : fgetc(f);

      return wholeResult(c == EOF ? -1 : c);
    }

    // ── the table ──

    void addPrinting(Table& t) {
      t["nina_print_text"] = printText;
      t["nina_println_text"] = printlnText;
      t["nina_eprint_text"] = eprintText;
      t["nina_eprintln_text"] = eprintlnText;
      t["nina_process_exit"] = processExit;
    }

    void addNumberText(Table& t) {
      t["nina_text_from_bool"] = textFromBool;
      t["nina_text_from_i64"] = textFromI64;
      t["nina_text_from_u64"] = textFromU64;
      t["nina_text_from_i128"] = textFromI128;
      t["nina_text_from_u128"] = textFromU128;

      t["nina_text_from_f64"] = textFromF64;
      t["nina_text_from_f64_lit"] = textFromF64Lit;
    }

    void addTextEdit(Table& t) {
      t["nina_text_trim"] = textTrim;
      t["nina_text_upper"] = textUpper;
      t["nina_text_lower"] = textLower;

      t["nina_text_slice"] = textSlice;
      t["nina_text_char_at"] = textCharAt;
      t["nina_text_from_code"] = textFromCode;
    }

    void addTextSearch(Table& t) {
      t["nina_text_code_at"] = textCodeAt;
      t["nina_text_index_of_from"] = textIndexOfFrom;
      t["nina_text_is_alnum"] = textIsAlnum;

      t["nina_console_read_line"] = consoleReadLine;
      t["nina_text_parse_f64"] = textParseF64;
      t["nina_text_parse_i64"] = textParseI64;
    }

    void addMathOfOne(Table& t) {
      t["nina_math_sqrt"] = mathSqrt;
      t["nina_math_floor"] = mathFloor;
      t["nina_math_ceil"] = mathCeil;
      t["nina_math_round"] = mathRound;

      t["nina_math_sin"] = mathSin;
      t["nina_math_cos"] = mathCos;
    }

    void addMath(Table& t) {
      addMathOfOne(t);
      t["nina_math_pow"] = mathPow;
      t["nina_math_atan2"] = mathAtan2;
    }

    void addFileProbes(Table& t) {
      t["nina_fs_exists"] = fsExists;
      t["nina_fs_is_dir"] = fsIsDir;
      t["nina_fs_is_file"] = fsIsFile;

      t["nina_fs_modified"] = fsModified;
      t["nina_fs_size"] = fsSize;
      t["nina_fs_canonical"] = fsCanonical;
      t["nina_fs_list"] = fsList;
    }

    void addFileEdits(Table& t) {
      t["nina_fs_read_text"] = fsReadText;
      t["nina_fs_write_text"] = fsWriteText;
      t["nina_fs_append"] = fsAppend;
      t["nina_fs_delete"] = fsDelete;
    }

    void addFileTree(Table& t) {
      t["nina_fs_create_dir"] = fsCreateDir;
      t["nina_fs_remove_dir"] = fsRemoveDir;
      t["nina_fs_copy"] = fsCopy;
      t["nina_fs_move"] = fsMove;
    }

    void addFileStreams(Table& t) {
      t["nina_fs_open"] = fsOpen;
      t["nina_fs_write"] = fsWrite;
      t["nina_fs_write_byte"] = fsWriteByte;

      t["nina_fs_read_line"] = fsReadLine;
      t["nina_fs_at_end"] = fsAtEnd;
      t["nina_fs_read_byte"] = fsReadByte;
      t["nina_fs_close"] = fsClose;
    }

    void addFiles(Table& t) {
      addFileProbes(t);
      addFileEdits(t);
      addFileTree(t);
      addFileStreams(t);
    }

    void addText(Table& t) {
      addTextEdit(t);
      addTextSearch(t);
    }

    Table fullTable() {
      Table t;

      addPrinting(t);
      addNumberText(t);
      addText(t);
      addMath(t);
      addFiles(t);

      return t;
    }

    const Table& table() {
      static const Table t = fullTable();

      return t;
    }

  }  // namespace

  bool BindsNinaRuntime(const String& name) {
    return table().count(name) != 0;
  }

  Outcome CallNinaRuntime(Machine& m, const String& name, const Vector<Value>& args) {
    const auto bound = table().find(name);

    if (bound == table().end()) {
      return UnboundExternTrap(name);
    }

    return bound->second(m, args);
  }

}  // namespace polyir::vm
