// Labels and the jumps to them. A label's parameters are phi nodes at the head of its block (the value each jump hands over is recorded with the block
// the jump leaves from, and written into the phi when the function ends), unless the function assigns the name or a closure captures it: then the
// parameter is a stack slot that every jump stores into. A `branch` or `switch` names the label's block directly when its arguments are cheap and
// safe to work out before the choice is made; otherwise each edge gets a block of its own.
#include "polyir/Std.hpp"
#include "emit/llvm/LlvmEmitter.hpp"

namespace polyir::llvm {

  namespace {

    bool isScalar(const TypePtr& type) {
      return IsInteger(*type) || IsFloat(*type) || IsBoolean(*type);
    }

    bool isPure(const Expr& expr);

    bool allPure(const Vector<ExprPtr>& exprs) {
      for (const ExprPtr& expr : exprs) {
        if (!isPure(*expr)) {
          return false;
        }
      }

      return true;
    }

    // An operation that cannot trap, call or allocate, so it may be worked out on a path the program might not take.
    bool isPureOperation(const Expr& expr) {
      const bool arithmetic = expr.kind == ExprKind::Binary && !IsLogical(expr.binaryOp) && expr.binaryOp != BinOp::Div && expr.binaryOp != BinOp::Rem;
      const bool conversion = expr.kind == ExprKind::Unary || expr.kind == ExprKind::Cast || expr.kind == ExprKind::Coerce;

      return (arithmetic || conversion) && isScalar(expr.type) && allPure(expr.operands) && (expr.operands.empty() || isScalar(expr.operands[0]->type));
    }

    bool isPure(const Expr& expr) {
      static const Set<ExprKind> kLeaves = {ExprKind::ConstInt, ExprKind::ConstFloat, ExprKind::ConstBool, ExprKind::Local, ExprKind::LoadSlot};

      return kLeaves.count(expr.kind) != 0 || isPureOperation(expr);
    }

    String joined(const Vector<String>& parts) {
      String out;

      for (size_t i = 0; i < parts.size(); i++) {
        out += (i == 0 ? "" : ", ") + parts[i];
      }

      return out;
    }

  }  // namespace

  void CollectLabels(FunctionEmitter& emitter) {
    for (const StmtPtr& statement : emitter.function_.body) {
      if (statement->kind == StmtKind::Label) {
        emitter.labels_[statement->name] = LabelInfoOf(emitter, *statement);
      }
    }
  }

  FunctionEmitter::LabelInfo LabelInfoOf(FunctionEmitter& emitter, const Stmt& statement) {
    FunctionEmitter::LabelInfo info{UniqueName(emitter, SafeName(statement.name)), {}, Vector<Vector<String>>(statement.parameterNames.size()), Vector<size_t>(statement.parameterNames.size(), 0), true};

    for (size_t i = 0; i < statement.parameterNames.size(); i++) {
      info.slots.push_back(LabelSlot(emitter, statement, i));
      info.allPhis = info.allPhis && info.slots.back().pointer.empty();
    }

    return info;
  }

  // A parameter nobody assigns or captures is a phi (a slot with no pointer); any other keeps a stack slot. So does a reference, whose slot is
  // in the frame record: a phi is a register the collector cannot see.
  Slot LabelSlot(FunctionEmitter& emitter, const Stmt& statement, size_t i) {
    const String& name = statement.parameterNames[i];

    if (emitter.boxed_.count(name) == 0 && emitter.assigned_.count(name) == 0 && !HoldsRoot(emitter, statement.parameterTypes[i], false)) {
      return Slot{"", statement.parameterTypes[i], false, ""};
    }

    return MakeSlot(emitter, name, statement.parameterTypes[i], statement.name + "." + name);
  }

  void AllocateLabelCells(FunctionEmitter& emitter) {
    for (const auto& label : emitter.labels_) {
      for (const Slot& slot : label.second.slots) {
        AllocateCell(emitter, slot);
      }
    }
  }

  void LabelStatement(FunctionEmitter& emitter, const Stmt& statement) {
    FunctionEmitter::LabelInfo& info = emitter.labels_.at(statement.name);
    FallInto(emitter, info);
    StartBlock(emitter, info.block);

    for (size_t i = 0; i < statement.parameterNames.size(); i++) {
      Declare(emitter, statement.parameterNames[i], OpenParameter(emitter, info, statement, i));
    }

    SafePoint(emitter);  // a label may be a loop's head
  }

  // Running on into a label hands it no values: its phis get `undef` from this block.
  void FallInto(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info) {
    if (emitter.terminated_) {
      return;
    }

    for (size_t i = 0; i < info.slots.size(); i++) {
      HandOne(emitter, info, i, Value{"undef", info.slots[i].type});
    }

    BranchTo(emitter, info.block);
  }

