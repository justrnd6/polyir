// The string built-ins (`str(x)`, `s[i]`, `s.slice(from, to)`, `s.find(sub, from)`, `print(s)`). A string is a zero-terminated block of bytes, and positions count
// bytes. Each operation is a small helper function the module gets the first time a program uses it:
//
//   StrOf(x)              a bool as "true" / "false"; a char (i8, u8) as the character with that code, in UTF-8; any other whole number in decimal
//                         (unsigned for uN); a string is itself
//   CharAt(s, i)          the byte at `i`; 0 when `i` is outside [0, len(s))
//   Slice(s, from, to)    a new string of the bytes [from, to), `from` and `to` each clamped into [0, len(s)]; "" when to <= from
//   Find(s, sub, from)    the first index >= from (clamped into [0, len(s)]) where `sub` starts, else -1; an empty `sub` gives the clamped `from`
//   Print(s)              libc's `puts`: the string and a newline on standard output
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    const char* const kStrlen = "declare i64 @strlen(ptr)";
    const char* const kMemcpy = "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)";
    const char* const kMemcmp = "declare i32 @memcmp(ptr, ptr, i64)";
    const char* const kPuts = "declare i32 @puts(ptr)";
    const char* const kWrite = "declare i64 @write(i32, ptr, i64)";
    const char* const kExit = "declare void @exit(i32)";
    const char* const kTrapWord = "@polyir_trap_word = private constant [7 x i8] c\"trap: \\00\"";
    const char* const kNewline = "@polyir_newline = private constant [2 x i8] c\"\\0A\\00\"";

    // `fail(message)`: `trap: `, the message and a newline on standard error, then the program ends with status 1 (exit flushes standard output).
    const char* const kFail = R"(define internal void @polyir_fail(ptr %message) {
  %length = call i64 @strlen(ptr %message)
  call i64 @write(i32 2, ptr @polyir_trap_word, i64 6)
  call i64 @write(i32 2, ptr %message, i64 %length)
  call i64 @write(i32 2, ptr @polyir_newline, i64 1)
  call void @exit(i32 1)
  unreachable
})";

    const char* const kClamp = R"(define internal i64 @polyir_text_clamp(i64 %v, i64 %n) {
entry:
  %low = icmp slt i64 %v, 0
  %up = select i1 %low, i64 0, i64 %v
  %high = icmp sgt i64 %up, %n
  %in = select i1 %high, i64 %n, i64 %up
  ret i64 %in
})";

    const char* const kCharAt = R"(define internal i8 @polyir_text_char_at(ptr %s, i64 %i) {
entry:
  %n = call i64 @strlen(ptr %s)
  %inside = icmp ult i64 %i, %n
  br i1 %inside, label %read, label %outside
read:
  %at = getelementptr i8, ptr %s, i64 %i
  %c = load i8, ptr %at
  ret i8 %c
outside:
  ret i8 0
})";

    const char* const kSlice = R"(define internal ptr @polyir_text_slice(ptr %s, i64 %from, i64 %to) {
entry:
  %n = call i64 @strlen(ptr %s)
  %f = call i64 @polyir_text_clamp(i64 %from, i64 %n)
  %t = call i64 @polyir_text_clamp(i64 %to, i64 %n)
  %wide = icmp sgt i64 %t, %f
  %diff = sub i64 %t, %f
  %size = select i1 %wide, i64 %diff, i64 0
  %room = add i64 %size, 1
  %out = call ptr @gc_allocate(i64 %room, i64 0)
  %src = getelementptr i8, ptr %s, i64 %f
  call void @llvm.memcpy.p0.p0.i64(ptr %out, ptr %src, i64 %size, i1 false)
  %end = getelementptr i8, ptr %out, i64 %size
  store i8 0, ptr %end
  ret ptr %out
})";

    const char* const kFind = R"(define internal i64 @polyir_text_find(ptr %s, ptr %sub, i64 %from) {
entry:
  %n = call i64 @strlen(ptr %s)
  %m = call i64 @strlen(ptr %sub)
  %start = call i64 @polyir_text_clamp(i64 %from, i64 %n)
  %last = sub i64 %n, %m
  br label %loop
