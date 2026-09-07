"""A large synthetic call-graph corpus (B-042).

Thousands of roots make the `-v 2` per-root lines exceed the stderr pipe, so a
reader that stops after `roots selected` parks the tool before the
pre-traversal checkpoint and SIGINT lands there deterministically.
"""
import subprocess


def build(context, count=20000):
    root = context.run_root_path / "matrix-large"
    files_db = root / "files.sqlite"
    facts_db = root / "facts.sqlite"
    if root.exists():
        return files_db, facts_db
    root.mkdir()
    source = root / "large.cpp"
    functions = [f"int f{count}(){{return 0;}}"]
    functions.extend(f"int f{i}(){{return f{i + 1}();}}"
                     for i in range(count - 1, -1, -1))
    source.write_text("namespace matrix_large {\n" + "\n".join(functions) +
                      "\nint root(){return f0();}\n}\n")
    tool = str(context.facts_tool)
    imported = subprocess.run([tool, "import", "-v", "0", "-c", str(files_db),
                               "--extra-arg=-std=c++23", str(source)],
                              capture_output=True, text=True)
    assert imported.returncode == 0, imported.stderr
    extracted = subprocess.run([tool, "extract", "-v", "0", "-o", str(facts_db),
                                "-c", str(files_db), str(source)],
                               capture_output=True, text=True)
    assert extracted.returncode == 0, extracted.stderr
    return files_db, facts_db
