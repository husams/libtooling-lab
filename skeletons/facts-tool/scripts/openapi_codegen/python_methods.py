"""Operation semantics for the stable SDK signatures and typed return values."""

from .python_domain import OPERATIONS
from .python_routes import operations

METHODS = {
    "health": ("health(self) -> dict[str, object]", "return value"),
    "openapi": ("openapi(self) -> dict[str, object]", "return value"),
    "openapiYaml": ("openapi_yaml(self) -> str", "return value"),
    "commands": ("commands(self) -> list[dict[str, object]]", "return command_catalog(value)"),
    "listJobs": ("list_jobs(self) -> list[Job | DomainJob]",
                 'return [snapshot(item, metadata=True) for item in object_list(value, "jobs")]'),
    "submit": ("submit(self, *argv: str) -> Job", "return job(value)"),
    "getJob": ("get_job(self, job_id: str) -> Job | DomainJob", "return snapshot(value)"),
    "cancelJob": ("cancel_job(self, job_id: str) -> Job | DomainJob", "return snapshot(value)"),
    "watchStatus": ("watch_status(self) -> dict[str, object]", "return value"),
    "shutdown": ("shutdown(self) -> dict[str, object]", "return value"),
    "command": ("command(self, path: str, *argv: str) -> Job", "return job(value)"),
}


def method(name: str, asynchronous: bool) -> str:
    signature, result = METHODS[name]
    qualifier = "async " if asynchronous else ""
    request = "await async_request" if asynchronous else "request"
    if name == "openapiYaml":
        request += "_text"
    params = ", id=identifier(job_id)" if name in {"getJob", "cancelJob"} else ""
    prelude, body = [], ""
    if name == "submit":
        body = ", body=arguments(argv)"
    if name == "command":
        prelude = [
            "path = command_path(path)",
            'body = arguments(argv, prefix_count=len(path.split("/")))',
        ]
        params, body = ", commandPath=path", ", body=body"
    expression = f'{request}(self._http, *endpoint("{name}"{params}){body})'
    statement = result.replace("value", expression)
    if len(statement) + 8 > 88:
        prelude.append(f'route = endpoint("{name}"{params})')
        expression = f"{request}(self._http, *route{body})"
        statement = result.replace("value", expression)
    if len(statement) + 8 > 88:
        prelude.append(f"value = {expression}")
        statement = result
    return "\n".join([
        f"    {qualifier}def {signature}:",
        *("        " + line for line in (*prelude, statement)),
    ])


def methods(spec: dict, *, asynchronous: bool) -> str:
    declared = {name: route for name, route in operations(spec).items()
                if not route[1].startswith("api/v2/")}
    if set(declared) != set(METHODS) | OPERATIONS:
        raise ValueError(f"Unsupported Python operations: {set(declared) ^ (set(METHODS) | OPERATIONS)}")
    return "\n\n".join(method(name, asynchronous) for name in METHODS)
