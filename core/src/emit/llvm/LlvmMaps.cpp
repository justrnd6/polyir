// Maps and sets: open addressing with linear probing, the table doubling once three quarters of its slots are used (as the Nina implementation does).
//
// A map is a heap header of five 8-byte cells (keys, values, states, slot count, entry count) pointing at three tables of one cell per slot; a set is a
// map whose values are never read. Keys travel as 64-bit words (integers widened, floats bit-cast, strings and objects as addresses) and are compared
// and hashed by functions made for the key's kind: a number by its bits, a string by its text, a struct by its fields. Reading a key that is not there
// gives zero. The runtime is written into the module as LLVM IR functions, once per kind of key.
//
// A map is made of collector objects: its header (whose words are its three tables), keys and values (any word may be a reference: an
// integer key that equals an object's address only keeps that object a little longer) and its states (numbers). Writing a key may grow
// the tables, which allocates; the old ones go back at once (gc_release), nothing else refers to them.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    // The runtime's IR functions. In the ones made per kind of key, `TAG` stands for the key's tag (keyTag below: `word`, `string`,
    // `S_<struct>` or `L_<element's tag>`). A slot's state is 0 while it is empty and 1 once it holds an entry.

    // What reading a missing key gives the address of.
    const char* const kZero = "@zero_cell = internal constant i64 0";

    // A new, empty map. Its slot count is the request (at least 1) rounded up to a power of two, so `hash & (slots - 1)` picks a slot:
    // the shifts and ors set every bit below the highest one of `request - 1`, and adding 1 carries into the next power.
    const char* const kNew = R"(define ptr @map_new(i64 %requested) {
entry:
  %atLeast = icmp ugt i64 %requested, 1
  %r = select i1 %atLeast, i64 %requested, i64 1
  %n0 = sub i64 %r, 1
  %s1 = lshr i64 %n0, 1
  %a1 = or i64 %n0, %s1
  %s2 = lshr i64 %a1, 2
  %a2 = or i64 %a1, %s2
  %s3 = lshr i64 %a2, 4
  %a3 = or i64 %a2, %s3
  %s4 = lshr i64 %a3, 8
  %a4 = or i64 %a3, %s4
  %s5 = lshr i64 %a4, 16
  %a5 = or i64 %a4, %s5
  %s6 = lshr i64 %a5, 32
  %a6 = or i64 %a5, %s6
  %slots = add i64 %a6, 1
  %m = call ptr @gc_allocate(i64 40, i64 1)
  %keys = call ptr @gc_allocate_zeroed(i64 %slots, i64 8, i64 1)
  %values = call ptr @gc_allocate_zeroed(i64 %slots, i64 8, i64 1)
  %states = call ptr @gc_allocate_zeroed(i64 %slots, i64 8, i64 0)
  store ptr %keys, ptr %m
  %vp = getelementptr i64, ptr %m, i64 1
  store ptr %values, ptr %vp
  %stp = getelementptr i64, ptr %m, i64 2
  store ptr %states, ptr %stp
  %sp = getelementptr i64, ptr %m, i64 3
  store i64 %slots, ptr %sp
  %cp = getelementptr i64, ptr %m, i64 4
  store i64 0, ptr %cp
  ret ptr %m
})";

    // A copy of a map: a new one with as many slots, its three tables copied word for word.
    const char* const kCopy = R"(define ptr @map_copy(ptr %m) {
entry:
  %sp = getelementptr i64, ptr %m, i64 3
  %slots = load i64, ptr %sp
  %copy = call ptr @map_new(i64 %slots)
  %bytes = mul i64 %slots, 8
  br label %tables
tables:
  %t = phi i64 [ 0, %entry ], [ %t2, %tables ]
  %from.p = getelementptr i64, ptr %m, i64 %t
  %from = load ptr, ptr %from.p
  %to.p = getelementptr i64, ptr %copy, i64 %t
  %to = load ptr, ptr %to.p
  call void @llvm.memcpy.p0.p0.i64(ptr %to, ptr %from, i64 %bytes, i1 false)
  %t2 = add i64 %t, 1
  %more = icmp ult i64 %t2, 3
  br i1 %more, label %tables, label %done
done:
  %cp = getelementptr i64, ptr %m, i64 4
  %count = load i64, ptr %cp
  %ncp = getelementptr i64, ptr %copy, i64 4
  store i64 %count, ptr %ncp
  ret ptr %copy
})";

    // The slot that holds `key`, or -1: linear probing from the slot its hash picks, until the key or an empty slot.
    const char* const kFind = R"(define i64 @map_find.TAG(ptr %m, i64 %key) {
entry:
  %sp = getelementptr i64, ptr %m, i64 3
  %slots = load i64, ptr %sp
  %mask = sub i64 %slots, 1
  %keys = load ptr, ptr %m
  %stp = getelementptr i64, ptr %m, i64 2
  %states = load ptr, ptr %stp
  %h = call i64 @hash_TAG(i64 %key)
  %start = and i64 %h, %mask
  br label %probe
probe:
  %i = phi i64 [ %start, %entry ], [ %next, %step ]
  %st.p = getelementptr i64, ptr %states, i64 %i
  %st = load i64, ptr %st.p
  %empty = icmp eq i64 %st, 0
  br i1 %empty, label %missing, label %check
