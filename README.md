# Concurrency bug linter

The goal of this program is to highlight variables that are shared by multiple threads. The use of those variables could potentially be the root cause of a concurrency bug.
Note that this tool does **not** point out bugs. You will have to manually triage warnings.

This program relies on [sparse](https://sparse.docs.kernel.org/en/latest/) and [cJSON](https://github.com/DaveGamble/cJSON).

Before your first build, run `git submodule init && git submodule update`, then type `make` to build this program.

Before running this program, you will need two things:

- provide a list of threads in a JSON file
- provide a [compilation database](https://clang.llvm.org/docs/JSONCompilationDatabase.html) file

  So let's assume the program to be analyzed contains two threads: `threadA` and `threadB`, then the thread configuration file can look like:

```json
[
    {"name":"threadA", "file":"absolute-path-to-my-file", "line":23},
    {"name":"threadB", "file":"absolute-path-to-my-file", "line":45}
]
```

To run this program:

```sh
./concurrency-bug-linter <thread-config-file> <compile-commands>
```

