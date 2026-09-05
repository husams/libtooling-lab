Feature: Shared configuration read and write policies
  Scenario Outline: Read consumer <family> requires the resolved database
    Given defaults exist at tiers "project"
    When I invoke the "<family>" configuration consumer
    Then the generated database is required without creating files
    Examples:
      | family |
      | extract |
      | dependency |
      | repo |
      | component |
      | dir |
      | file |

  Scenario Outline: YAML and CLI tokens reach <family> under <override>
    Given a compile fixture using "<override>" override and "<input>" commands
    When I run "<family>" with ordered YAML and CLI tokens twice
    Then compiler effects and stored base arguments prove ordering without accumulation
    Examples:
      | family | override | input |
      | import | cli | json |
      | import | cli | fixed |
      | import | env | json |
      | import | env | fixed |
      | import | generated | json |
      | import | generated | fixed |
      | extract | cli | json |
      | extract | cli | fixed |
      | extract | env | json |
      | extract | env | fixed |
      | extract | generated | json |
      | extract | generated | fixed |
      | dependency | cli | json |
      | dependency | cli | fixed |
      | dependency | env | json |
      | dependency | env | fixed |
      | dependency | generated | json |
      | dependency | generated | fixed |

  Scenario: Changed defaults work without reimport
    Given a compile fixture using "cli" override and "json" commands
    When I change YAML defaults between extractions
    Then changed defaults apply once without changing stored arguments

  Scenario Outline: Facts-only and help are independent of defaults
    Given a selected YAML file with "malformed"
    When I run configuration-independent "<command>"
    Then no configuration discovery occurs
    Examples:
      | command |
      | help |
      | symbol |
