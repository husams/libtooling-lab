Feature: CLI values take precedence over YAML defaults (B-033)
  Every consumer keeps YAML fallbacks when a value is omitted and uses the
  complete explicit CLI value when it is supplied.

  Scenario Outline: compiler extras replace YAML for every consumer
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
      | project     | extract    | whitespace | none     |
      | project     | dependency | whitespace | none     |
      | project     | import     | whitespace | none     |
      | project     | import     | empty      | error    |
      | project     | extract    | empty      | error    |
      | project     | dependency | empty      | error    |
      | user        | import     | omitted    | yaml     |
      | user        | extract    | omitted    | yaml     |
      | user        | dependency | omitted    | yaml     |
      | user        | import     | explicit   | cli      |
      | user        | extract    | explicit   | cli      |
      | user        | dependency | explicit   | cli      |
      | user        | extract    | whitespace | none     |
      | user        | dependency | whitespace | none     |
      | user        | import     | whitespace | none     |
      | user        | import     | empty      | error    |
      | user        | extract    | empty      | error    |
      | user        | dependency | empty      | error    |
      | config-file | import     | omitted    | yaml     |
      | config-file | extract    | omitted    | yaml     |
      | config-file | dependency | omitted    | yaml     |
      | config-file | import     | explicit   | cli      |
      | config-file | extract    | explicit   | cli      |
      | config-file | dependency | explicit   | cli      |
      | config-file | extract    | whitespace | none     |
      | config-file | dependency | whitespace | none     |
      | config-file | import     | whitespace | none     |
      | config-file | import     | empty      | error    |
      | config-file | extract    | empty      | error    |
      | config-file | dependency | empty      | error    |
      | env         | import     | omitted    | yaml     |
      | env         | extract    | omitted    | yaml     |
      | env         | dependency | omitted    | yaml     |
      | env         | import     | explicit   | cli      |
      | env         | extract    | explicit   | cli      |
      | env         | dependency | explicit   | cli      |
      | env         | extract    | whitespace | none     |
      | env         | dependency | whitespace | none     |
      | env         | import     | whitespace | none     |
      | env         | import     | empty      | error    |
      | env         | extract    | empty      | error    |
      | env         | dependency | empty      | error    |

  Scenario: direct --conf keeps the YAML facts_template fallback for symbols
    Given a direct-conf fixture with a project facts_template
    When I extract and list symbols with only --conf
    Then symbol listing succeeds from the YAML facts_template
