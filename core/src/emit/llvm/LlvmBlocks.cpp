// Tidies a function's basic blocks so the output has no block that a person would not write: blocks nothing jumps to are dropped, a block that only
// jumps on is skipped over, a block that is the only way into the next one (the jump between them being unconditional) is joined to it, and a
// phi with a single entry is replaced by its value.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    struct Block {
      String name;
      Vector<String> body;  // the instructions, the terminator last
    };

    bool isLabelChar(char c) {
      return isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '$' || c == '-';
    }

    // The index just past the label name that starts at `from`.
    size_t labelEnd(const String& text, size_t from) {
      while (from < text.size() && isLabelChar(text[from])) {
        from++;
      }

      return from;
    }

    // The names after each `label %` in a terminator.
    Vector<String> successors(const String& terminator) {
      Vector<String> names;
      const String marker = "label %";

      for (size_t at = terminator.find(marker); at != String::npos; at = terminator.find(marker, at + 1)) {
        const size_t start = at + marker.size();
        names.push_back(terminator.substr(start, labelEnd(terminator, start) - start));
      }

      return names;
    }

    bool labelIs(const String& text, size_t start, const String& name) {
      return labelEnd(text, start) - start == name.size() && text.compare(start, name.size(), name) == 0;
    }

    // Renames the label at `start` when it is `from`; returns where the next search for a label begins.
    size_t retargetOne(String& out, size_t start, const String& from, const String& to) {
      const bool match = labelIs(out, start, from);

      if (match) {
        out.replace(start, from.size(), to);
      }

      return start + (match ? to.size() : 0);
    }

    // Every `label %from` becomes `label %to`.
    String retargetLabels(const String& line, const String& from, const String& to) {
      String out = line;
      const String marker = "label %";
      size_t at = out.find(marker);

      while (at != String::npos) {
        at = out.find(marker, retargetOne(out, at + marker.size(), from, to));
      }

      return out;
    }

    // A jump or branch target changes, and so does the predecessor named in a phi (`[ value, %from ]`).
    String retarget(const String& line, const String& from, const String& to) {
      String out = retargetLabels(line, from, to);
      const String old = ", %" + from + " ]";

      for (size_t at = out.find(old); at != String::npos; at = out.find(old, at + 1)) {
        out.replace(at, old.size(), ", %" + to + " ]");
      }

      return out;
    }

    // The phi line with one entry cut out (from `open`, its `[`, to `close`, just past its `]`). The ", " that joined it to a neighbour
    // goes too: the one after it when another entry follows, else the one before it.
    String cutEntry(const String& line, size_t open, size_t close) {
      const String separator = ", ";
      const String nextEntry = ", [";
      const bool hasEntryAfter = line.compare(close, nextEntry.size(), nextEntry) == 0;

      if (hasEntryAfter) {
        return line.substr(0, open) + line.substr(close + separator.size());
      }

      const size_t from = open < separator.size() ? open : open - separator.size();

      return line.substr(0, from) + line.substr(close);
    }

    // A phi line without the entries that come from `predecessor`; any other line as it is.
    String withoutIncoming(const String& line, const String& predecessor) {
      const String tail = ", %" + predecessor + " ]";
      const size_t end = line.find(tail);

      if (end == String::npos || line.find(" = phi ") == String::npos) {
        return line;
      }

      return withoutIncoming(cutEntry(line, line.rfind("[ ", end), end + tail.size()), predecessor);
    }

    // Every use of the value `name` (not of a longer name that starts the same) becomes `value`.
    String replaceUses(const String& line, const String& name, const String& value) {
      String out = line;

      for (size_t at = out.find(name); at != String::npos; at = out.find(name, at + 1)) {
        if (labelEnd(out, at + name.size()) != at + name.size()) {
          continue;
        }

        out.replace(at, name.size(), value);
        at += value.size() - 1;
      }

      return out;
    }

    // The value of a phi with a single entry, `[ value, %block ]`; empty when the line is not one.
    String singleEntry(const String& line) {
      const size_t open = line.find("[ ");

      if (line.find(" = phi ") == String::npos || open == String::npos || line.find("[ ", open + 1) != String::npos) {
        return "";
      }

      return line.substr(open + 2, line.find(", %", open) - open - 2);
    }

    struct Function {
      Vector<Block> blocks_;
    };

    Vector<String> lines(const Function& function);

    bool tidy(Function& function);

    void split(Function& function, const Vector<String>& lines);

    Set<String> reachable(Function& function);

    bool dropUnreachable(Function& function);

    bool foldSingletons(Function& function);

    bool foldIn(Function& function, Block& block);

    void fold(Function& function, Block& block, size_t i, const String& value);

    void dropBlockAt(Function& function, size_t i);

    void replaceEverywhere(Function& function, const String& name, const String& value);

    void forget(Function& function, const String& predecessor);

    void visitSuccessors(Function& function, const String& name, Set<String>& seen, Vector<String>& work);

    const Block* forwardedTo(Function& function, const Block& block, String& target);

    void bypass(Function& function, String name, const String& target);

    bool skipForwarders(Function& function);

    Block* joinableNext(Function& function, Block& block);

    void merge(Function& function, Block& block, Block& next);

    bool joinChains(Function& function);

    Block* find(Function& function, const String& name);

    int entriesInto(const Function& function, const String& name);

    void retargetAll(Function& function, const String& from, const String& to);

    void remove(Function& function, const String& name);

    String lastLine(const Block& block);

    bool isJump(const String& line, String& target);

    bool hasPhi(const Block& block);

    String lastLine(const Block& block) {
      return block.body.empty() ? "" : block.body.back();
    }

    bool startsBlock(const String& line) {
      return line.compare(0, 2, "  ") != 0 && !line.empty() && line.back() == ':';
    }

    String popBack(Vector<String>& names) {
      String last = names.back();
      names.pop_back();

      return last;
    }

    Function makeFunction(const Vector<String>& lines) {
      Function function;
      split(function, lines);

      return function;
    }

    void split(Function& function, const Vector<String>& lines) {
      function.blocks_.push_back(Block{"entry", {}});

      for (const String& line : lines) {
        if (startsBlock(line)) {
          function.blocks_.push_back(Block{line.substr(0, line.size() - 1), {}});
        } else {
          function.blocks_.back().body.push_back(line);
        }
      }
    }

    Vector<String> lines(const Function& function) {
      Vector<String> out;

      for (size_t i = 0; i < function.blocks_.size(); i++) {
        if (i > 0) {
          out.push_back(function.blocks_[i].name + ":");
        }

        out.insert(out.end(), function.blocks_[i].body.begin(), function.blocks_[i].body.end());
      }

      return out;
    }

    Block* find(Function& function, const String& name) {
      for (Block& block : function.blocks_) {
        if (block.name == name) {
          return &block;
        }
      }

      return nullptr;
    }

    bool isJump(const String& line, String& target) {
      const size_t at = line.find("br label %");

      if (line.compare(0, 2, "  ") != 0 || at != 2) {
        return false;
      }

      target = successors(line)[0];

      return true;
    }

    bool hasPhi(const Block& block) {
      return !block.body.empty() && block.body.front().find(" = phi ") != String::npos;
    }

    int entriesInto(const Function& function, const String& name) {
      int count = 0;

      for (const Block& block : function.blocks_) {
        for (const String& s : successors(lastLine(block))) {
          count += s == name ? 1 : 0;
        }
      }

      return count;
    }

    void retargetAll(Function& function, const String& from, const String& to) {
      for (Block& block : function.blocks_) {
        for (String& line : block.body) {
          line = retarget(line, from, to);
        }
      }
    }

    void remove(Function& function, const String& name) {
      for (size_t i = 0; i < function.blocks_.size(); i++) {
        if (function.blocks_[i].name == name) {
          function.blocks_.erase(function.blocks_.begin() + static_cast<long>(i));
        }
      }
    }

    void visitSuccessors(Function& function, const String& name, Set<String>& seen, Vector<String>& work) {
      for (const String& s : successors(lastLine(*find(function, name)))) {
        if (seen.insert(s).second) {
          work.push_back(s);
        }
      }
    }

    Set<String> reachable(Function& function) {
      Set<String> seen = {"entry"};
      Vector<String> work = {"entry"};

      while (!work.empty()) {
        visitSuccessors(function, popBack(work), seen, work);
      }

      return seen;
    }

    // The phis of the other blocks stop listing a block that is gone.
    void forget(Function& function, const String& predecessor) {
      for (Block& block : function.blocks_) {
        for (String& line : block.body) {
          line = withoutIncoming(line, predecessor);
        }
      }
    }

    void dropBlockAt(Function& function, size_t i) {
      const String gone = function.blocks_[i].name;
      function.blocks_.erase(function.blocks_.begin() + static_cast<long>(i));
      forget(function, gone);
    }

    bool dropUnreachable(Function& function) {
      const Set<String> seen = reachable(function);
      const size_t before = function.blocks_.size();

      for (size_t i = function.blocks_.size(); i > 0; i--) {
        if (!seen.count(function.blocks_[i - 1].name)) {
          dropBlockAt(function, i - 1);
        }
      }

      return function.blocks_.size() != before;
    }

    // The block a forwarder jumps on to, or null when `block` is not a forwarder worth skipping.
    const Block* forwardedTo(Function& function, const Block& block, String& target) {
      const Block* next = block.body.size() == 1 && isJump(block.body[0], target) ? find(function, target) : nullptr;

      if (block.name == "entry" || next == nullptr || next->name == block.name || hasPhi(*next)) {
        return nullptr;
      }

      return next;
    }

    void bypass(Function& function, String name, const String& target) {
      retargetAll(function, name, target);
      remove(function, name);
    }

    // A block that holds only `br label %next` is not needed: whatever jumped to it jumps to `next`. (Not when `next` starts with a phi, which names it.)
    bool skipForwarders(Function& function) {
      for (const Block& block : function.blocks_) {
        String target;

        if (forwardedTo(function, block, target) == nullptr) {
          continue;
        }

        bypass(function, block.name, target);
        return true;
      }

      return false;
    }

    // The block `block` jumps to when nothing else enters it, or null.
    Block* joinableNext(Function& function, Block& block) {
      String target;
      Block* next = isJump(lastLine(block), target) ? find(function, target) : nullptr;

      if (next == nullptr || next == &block || next->name == "entry" || hasPhi(*next) || entriesInto(function, target) != 1) {
        return nullptr;
      }

      return next;
    }

    void merge(Function& function, Block& block, Block& next) {
      const String joined = next.name;
      block.body.pop_back();
      block.body.insert(block.body.end(), next.body.begin(), next.body.end());
      retargetAll(function, joined, block.name);
      remove(function, joined);
    }

    // `a: ...; br label %b` where nothing else enters `b`: one block.
    bool joinChains(Function& function) {
      for (Block& block : function.blocks_) {
        Block* next = joinableNext(function, block);

        if (next == nullptr) {
          continue;
        }

        merge(function, block, *next);
        return true;
      }

      return false;
    }

    void replaceEverywhere(Function& function, const String& name, const String& value) {
      for (Block& block : function.blocks_) {
        for (String& line : block.body) {
          line = replaceUses(line, name, value);
        }
      }
    }

    void fold(Function& function, Block& block, size_t i, const String& value) {
      const String name = block.body[i].substr(2, block.body[i].find(" = ") - 2);
      block.body.erase(block.body.begin() + static_cast<long>(i));
      replaceEverywhere(function, name, value);
    }

    bool foldIn(Function& function, Block& block) {
      for (size_t i = 0; i < block.body.size(); i++) {
        const String value = singleEntry(block.body[i]);

        if (value.empty()) {
          continue;
        }

        fold(function, block, i, value);
        return true;
      }

      return false;
    }

    // A phi that can only be one value is that value.
    bool foldSingletons(Function& function) {
      for (Block& block : function.blocks_) {
        if (foldIn(function, block)) {
          return true;
        }
      }

      return false;
    }

    bool tidy(Function& function) {
      bool changed = false;

      while (dropUnreachable(function) || foldSingletons(function) || skipForwarders(function) || joinChains(function)) {
        changed = true;
      }

      return changed;
    }

  }  // namespace

  Vector<String> TidyBlocks(const Vector<String>& lines) {
    Function function = makeFunction(lines);
    tidy(function);

    return polyir::llvm::lines(function);
  }

}  // namespace polyir::llvm