check:
  %k.p = getelementptr i64, ptr %keys, i64 %i
  %k = load i64, ptr %k.p
  %same = call i1 @eq_TAG(i64 %k, i64 %key)
  br i1 %same, label %found, label %step
step:
  %n = add i64 %i, 1
  %next = and i64 %n, %mask
  br label %probe
found:
  ret i64 %i
missing:
  ret i64 -1
})";

    // The slot for `key`, which is added when it is not there (the tables grow first once three quarters of the slots are used).
    const char* const kSlot = R"(define i64 @map_slot.TAG(ptr %m, i64 %key) {
entry:
  %cp = getelementptr i64, ptr %m, i64 4
  %count = load i64, ptr %cp
  %sp = getelementptr i64, ptr %m, i64 3
  %slots0 = load i64, ptr %sp
  %used = mul i64 %count, 4
  %limit = mul i64 %slots0, 3
  %full = icmp uge i64 %used, %limit
  br i1 %full, label %grow, label %look
grow:
  call void @map_grow.TAG(ptr %m)
  br label %look
look:
  %found = call i64 @map_find.TAG(ptr %m, i64 %key)
  %have = icmp sge i64 %found, 0
  br i1 %have, label %done, label %insert
done:
  ret i64 %found
insert:
  %slots = load i64, ptr %sp
  %mask = sub i64 %slots, 1
  %keys = load ptr, ptr %m
  %stp = getelementptr i64, ptr %m, i64 2
  %states = load ptr, ptr %stp
  %h = call i64 @hash_TAG(i64 %key)
  %start = and i64 %h, %mask
  br label %probe
probe:
  %i = phi i64 [ %start, %insert ], [ %next, %step ]
  %st.p = getelementptr i64, ptr %states, i64 %i
  %st = load i64, ptr %st.p
  %empty = icmp eq i64 %st, 0
  br i1 %empty, label %place, label %step
step:
  %n = add i64 %i, 1
  %next = and i64 %n, %mask
  br label %probe
place:
  %k.p = getelementptr i64, ptr %keys, i64 %i
  store i64 %key, ptr %k.p
  store i64 1, ptr %st.p
  %count2 = load i64, ptr %cp
  %count3 = add i64 %count2, 1
  store i64 %count3, ptr %cp
  ret i64 %i
})";

    // Twice the slots: every entry placed again in new tables, the old ones given back at once.
    const char* const kGrow = R"(define void @map_grow.TAG(ptr %m) {
entry:
  %sp = getelementptr i64, ptr %m, i64 3
  %old = load i64, ptr %sp
  %new = mul i64 %old, 2
  %oldKeys = load ptr, ptr %m
  %vp = getelementptr i64, ptr %m, i64 1
  %oldValues = load ptr, ptr %vp
  %stp = getelementptr i64, ptr %m, i64 2
  %oldStates = load ptr, ptr %stp
  %newKeys = call ptr @gc_allocate_zeroed(i64 %new, i64 8, i64 1)
  %newValues = call ptr @gc_allocate_zeroed(i64 %new, i64 8, i64 1)
  %newStates = call ptr @gc_allocate_zeroed(i64 %new, i64 8, i64 0)
  store ptr %newKeys, ptr %m
  store ptr %newValues, ptr %vp
  store ptr %newStates, ptr %stp
  store i64 %new, ptr %sp
  %cp = getelementptr i64, ptr %m, i64 4
  store i64 0, ptr %cp
  br label %loop
loop:
  %j = phi i64 [ 0, %entry ], [ %j2, %next ]
  %done = icmp uge i64 %j, %old
  br i1 %done, label %finish, label %visit
visit:
  %st.p = getelementptr i64, ptr %oldStates, i64 %j
  %st = load i64, ptr %st.p
  %live = icmp ne i64 %st, 0
  br i1 %live, label %move, label %next
move:
  %k.p = getelementptr i64, ptr %oldKeys, i64 %j
  %k = load i64, ptr %k.p
  %slot = call i64 @map_slot.TAG(ptr %m, i64 %k)
  %v.p = getelementptr i64, ptr %oldValues, i64 %j
  %v = load i64, ptr %v.p
  %nv.p = getelementptr i64, ptr %newValues, i64 %slot
  store i64 %v, ptr %nv.p
  br label %next
next:
  %j2 = add i64 %j, 1
  br label %loop
finish:
  call void @gc_release(ptr %oldKeys)
  call void @gc_release(ptr %oldValues)
  call void @gc_release(ptr %oldStates)
  ret void
})";

    // The address of `key`'s value, for reading: a missing key gives the address of @zero_cell.
    const char* const kCell = R"(define ptr @map_cell.TAG(ptr %m, i64 %key) {
entry:
  %slot = call i64 @map_find.TAG(ptr %m, i64 %key)
  %miss = icmp slt i64 %slot, 0
  br i1 %miss, label %none, label %some
none:
  ret ptr @zero_cell
some:
  %vp = getelementptr i64, ptr %m, i64 1
  %values = load ptr, ptr %vp
  %cell = getelementptr i64, ptr %values, i64 %slot
  ret ptr %cell
})";

    // The address of `key`'s value, for writing: a missing key is added first.
    const char* const kPut = R"(define ptr @map_put.TAG(ptr %m, i64 %key) {
