Feature: Parallel extraction and dependency wrapper
  Users choose a job limit and retain independent per-source outputs.

  Background:
    Given an imported batch project with shared headers and spaced filenames

  Scenario Outline: Batch each operation with different inputs and job limits
    When I batch "<mode>" using <jobs> jobs and "<inputs>" inputs
    Then every source has valid "<mode>" results in its own database
    Examples:
      | mode       | jobs | inputs   |
      | extract    | 1    | explicit |
      | extract    | 3    | compdb   |
      | dependency | 1    | list     |
      | dependency | 3    | compdb   |

  Scenario: Repeated modes preserve symbols and dependencies
    When I repeat extraction and dependency batching in the same output directory
    Then both symbols and dependencies remain readable after repeated batching

  Scenario: A failed source does not hide successful work
    When one batch source becomes invalid before extraction
    Then batching reports failure and retains successful source results and diagnostics
