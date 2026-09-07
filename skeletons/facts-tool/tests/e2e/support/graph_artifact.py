"""Native S-025 artifact helpers over the shared cross-component fixture."""
import json
from support.recovery import run


def command(context, format="mermaid", extra=(), recover=False, output=True):
    return ["analyse", "call-graph", "--conf", str(context.files_database_path),
            "--facts", str(context.facts_database_path), "--function", "root",
            "--format", format,
            *(["--output", str(context.graph_artifact)] if output else []),
            *(["--recover-missing"] if recover else []), *map(str, extra)]


def invoke(context, **options):
    context.graph_result = run(context, *command(context, **options))
    return context.graph_result


def metadata(path):
    text = path.read_text()
    assert "\nflowchart TD\n" in text, text
    rows = [line[3:] for line in text.splitlines() if line.startswith("%% {")]
    assert len(rows) == 1, text
    return json.loads(rows[0])


def database_bytes(context):
    return (context.files_database_path.read_bytes(),
            context.facts_database_path.read_bytes())
