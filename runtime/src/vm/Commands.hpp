// The child processes the VM starts for a program: the host layer's `polyirRun`/`polyirShow`/`polyirFetch` (Host.cpp) and the std
// Process natives (ProcessHost.cpp). Every child is one /bin/sh command line, each word quoted so the shell interprets nothing in it; its
// output is captured, streamed to this process's own streams, or left running in the background as a job.
#pragma once

#include "polyir/Std.hpp"

namespace polyir::vm {

  // What a finished child gave: its exit code (-1 when it did not exit normally) and its two output streams.
  struct ChildRun {
    int64_t code = -1;
    String output;
    String errorOutput;
  };

  // A child to start: its words, the folder it runs in ("" keeps the current one) and the "NAME=value" variables added for it alone.
  struct ChildCommand {
    Vector<String> words;
    String directory;
    Vector<String> environment;
  };

  // `cd <directory> && NAME=value … '<word>' …`: the directory and the variables apply to the child only.
  String CommandLine(const ChildCommand& command);

  // Runs a shell line to its end: standard output through a pipe, standard error through a temporary file read back afterwards.
  ChildRun RunCaptured(const String& line);

  // Runs a shell line writing straight to this process's streams; the exit status, 128 + the signal for a child that was killed.
  int64_t RunPassingThrough(const String& line);

  // Starts a shell line in the background and returns at once: the job is a file stem (`<stem>.out`, `<stem>.err`, `<stem>.code`).
  String StartInBackground(const String& line);

  // A job is done once its `.code` file exists (written last).
  bool IsJobDone(const String& job);

  // Waits for the job, collects what it gave and removes its files.
  ChildRun WaitForJob(const String& job);

}  // namespace polyir::vm
