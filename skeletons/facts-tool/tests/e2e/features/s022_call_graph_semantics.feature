Feature: Cross-component callable semantics
  Semantic call graphs preserve stored primitives and observable coverage gaps.

  Background:
    Given the S-022 multi-component corpus is extracted

  Scenario: represents constructor and cleanup invocations at their source sites
    Then explicit and implicit construction and cleanup edges are semantic

  Scenario: retains lambda ownership and cross-component target identity
    Then the lambda edge is unique and cross-component targets resolve

  Scenario: distinguishes exact and possible virtual dispatch
    Then static targets and dispatch expansions retain receiver certainty

  Scenario: exposes raw relations and unsupported frontend semantics
    Then the Calls view and unsupported-semantic evidence remain inspectable
