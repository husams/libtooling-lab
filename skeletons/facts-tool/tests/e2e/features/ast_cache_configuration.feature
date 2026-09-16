@ast_cache
Feature: Configure AST persistence independently of the project database path
  User defaults, project defaults, and explicit YAML configure the AST cache even
  when a command selects its project database directly with --conf.

  Background:
    Given an isolated AST cache project

  Scenario Outline: AST cache paths follow YAML precedence with direct --conf
    Given AST cache configuration is selected from "<tier>"
    When the AST cache project runs "extract"
    Then only the "<tier>" configuration cache is populated

    Examples:
      | tier     |
      | user     |
      | project  |
      | explicit |

  Scenario: Explicit configuration can disable a project enabled cache
    Given explicit configuration disables a project enabled cache
    When the AST cache project runs "extract"
    Then no AST cache directory is created

  Scenario: A relative cache path is resolved against the project root
    Given the configured AST cache directory is relative
    When the AST cache project runs "extract"
    Then the relative AST cache directory is populated under the project

  Scenario: Configuration inspection reports caching without creating directories
    Given AST caching is enabled
    When the AST cache project runs "show"
    Then configuration inspection reports the AST settings without creating the cache
