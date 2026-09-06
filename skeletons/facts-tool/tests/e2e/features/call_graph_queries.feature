Feature: Opt-in callers and paths across components
  Reverse and path work runs only when explicitly requested.

  Background:
    Given the S-024 multi-component call graph corpus is extracted

  Scenario: forward remains default and callers preserve oriented sites
    Then S-024 forward traversal stays default and callers are explicit

  Scenario: shortest and all-simple paths are deterministic and cycle safe
    Then S-024 paths cross components with deterministic cycle semantics

  Scenario: exact selectors diagnose ambiguity and incompatible options
    Then S-024 selector and option failures are explicit usage errors

  Scenario: path results distinguish complete unknown and truncated evidence
    Then S-024 path result coverage states remain distinct
