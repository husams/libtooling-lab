Feature: Register components before importing any compilation commands
  Background:
    Given a new catalog path and a Git checkout with a nested component

  Scenario Outline: Register an initial component with explicit Git discovery behavior
    When I register the nested component as "<kind>" with "<options>"
    Then registration creates a consistent catalog with root "<root>" and kind "<kind>"
    And no source files or extracted facts have been created

    Examples:
      | kind     | options  | root   |
      | repo     |          | .      |
      | repo     | --no-git | nested |
      | external |          | nested |

  Scenario Outline: Missing configurations are reported without creating a database
    When I query the missing catalog with "<command>"
    Then the command fails without creating the missing configuration

    Examples:
      | command                    |
      | repo list                  |
      | component show absent      |
      | dir list                   |
      | component rm --name absent |
