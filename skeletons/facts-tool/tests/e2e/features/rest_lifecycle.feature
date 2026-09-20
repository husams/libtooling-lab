Feature: Native REST server and daemon deployment
  Persisted configuration discovers a running service across foreground and daemon starts.

  Scenario: Automatically selected ports are saved and reused on restart
    Given a running authenticated native REST server
    Then the server configuration stores its automatically allocated listening port
    When I restart the server using only its saved configuration
    Then the restarted server reuses the saved port

  Scenario: An occupied saved port is replaced and persisted automatically
    Given a running authenticated native REST server
    Then the server configuration stores its automatically allocated listening port
    When I restart while another listener occupies the saved port
    Then the server saves a different available port and accepts requests

  Scenario: Foreground servers shut down gracefully through HTTP
    Given a running authenticated native REST server
    When I request graceful server shutdown through REST
    Then the server releases its PID lock and listening socket

  Scenario: Daemon startup waits for readiness and excludes duplicate instances
    Given a running authenticated native REST daemon
    Then the daemon launcher has succeeded and its child is ready with a PID
    When another server tries to use the same server configuration
    Then the duplicate instance fails while the original continues serving requests
    When I request graceful server shutdown through REST
    Then the server releases its PID lock and listening socket