  // The variable a parameter is seen as in the label's block. It is its stack slot, or a new phi whose entries are filled in at the end.
  Slot OpenParameter(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, const Stmt& statement, size_t i) {
    Slot slot = info.slots[i];

    if (!slot.pointer.empty()) {
      return slot;
    }

    slot.value = "%" + UniqueName(emitter, SafeName(statement.parameterNames[i]));
    info.phiLines[i] = emitter.lines_.size();
    Line(emitter, slot.value + " = phi " + LlvmTypeText(slot.type) + " @PHI");

    return slot;
  }

  void FeedPhi(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, size_t i, const Value& value) {
    info.incoming[i].push_back("[ " + value.text + ", %" + emitter.currentBlock_ + " ]");
  }

  // A jump hands one value to a parameter: a phi entry from the block that jumps, or a store into the slot.
  void HandOne(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, size_t i, const Value& value) {
    if (info.slots[i].pointer.empty()) {
      FeedPhi(emitter, info, i, value);
    } else {
      SlotStore(emitter, value, info.slots[i]);
    }
  }

  // The phi placeholders get their entries; the function is complete, so every jump has been seen.
  void PatchPhis(FunctionEmitter& emitter) {
    for (const auto& label : emitter.labels_) {
      PatchLabel(emitter, label.second);
    }
  }

  void PatchLabel(FunctionEmitter& emitter, const FunctionEmitter::LabelInfo& info) {
    for (size_t i = 0; i < info.phiLines.size(); i++) {
      if (info.phiLines[i] == 0) {
        continue;
      }

      String& text = emitter.lines_[info.phiLines[i]];
      text.replace(text.find("@PHI"), 4, joined(info.incoming[i]));
    }
  }

  FunctionEmitter::LabelInfo& LabelNamed(FunctionEmitter& emitter, const String& label) {
    const auto found = emitter.labels_.find(label);

    if (found == emitter.labels_.end()) {
      Fail("no label called '" + label + "' in " + emitter.function_.name);
    }

    return found->second;
  }

  // A jump hands its arguments to the label's parameters (all worked out first, so `goto loop(b, a)` can swap).
  void StoreArguments(FunctionEmitter& emitter, const String& label, const Vector<ExprPtr>& args) {
    FunctionEmitter::LabelInfo& info = LabelNamed(emitter, label);
    const Vector<Value> values = ValuesOf(emitter, args);
    EnsureOpen(emitter);

    for (size_t i = 0; i < values.size() && i < info.slots.size(); i++) {
      HandOne(emitter, info, i, values[i]);
    }
  }

  void JumpToLabel(FunctionEmitter& emitter, const String& label, const Vector<ExprPtr>& args) {
    StoreArguments(emitter, label, args);
    BranchTo(emitter, LabelNamed(emitter, label).block);
  }

  // A block of its own that jumps to the label.
  void EdgeTo(FunctionEmitter& emitter, const String& block, const String& label, const Vector<ExprPtr>& args) {
    StartBlock(emitter, block);
    JumpToLabel(emitter, label, args);
  }

  void GotoStatement(FunctionEmitter& emitter, const Stmt& statement) {
    JumpToLabel(emitter, statement.name, statement.arguments);
  }

  // ── branch and switch ────────────────────────────────────────────────────────────────────────────────────────────────

  namespace {

    Vector<JumpEdge> caseEdges(const Stmt& statement) {
      Vector<JumpEdge> edges;

      for (size_t i = 0; i < statement.caseLabels.size(); i++) {
        edges.push_back(JumpEdge{statement.caseLabels[i], i < statement.caseArguments.size() ? statement.caseArguments[i] : Vector<ExprPtr>{}});
      }

      edges.push_back(JumpEdge{statement.name, statement.arguments});

      return edges;
    }

    Vector<JumpEdge> edgesOf(const Stmt& statement) {
      if (statement.kind == StmtKind::Branch) {
        return {JumpEdge{statement.name, statement.arguments}, JumpEdge{statement.elseName, statement.elseArguments}};
      }

      return caseEdges(statement);
    }

  }  // namespace

  // The edge can name the label's block when every parameter is a phi and the arguments are safe to compute early. Also, no other edge
  // of the same jump may go to that label. A phi has one entry per block, so two edges would need the same value.
  bool EdgeIsDirect(FunctionEmitter& emitter, const JumpEdge& edge, Set<String>& seen) {
    const FunctionEmitter::LabelInfo& info = LabelNamed(emitter, edge.label);
    const bool isNewLabel = seen.insert(edge.label).second;
    const bool noPhiClash = isNewLabel || info.slots.empty();  // a label with no parameters has no phis to clash

    return noPhiClash && info.allPhis && allPure(edge.args);
  }