entry:
  %slot = call i64 @map_slot.TAG(ptr %m, i64 %key)
  %vp = getelementptr i64, ptr %m, i64 1
  %values = load ptr, ptr %vp
  %cell = getelementptr i64, ptr %values, i64 %slot
  ret ptr %cell
})";

    // Takes a key out by shifting the entries of the cluster after it back over the hole (no tombstones): an entry moves into the hole unless its home slot
    // lies cyclically after the hole and at or before the entry. The result is whether the key was there. Every word it overwrites in the keys and values
    // tables goes through the write barrier first (a key or value may be a reference). The other helpers write only into empty slots or fresh tables
    // (map_slot, map_grow, map_copy), and a value stored through map_put's or map_cell's cell gets its barrier where the compiled code stores it.
    const char* const kRemove = R"(define i1 @map_remove.TAG(ptr %m, i64 %key) {
entry:
  %found = call i64 @map_find.TAG(ptr %m, i64 %key)
  %have = icmp sge i64 %found, 0
  br i1 %have, label %setup, label %no
no:
  ret i1 false
setup:
  %sp = getelementptr i64, ptr %m, i64 3
  %slots = load i64, ptr %sp
  %mask = sub i64 %slots, 1
  %keys = load ptr, ptr %m
  %vp = getelementptr i64, ptr %m, i64 1
  %values = load ptr, ptr %vp
  %stp = getelementptr i64, ptr %m, i64 2
  %states = load ptr, ptr %stp
  br label %scan
scan:
  %hole = phi i64 [ %found, %setup ], [ %hole2, %cont ]
  %j = phi i64 [ %found, %setup ], [ %j2, %cont ]
  %j1 = add i64 %j, 1
  %j2 = and i64 %j1, %mask
  %st.p = getelementptr i64, ptr %states, i64 %j2
  %st = load i64, ptr %st.p
  %empty = icmp eq i64 %st, 0
  br i1 %empty, label %finish, label %inspect
inspect:
  %k.p = getelementptr i64, ptr %keys, i64 %j2
  %k = load i64, ptr %k.p
  %h = call i64 @hash_TAG(i64 %k)
  %ideal = and i64 %h, %mask
  %ordered = icmp ule i64 %hole, %j2
  %after = icmp ugt i64 %ideal, %hole
  %before = icmp ule i64 %ideal, %j2
  %between = and i1 %after, %before
  %wrapped = or i1 %after, %before
  %stay = select i1 %ordered, i1 %between, i1 %wrapped
  br i1 %stay, label %cont, label %move
move:
  %hk.p = getelementptr i64, ptr %keys, i64 %hole
  call void @gc_write_barrier(ptr %hk.p)
  store i64 %k, ptr %hk.p
  %v.p = getelementptr i64, ptr %values, i64 %j2
  %v = load i64, ptr %v.p
  %hv.p = getelementptr i64, ptr %values, i64 %hole
  call void @gc_write_barrier(ptr %hv.p)
  store i64 %v, ptr %hv.p
  br label %cont
cont:
  %hole2 = phi i64 [ %hole, %inspect ], [ %j2, %move ]
  br label %scan
finish:
  %fk.p = getelementptr i64, ptr %keys, i64 %hole
  call void @gc_write_barrier(ptr %fk.p)
  store i64 0, ptr %fk.p
  %fv.p = getelementptr i64, ptr %values, i64 %hole
  call void @gc_write_barrier(ptr %fv.p)
  store i64 0, ptr %fv.p
  %fs.p = getelementptr i64, ptr %states, i64 %hole
  store i64 0, ptr %fs.p
  %cp = getelementptr i64, ptr %m, i64 4
  %count = load i64, ptr %cp
  %count2 = sub i64 %count, 1
  store i64 %count2, ptr %cp
  ret i1 true
})";

    // `a == b` on two maps: as many entries, and each key of `a` is in `b` with an equal value (`VTAG` is the values' tag).
    const char* const kEqual = R"(define i1 @map_equal.TAG.VTAG(ptr %a, ptr %b) {
entry:
  %ac.p = getelementptr i64, ptr %a, i64 4
  %ac = load i64, ptr %ac.p
  %bc.p = getelementptr i64, ptr %b, i64 4
  %bc = load i64, ptr %bc.p
  %sameCount = icmp eq i64 %ac, %bc
  br i1 %sameCount, label %scan, label %no
scan:
  %sp = getelementptr i64, ptr %a, i64 3
  %slots = load i64, ptr %sp
  %keys = load ptr, ptr %a
  %vp = getelementptr i64, ptr %a, i64 1
  %values = load ptr, ptr %vp
  %stp = getelementptr i64, ptr %a, i64 2
  %states = load ptr, ptr %stp
  %bvp = getelementptr i64, ptr %b, i64 1
  %bvalues = load ptr, ptr %bvp
  br label %loop
loop:
  %j = phi i64 [ 0, %scan ], [ %j2, %next ]
  %end = icmp uge i64 %j, %slots
  br i1 %end, label %yes, label %visit
visit:
  %st.p = getelementptr i64, ptr %states, i64 %j
  %st = load i64, ptr %st.p
  %live = icmp ne i64 %st, 0
  br i1 %live, label %check, label %next
