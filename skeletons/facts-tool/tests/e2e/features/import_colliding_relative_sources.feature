Feature: Import tolerates identical relative source paths across components
  A compilation database may list the same relative file under different
  directories. Each command must be preprocessed with its own file identity:
  a translation unit or header that shares a relative spelling with another
  component must never inherit that component's file metadata.

  Background:
    Given component "component-a" defines "src/generated/Same.cpp" of about 80 KiB
    And component "component-b" defines "src/generated/Same.cpp" of about 400 bytes
    And each component has its own "include/common.h" with different content, included by its Same.cpp via -Iinclude
    And a compilation database lists both sources as "src/generated/Same.cpp" relative to their component directory

  Scenario: Regression - large-then-small order imports cleanly
    When the real facts-tool imports "component-a/src/generated/Same.cpp" then "component-b/src/generated/Same.cpp"
    Then import succeeds
    And stderr contains no "null character ignored" diagnostic
    And the file registry contains both absolute source identities

  Scenario: Control - small-then-large order imports cleanly
    When the real facts-tool imports "component-b/src/generated/Same.cpp" then "component-a/src/generated/Same.cpp"
    Then import succeeds
    And stderr contains no "null character ignored" diagnostic
    And the file registry contains both absolute source identities

  Scenario Outline: Header collision - both include/common.h identities are retained in either order
    When the real facts-tool imports "<first>" then "<second>"
    Then import succeeds
    And stderr contains no "null character ignored" diagnostic
    And the file registry contains "component-a/include/common.h" and "component-b/include/common.h"

    Examples:
      | first                              | second                             |
      | component-a/src/generated/Same.cpp | component-b/src/generated/Same.cpp |
      | component-b/src/generated/Same.cpp | component-a/src/generated/Same.cpp |

  Scenario: Repeated import is idempotent
    When the real facts-tool imports the whole compilation database twice
    Then import succeeds
    And the file registry contains each source and header identity exactly once

  Scenario: A failing translation unit reports a controlled error
    Given "component-b/src/generated/Same.cpp" includes a header that does not exist
    When the real facts-tool imports the whole compilation database
    Then import fails with a nonzero exit and the message names the absolute source path
    And the file registry is not marked complete
