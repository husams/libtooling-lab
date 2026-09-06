@s021
Feature: Recovery reuses shipped evidence and respects candidate boundaries
  Background:
    Given an S-021 app calls an unextracted registered library

  Scenario: Repeated recovery does not alternate extraction between valid TUs
    When S-021 missing recovery is requested
    Then repeated S-021 invocations reuse production facts without extraction

  Scenario: Unavailable system functions do not become project recovery requests
    Given S-021 root also calls an unavailable standard library function
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command
    And S-021 never probes the external unavailable target
    And repeated S-021 invocations reuse production facts without extraction

  Scenario: An unrelated missing file does not poison the selected library inputs
    Given the S-021 library has only symbol-match evidence
    And the unrelated S-021 registered alternative is missing
    When S-021 missing recovery is requested
    Then S-021 recovers the library body with its stored command

  Scenario: Reused header evidence names a real owning TU and batches USRs
    Given S-021 bridge is defined in a header included by two registered TUs
    When S-021 missing recovery is requested
    Then repeated S-021 invocations reuse production facts without extraction
    And S-021 reused entries batch symbols under actual translation units

  Scenario: Explicit depth stops recovery before a deeper missing callee
    Given S-021 library calls a deeper missing function
    When S-021 recovery is limited to one edge
    Then S-021 does not attempt the deeper missing function

  Scenario: Unknown freshness cannot hide a changed body behind an existing entry
    Given the S-021 previously extracted library gains another call
    When S-021 missing recovery is requested
    Then S-021 refreshes the changed library call evidence
