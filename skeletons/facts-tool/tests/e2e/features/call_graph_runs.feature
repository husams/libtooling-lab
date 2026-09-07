Feature: One native call graph invocation persists its run
  Background:
    Given the S-025 two-component graph fixture

  Scenario: Recovery persists the cross-component graph and provenance
    When S-025 recovers the run for function root
    Then the run status is complete
    And the run edges are root->bridge and bridge->leaf
    And the run row paths match the resolved project and facts databases
    And the run recovered missing definitions with roots [root]

  Scenario: A read-only re-run reuses the graph without extraction
    Given S-025 recovers the run for function root
    When S-025 re-runs without recovery
    Then no new recovery rows were recorded
    And no new symbols or definitions were extracted
    And the project store bytes are unchanged
    And stderr is empty

  Scenario: Recovery failure retains the edges reached before the failure
    Given the S-025 library fails compilation
    When S-025 recovers the run for function root
    Then the run exit code is 1
    And the completion status is recovery-failed
    And stderr has exactly one line
    And the run edges are only root->bridge
    And the library recovery row failed with a diagnostic containing "S025_RECOVERY_FAILURE"

  Scenario: Repeated recovery of an already recovered graph traverses once
    Given S-025 recovers the run for function root
    When S-025 repeats recovery at verbosity 1
    Then stderr has exactly one "facts-tool: graph traversal" line
    And no recovery rows were attempted

  Scenario: Configured pair defaults produce the same recovered run
    When S-025 renders using configured pair defaults
    Then the run status is complete
    And the run edges are root->bridge and bridge->leaf

  Scenario: A one-node budget truncates with a non-empty frontier
    When S-025 runs with a one-node budget
    Then the run status is truncated
    And the truncation reason is max_nodes
    And the frontier is not empty
    And exactly one distinct node was reached

  Scenario Outline: Optional query modes persist their mode
    Given S-025 recovers the run for function root
    When S-025 runs a <mode> query
    Then the run row mode is <mode>
    Examples:
      | mode    |
      | callers |
      | path    |

  Scenario Outline: Budgeted callers and path queries truncate
    Given S-025 recovers the run for function root
    When S-025 runs a budgeted <mode> query
    Then the run status is truncated
    Examples:
      | mode    |
      | callers |
      | path    |

  Scenario Outline: Invalid requests write no run and start no recovery
    When S-025 requests an invalid <kind> call graph run
    Then the invalid request exit code is <code>
    And stderr has exactly one line
    And no recovery-start was printed
    And no run was written

    Examples:
      | kind          | code |
      | root          | 2    |
      | target        | 2    |
      | configuration | 1    |
      | component     | 2    |

  Scenario Outline: The outcome matrix holds without and with verbosity
    When S-025 runs the <outcome> outcome case at verbosity <verbosity>
    Then the outcome matches its documented shape
    And the outcome exit code is <exit>
    And a run row is <presence> for the outcome case

    Examples:
      | outcome                  | verbosity | exit | presence |
      | complete                 | 0         | 0    | present  |
      | complete                 | 1         | 0    | present  |
      | truncated                | 0         | 0    | present  |
      | truncated                | 1         | 0    | present  |
      | help                     | 0         | 0    | absent   |
      | help                     | 1         | 0    | absent   |
      | usage_format             | 0         | 2    | absent   |
      | usage_format             | 1         | 2    | absent   |
      | usage_output             | 0         | 2    | absent   |
      | usage_output             | 1         | 2    | absent   |
      | usage_unknown_option     | 0         | 2    | absent   |
      | usage_unknown_option     | 1         | 2    | absent   |
      | usage_missing_root       | 0         | 2    | absent   |
      | usage_missing_root       | 1         | 2    | absent   |
      | usage_to_with_all        | 0         | 2    | absent   |
      | usage_to_with_all        | 1         | 2    | absent   |
      | usage_invalid_budget     | 0         | 2    | absent   |
      | usage_invalid_budget     | 1         | 2    | absent   |
      | configuration_no_project | 0         | 3    | absent   |
      | configuration_no_project | 1         | 3    | absent   |
      | database_missing_facts   | 0         | 1    | absent   |
      | database_missing_facts   | 1         | 1    | absent   |
      | database_empty_all       | 0         | 1    | absent   |
      | database_empty_all       | 1         | 1    | absent   |
      | recovery_failure         | 0         | 1    | present  |
      | recovery_failure         | 1         | 1    | present  |
      | failed_after_traversal   | 0         | 1    | present  |
      | failed_after_traversal   | 1         | 1    | present  |
      | cancel_before_traversal  | 0         | 130  | absent   |
      | cancel_before_traversal  | 1         | 130  | absent   |
      | cancel_during_recovery   | 0         | 130  | present  |
      | cancel_during_recovery   | 1         | 130  | present  |
      | commit_failure           | 0         | 1    | absent   |
      | commit_failure           | 1         | 1    | absent   |

  Scenario: Two successive runs against the same graph are ordered and independently accurate
    When S-025 recovers the run for function root
    And S-025 runs a project-scoped depth-1 query for root
    Then run B has a greater run_id than run A
    And run A has both edges and status complete
    And run B has only root->bridge and status truncated with reason max_depth

  Scenario: SIGINT during recovery retains the last usable generation
    When S-025 grows the library and interrupts recovery at the validation checkpoint
    Then the interrupted run exits 130 with the cancellation completion line
    And the interrupted run status is cancelled with reason cancelled
    And the interrupted run has a cancelled frontier row and edge root->bridge

  Scenario: SIGINT before traversal cancels before the graph is touched
    When S-025 interrupts a large all-roots traversal before it starts
    Then the interrupt lands with exit 130 and empty stdout
    And the last stderr line is "facts-tool: cancelled before traversal"
    And no call graph run was recorded for the large corpus

  Scenario: A read-only facts store rejects the final commit
    Given S-025 recovers the run for function root
    When S-025 makes the facts store read-only and runs again
    Then the commit failure exits 1 with empty stdout
    And the commit failure stderr is exactly the readonly diagnostic
    And no run or child rows were added by the failed commit

  Scenario Outline: An operational failure after traversal persists a failed run
    Given S-025 recovers the run for function root
    When S-025 breaks the project index and recovers again at verbosity <verbosity>
    Then the failed run exits 1 with a completion line of status failed
    And the failed run stderr carries exactly one non-verbose line naming the SQLite error
    And the failed run row has status failed with the SQLite error and its reached edges
    Examples:
      | verbosity |
      | 0         |
      | 1         |

  Scenario Outline: A forced child-row failure rolls the whole run back
    Given S-025 recovers the run for function root
    When S-025 forces the edge insert to fail and runs again at verbosity <verbosity>
    Then the rolled-back run exits 1 with empty stdout and the forced SQLite diagnostic
    And no run or child rows were added and the earlier run is intact
    Examples:
      | verbosity |
      | 0         |
      | 1         |
