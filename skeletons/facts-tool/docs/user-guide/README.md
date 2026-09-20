# facts-tool User Guide

This guide documents every feature of facts-tool: the C++ fact extractor, the
project and configuration catalog, dynamic AST matching, native call-graph
generation, batch processing, and the read-only Python SDK, including the
persisted call-graph reader delivered by feature F-013 (S-029). Every command
and API example was run against the current main branch; anything not yet on
main is labelled as such in the in-flight appendix.

Start with the [table of contents](toc.md). New users should read the
Introduction chapter in order and then follow the Quick start; engineers with a
concrete task should jump straight to the Workflows chapter.

Python plan queries use lazy iteration by default. See
[Lazy queries and performance](05-python-sdk/10-query-performance.md) for
streaming examples, eager compatibility, index coverage, and measurements
on facts-tool's own source code.

Run `facts-tool serve` for the asynchronous REST interface, optional daemon mode,
automatic port allocation and Linux directory monitoring. See
[Running the server](09-rest-api/01-running-the-server.md) for startup and saved
configuration, [Requests and jobs](09-rest-api/02-requests-and-jobs.md) for command
parity, and [Watching directories](09-rest-api/03-watching-directories.md) for
automatic reimport and forced reindexing.