  bool GoesDirect(FunctionEmitter& emitter, const Vector<JumpEdge>& edges) {
    Set<String> seen;

    for (const JumpEdge& edge : edges) {
      if (!EdgeIsDirect(emitter, edge, seen)) {
        return false;
      }
    }

    return true;
  }

  // Every edge's arguments, worked out before any is handed over.
  Vector<Vector<Value>> ValuesOfEdges(FunctionEmitter& emitter, const Vector<JumpEdge>& edges) {
    Vector<Vector<Value>> values;

    for (const JumpEdge& edge : edges) {
      values.push_back(ValuesOf(emitter, edge.args));
    }

    return values;
  }

  // Works out every edge's arguments, then records them as coming from the block that is about to end.
  void HandOverAll(FunctionEmitter& emitter, const Vector<JumpEdge>& edges) {
    const Vector<Vector<Value>> values = ValuesOfEdges(emitter, edges);
    EnsureOpen(emitter);

    for (size_t k = 0; k < edges.size(); k++) {
      FeedAll(emitter, LabelNamed(emitter, edges[k].label), values[k]);
    }
  }

  void FeedAll(FunctionEmitter& emitter, FunctionEmitter::LabelInfo& info, const Vector<Value>& values) {
    for (size_t i = 0; i < values.size() && i < info.slots.size(); i++) {
      HandOne(emitter, info, i, values[i]);
    }
  }

  void BranchStatement(FunctionEmitter& emitter, const Stmt& statement) {
    const Value condition = EmitExpr(emitter, *statement.value);

    if (GoesDirect(emitter, edgesOf(statement))) {
      DirectBranch(emitter, statement, condition);
    } else {
      TrampolineBranch(emitter, statement, condition);
    }
  }

  void DirectBranch(FunctionEmitter& emitter, const Stmt& statement, const Value& condition) {
    HandOverAll(emitter, edgesOf(statement));
    Terminate(emitter, "br i1 " + condition.text + ", label %" + LabelNamed(emitter, statement.name).block + ", label %" + LabelNamed(emitter, statement.elseName).block);
  }

  void TrampolineBranch(FunctionEmitter& emitter, const Stmt& statement, const Value& condition) {
    const String yes = UniqueName(emitter, "to." + SafeName(statement.name));
    const String no = UniqueName(emitter, "to." + SafeName(statement.elseName));
    Terminate(emitter, "br i1 " + condition.text + ", label %" + yes + ", label %" + no);
    EdgeTo(emitter, yes, statement.name, statement.arguments);
    EdgeTo(emitter, no, statement.elseName, statement.elseArguments);
  }

  void SwitchStatement(FunctionEmitter& emitter, const Stmt& statement) {
    const Value subject = EmitExpr(emitter, *statement.value);

    if (GoesDirect(emitter, edgesOf(statement))) {
      DirectSwitch(emitter, statement, subject);
    } else {
      TrampolineSwitch(emitter, statement, subject);
    }
  }

  void DirectSwitch(FunctionEmitter& emitter, const Stmt& statement, const Value& subject) {
    HandOverAll(emitter, edgesOf(statement));
    Vector<String> targets;

    for (const String& label : statement.caseLabels) {
      targets.push_back(LabelNamed(emitter, label).block);
    }

    const String table = CaseTable(statement, subject, targets);
    Terminate(emitter, "switch " + Typed(subject) + ", label %" + LabelNamed(emitter, statement.name).block + " [" + table + "\n  ]");
  }

  void TrampolineSwitch(FunctionEmitter& emitter, const Stmt& statement, const Value& subject) {
    Vector<String> targets;

    for (size_t i = 0; i < statement.caseValues.size(); i++) {
      targets.push_back(UniqueName(emitter, "case"));
    }

    const String table = CaseTable(statement, subject, targets);
    const String fallback = UniqueName(emitter, "case.default");
    Terminate(emitter, "switch " + Typed(subject) + ", label %" + fallback + " [" + table + "\n  ]");
    CaseBlocks(emitter, statement, targets);
    EdgeTo(emitter, fallback, statement.name, statement.arguments);
  }

  // The cases of a `switch` instruction, each going to its block in `targets`.
  String CaseTable(const Stmt& statement, const Value& subject, const Vector<String>& targets) {
    String table;

    for (size_t i = 0; i < statement.caseValues.size(); i++) {
      table += "\n    " + LlvmTypeText(subject.type) + " " + IntText(statement.caseValues[i], subject.type->bits) + ", label %" + targets[i];
    }

    return table;
  }

  void CaseBlocks(FunctionEmitter& emitter, const Stmt& statement, const Vector<String>& targets) {
    for (size_t i = 0; i < targets.size(); i++) {
      EdgeTo(emitter, targets[i], statement.caseLabels[i], i < statement.caseArguments.size() ? statement.caseArguments[i] : Vector<ExprPtr>{});
    }
  }

}  // namespace polyir::llvm
