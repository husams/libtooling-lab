Feature: Validate catalog identities and paths before writing
  Background:
    Given an imported catalog with two repositories and independent components

  Scenario Outline: Reject duplicate clones and components without changing rows
    When I run the catalog command "<command>"
    Then the catalog command fails with "already registered"
    And the entire catalog is unchanged
    And the catalog database is consistent

    Examples:
      | command                                                                                   |
      | repo add-clone demo {second-clone} --label original                                          |
      | repo add-clone demo {neighbor-root}                                                         |
      | component add --path {external-root} --name core --kind external                             |
      | component add --path {external-root} --name external --kind external                         |
      | component add --path {core-root} --name duplicate --kind external                            |

  Scenario Outline: Reject versions that escape the component root
    When I run the catalog command "component set-version core <version>"
    Then the catalog command fails with "one relative path segment"
    And the entire catalog is unchanged

    Examples:
      | version   |
      | ../escape |
      | /absolute |
      | .         |
      | a/b       |

  Scenario: Reject registration outside an existing repository clone
    When I run the catalog command "component add --path {external-root} --name outside --repo demo --no-git"
    Then the catalog command fails with "outside the repository"
    And the entire catalog is unchanged

  Scenario: Reject an ambiguous component name
    Given two components have the same name
    When I run the catalog command "component rm --name core"
    Then the catalog command fails with "ambiguous component"
    And the entire catalog is unchanged

  Scenario: Reject an ambiguous directory path without choosing an arbitrary component
    Given two components contain the same indexed directory path
    When I run the catalog command "dir rm --path src/deep"
    Then the catalog command fails with "ambiguous directory"
    And the entire catalog is unchanged

  Scenario Outline: Require exactly one valid removal selector
    When I run the catalog command "<command>"
    Then the catalog parser rejects the invalid arguments
    And the entire catalog is unchanged

    Examples:
      | command                                |
      | component rm                           |
      | component rm --id 1 --name core         |
      | component rm --id 0                    |
      | dir rm                                 |
      | dir rm --id {deep-id} --path src/deep   |
      | dir rm --id -1                         |
