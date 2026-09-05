Feature: Configuration YAML validation before mutation
  Scenario Outline: Reject <case>
    Given a selected YAML file with "<case>"
    When I attempt configuration inspection
    Then configuration fails with "configuration error:"
    Examples:
      | case |
      | malformed |
      | duplicate |
      | unknown |
      | null-root |
      | empty-root |
      | number-root |
      | sequence-root |
      | null-template |
      | empty-template |
      | mapping-template |
      | null-args |
      | scalar-args |
      | mapping-args |
      | number-arg |
      | boolean-arg |
      | null-arg |
      | mapping-arg |
      | sequence-arg |
      | nul-arg |
      | newline-arg |
      | tag |
      | anchor |
      | alias |
      | documents |
      | not-map |
      | nul-root |
      | newline-root |

  Scenario Outline: Compiler consumers validate YAML despite <kind> override
    Given a selected YAML file with "<case>"
    And a "<kind>" direct database override
    When I invoke the "<family>" configuration consumer
    Then configuration fails with "configuration error:"
    Examples:
      | family | kind | case |
      | import | cli | malformed |
      | import | cli | number-arg |
      | import | cli | null-args |
      | import | env | malformed |
      | import | env | number-arg |
      | import | env | null-args |
      | extract | cli | malformed |
      | extract | cli | number-arg |
      | extract | cli | null-args |
      | extract | env | malformed |
      | extract | env | number-arg |
      | extract | env | null-args |
      | dependency | cli | malformed |
      | dependency | cli | number-arg |
      | dependency | cli | null-args |
      | dependency | env | malformed |
      | dependency | env | number-arg |
      | dependency | env | null-args |

  Scenario Outline: Catalog <family> bypasses malformed YAML with <kind> override
    Given a selected YAML file with "malformed"
    And a "<kind>" direct database override
    When I invoke the "<family>" configuration consumer
    Then the direct database is required without reading YAML
    Examples:
      | family | kind |
      | repo | cli |
      | repo | env |
      | component | cli |
      | component | env |
      | dir | cli |
      | dir | env |
      | file | cli |
      | file | env |
      | symbol | cli |
      | symbol | env |
