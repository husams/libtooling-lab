Feature: Schema13 expression and source evidence

  Scenario: Query every evidence facade with provenance
    Given a schema13 evidence pair
    When I query expression, field, ancestor, and source evidence
    Then evidence results preserve identity, provenance, and immutability
