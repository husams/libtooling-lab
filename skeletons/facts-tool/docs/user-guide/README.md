# facts-tool User Guide

This guide documents every feature of facts-tool: the C++ fact extractor, the
project and configuration catalog, dynamic AST matching, native call-graph
generation, batch processing, the read-only Python database SDK, and the
asynchronous REST server with synchronous and asynchronous Python clients.
The SDK also includes the persisted call-graph reader delivered by feature
F-013 (S-029).

Start with the [table of contents](toc.md). New users should read the
Introduction chapter in order and then follow the Quick start; engineers with a
concrete task should jump straight to the Workflows chapter.

Python plan queries use lazy iteration by default. See
[Lazy queries and performance](05-python-sdk/10-query-performance.md) for
streaming examples, eager compatibility, index coverage, and measurements
on facts-tool's own source code.

Run `facts-tool serve` for the asynchronous REST interface, optional daemon mode,
automatic port allocation and Linux repository monitoring. See
[Running the server](09-rest-api/01-running-the-server.md) for startup and saved
configuration, [Requests and jobs](09-rest-api/02-requests-and-jobs.md) for command
parity, and [Repository monitoring](09-rest-api/03-watching-directories.md) for
automatic reimport and forced reindexing.

For a new server or client machine, follow [REST installation](09-rest-api/05-installation.md)
and [Deployment and operation](09-rest-api/06-deployment.md). Python applications
and agents can use the [REST client](05-python-sdk/11-rest-client.md) to submit
CLI commands, await results, cancel jobs, and inspect the server.
