Feature: Legacy evidence capability

  Scenario: Legacy schema rejects schema13 evidence
    Given a legacy evidence pair
    When I ask the legacy pair for expressions
    Then the legacy capability is rejected
