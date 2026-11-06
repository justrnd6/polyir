// The nina_* runtime ABI the bytecode VM binds (src/vm/NinaRuntime.cpp), on modules built with PolyIRBuilder that declare the externs the
// way ninac's polyir lowering does (ninac/codegen/polyir-lower/Runtime.cpp, CollectionAbi.cpp). Every expected value is what the LLVM
// route's runtime pieces give (PolyirRuntimeText.cpp, runtime/NinaText.c): shortest round-trip floats, byte-indexed ASCII-only text, -1 for a missing
// needle, 1-based file handles that are never reused, a carriage return dropped by the file reader. Each program's `main` appends what it
// notes to a string, comma first, and returns it.
#include "polyir/Std.hpp"
#include "polyir/Builder.hpp"
#include "polyir/Vm.hpp"

using namespace polyir;

namespace {

  int failures = 0;

  void pass(const String& name, const String& got) {
    cout << "ok   " << name << " = " << got << "\n";
  }

  void fail(const String& name, const String& want, const String& got) {
    cout << "FAIL " << name << ": expected " << want << ", got " << got << "\n";
    failures++;
  }

  String shown(const VmOutcome& ran) {
    return ran.ok ? ran.result : "trap: " + ran.trap;
  }

  VmOutcome ran(const Module& program, OStream& out) {
    VmOptions options;
    options.out = &out;

    return RunModule(program, options);
  }

  void expectText(const String& name, const Module& program, const String& want) {
    OStringStream out;
    const VmOutcome r = ran(program, out);

    if (r.ok && r.result == want) {
      pass(name, r.result);
    } else {
      fail(name, want, shown(r));
    }
  }

  void expectPrinted(const String& name, const Module& program, const String& want) {
    OStringStream out;
    const VmOutcome r = ran(program, out);

    if (r.ok && out.str() == want) {
      pass(name, "printed " + out.str());
    } else {
      fail(name, "printed " + want, shown(r) + " printed " + out.str());
    }
  }

  void expectDeclined(const String& name, const Module& program, const String& want) {
    OStringStream out;
    const VmOutcome r = ran(program, out);

    if (r.declined && r.declineReason == want) {
      pass(name, "declined: " + r.declineReason);
    } else {
      fail(name, "declined: " + want, shown(r));
    }
  }

  // ── the externs, declared with the ABI's signatures (parameters p0, p1, …, as polyir_lower names them) ──

  TypePtr text() {
    return MakeString();
  }

  TypePtr whole() {
    return MakeInt(64);
  }

  TypePtr real() {
    return MakeFloat(64);
  }

  void abi(PolyIRBuilder& b, const String& name, const Vector<TypePtr>& params, TypePtr result) {
    Vector<Parameter> ps;

    for (const TypePtr& t : params) {
      ps.push_back({"p" + to_string(ps.size()), t});
    }

    DeclareExtern(b, name, move(ps), move(result));
  }

  void declareNumberText(PolyIRBuilder& b) {
    abi(b, "nina_text_from_f64", {real()}, text());
    abi(b, "nina_text_from_f64_lit", {real()}, text());

    abi(b, "nina_text_from_i64", {whole()}, text());
    abi(b, "nina_text_from_u64", {MakeUInt(64)}, text());
    abi(b, "nina_text_from_i128", {MakeInt(128)}, text());
    abi(b, "nina_text_from_bool", {MakeBool()}, text());
  }

  void declareTextEdit(PolyIRBuilder& b) {
    abi(b, "nina_text_trim", {text()}, text());
    abi(b, "nina_text_upper", {text()}, text());
    abi(b, "nina_text_lower", {text()}, text());

    abi(b, "nina_text_slice", {text(), whole(), whole()}, text());
    abi(b, "nina_text_char_at", {text(), whole()}, text());
    abi(b, "nina_text_from_code", {whole()}, text());
  }

  void declareTextSearch(PolyIRBuilder& b) {
    abi(b, "nina_text_code_at", {text(), whole()}, whole());
    abi(b, "nina_text_index_of_from", {text(), text(), whole()}, whole());
    abi(b, "nina_text_is_alnum", {text()}, whole());
    abi(b, "nina_text_parse_f64", {text()}, real());
    abi(b, "nina_text_parse_i64", {text()}, whole());
  }

