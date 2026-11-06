// The child processes the VM starts (Commands.hpp): one /bin/sh line per child, run through popen (captured), system (passed through)
// or a background subshell (a job). The behaviour is the cpp17 Process natives' (ninac/sem/std/process/Process.cpp17.native.nina), so a
// Nina program that runs commands sees the same codes and output on the VM as in its cpp17 build.
#include "vm/Commands.hpp"

#include <chrono>
#include <cstdio>
#include <random>
#include <sys/wait.h>
#include <thread>

namespace polyir::vm {

  namespace {

    using std::fgets;
    using std::random_device;
    using std::filesystem::exists;
    using std::filesystem::remove;
    using std::filesystem::temp_directory_path;
    using std::this_thread::sleep_for;
    using Wait = std::chrono::milliseconds;

    // One word quoted for /bin/sh, so the shell interprets nothing in it.
    String quoted(const String& word) {
      String out = "'";

      for (const char c : word) {
        out += c == '\'' ? String("'\\''") : String(1, c);
      }

      return out + "'";
    }

    // `NAME='value' ` for each `NAME=value` entry (an entry without `=` is skipped).
    String environmentPrefix(const Vector<String>& environment) {
      String out;

      for (const String& entry : environment) {
        const size_t equals = entry.find('=');

        if (equals != String::npos) {
          out += entry.substr(0, equals) + "=" + quoted(entry.substr(equals + 1)) + " ";
        }
      }

      return out;
    }

    String directoryPrefix(const String& directory) {
      return directory.empty() ? "" : "cd " + quoted(directory) + " && ";
    }

    // A fresh path in the temporary folder: `<prefix><random number>`.
    String temporaryStem(const String& prefix) {
      return (temp_directory_path() / (prefix + to_string(random_device{}()))).string();
    }

    String fileText(const String& path) {
      IFStream in{path};
      OStringStream text;
      text << in.rdbuf();

      return text.str();
    }

    void removeFile(const String& path) {
      ErrorCode ignored;
      remove(path, ignored);
    }

    int64_t exitCodeOf(int status) {
      return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    // Everything the pipe gives until the child closes it, then the child's exit code.
    ChildRun drained(FILE* pipe) {
      ChildRun ran;

      if (pipe == nullptr) {
        return ran;
      }

      char chunk[4096];

      while (fgets(chunk, sizeof chunk, pipe) != nullptr) {
        ran.output += chunk;
      }

      ran.code = exitCodeOf(pclose(pipe));

      return ran;
    }

    // The job's line, its streams into the job's files; the exit code goes to `.tmp` and is renamed to `.code`, so `.code` appears whole.
    String jobScript(const String& line, const String& job) {
      const String streams = " >" + quoted(job + ".out") + " 2>" + quoted(job + ".err");
      const String code = "; echo $? >" + quoted(job + ".tmp") + "; mv " + quoted(job + ".tmp") + " " + quoted(job + ".code");

      return "( " + line + " )" + streams + code;
    }

    int64_t jobCode(const String& job) {
      return strtoll(fileText(job + ".code").c_str(), nullptr, 10);
    }

  }  // namespace

  String CommandLine(const ChildCommand& command) {
    String line = directoryPrefix(command.directory) + environmentPrefix(command.environment);

    for (const String& word : command.words) {
      line += quoted(word) + " ";
    }

    return line;
  }

  ChildRun RunCaptured(const String& line) {
    const String errorPath = temporaryStem("polyir-proc-") + ".err";
    ChildRun ran = drained(popen((line + " 2>" + quoted(errorPath)).c_str(), "r"));
    ran.errorOutput = fileText(errorPath);
    removeFile(errorPath);

    return ran;
  }

  int64_t RunPassingThrough(const String& line) {
    const int status = system(line.c_str());

    if (status != -1 && WIFSIGNALED(status)) {
      return 128 + WTERMSIG(status);
    }

    return exitCodeOf(status);
  }

  String StartInBackground(const String& line) {
    const String job = temporaryStem("polyir-job-");
    system(("( " + jobScript(line, job) + " ) </dev/null >/dev/null 2>&1 &").c_str());

    return job;
  }

  bool IsJobDone(const String& job) {
    ErrorCode ignored;

    return exists(job + ".code", ignored);
  }

  ChildRun WaitForJob(const String& job) {
    while (!IsJobDone(job)) {
      sleep_for(Wait(5));
    }

    const ChildRun done{jobCode(job), fileText(job + ".out"), fileText(job + ".err")};

    for (const char* extension : {".out", ".err", ".code"}) {
      removeFile(job + extension);
    }

    return done;
  }

}  // namespace polyir::vm
