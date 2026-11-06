// The std Process natives on the VM (ProcessHost.hpp): each `Process__<action>__host` extern runs its child through Commands.cpp and
// answers what the cpp17 natives answer (Process.cpp17.native.nina): a `Run(code, output, errorOutput)`, an exit status, a `Job(id)`,
// a variable's value. `Process.exit` is not here: the lowering calls `nina_process_exit` for it (NinaRuntime.cpp).
#include "vm/ProcessHost.hpp"
#include "vm/Commands.hpp"

#include <cstdlib>

namespace polyir::vm {

  namespace {

    using std::getenv;
    using std::filesystem::current_path;
    using ProcessFn = Outcome (*)(Machine&, const Vector<Value>&);
    using Table = Map<String, ProcessFn>;

    // ── reading the arguments (ArgumentAt, WordAt, WordsAt: VmValues.hpp) ──

    // `(command, dir, env)`, the parameters runWith, passThrough and start share.
    ChildCommand childAt(Machine& m, const Vector<Value>& args) {
      return ChildCommand{WordsAt(m.heap, args, 0), WordAt(args, 1), WordsAt(m.heap, args, 2)};
    }

    // A Job's `id`, its one field.
    String jobAt(Machine& m, const Vector<Value>& args) {
      HeapObject* job = ObjectOf(m.heap, ArgumentAt(args, 0));

      if (job == nullptr || job->items.empty()) {
        return "";
      }

      return Show(job->items[0]);
    }

    // ── building the answers ──

    // The struct the extern's declared result names (`Std__Run`), so the program's own field reads and `show` see its type.
    String resultName(const Machine& m, const String& externName) {
      for (const ExternInfo& declared : m.program->externs) {
        if (declared.name == externName && declared.result != nullptr) {
          return declared.result->name;
        }
      }

      return "";
    }

    Outcome recordResult(Machine& m, const String& externName, Vector<Value> fields) {
      HeapObject record;
      record.kind = ObjectKind::Struct;
      record.name = resultName(m, externName);
      record.items = move(fields);

      return ValueOutcome(RefValue(Alloc(m.heap, move(record))));
    }

    // `Run(code, output, errorOutput)`, in the order std Process declares the fields.
    Outcome runResult(Machine& m, const String& externName, const ChildRun& ran) {
      return recordResult(m, externName, {I64Value(ran.code), StrValue(m.heap, ran.output), StrValue(m.heap, ran.errorOutput)});
    }

    Outcome textResult(Machine& m, String text) {
      return ValueOutcome(StrValue(m.heap, move(text)));
    }

    // ── the natives ──

    // The line runs in a shell of its own (`/bin/sh -c LINE`), as java21's shell does, so the whole line's error output is captured.
    Outcome shell(Machine& m, const Vector<Value>& args) {
      const ChildCommand line{{"/bin/sh", "-c", WordAt(args, 0)}, "", {}};

      return runResult(m, "Process__shell__host", RunCaptured(CommandLine(line)));
    }

    Outcome run(Machine& m, const Vector<Value>& args) {
      return runResult(m, "Process__run__host", RunCaptured(CommandLine(ChildCommand{WordsAt(m.heap, args, 0), "", {}})));
    }

    Outcome runWith(Machine& m, const Vector<Value>& args) {
      return runResult(m, "Process__runWith__host", RunCaptured(CommandLine(childAt(m, args))));
    }

    // What the program printed so far goes out before the child writes to the same terminal.
    Outcome passThrough(Machine& m, const Vector<Value>& args) {
      m.out->flush();
      cout.flush();

      return ValueOutcome(I64Value(RunPassingThrough(CommandLine(childAt(m, args)))));
    }

    // `Optional<Text>`: present as the VM's `some` (an ok either), absent as `none`.
    Outcome env(Machine& m, const Vector<Value>& args) {
      const char* value = getenv(WordAt(args, 0).c_str());

      if (value == nullptr) {
        return ValueOutcome(NoneValue());
      }

      return ValueOutcome(EitherValue(true, StrValue(m.heap, value)));
    }

    // "" as the value removes the variable; later children inherit the change.
    Outcome setEnv(Machine&, const Vector<Value>& args) {
      const String name = WordAt(args, 0);
      const String value = WordAt(args, 1);

      if (value.empty()) {
        unsetenv(name.c_str());
      } else {
        setenv(name.c_str(), value.c_str(), 1);
      }

      return VoidOutcome();
    }

    Outcome workingDir(Machine& m, const Vector<Value>&) {
      ErrorCode ignored;

      return textResult(m, current_path(ignored).string());
    }

    Outcome start(Machine& m, const Vector<Value>& args) {
      const String job = StartInBackground(CommandLine(childAt(m, args)));

      return recordResult(m, "Process__start__host", {StrValue(m.heap, job)});
    }

    Outcome wait(Machine& m, const Vector<Value>& args) {
      return runResult(m, "Process__wait__host", WaitForJob(jobAt(m, args)));
    }

    Outcome isDone(Machine& m, const Vector<Value>& args) {
      return ValueOutcome(BoolValue(IsJobDone(jobAt(m, args))));
    }

    // ── the table ──

    void addChildProcessRuns(Table& natives) {
      natives["Process__shell__host"] = shell;
      natives["Process__run__host"] = run;
      natives["Process__runWith__host"] = runWith;
      natives["Process__passThrough__host"] = passThrough;
    }

    void addJobsAndEnvironment(Table& natives) {
      natives["Process__start__host"] = start;
      natives["Process__wait__host"] = wait;
      natives["Process__isDone__host"] = isDone;

      natives["Process__env__host"] = env;
      natives["Process__setEnv__host"] = setEnv;
      natives["Process__workingDir__host"] = workingDir;
    }

    Table fullTable() {
      Table natives;
      addChildProcessRuns(natives);
      addJobsAndEnvironment(natives);

      return natives;
    }

    const Table& table() {
      static const Table t = fullTable();

      return t;
    }

  }  // namespace

  bool BindsProcess(const String& name) {
    return table().count(name) != 0;
  }

  Outcome CallProcess(Machine& m, const String& name, const Vector<Value>& args) {
    const auto bound = table().find(name);

    if (bound == table().end()) {
      return Trap("'" + name + "' is not one of the Process natives the VM binds");
    }

    return bound->second(m, args);
  }

}  // namespace polyir::vm