check:
  %k.p = getelementptr i64, ptr %keys, i64 %j
  %k = load i64, ptr %k.p
  %slot = call i64 @map_find.TAG(ptr %b, i64 %k)
  %have = icmp sge i64 %slot, 0
  br i1 %have, label %compare, label %no
compare:
  %va.p = getelementptr i64, ptr %values, i64 %j
  %va = load i64, ptr %va.p
  %vb.p = getelementptr i64, ptr %bvalues, i64 %slot
  %vb = load i64, ptr %vb.p
  %same = call i1 @eq_VTAG(i64 %va, i64 %vb)
  br i1 %same, label %next, label %no
next:
  %j2 = add i64 %j, 1
  br label %loop
yes:
  ret i1 true
no:
  ret i1 false
})";

    // A number key is hashed by Fibonacci hashing: multiply by 2^64 divided by the golden ratio (0x9E3779B97F4A7C15, written as a signed
    // i64), then fold the high half into the low one, because a slot is picked by the hash's low bits.
    const char* const kWordKey = R"(define i64 @hash_word(i64 %key) {
entry:
  %m = mul i64 %key, -7046029254386353131
  %s = lshr i64 %m, 32
  %x = xor i64 %m, %s
  ret i64 %x
}
define i1 @eq_word(i64 %a, i64 %b) {
entry:
  %same = icmp eq i64 %a, %b
  ret i1 %same
})";

    const char* const kStringKey = R"(define i64 @hash_string(i64 %key) {
