Feature: Schema13 checkout provenance

  Scenario: Same-content alternate checkout is unavailable
    Given a schema13 checkout pair
    When I activate an identical alternate checkout
    Then source evidence reports checkout unavailability
