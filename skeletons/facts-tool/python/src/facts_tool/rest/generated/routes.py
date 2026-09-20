"""Generated from src/apis/openapi/openapi.yaml; do not edit."""

ROUTES: dict[str, tuple[str, str]] = {
    "cancelJob": ("DELETE", "v1/jobs/{id}"),
    "command": ("POST", "v1/commands/{commandPath}"),
    "commands": ("GET", "v1/commands"),
    "getJob": ("GET", "v1/jobs/{id}"),
    "health": ("GET", "health"),
    "listJobs": ("GET", "v1/jobs"),
    "openapi": ("GET", "openapi.json"),
    "openapiYaml": ("GET", "openapi.yaml"),
    "shutdown": ("POST", "v1/shutdown"),
    "submit": ("POST", "v1/jobs"),
    "watchStatus": ("GET", "v1/watch"),
}

MAX_ARGUMENTS = 4096
MAX_ARGUMENT_BYTES = 65536
