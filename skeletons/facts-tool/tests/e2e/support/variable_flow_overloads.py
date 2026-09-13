from __future__ import annotations

import json

from support.scenario import FactsToolContext

SOURCE = """\
namespace overloads {
int bar(bool value, int marker) {
  int result = value ? marker : 0;
  return result;
}
int bar(int value, int marker) {
  int result = value + marker;
  return result;
}
int legacy(int value) {
  int result = value + 5;
  return result;
}
struct Worker {
  int run(int value) const {
    int result = value + 1;
    return result;
  }
  int run(int value) & {
    int result = value + 2;
    return result;
  }
  int operator()(int value) const {
    int result = value + 3;
    return result;
  }
  int operator()(int value) & {
    int result = value + 4;
    return result;
  }
};
}
namespace other {
int bar(bool value, int marker) {
  int result = value ? marker : 0;
  return result;
}
}
"""


def prepare(context: FactsToolContext) -> None:
    """Build and index one generated, headerless overload source per scenario."""
    context.prepare()
    root = context.run_root_path
    source = root / "overloads.cpp"
    source.write_text(SOURCE, encoding="utf-8")
    context.sources = (source,)
    command = {
        "directory": str(root),
        "file": str(source),
        "arguments": [str(context.compiler), "-std=c++23", "-c", str(source)],
    }
    (root / "compile_commands.json").write_text(
        json.dumps([command], indent=2) + "\n", encoding="utf-8"
    )
    context.run_import((source,))
    extracted = context.run(
        [
            str(context.facts_tool),
            "extract",
            "--output",
            str(context.facts_database_path),
            "--conf",
            str(context.files_database_path),
            str(source),
        ]
    )
    assert extracted.returncode == 0, extracted.stdout + extracted.stderr
    context.variable_flow_sources = (source,)
    context.variable_flow_config = root / "variable-flow-overloads.yaml"
    context.variable_flow_config.write_text(
        f"conf_template: '{context.files_database_path}'\n"
        f"facts_template: '{context.facts_database_path}'\n",
        encoding="utf-8",
    )
