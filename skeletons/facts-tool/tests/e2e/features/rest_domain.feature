Feature: Repository-aware asynchronous REST resources
  Clients describe source files and symbols without selecting databases or CLI options.

  Background:
    Given two real repositories with separate extracted fact databases

  Scenario: Queued native operations cancel without interrupting running transactions
    Given the repository-aware server is running
    When I cancel queued domain work while another operation is waiting for the database
    Then only the queued domain operation is cancelled and the running operation completes

  Scenario: A clone identifier from symbol lookup can be reused in an operation
    Given an indexed repository whose initial clone has no label
    And the repository-aware server is running
    When I extract using the clone identifier returned by symbol lookup
    Then the returned clone identity resolves to the original source file

  Scenario: First server startup builds a global symbol index across repositories
    When the repository-aware server starts for the first time
    Then fully qualified symbol searches find both repositories and defining files
    And symbol kind, USR, repository, and component filters are exact
    And the global symbol index is stored in the project database

  Scenario Outline: Extraction resolves a file identity and refreshes global search
    Given the repository-aware server is running
    When I request typed extraction using the "<selector>" file identity
    Then the extraction job returns structured results without command output
    And global search contains the new symbol and removes the replaced symbol

    Examples:
      | selector   |
      | absolute   |
      | repository |
      | component  |
      | clone      |

  Scenario: Matcher requests return named bindings and refresh global search
    Given the repository-aware server is running
    When I submit a Clang DSL query and source identity to the match API
    Then the match job returns structured bindings and the matched symbol is searchable

  Scenario: Dependency analysis returns file relationships
    Given the repository-aware server is running
    When I request dependency analysis for a repository-relative file
    Then the dependency job returns structured source and header relationships

  Scenario: Relative file ambiguity produces an explicit typed error
    Given the repository-aware server is running
    When I request extraction for a relative file shared by both repositories
    Then the job fails with a typed ambiguity error and the server remains healthy

  Scenario: A registered inactive clone can be selected without switching repositories
    Given alpha has a registered inactive clone with different source content
    And the repository-aware server is running
    When I request extraction from the explicitly selected inactive clone
    Then global search reports the inactive clone definition and the active clone is unchanged

  Scenario: First indexing stays asynchronous under a database lock
    When I start the repository-aware server while the catalog is locked
    Then health and index status respond before the lock is released
    And global indexing finishes after the lock is released

  Scenario: Extraction acceptance stays asynchronous under a database lock
    Given the repository-aware server is running
    When I submit typed extraction while the catalog is locked
    Then the extraction is accepted and health responds before the lock is released
    And the accepted extraction and global indexing finish after the lock is released

  Scenario Outline: Partial clone analysis preserves the need for full extraction
    Given alpha has a registered inactive clone with different source content
    And the repository-aware server is running
    When I request partial "<operation>" analysis from the inactive clone
    Then the inactive clone remains due for full extraction
    When I request extraction from the explicitly selected inactive clone
    Then global search includes symbols outside the partial analysis
    And global search reports the inactive clone definition and the active clone is unchanged

    Examples:
      | operation    |
      | matches      |
      | dependencies |