entry:
  %p = inttoptr i64 %key to ptr
  %h = call i64 @string_hash(ptr %p)
  ret i64 %h
}
define i1 @eq_string(i64 %a, i64 %b) {
entry:
  %pa = inttoptr i64 %a to ptr
  %pb = inttoptr i64 %b to ptr
  %order = call i32 @strcmp(ptr %pa, ptr %pb)
  %same = icmp eq i32 %order, 0
  ret i1 %same
})";

    String replaceAll(String text, const String& from, const String& to) {
      for (size_t at = text.find(from); at != String::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
      }

      return text;
    }

  }  // namespace

  namespace {

    // A value as a 64-bit word, and back: the IR lines that do it and the register that holds the result.
    WordCast integerWord(const TypePtr& type, const String& value, const String& name) {
      if (type->bits == 64) {
        return WordCast{{}, value};
      }

      // A Bool is a 1-bit integer, and it always zero-extends: true is 1, not -1.
      const String extension = IsSigned(*type) && type->bits > 1 ? "sext" : "zext";

      return WordCast{{name + " = " + extension + " " + LlvmTypeText(type) + " " + value + " to i64"}, name};
    }

    WordCast floatWord(const TypePtr& type, const String& value, const String& name) {
      if (type->bits == 64) {
        return WordCast{{name + " = bitcast double " + value + " to i64"}, name};
      }

      return WordCast{{name + ".i = bitcast float " + value + " to i32", name + " = zext i32 " + name + ".i to i64"}, name};
    }

    WordCast integerFromWord(const TypePtr& type, const String& word, const String& name) {
      if (type->bits == 64) {
        return WordCast{{}, word};
      }

      return WordCast{{name + " = trunc i64 " + word + " to " + LlvmTypeText(type)}, name};
    }

    WordCast floatFromWord(const TypePtr& type, const String& word, const String& name) {
      if (type->bits == 64) {
        return WordCast{{name + " = bitcast i64 " + word + " to double"}, name};
      }

      return WordCast{{name + ".i = trunc i64 " + word + " to i32", name + " = bitcast i32 " + name + ".i to float"}, name};
    }

  }  // namespace

  WordCast ToWord(const TypePtr& type, const String& value, const String& name) {
    if (IsInteger(*type)) {
      return integerWord(type, value, name);
    }

    if (IsFloat(*type)) {
      return floatWord(type, value, name);
    }

    return WordCast{{name + " = ptrtoint ptr " + value + " to i64"}, name};
  }

  WordCast FromWord(const TypePtr& type, const String& word, const String& name) {
    if (IsInteger(*type)) {
      return integerFromWord(type, word, name);
    }

    if (IsFloat(*type)) {
      return floatFromWord(type, word, name);
    }

    return WordCast{{name + " = inttoptr i64 " + word + " to ptr"}, name};
  }

  namespace {

    // ── what a key is compared by ──

    String keyTag(const TypePtr& key) {
      if (key->kind == TypeKind::String) {
        return "string";
      }

      if (key->kind == TypeKind::DynamicArray) {
        return "L_" + keyTag(key->element);
      }

      return key->kind == TypeKind::Struct ? "S_" + SafeName(key->name) : "word";
    }

    bool isListKey(const TypePtr& key) {
      return key->kind == TypeKind::DynamicArray;
    }

    bool isStructKey(const TypePtr& key) {
      return key->kind == TypeKind::Struct;
    }

    Vector<String> fieldLoad(const StructDef& structure, size_t index, const String& pointer, const String& name) {
      const TypePtr& type = structure.fields[index].type;
      const String address = name + ".p";

      return {address + " = getelementptr " + StructName(structure.name) + ", ptr " + pointer + ", i32 0, i32 " + to_string(index),
              name + " = load " + LlvmTypeText(type) + ", ptr " + address};
    }

    void append(Vector<String>& lines, const Vector<String>& more) {
      lines.insert(lines.end(), more.begin(), more.end());
    }

    // The lines that read field `index` of the struct held in the word `%key` (or `%a`/`%b`) as a word of its own.
    Vector<String> fieldWord(const StructDef& structure, size_t index, const String& pointer, const String& name) {
      Vector<String> lines = fieldLoad(structure, index, pointer, name);
      const WordCast word = ToWord(structure.fields[index].type, name, name + ".w");
      append(lines, word.lines);
      lines.push_back(name + ".key = add i64 " + word.result + ", 0");

      return lines;
    }

    String joinLines(const Vector<String>& lines) {
      String out;

      for (const String& line : lines) {
        out += "  " + line + "\n";
      }

      return out;
    }

    // The lines that fold field `i` into the running hash.
    Vector<String> hashStep(const StructDef& structure, size_t i) {
      const String f = "%f" + to_string(i);
      Vector<String> lines = fieldWord(structure, i, "%p", f);
      lines.push_back(f + ".h = call i64 @hash_" + keyTag(structure.fields[i].type) + "(i64 " + f + ".key)");
      lines.push_back(f + ".m = mul i64 %acc" + to_string(i) + ", 31");
      lines.push_back("%acc" + to_string(i + 1) + " = add i64 " + f + ".m, " + f + ".h");

      return lines;
    }

    String structHash(const StructDef& structure) {
      Vector<String> lines = {"%p = inttoptr i64 %key to ptr", "%acc0 = add i64 17, 0"};

      for (size_t i = 0; i < structure.fields.size(); i++) {
        append(lines, hashStep(structure, i));
      }

      return "define i64 @hash_S_" + SafeName(structure.name) + "(i64 %key) {\nentry:\n" + joinLines(lines) + "  ret i64 %acc" + to_string(structure.fields.size()) + "\n}";
    }

    // The lines that compare field `i` of both structs and fold the answer into the running one.
    Vector<String> equalStep(const StructDef& structure, size_t i) {
      const String x = "%x" + to_string(i);
      const String y = "%y" + to_string(i);
      Vector<String> lines = fieldWord(structure, i, "%pa", x);
      append(lines, fieldWord(structure, i, "%pb", y));

      lines.push_back("%e" + to_string(i) + " = call i1 @eq_" + keyTag(structure.fields[i].type) + "(i64 " + x + ".key, i64 " + y + ".key)");
      lines.push_back("%acc" + to_string(i + 1) + " = and i1 %acc" + to_string(i) + ", %e" + to_string(i));

      return lines;
    }

    String structEq(const StructDef& structure) {
      Vector<String> lines = {"%pa = inttoptr i64 %a to ptr", "%pb = inttoptr i64 %b to ptr", "%acc0 = add i1 true, false"};

      for (size_t i = 0; i < structure.fields.size(); i++) {
        append(lines, equalStep(structure, i));
      }

      return "define i1 @eq_S_" + SafeName(structure.name) + "(i64 %a, i64 %b) {\nentry:\n" + joinLines(lines) + "  ret i1 %acc" + to_string(structure.fields.size()) + "\n}";
    }

    // The loop over a list key's elements, as the VM compares them. `ACC` is the type of the running answer `%acc` and `START` its value
    // for an empty list. The caller writes the length `%n` before it and, after its `body:`, the loop body (element `%i` gives the next
    // answer `%acc2`; `%i2 = %i + 1`; back to `%loop`) and the `done:` block. The loop is entered from the block `%entry`.
    const char* const kListLoop = R"(  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %acc = phi ACC [ START, %entry ], [ %acc2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
)";

    // The lines that read element `%i` of the list whose elements start at `data` as the word `name`.
    String elementWord(const TypePtr& element, const String& data, const String& name) {
      const String type = LlvmTypeText(element);
      Vector<String> lines = {name + ".p = getelementptr " + type + ", ptr " + data + ", i64 %i", name + " = load " + type + ", ptr " + name + ".p"};
      const WordCast word = ToWord(element, name, name + ".w");
      append(lines, word.lines);
      lines.push_back(name + ".key = add i64 " + word.result + ", 0");

      return joinLines(lines);
    }

    // The lines that read the length (`%<prefix>n`) and the elements (`%<prefix>data`) of the list whose address is the word `word`.
    String listParts(const String& prefix, const String& word) {
      const String p = "%" + prefix + "p";

      return "  " + p + " = inttoptr i64 " + word + " to ptr\n  %" + prefix + "n = load i64, ptr " + p + "\n  %" + prefix + "dp = getelementptr i64, ptr " + p +
             ", i64 2\n  %" + prefix + "data = load ptr, ptr %" + prefix + "dp\n";
    }

    String listLoop(const char* accType, const char* start) {
      return replaceAll(replaceAll(kListLoop, "ACC", accType), "START", start);
    }

    String listHash(const TypePtr& list) {
      const String tag = keyTag(list->element);
      const String body = elementWord(list->element, "%data", "%x") + "  %h = call i64 @hash_" + tag + "(i64 %x.key)\n  %m = mul i64 %acc, 31\n  %acc2 = add i64 %m, %h\n";

      return "define i64 @hash_" + keyTag(list) + "(i64 %key) {\nentry:\n" + listParts("", "%key") + listLoop("i64", "17") + body +
             "  %i2 = add i64 %i, 1\n  br label %loop\ndone:\n  ret i64 %acc\n}";
    }

    // Two lists of different lengths differ; otherwise the elements are compared pairwise until two differ. The loop is entered from the
    // block `%start`, after the length check, so its phis name `%start` where kListLoop says `%entry`.
    String listEq(const TypePtr& list) {
      const String tag = keyTag(list->element);
      const String body = elementWord(list->element, "%adata", "%x") + elementWord(list->element, "%bdata", "%y") + "  %same = call i1 @eq_" + tag +
                          "(i64 %x.key, i64 %y.key)\n  %acc2 = and i1 %acc, %same\n";
      const String lengths = "  %n = add i64 %an, 0\n  %sameLength = icmp eq i64 %an, %bn\n  br i1 %sameLength, label %start, label %differ\ndiffer:\n  ret i1 false\nstart:\n";

      return "define i1 @eq_" + keyTag(list) + "(i64 %a, i64 %b) {\nentry:\n" + listParts("a", "%a") + listParts("b", "%b") + lengths +
             replaceAll(listLoop("i1", "true"), "%entry", "%start") + body + "  %i2 = add i64 %i, 1\n  br label %loop\ndone:\n  ret i1 %acc\n}";
    }

  }  // namespace

  // Makes sure the module has the functions that hash and compare keys of this type and of the types of its fields. The table functions for
  // it are made too. Returns the key's tag: the end of those functions' names (`@hash_<tag>`, `@eq_<tag>`, `@map_find.<tag>`, ...).
  String EnsureKeyFunctions(FunctionEmitter& emitter, const TypePtr& key) {
    const String tag = keyTag(key);
    NeedKeyRuntime(emitter);

    if (FirstUse(emitter.context_, "keys." + tag)) {
      DefineKeyFunctions(emitter, key, tag);
    }

    return tag;
  }

  void NeedKeyRuntime(FunctionEmitter& emitter) {
    Need(emitter.context_, kZero);
    NeedCollector(emitter.context_);
    Need(emitter.context_, "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)");
  }

  // The function that makes a map (@map_new), and the collector it allocates from.
  void NeedMapNew(FunctionEmitter& emitter) {
    NeedCollector(emitter.context_);
    Need(emitter.context_, kNew);
  }

  void DefineKeyFunctions(FunctionEmitter& emitter, const TypePtr& key, const String& tag) {
    NeedMapNew(emitter);
    Need(emitter.context_, kCopy);
    DefineKeyKind(emitter, key, tag);

    for (const char* const table : {kFind, kSlot, kGrow, kCell, kPut, kRemove}) {
      Need(emitter.context_, replaceAll(table, "TAG", tag));
    }
  }

  void DefineKeyKind(FunctionEmitter& emitter, const TypePtr& key, const String& tag) {
    if (tag == "string") {
      DefineStringKey(emitter);
    } else if (isStructKey(key)) {
      DefineStructKey(emitter, *FindStruct(ModuleOf(emitter.context_), key->name));
    } else if (isListKey(key)) {
      DefineListKey(emitter, key);
    } else {
      Need(emitter.context_, kWordKey);
    }
  }

  void DefineListKey(FunctionEmitter& emitter, const TypePtr& list) {
    EnsureKeyFunctions(emitter, list->element);
    Need(emitter.context_, listHash(list));
    Need(emitter.context_, listEq(list));
  }

  void DefineStringKey(FunctionEmitter& emitter) {
    Need(emitter.context_, "declare i32 @strcmp(ptr, ptr)");
    Need(emitter.context_, HashFunctionText());
    Need(emitter.context_, kStringKey);
  }

  void DefineStructKey(FunctionEmitter& emitter, const StructDef& structure) {
    for (const Field& field : structure.fields) {
      EnsureKeyFunctions(emitter, field.type);
    }

    Need(emitter.context_, structHash(structure));
    Need(emitter.context_, structEq(structure));
  }

  String CastLines(FunctionEmitter& emitter, const WordCast& cast) {
    for (const String& line : cast.lines) {
      Line(emitter, line);
    }

    return cast.result;
  }

  String KeyWord(FunctionEmitter& emitter, const Value& key) {
    return CastLines(emitter, ToWord(key.type, key.text, NewTemporary(emitter)));
  }

  Value WordValue(FunctionEmitter& emitter, const String& word, const TypePtr& type) {
    return Value{CastLines(emitter, FromWord(type, word, NewTemporary(emitter))), type};
  }

  // The address of the value cell of a key: reading a missing key gives the address of a zero.
  String MapReadCell(FunctionEmitter& emitter, const Value& map, const Value& key) {
    const String tag = EnsureKeyFunctions(emitter, key.type);

    return Instruction(emitter, "call ptr @map_cell." + tag + "(ptr " + map.text + ", i64 " + KeyWord(emitter, key) + ")");
  }

  // ... and for writing, which adds the key when it is not there.
  String MapWriteCell(FunctionEmitter& emitter, const Value& map, const Value& key) {
    const String tag = EnsureKeyFunctions(emitter, key.type);
    NoteAllocation(emitter);

    return Instruction(emitter, "call ptr @map_put." + tag + "(ptr " + map.text + ", i64 " + KeyWord(emitter, key) + ")");
  }

  Value NewMap(FunctionEmitter& emitter, const TypePtr& type, const String& requested) {
    NeedMapNew(emitter);
    NoteAllocation(emitter);

    return Value{Instruction(emitter, "call ptr @map_new(i64 " + requested + ")"), type};
  }

  Value NewMapFrom(FunctionEmitter& emitter, const Expr& expr) {
    const Value requested = expr.operands.empty() ? Value{"4", MakeInt(64)} : IndexValue(emitter, EmitExpr(emitter, *expr.operands[0]));
    EnsureKeyFunctions(emitter, expr.type->element);

    return NewMap(emitter, expr.type, requested.text);
  }

  Value SetLiteral(FunctionEmitter& emitter, const Expr& expr) {
    const Vector<Value> items = OperandValues(emitter, expr, 0);
    const Value set = NewMap(emitter, expr.type, to_string(items.size()));

    for (const Value& item : items) {
      MapAdd(emitter, set, item);
    }

    return set;
  }

  Value MapAdd(FunctionEmitter& emitter, const Value& set, const Value& member) {
    MapWriteCell(emitter, set, member);

    return set;
  }

  Value MapHas(FunctionEmitter& emitter, const Value& map, const Value& key) {
    const String tag = EnsureKeyFunctions(emitter, key.type);
    const String slot = Instruction(emitter, "call i64 @map_find." + tag + "(ptr " + map.text + ", i64 " + KeyWord(emitter, key) + ")");

    return Value{Instruction(emitter, "icmp sge i64 " + slot + ", 0"), MakeBool()};
  }

  Value MapRemove(FunctionEmitter& emitter, const Value& map, const Value& key) {
    const String tag = EnsureKeyFunctions(emitter, key.type);

    return Value{Instruction(emitter, "call i1 @map_remove." + tag + "(ptr " + map.text + ", i64 " + KeyWord(emitter, key) + ")"), MakeBool()};
  }

  Value MapCopy(FunctionEmitter& emitter, const Value& map) {
    Need(emitter.context_, kCopy);
    NeedMapNew(emitter);
    NoteAllocation(emitter);
    Need(emitter.context_, "declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)");

    return Value{Instruction(emitter, "call ptr @map_copy(ptr " + map.text + ")"), map.type};
  }

  // The key and the value are worked out before the copy is made. They may call, and the copy is in no root until it is returned.
  Value MapWith(FunctionEmitter& emitter, const Expr& expr) {
    const Vector<Value> parts = ValuesOf(emitter, expr.operands);  // the map, the key and (for a map, not a set) the value
    Value copy = MapCopy(emitter, parts[0]);
    const String cell = MapWriteCell(emitter, copy, parts[1]);

    if (parts.size() > 2) {
      Store(emitter, Fit(emitter, parts[2], copy.type->second), cell);
    }

    copy.type = expr.type;

    return copy;
  }

  // A struct with no `var` field is a value: two of them are equal when their fields are, whatever object they are held in.
  bool IsValueStruct(const FunctionEmitter& emitter, const TypePtr& type) {
    const StructDef* structure = type->kind == TypeKind::Struct ? FindStruct(ModuleOf(emitter.context_), type->name) : nullptr;
    const auto mutableField = [](const Field& field) { return field.isMutable; };

    return structure != nullptr && !FindInterface(ModuleOf(emitter.context_), type->name) && none_of(structure->fields.begin(), structure->fields.end(), mutableField);
  }

  // Whether the module has the function that compares two values of the type (LlvmEquality.cpp adds it when the program compares them).
  bool HasEqualityHelper(const FunctionEmitter& emitter, const TypePtr& type) {
    return NeedsEquality(ModuleOf(emitter.context_), type) && FindFunction(ModuleOf(emitter.context_), EqualityName(type)) != nullptr;
  }

  // `a == b` and `a != b` on structs, lists, eithers, optionals and variants: a call of their comparison function.
  Value HelperComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
    const String call = Instruction(emitter, "call i1 @" + QuotedName(EqualityName(left.type)) + "(" + Typed(left) + ", " + Typed(right) + ")");

    return op == BinOp::Eq ? Value{call, MakeBool()} : Value{Instruction(emitter, "xor i1 " + call + ", true"), MakeBool()};
  }

  Value StructComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
    const String tag = EnsureKeyFunctions(emitter, left.type);
    const String same = Instruction(emitter, "call i1 @eq_" + tag + "(i64 " + KeyWord(emitter, left) + ", i64 " + KeyWord(emitter, right) + ")");

    return op == BinOp::Eq ? Value{same, MakeBool()} : Value{Instruction(emitter, "xor i1 " + same + ", true"), MakeBool()};
  }

  // `a == b` on maps and sets: the same entries, whatever order they were added in.
  Value MapComparison(FunctionEmitter& emitter, BinOp op, const Value& left, const Value& right) {
    const TypePtr valueType = left.type->kind == TypeKind::Hashmap ? left.type->second : MakeInt(64);
    const String tag = EnsureKeyFunctions(emitter, left.type->element);
    const String valueTag = EnsureKeyFunctions(emitter, valueType);
    Need(emitter.context_, replaceAll(replaceAll(kEqual, "VTAG", valueTag), "TAG", tag));  // VTAG first: TAG is part of it
    const String same = Instruction(emitter, "call i1 @map_equal." + tag + "." + valueTag + "(ptr " + left.text + ", ptr " + right.text + ")");

    return op == BinOp::Eq ? Value{same, MakeBool()} : Value{Instruction(emitter, "xor i1 " + same + ", true"), MakeBool()};
  }

  Value MapWord(FunctionEmitter& emitter, const Value& map, int64_t cell) {
    return LoadFrom(emitter, WordAddress(emitter, map.text, cell), MakeInt(64));
  }

  void MapFree(FunctionEmitter& emitter, const Value& map) {
    for (int64_t table = 0; table < 3; table++) {
      FreeHint(emitter, LoadFrom(emitter, WordAddress(emitter, map.text, table), MakePointer(MakeInt(8))).text);
    }

    FreeHint(emitter, map.text);
  }

  bool MapBuiltin(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    const TypeKind kind = expr.operands[0]->type->kind;

    if (kind != TypeKind::Hashmap && kind != TypeKind::Set) {
      return false;
    }

    return MapOperation(emitter, expr, out);
  }

  bool MapOperation(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    switch (expr.builtin) {
      case Builtin::Len: {
        out = MapWord(emitter, EmitExpr(emitter, *expr.operands[0]), 4);
        return true;
      }
      case Builtin::Capacity: {
        out = MapWord(emitter, EmitExpr(emitter, *expr.operands[0]), 3);
        return true;
      }
      case Builtin::Has: {
        out = MapHas(emitter, EmitExpr(emitter, *expr.operands[0]), EmitExpr(emitter, *expr.operands[1]));
        return true;
      }
      default:
        return MapUpdate(emitter, expr, out);
    }
  }

  namespace {

    // `copy` and `with` make a new map and leave the old one as it was. Any other built-in goes on to MapOther.
    bool mapCopyOrWith(FunctionEmitter& emitter, const Expr& expr, Value& out) {
      switch (expr.builtin) {
        case Builtin::Copy: {
          out = MapCopy(emitter, EmitExpr(emitter, *expr.operands[0]));
          return true;
        }
        case Builtin::With: {
          out = MapWith(emitter, expr);
          return true;
        }
        default:
          return MapOther(emitter, expr, out);
      }
    }

  }  // namespace

  // `add` and `remove` change the map in place. `copy`, `with` and the rest go on to mapCopyOrWith.
  bool MapUpdate(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    switch (expr.builtin) {
      case Builtin::Add: {
        out = MapAdd(emitter, EmitExpr(emitter, *expr.operands[0]), EmitExpr(emitter, *expr.operands[1]));
        return true;
      }
      case Builtin::Remove: {
        out = MapRemove(emitter, EmitExpr(emitter, *expr.operands[0]), EmitExpr(emitter, *expr.operands[1]));
        return true;
      }
      default:
        return mapCopyOrWith(emitter, expr, out);
    }
  }

  Value FreedMap(FunctionEmitter& emitter, const Expr& expr) {
    MapFree(emitter, EmitExpr(emitter, *expr.operands[0]));

    return Value{"", MakeVoid()};
  }

  bool MapOther(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin == Builtin::Retype) {
      out = Value{EmitExpr(emitter, *expr.operands[0]).text, expr.type};
    } else if (expr.builtin == Builtin::Free) {
      out = FreedMap(emitter, expr);
    } else {
      return MapSlot(emitter, expr, out);
    }

    return true;
  }

  // The cell of slot `slot` in one of the map's three tables (0 keys, 1 values, 2 states).
  String SlotAddress(FunctionEmitter& emitter, const Value& map, int64_t table, const Value& slot) {
    const String base = LoadFrom(emitter, WordAddress(emitter, map.text, table), MakePointer(MakeInt(8))).text;

    return WordAddress(emitter, base, slot.text);
  }

  // `AssocLive(map, i)` is whether slot i holds an entry, `AssocKey(map, i)` its key (or the member of a set) and `AssocValue(map, i)` its value.
  // With a `Capacity` loop they walk a map or set in table order.
  bool MapSlot(FunctionEmitter& emitter, const Expr& expr, Value& out) {
    if (expr.builtin != Builtin::AssocLive && expr.builtin != Builtin::AssocKey && expr.builtin != Builtin::AssocValue) {
      return false;
    }

    const Value map = EmitExpr(emitter, *expr.operands[0]);
    const Value slot = IndexValue(emitter, EmitExpr(emitter, *expr.operands[1]));

    if (expr.builtin == Builtin::AssocLive) {
      out = Value{Instruction(emitter, "icmp ne i64 " + LoadFrom(emitter, SlotAddress(emitter, map, 2, slot), MakeInt(64)).text + ", 0"), MakeBool()};
    } else if (expr.builtin == Builtin::AssocKey) {
      out = WordValue(emitter, LoadFrom(emitter, SlotAddress(emitter, map, 0, slot), MakeInt(64)).text, expr.type);
    } else {
      out = LoadFrom(emitter, SlotAddress(emitter, map, 1, slot), expr.type);
    }

    return true;
  }

}  // namespace polyir::llvm