loop:
  %i = phi i64 [ %start, %entry ], [ %next, %miss ]
  %past = icmp sgt i64 %i, %last
  br i1 %past, label %none, label %test
test:
  %at = getelementptr i8, ptr %s, i64 %i
  %order = call i32 @memcmp(ptr %at, ptr %sub, i64 %m)
  %same = icmp eq i32 %order, 0
  br i1 %same, label %found, label %miss
miss:
  %next = add i64 %i, 1
  br label %loop
found:
  ret i64 %i
none:
  ret i64 -1
})";

    // The character with the byte's code, as UTF-8: one byte below 128, two from there up (0xC0 | c >> 6, 0x80 | c & 63).
    const char* const kCharText = R"(define internal ptr @polyir_text_char(i8 %c) {
entry:
  %wide = icmp slt i8 %c, 0
  %out = call ptr @gc_allocate(i64 3, i64 0)
  %high = lshr i8 %c, 6
  %lead = or i8 %high, -64
  %first = select i1 %wide, i8 %lead, i8 %c
  store i8 %first, ptr %out
  %low = and i8 %c, 63
  %tail = or i8 %low, -128
  %second = select i1 %wide, i8 %tail, i8 0
  %at1 = getelementptr i8, ptr %out, i64 1
  store i8 %second, ptr %at1
  %at2 = getelementptr i8, ptr %out, i64 2
  store i8 0, ptr %at2
  ret ptr %out
})";

    // The decimal digits of `%v` (an unsigned magnitude), after a `-` when `%negative`, written backwards into a buffer and copied out.
    const char* const kDecimal = R"(define internal ptr @polyir_text_decimal(i64 %v, i1 %negative) {
entry:
  %buf = alloca [24 x i8]
  %end = getelementptr i8, ptr %buf, i64 23
  store i8 0, ptr %end
  br label %digit
digit:
  %pos = phi i64 [ 23, %entry ], [ %at, %digit ]
  %rest = phi i64 [ %v, %entry ], [ %q, %digit ]
  %q = udiv i64 %rest, 10
  %r = urem i64 %rest, 10
  %r8 = trunc i64 %r to i8
  %c = add i8 %r8, 48
  %at = sub i64 %pos, 1
  %p = getelementptr i8, ptr %buf, i64 %at
  store i8 %c, ptr %p
  %more = icmp ne i64 %q, 0
  br i1 %more, label %digit, label %sign
