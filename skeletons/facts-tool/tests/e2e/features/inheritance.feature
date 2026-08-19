Feature: C++ record inheritance
  Direct bases are stored with their order, access, and virtual inheritance state.
  Integer boolean values are shown exactly as persisted by SQLite: 0 or 1.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario Outline: Store direct inheritance from <source> to <destination>
    Then the persisted direct inheritance fields include
      | source   | destination   | kind   | position   | access   | is_virtual_base   | is_implicit   | is_lexical   | count   |
      | <source> | <destination> | <kind> | <position> | <access> | <is_virtual_base> | <is_implicit> | <is_lexical> | <count> |
    And exactly 6 direct inheritance relations are stored

    Examples:
      | source               | destination | kind | position | access    | is_virtual_base | is_implicit | is_lexical | count |
      | e2e::CompositeWidget | e2e::Widget | 2    | 0        | public    | 0               | 0           | 0          | 1     |
      | e2e::CompositeWidget | e2e::Policy | 2    | 1        | protected | 1               | 0           | 0          | 1     |
      | e2e::PrivateWidget   | e2e::Widget | 2    | 0        | private   | 0               | 0           | 0          | 1     |
      | e2e::PublicWidget    | e2e::Widget | 2    | 0        | public    | 0               | 0           | 0          | 1     |

  Scenario: Preserve a lightweight target for a filtered system-header base
    Then the external inheritance target is queryable without its header body

  Scenario: Skip an unresolved dependent base without failing indexing
    When the real facts-tool indexes a dependent-base template
    Then the dependent-base indexing command succeeds without incomplete diagnostics
    And the concrete dependent-base instance keeps its inheritance relation

  Scenario: Report a genuine post-save relation failure as incomplete indexing
    When relation persistence is forced to fail on a rerun
    Then the indexing command exits unsuccessfully
    And the relation failure diagnostic identifies both symbols and the base USR

  Scenario: Report a non-inheritance relation failure with relation semantics
    When field relation persistence is forced to fail on a rerun
    Then the indexing command exits unsuccessfully
    And the field relation failure diagnostic identifies its source and target

  Scenario: Attribute a multi-base persistence failure to the failing base
    When the second inheritance relation is forced to fail on a rerun
    Then the indexing command exits unsuccessfully
    And the inheritance diagnostic identifies the second base