  void declareMath(PolyIRBuilder& b) {
    for (const char* one : {"sqrt", "floor", "ceil", "round", "sin", "cos"}) {
      abi(b, String("nina_math_") + one, {real()}, real());
    }

    abi(b, "nina_math_pow", {real(), real()}, real());
    abi(b, "nina_math_atan2", {real(), real()}, real());
  }

  void declarePrinting(PolyIRBuilder& b) {
    abi(b, "nina_print_text", {text()}, MakeVoid());
    abi(b, "nina_println_text", {text()}, MakeVoid());
  }

  void declareFileProbes(PolyIRBuilder& b) {
    for (const char* one : {"exists", "is_dir", "is_file", "modified", "size"}) {
      abi(b, String("nina_fs_") + one, {text()}, whole());
    }

    abi(b, "nina_fs_canonical", {text()}, text());
    abi(b, "nina_fs_list", {text()}, text());
    abi(b, "nina_fs_read_text", {text()}, text());
  }

  void declareFileEdits(PolyIRBuilder& b) {
    for (const char* one : {"write_text", "append", "copy", "move"}) {
      abi(b, String("nina_fs_") + one, {text(), text()}, whole());
    }

    for (const char* one : {"delete", "create_dir", "remove_dir"}) {
      abi(b, String("nina_fs_") + one, {text()}, whole());
    }
  }

  void declareFileStreams(PolyIRBuilder& b) {
    abi(b, "nina_fs_open", {text(), whole()}, whole());
    abi(b, "nina_fs_write", {whole(), text()}, whole());
    abi(b, "nina_fs_write_byte", {whole(), whole()}, whole());
    abi(b, "nina_fs_read_line", {whole()}, text());

    for (const char* one : {"at_end", "read_byte", "close"}) {
      abi(b, String("nina_fs_") + one, {whole()}, whole());
    }
  }

  void declareFiles(PolyIRBuilder& b) {
    declareNumberText(b);
    declareTextSearch(b);
    declareFileProbes(b);
    declareFileEdits(b);
    declareFileStreams(b);
  }

  // ── the programs ──

  using Steps = Function<void(PolyIRBuilder&, Block&)>;

  // `main() -> string`: `var acc = ""`, the steps, `return acc`.
  Module noted(const Function<void(PolyIRBuilder&)>& declare, const Steps& steps) {
    PolyIRBuilder b;
    declare(b);

    auto& main = DefineFunction(b, "main", {}, MakeString());
    Var(main.body, "acc", MakeString(), StringConst(""));
    steps(b, main.body);
    Return(main.body, Local(main.body, "acc"));

    return Build(b);
  }

  void note(Block& body, ExprPtr part) {
    SetLocal(body, "acc", Concat(Local(body, "acc"), Concat(StringConst(","), move(part))));
  }

  ExprPtr str(const String& s) {
    return StringConst(s);
  }

  ExprPtr wholeText(PolyIRBuilder& b, ExprPtr v) {
    return Call(b, "nina_text_from_i64", {move(v)});
  }

  ExprPtr realText(PolyIRBuilder& b, ExprPtr v) {
    return Call(b, "nina_text_from_f64", {move(v)});
  }

  ExprPtr boolText(PolyIRBuilder& b, ExprPtr v) {
    return Call(b, "nina_text_from_bool", {move(v)});
  }

  ExprPtr fileCall(PolyIRBuilder& b, const String& operation, Vector<ExprPtr> args) {
    return Call(b, "nina_fs_" + operation, move(args));
  }

  void noteWhole(PolyIRBuilder& b, Block& body, ExprPtr v) {
    note(body, wholeText(b, move(v)));
  }

  // 1. Doubles: integral below 2^53 as an integer (the literal form keeps `.0`), else the shortest digits that read back exactly.
  void floatSteps(PolyIRBuilder& b, Block& body) {
    for (const double v : {3.14159265, 5.0, 1e20, -0.0, 9007199254740992.0, -1234567.0, 0.1 + 0.2}) {
      note(body, realText(b, FloatConst(v)));
    }

    note(body, Call(b, "nina_text_from_f64_lit", {FloatConst(5.0)}));
    note(body, Call(b, "nina_text_from_f64_lit", {FloatConst(2.5)}));
  }

