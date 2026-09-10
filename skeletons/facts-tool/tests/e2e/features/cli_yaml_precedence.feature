Feature: CLI values take precedence over YAML defaults (B-033)
  Every consumer keeps YAML fallbacks when a value is omitted and uses the
  explicit CLI value for matching options while retaining unrelated defaults.

  Scenario Outline: compiler extras override matching YAML options for every consumer
    Given a CLI/YAML precedence fixture at "<tier>"
    When I run "<family>" with "<mode>" CLI values
    Then only the "<expected>" compiler token is observed for "<family>"
    Examples:
      | tier        | family     | mode       | expected |
      | project     | import     | omitted    | yaml     |
      | project     | extract    | omitted    | yaml     |
      | project     | dependency | omitted    | yaml     |
      | project     | import     | explicit   | cli      |
      | project     | extract    | explicit   | cli      |
      | project     | dependency | explicit   | cli      |
      | project     | extract    | whitespace | yaml     |
      | project     | dependency | whitespace | yaml     |
      | project     | import     | whitespace | yaml     |
      | project     | import     | empty      | error    |
      | project     | extract    | empty      | error    |
      | project     | dependency | empty      | error    |
      | user        | import     | omitted    | yaml     |
      | user        | extract    | omitted    | yaml     |
      | user        | dependency | omitted    | yaml     |
      | user        | import     | explicit   | cli      |
      | user        | extract    | explicit   | cli      |
      | user        | dependency | explicit   | cli      |
      | user        | extract    | whitespace | yaml     |
      | user        | dependency | whitespace | yaml     |
      | user        | import     | whitespace | yaml     |
      | user        | import     | empty      | error    |
      | user        | extract    | empty      | error    |
      | user        | dependency | empty      | error    |
      | config-file | import     | omitted    | yaml     |
      | config-file | extract    | omitted    | yaml     |
      | config-file | dependency | omitted    | yaml     |
      | config-file | import     | explicit   | cli      |
      | config-file | extract    | explicit   | cli      |
      | config-file | dependency | explicit   | cli      |
      | config-file | extract    | whitespace | yaml     |
      | config-file | dependency | whitespace | yaml     |
      | config-file | import     | whitespace | yaml     |
      | config-file | import     | empty      | error    |
      | config-file | extract    | empty      | error    |
      | config-file | dependency | empty      | error    |
      | env         | import     | omitted    | yaml     |
      | env         | extract    | omitted    | yaml     |
      | env         | dependency | omitted    | yaml     |
      | env         | import     | explicit   | cli      |
      | env         | extract    | explicit   | cli      |
      | env         | dependency | explicit   | cli      |
      | env         | extract    | whitespace | yaml     |
      | env         | dependency | whitespace | yaml     |
      | env         | import     | whitespace | yaml     |
      | env         | import     | empty      | error    |
      | env         | extract    | empty      | error    |
      | env         | dependency | empty      | error    |

  Scenario: direct --conf keeps the YAML facts_template fallback for symbols
    Given a direct-conf fixture with a project facts_template
    When I extract and list symbols with only --conf
    Then symbol listing succeeds from the YAML facts_template
