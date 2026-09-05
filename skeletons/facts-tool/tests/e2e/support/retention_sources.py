"""Preprocessor probes shared by all three real compiler consumers."""


def write_sources(d):
    def put(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    put(d.cwd / "include/json a/json_a.hpp", "struct JsonAHeader {};\n")
    put(d.cwd / "include/json a/json words", "struct JsonSpace {};\n")
    put(d.cwd / "sub/base/json_b.hpp", "#define JSON_B_FORCED 1\n")
    put(d.root / "outside/json_inc/json_only.hpp", '#include "../../observed/both_a.hpp"\n')
    # Include roots are recursively enumerated too: probes must live outside
    # BOTH the component and every -I directory to prove preprocessing.
    for name in ("both_a", "both_b", "cli_a", "cli_b"):
        put(d.root / ("observed/" + name + ".hpp"), "#pragma once\n")
    for version in ("YAML", "NEW"):
        put(d.cwd / (version + " marker.hpp"), "#pragma once\n")
        put(d.cwd / (version + " header.hpp"),
            f"#ifdef {version}_SEEN\n#error repeated YAML forced header\n#endif\n"
            f"#define {version}_SEEN 1\n"
            '#define RET_STR_(x) #x\n#define RET_STR(x) RET_STR_(x)\n'
            '#include RET_STR(YAML_SPACE)\n'
            f'#include "{version} marker.hpp"\n')
    put(d.cwd / "yaml words", "#pragma once\n")
    for unit in ("a", "b"):
        upper = unit.upper()
        other = "B" if unit == "a" else "A"
        prefix = '' if unit == "a" else '../'
        base = ('#include "json_a.hpp"\n#define JSON_STR_(x) #x\n'
                '#define JSON_STR(x) JSON_STR_(x)\n#include JSON_STR(JSON_SPACE)\n'
                if unit == "a" else
                '#if !JSON_B_FORCED\n#error lost JSON forced header\n#endif\n')
        probe = '"json_only.hpp"' if unit == "a" else '"../../observed/both_b.hpp"'
        text = base + f'#if JSON_{upper} != 1 || defined(JSON_{other})\n'
        text += '#error lost or leaked JSON macro\n#endif\n'
        standard = '202302L' if unit == "a" else '202002L'
        text += f'#if __cplusplus != {standard}\n#error lost JSON language standard\n#endif\n'
        text += f'struct JsonOnly{upper} {{}};\n'
        text += '#ifdef YAML_ONLY\n#if !YAML_SEEN\n#error lost YAML header\n#endif\n'
        text += f'#include {probe}\nstruct Both{upper} {{}};\nstruct YamlOnly{upper} {{}};\n#endif\n'
        text += '#ifdef NEW_ONLY\n#if !NEW_SEEN\n#error lost new YAML header\n#endif\n'
        text += f'#include {probe}\nstruct NewBoth{upper} {{}};\n#endif\n'
        text += '#ifdef CLI_ONLY\n'
        text += '#define CLI_STR_(x) #x\n#define CLI_STR(x) CLI_STR_(x)\n'
        text += '#include CLI_STR(CLI_SPACE)\n'
        text += f'#include "{prefix}../observed/cli_{unit}.hpp"\n'
        text += 'static_assert(sizeof(CLI_STR(CLI_SPACE)) == sizeof("two words"));\n'
        text += f'struct Cli{upper} {{}};\n#endif\n'
        text += '#ifdef RUNTIME_ONLY\n'
        text += '#define RUN_STR_(x) #x\n#define RUN_STR(x) RUN_STR_(x)\n'
        text += '#include RUN_STR(RUNTIME_SPACE)\n'
        text += f'struct Runtime{upper} {{}};\n#endif\n'
        text += '#if VALUE == 2\nstruct ValueTwo' + upper + ' {};\n#endif\n'
        put(d.cwd / ("unit_a.cpp" if unit == "a" else "sub/unit_b.cpp"), text)
    put(d.cwd / "two words", "#pragma once\n")
    put(d.cwd / "sub/two words", "#pragma once\n")
    for directory in (d.cwd, d.cwd / "sub"):
        put(directory / "runtime words.hpp", "#pragma once\n")
