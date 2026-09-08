Feature: Schema13 source outcomes

  Scenario: Bounded source and unavailable ranges retain distinctions
    Given a schema13 outcome pair
    When I query bounded and unavailable source outcomes
    Then source paging and availability outcomes are explicit
