Feature: The async Python client keeps the event loop responsive
  Background:
    Given a native facts-tool REST server
    And an authenticated async SDK client
    And a C++ project with an answer function
    And the project was imported through the SDK
    And another connection holds the project database lock
    And an import job is running and waiting for the database lock

  Scenario: Concurrent async requests progress while an import is blocked
    When I poll the import alongside concurrent submissions and health requests
    Then health responds during polling and all independent jobs complete

  Scenario: Cancelling an asyncio task does not cancel the remote process
    When I cancel the asyncio task polling the blocked import
    Then only local polling stops and the remote import can still finish
