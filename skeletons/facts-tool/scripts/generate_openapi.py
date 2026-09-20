#!/usr/bin/env python3
"""Generate native and Python bindings from the authoritative OpenAPI YAML."""
import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "src/apis/openapi/openapi.yaml")
    parser.add_argument("--output-root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true", help="Fail if generated files differ")
    parser.add_argument("--bundle", type=Path, help="Also export one self-contained YAML file")
    args = parser.parse_args()
    try:
        import yaml
        from openapi_codegen.bundling import bundle
        from openapi_codegen.native import render_native
        from openapi_codegen.output import emit
        from openapi_codegen.python import render_python
        from openapi_codegen.validation import validate
        from openapi_spec_validator.validation.exceptions import OpenAPIValidationError
    except ImportError as error:
        print(f"Install the native test requirements or SDK dev dependencies: {error}", file=sys.stderr)
        return 2
    try:
        document = bundle(args.source)
        validate(document)
        output = render_native(document) | render_python(document)
        if not emit(output, args.output_root.resolve(), args.check):
            return 1
        if args.bundle:
            content = yaml.safe_dump(document, sort_keys=False, allow_unicode=True)
            if args.check:
                if not args.bundle.exists() or args.bundle.read_text(encoding="utf-8") != content:
                    print(f"Bundled YAML is missing or stale: {args.bundle}")
                    return 1
            else:
                args.bundle.parent.mkdir(parents=True, exist_ok=True)
                args.bundle.write_text(content, encoding="utf-8")
        return 0
    except (OSError, ValueError, TypeError, KeyError, yaml.YAMLError, OpenAPIValidationError) as error:
        print(f"OpenAPI generation failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