  // 2. Whole numbers and bools; a u64 above the i64 range prints unsigned; an i128 within 64 bits prints as itself.
  void integerSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, IntConst(-42));
    note(body, Call(b, "nina_text_from_u64", {Cast(IntConst(-1), MakeUInt(64))}));
    note(body, Call(b, "nina_text_from_i128", {Cast(IntConst(-7), MakeInt(128))}));
    note(body, boolText(b, BoolConst(true)));
    note(body, boolText(b, BoolConst(false)));
  }

  // 3. trim/upper/lower are ASCII-only; slice clamps `from` to 0 and `to` to the end and gives "" when they cross.
  void textEditSteps(PolyIRBuilder& b, Block& body) {
    note(body, Call(b, "nina_text_trim", {str("  a b \t\n")}));
    note(body, Call(b, "nina_text_upper", {str("abC1\xc3\xa9")}));
    note(body, Call(b, "nina_text_lower", {str("AbC")}));

    note(body, Call(b, "nina_text_slice", {str("hello"), IntConst(-2), IntConst(99)}));
    note(body, Call(b, "nina_text_slice", {str("hello"), IntConst(3), IntConst(1)}));
    note(body, Call(b, "nina_text_slice", {str("hello"), IntConst(1), IntConst(-1)}));
    note(body, Call(b, "nina_text_slice", {str("hello"), IntConst(1), IntConst(3)}));
  }

  // 3b. a slice or a character cut out of a UTF-8 sequence decodes it: every maximal invalid subpart is one U+FFFD.
  void cutSteps(PolyIRBuilder& b, Block& body) {
    note(body, Call(b, "nina_text_slice", {str("h\xc3\xa9!"), IntConst(0), IntConst(2)}));
    note(body, Call(b, "nina_text_slice", {str("\xf0\x9f\x98\x80"), IntConst(1), IntConst(4)}));
    note(body, Call(b, "nina_text_slice", {str("\xf0\x9f\x98\x80!"), IntConst(0), IntConst(3)}));
    note(body, Call(b, "nina_text_char_at", {str("\xc3\xa9"), IntConst(1)}));
  }

  // 4. char_at gives one byte or ""; from_code one raw byte, and "" for 0.
  void charSteps(PolyIRBuilder& b, Block& body) {
    note(body, Call(b, "nina_text_char_at", {str("hey"), IntConst(1)}));
    note(body, Call(b, "nina_text_char_at", {str("hey"), IntConst(3)}));
    note(body, Call(b, "nina_text_from_code", {IntConst(65)}));
    note(body, Call(b, "nina_text_from_code", {IntConst(0)}));
    note(body, Call(b, "nina_text_from_code", {IntConst(233)}));
  }

  // 5. code_at is the unsigned byte (-1 outside); index_of_from is -1 past the end (no clamping), an empty needle is found at `from`.
  void searchSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, Call(b, "nina_text_code_at", {str("A"), IntConst(0)}));
    noteWhole(b, body, Call(b, "nina_text_code_at", {str("A"), IntConst(1)}));
    noteWhole(b, body, Call(b, "nina_text_code_at", {str("\xc3\xa9"), IntConst(0)}));

    noteWhole(b, body, Call(b, "nina_text_index_of_from", {str("hello world"), str("o"), IntConst(5)}));
    noteWhole(b, body, Call(b, "nina_text_index_of_from", {str("hello world"), str("o"), IntConst(8)}));
    noteWhole(b, body, Call(b, "nina_text_index_of_from", {str("abc"), str(""), IntConst(3)}));
    noteWhole(b, body, Call(b, "nina_text_index_of_from", {str("abc"), str(""), IntConst(4)}));
  }

  void alnumSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, Call(b, "nina_text_index_of_from", {str("abc"), str(""), IntConst(-5)}));

    for (const char* s : {"abc1", "", "a b"}) {
      noteWhole(b, body, Call(b, "nina_text_is_alnum", {str(s)}));
    }
  }

  // 6. strtol/strtod: leading spaces, trailing text ignored, 0 for no number; inf and nan read, and print as java21 spells them.
  void parseSteps(PolyIRBuilder& b, Block& body) {
    for (const char* s : {"3.7", "  -12abc", "abc"}) {
      noteWhole(b, body, Call(b, "nina_text_parse_i64", {str(s)}));
    }

    for (const char* s : {" 2.5x", "inf", "nan", "junk", "-inf"}) {
      note(body, realText(b, Call(b, "nina_text_parse_f64", {str(s)})));
    }
  }

  // 7. Math: round is floor(x + 0.5), so -2.5 rounds to -2 and 0.49999999999999994 to 1.
  void mathSteps(PolyIRBuilder& b, Block& body) {
    for (const double v : {-2.5, 2.5, 0.49999999999999994}) {
      note(body, realText(b, Call(b, "nina_math_round", {FloatConst(v)})));
    }

    note(body, realText(b, Call(b, "nina_math_floor", {FloatConst(-1.5)})));
    note(body, realText(b, Call(b, "nina_math_ceil", {FloatConst(1.2)})));
    note(body, realText(b, Call(b, "nina_math_sqrt", {FloatConst(16)})));
    note(body, realText(b, Call(b, "nina_math_pow", {FloatConst(2), FloatConst(10)})));
    note(body, realText(b, Call(b, "nina_math_atan2", {FloatConst(0), FloatConst(-1)})));

    for (const char* function : {"nina_math_sin", "nina_math_cos"}) {
      note(body, realText(b, Call(b, function, {FloatConst(0)})));
    }
  }

  // 8. print adds nothing, println a newline; both go to the machine's stream.
  void printSteps(PolyIRBuilder& b, Block& body) {
    Eval(body, Call(b, "nina_print_text", {str("a")}));
    Eval(body, Call(b, "nina_println_text", {str("b")}));
    Eval(body, Call(b, "nina_print_text", {str("")}));
    Eval(body, Call(b, "nina_println_text", {str("")}));
  }

  // 9. Files, in a folder under the build directory (the test runs there). The parents of a written file are made.
  const String kDir = "nina_runtime_test";
  const String kA = kDir + "/sub/a.txt";

  void fileWriteSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, fileCall(b, "write_text", {str(kA), str("one\r\ntwo\n")}));
    note(body, fileCall(b, "read_text", {str(kA)}));

    noteWhole(b, body, fileCall(b, "exists", {str(kA)}));
    noteWhole(b, body, fileCall(b, "is_file", {str(kA)}));
    noteWhole(b, body, fileCall(b, "is_dir", {str(kDir + "/sub")}));
    noteWhole(b, body, fileCall(b, "size", {str(kA)}));
    noteWhole(b, body, fileCall(b, "is_file", {str(kDir + "/sub")}));
  }

  // The first handle of the process is 1; a line's "\r\n" loses both; at_end peeks; "" and -1 at the end.
  void fileReadSteps(PolyIRBuilder& b, Block& body) {
    Let(body, "h1", fileCall(b, "open", {str(kA), IntConst(0)}));
    noteWhole(b, body, Local(body, "h1"));

    note(body, fileCall(b, "read_line", {Local(body, "h1")}));
    noteWhole(b, body, fileCall(b, "at_end", {Local(body, "h1")}));
    note(body, fileCall(b, "read_line", {Local(body, "h1")}));
    noteWhole(b, body, fileCall(b, "at_end", {Local(body, "h1")}));
    note(body, fileCall(b, "read_line", {Local(body, "h1")}));
  }

  // Closing twice is fine; a closed handle refuses a write; the next handle is 2 (never reused); 266 writes the byte 10.
  void fileHandleSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, fileCall(b, "read_byte", {Local(body, "h1")}));
    noteWhole(b, body, fileCall(b, "close", {Local(body, "h1")}));
    noteWhole(b, body, fileCall(b, "close", {Local(body, "h1")}));
    noteWhole(b, body, fileCall(b, "write", {Local(body, "h1"), str("x")}));

    Let(body, "h2", fileCall(b, "open", {str(kDir + "/b.bin"), IntConst(1)}));
    noteWhole(b, body, Local(body, "h2"));
    noteWhole(b, body, fileCall(b, "write", {Local(body, "h2"), str("hi")}));
  }

  // A directory opened for reading is 0, as is a missing path; list is sorted and newline-joined; append adds at the end.
  void fileListSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, fileCall(b, "write_byte", {Local(body, "h2"), IntConst(266)}));
    noteWhole(b, body, fileCall(b, "close", {Local(body, "h2")}));
    note(body, fileCall(b, "read_text", {str(kDir + "/b.bin")}));

    noteWhole(b, body, fileCall(b, "open", {str(kDir), IntConst(0)}));
    noteWhole(b, body, fileCall(b, "open", {str(kDir + "/missing/x"), IntConst(0)}));
    note(body, fileCall(b, "list", {str(kDir)}));
    noteWhole(b, body, fileCall(b, "append", {str(kA), str("three")}));
  }

  // copy only copies a regular file; move renames. delete removes a file or an EMPTY directory (1 for a full one, 0 for a missing path).
  void fileCopySteps(PolyIRBuilder& b, Block& body) {
    note(body, fileCall(b, "read_text", {str(kA)}));
    noteWhole(b, body, fileCall(b, "copy", {str(kA), str(kDir + "/c.txt")}));
    noteWhole(b, body, fileCall(b, "move", {str(kDir + "/c.txt"), str(kDir + "/d.txt")}));
    noteWhole(b, body, fileCall(b, "exists", {str(kDir + "/c.txt")}));
    note(body, fileCall(b, "read_text", {str(kDir + "/d.txt")}));

    noteWhole(b, body, fileCall(b, "copy", {str(kDir + "/sub"), str(kDir + "/x")}));
    noteWhole(b, body, fileCall(b, "delete", {str(kDir + "/sub")}));
  }

  // canonical is "" for a missing path and absolute otherwise; modified is a millisecond time; remove_dir takes the whole tree.
  void fileRemoveSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, fileCall(b, "delete", {str(kDir + "/d.txt")}));
    noteWhole(b, body, fileCall(b, "delete", {str(kDir + "/nothing")}));

    note(body, fileCall(b, "canonical", {str(kDir + "/none")}));
    note(body, boolText(b, Ne(Call(b, "nina_text_index_of_from", {fileCall(b, "canonical", {str(kDir)}), str("/" + kDir), IntConst(0)}), IntConst(-1))));
    note(body, boolText(b, Gt(fileCall(b, "modified", {str(kA)}), IntConst(0))));

    noteWhole(b, body, fileCall(b, "create_dir", {str(kDir + "/p/q")}));
    noteWhole(b, body, fileCall(b, "is_dir", {str(kDir + "/p/q")}));
  }

  void fileGoneSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, fileCall(b, "remove_dir", {str(kDir)}));
    noteWhole(b, body, fileCall(b, "exists", {str(kDir)}));
    noteWhole(b, body, fileCall(b, "remove_dir", {str(kDir)}));
    note(body, fileCall(b, "read_text", {str(kA)}));
  }

  void fileSteps(PolyIRBuilder& b, Block& body) {
    fileWriteSteps(b, body);
    fileReadSteps(b, body);
    fileHandleSteps(b, body);
    fileListSteps(b, body);

    fileCopySteps(b, body);
    fileRemoveSteps(b, body);
    fileGoneSteps(b, body);
  }

  String fileExpectation() {
    return String(",0,one\r\ntwo\n,1,1,1,9,0") + ",1,one,0,two,1," + ",-1,0,0,1,2,0" + ",0,0,hi\n,0,0,b.bin\nsub,0" +
           ",one\r\ntwo\nthree,0,0,0,one\r\ntwo\nthree,1,1" + ",0,0,,true,true,0,1" + ",0,0,0,";
  }

  // 10. The std Process natives, declared as the lowering declares them (`<function>__host`, the std records as `Std__Run`/`Std__Job`).
  TypePtr words() {
    return MakeDynamicArray(text(), false);
  }

  TypePtr runRecord() {
    return MakeStruct("Std__Run");
  }

  void declareProcessRecords(PolyIRBuilder& b) {
    DefineStruct(b, "Std__Run", {{"code", whole(), false}, {"output", text(), false}, {"errorOutput", text(), false}});
    DefineStruct(b, "Std__Job", {{"id", text(), false}});
  }

  void declareProcess(PolyIRBuilder& b) {
    declareNumberText(b);
    declareTextSearch(b);
    declareProcessRecords(b);

    abi(b, "Process__shell__host", {text()}, runRecord());
    abi(b, "Process__run__host", {words()}, runRecord());
    abi(b, "Process__runWith__host", {words(), text(), words()}, runRecord());
    abi(b, "Process__passThrough__host", {words(), text(), words()}, whole());
  }

  void declareProcessState(PolyIRBuilder& b) {
    declareProcess(b);
    abi(b, "Process__env__host", {text()}, MakeOptional(text()));
    abi(b, "Process__setEnv__host", {text(), text()}, MakeVoid());
    abi(b, "Process__workingDir__host", {}, text());

    abi(b, "Process__start__host", {words(), text(), words()}, MakeStruct("Std__Job"));
    abi(b, "Process__wait__host", {MakeStruct("Std__Job")}, runRecord());
    abi(b, "Process__isDone__host", {MakeStruct("Std__Job")}, MakeBool());
  }

  ExprPtr wordList(const Vector<String>& items) {
    Vector<ExprPtr> elements;

    for (const String& item : items) {
      elements.push_back(str(item));
    }

    return VecLit(words(), move(elements));
  }

  ExprPtr noWords() {
    return wordList({});
  }

  // A Run noted as `,code,output,errorOutput`.
  void noteRun(PolyIRBuilder& b, Block& body, const String& name, ExprPtr ran) {
    Let(body, name, move(ran));
    noteWhole(b, body, FieldAccess(b, Local(body, name), "code"));
    note(body, FieldAccess(b, Local(body, name), "output"));
    note(body, FieldAccess(b, Local(body, name), "errorOutput"));
  }

  // shell goes through /bin/sh; run quotes every word (the space stays inside one argument); runWith runs in the folder with the variable.
  void processRunSteps(PolyIRBuilder& b, Block& body) {
    noteRun(b, body, "shelled", Call(b, "Process__shell__host", {str("printf hi; printf oops >&2; exit 3")}));
    noteRun(b, body, "ran", Call(b, "Process__run__host", {wordList({"printf", "[%s]", "a b"})}));

    const ExprPtr script = str("printf \"$V:$(basename \"$(pwd)\")\"");
    noteRun(b, body, "within", Call(b, "Process__runWith__host", {wordList({"sh", "-c", ""}), str("/"), noWords()}));
    noteRun(b, body, "inFolder", Call(b, "Process__runWith__host", {VecLit(words(), {str("sh"), str("-c"), script}), str("/usr"), wordList({"V=x y"})}));
    noteWhole(b, body, Call(b, "Process__passThrough__host", {wordList({"sh", "-c", "exit 7"}), str(""), noWords()}));
  }

  // setEnv is seen by env and by the children; "" removes the variable; a background job is collected by wait.
  void processStateSteps(PolyIRBuilder& b, Block& body) {
    Eval(body, Call(b, "Process__setEnv__host", {str("NINA_VM_PROCESS_TEST"), str("hello")}));
    note(body, Unwrap(Call(b, "Process__env__host", {str("NINA_VM_PROCESS_TEST")})));
    noteRun(b, body, "child", Call(b, "Process__shell__host", {str("printf \"$NINA_VM_PROCESS_TEST\"")}));

    Eval(body, Call(b, "Process__setEnv__host", {str("NINA_VM_PROCESS_TEST"), str("")}));
    note(body, boolText(b, IsNone(Call(b, "Process__env__host", {str("NINA_VM_PROCESS_TEST")}))));

    noteWhole(b, body, Call(b, "nina_text_index_of_from", {Call(b, "Process__workingDir__host", {}), str("/"), IntConst(0)}));
  }

  void processJobSteps(PolyIRBuilder& b, Block& body) {
    Let(body, "job", Call(b, "Process__start__host", {wordList({"sh", "-c", "printf \"$J\"; exit 4"}), str(""), wordList({"J=job"})}));
    noteRun(b, body, "done", Call(b, "Process__wait__host", {Local(body, "job")}));
    note(body, boolText(b, Call(b, "Process__isDone__host", {Local(body, "job")})));
  }

  void processSteps(PolyIRBuilder& b, Block& body) {
    processRunSteps(b, body);
    processStateSteps(b, body);
    processJobSteps(b, body);
  }

  // 11. std `Lifecycle.step`: its extern hands the verb, the directory and the words to the host's step runner, and answers the
  // runner's status. A run whose host gave no runner is declined before anything runs.
  void declareLifecycle(PolyIRBuilder& b) {
    declareNumberText(b);
    abi(b, "Lifecycle__inProcess__host", {text(), text(), words()}, whole());
  }

  void lifecycleSteps(PolyIRBuilder& b, Block& body) {
    noteWhole(b, body, Call(b, "Lifecycle__inProcess__host", {str("run"), str("/project"), wordList({"a", "b c"})}));
  }

  // What the runner saw, as one line: "run /project a|b c".
  String joinedStep(const String& verb, const String& dir, const Vector<String>& args) {
    String line = verb + " " + dir + " ";

    for (size_t i = 0; i < args.size(); i++) {
      line += (i == 0 ? "" : "|") + args[i];
    }

    return line;
  }

  // Options whose host step records what it saw in `seen` and answers 7.
  VmOptions recordingHostStep(String& seen) {
    VmOptions options;
    options.hostStep = [&seen](const String& verb, const String& dir, const Vector<String>& args) {
      seen = joinedStep(verb, dir, args);

      return int64_t{7};
    };

    return options;
  }

  void expectHostStep(const String& name) {
    String seen;
    const VmOutcome r = RunModule(noted(declareLifecycle, lifecycleSteps), recordingHostStep(seen));
    const String got = shown(r) + " after " + seen;

    if (r.ok && got == ",7 after run /project a|b c") {
      pass(name, got);
    } else {
      fail(name, ",7 after run /project a|b c", got);
    }
  }

  // 12. The externs of the other lowering options are not bound: the program is declined before anything runs.
  Module hostErrorProgram() {
    PolyIRBuilder b;
    abi(b, "nina_host_error", {}, text());
    auto& main = DefineFunction(b, "main", {}, MakeString());
    Return(main.body, Call(b, "nina_host_error", {}));

    return Build(b);
  }

  void declareTextAll(PolyIRBuilder& b) {
    declareNumberText(b);
    declareTextEdit(b);
    declareTextSearch(b);
  }

  void textPrograms() {
    expectText("f64 text", noted(declareNumberText, floatSteps), ",3.14159265,5,1e+20,0,9007199254740992.0,-1234567,0.30000000000000004,5.0,2.5");
    expectText("integer text", noted(declareNumberText, integerSteps), ",-42,18446744073709551615,-7,true,false");

    expectText("text edit", noted(declareTextAll, textEditSteps), ",a b,ABC1\xc3\xa9,abc,hello,,,el");
    expectText("chars", noted(declareTextAll, charSteps), String(",e,,A,,") + "\xe9");
    expectText("search", noted(declareTextAll, searchSteps), ",65,-1,195,7,-1,3,-1");
    expectText("alnum", noted(declareTextAll, alnumSteps), ",0,1,0,0");
    expectText("parse", noted(declareTextAll, parseSteps), ",3,-12,0,2.5,Infinity,NaN,0,-Infinity");
  }

  // Kept apart from textPrograms: the bytes a slice or char_at cuts out of a character.
  void cutPrograms() {
    expectText("cut UTF-8", noted(declareTextAll, cutSteps), ",h\xef\xbf\xbd" ",\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd" ",\xef\xbf\xbd" ",\xef\xbf\xbd");
  }

  void hostPrograms() {
    expectText("math", noted([](PolyIRBuilder& b) { declareNumberText(b); declareMath(b); }, mathSteps), ",-2,3,1,-2,2,4,1024,3.141592653589793,0,1");
    expectPrinted("print", noted(declarePrinting, printSteps), "ab\n\n");
    expectText("files", noted(declareFiles, fileSteps), fileExpectation());
    expectText("process", noted(declareProcessState, processSteps), ",3,hi,oops,0,[a b],,0,,,0,x y:usr,,7" ",hello,0,hello,,true,0" ",4,job,,false");

    expectDeclined("unbound host extern", hostErrorProgram(), "'nina_host_error' has no binding on the VM");
    expectHostStep("lifecycle step");
    expectDeclined("lifecycle step without a runner", noted(declareLifecycle, lifecycleSteps), "'Lifecycle__inProcess__host' has no binding on the VM");
  }

}  // namespace

int main() {
  textPrograms();
  cutPrograms();
  hostPrograms();
  cout << (failures == 0 ? "all checks passed" : "some checks failed") << "\n";

  return failures == 0 ? 0 : 1;
}