sign:
  %signAt = sub i64 %at, 1
  %sp = getelementptr i8, ptr %buf, i64 %signAt
  store i8 45, ptr %sp
  %first = select i1 %negative, i64 %signAt, i64 %at
  %from = getelementptr i8, ptr %buf, i64 %first
  %size = sub i64 24, %first
  %out = call ptr @gc_allocate(i64 %size, i64 0)
  call void @llvm.memcpy.p0.p0.i64(ptr %out, ptr %from, i64 %size, i1 false)
  ret ptr %out
})";

    bool isTextBuiltin(Builtin op) { return op == Builtin::StrOf || op == Builtin::CharAt || op == Builtin::Slice || op == Builtin::Find ||
        op == Builtin::Print || op == Builtin::Fail; }

    Value failed(FunctionEmitter& emitter, const Value& message) {
      NeedTextHelpers(emitter, {kStrlen, kWrite, kExit, kTrapWord, kNewline, kFail});
      Line(emitter, "call void @polyir_fail(" + Typed(message) + ")");

      return Value{"0", MakeInt(32)};
    }

  }  // namespace

  bool TextBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (!isTextBuiltin(expr.builtin)) {
      return false;
    }

    const Vector<Value> args = OperandValues(emitter, expr, 0);

    if (expr.builtin == Builtin::StrOf) {
      out = TextOf(emitter, args[0]);
    } else if (expr.builtin == Builtin::Print) {
      out = Printed(emitter, args[0]);
    } else if (expr.builtin == Builtin::Fail) {
      out = failed(emitter, args[0]);
    } else if (expr.builtin == Builtin::CharAt) {
      out = TextCall(emitter, kCharAt, "i8 @polyir_text_char_at", {args[0], IndexValue(emitter, args[1])}, MakeInt(8));
    } else {
      out = expr.builtin == Builtin::Slice ? TextSlice(emitter, args) : TextFind(emitter, args);
    }

    return true;
  }

  // `print(s)`: libc's `puts`, which wasm's host supplies too.
  Value Printed(FunctionEmitter& emitter, const Value& text) {
    Need(emitter.context_, kPuts);

    return Value{Instruction(emitter, "call i32 @puts(" + Typed(text) + ")"), MakeInt(32)};
  }

  // The helpers that make a string get it from the collector, as a block of data.
  Value TextSlice(FunctionEmitter& emitter, const Vector<Value>& args) {
    NoteAllocation(emitter);
    NeedTextHelpers(emitter, {kClamp, kMemcpy});

    return TextCall(emitter, kSlice, "ptr @polyir_text_slice", {args[0], IndexValue(emitter, args[1]), IndexValue(emitter, args[2])}, MakeString());
  }

  Value TextFind(FunctionEmitter& emitter, const Vector<Value>& args) {
    NeedTextHelpers(emitter, {kClamp, kMemcmp});

    return TextCall(emitter, kFind, "i64 @polyir_text_find", {args[0], args[1], IndexValue(emitter, args[2])}, MakeInt(64));
  }

  // A call of the helper `definition` (`result @name`), with what every helper uses.
  Value TextCall(FunctionEmitter& emitter, const char* definition, const String& target, const Vector<Value>& args, const TypePtr& result) {
    NeedTextHelpers(emitter, {definition, kStrlen});
    String list;

    for (const Value& a : args) {
      list += (list.empty() ? "" : ", ") + Typed(a);
    }

    return Value{Instruction(emitter, "call " + target + "(" + list + ")"), result};
  }

  void NeedTextHelpers(FunctionEmitter& emitter, InitializerList<const char*> lines) {
    for (const char* each : lines) {
      Need(emitter.context_, each);
    }
  }

  // `str(x)` (see the table at the top).
  Value TextOf(FunctionEmitter& emitter, const Value& value) {
    const TypePtr& type = value.type;

    if (type->kind == TypeKind::String) {
      return value;
    }

    if (IsBoolean(*type)) {
      return BoolText(emitter, value);
    }

    if (type->bits == 8) {
      return CharText(emitter, value);
    }

    return IsSigned(*type) ? SignedText(emitter, Resized(emitter, value, 64, true)) : DecimalText(emitter, Resized(emitter, value, 64, false), "false");
  }

  Value CharText(FunctionEmitter& emitter, const Value& value) {
    NoteAllocation(emitter);

    return TextCall(emitter, kCharText, "ptr @polyir_text_char", {value}, MakeString());
  }

  Value BoolText(FunctionEmitter& emitter, const Value& flag) {
    const String choice = "select " + Typed(flag) + ", ptr " + StringGlobal(emitter.context_, "true") + ", ptr " + StringGlobal(emitter.context_, "false");

    return Value{Instruction(emitter, choice), MakeString()};
  }

  Value SignedText(FunctionEmitter& emitter, const Value& wide) {
    const String negative = Instruction(emitter, "icmp slt i64 " + wide.text + ", 0");
    const String flipped = Instruction(emitter, "sub i64 0, " + wide.text);

    return DecimalText(emitter, Value{Instruction(emitter, "select i1 " + negative + ", i64 " + flipped + ", i64 " + wide.text), wide.type}, negative);
  }

  Value DecimalText(FunctionEmitter& emitter, const Value& magnitude, const String& negative) {
    NoteAllocation(emitter);
    NeedTextHelpers(emitter, {kMemcpy});

    return TextCall(emitter, kDecimal, "ptr @polyir_text_decimal", {magnitude, Value{negative, MakeBool()}}, MakeString());
  }

}  // namespace polyir::llvm
