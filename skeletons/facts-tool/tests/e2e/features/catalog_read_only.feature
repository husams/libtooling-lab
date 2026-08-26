Feature: Inspect and preview without requiring a writable catalog
  Background:
    Given an imported catalog with two repositories and independent components
    And the catalog is read-only

  Scenario Outline: Queries and deletion previews preserve the database byte for byte
    When I run the catalog command "<command>"
    Then the catalog command succeeds
    And the entire catalog is unchanged
    And the catalog database bytes are unchanged

    Examples:
      | command                                       |
      | repo ls                                       |
      | repo show demo                                |
      | repo rm demo --dry-run                         |
      | repo rm demo --delete-components --dry-run     |
      | component ls                                  |
      | component compile-commands core               |
      | component rm --name core --dry-run            |
      | dir ls                                        |
      | dir rm --id {deep-id} --dry-run                |
