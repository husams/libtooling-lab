Feature: Function-body reference facts
  Function and method bodies retain canonical Uses edges and every distinct
  source occurrence without stealing calls or construction edges.

  Background:
    Given a two-translation-unit reference project with shared inline and template bodies
    When the real facts-tool indexes the reference project using compile_commands.json

  Scenario: canonical Uses relations retain every source site
    Then the canonical Uses relations include
      | source                                  | destination                       | count | sites |
      | reference_fixture::sharedInline         | reference_fixture::primaryTarget   | 4     | 4     |
      | reference_fixture::sharedInline         | reference_fixture::secondaryTarget | 1     | 1     |
      | reference_fixture::Example::method      | reference_fixture::primaryTarget   | 1     | 1     |
      | reference_fixture::Example::method      | reference_fixture::Example::field  | 1     | 1     |
      | reference_fixture::redeclaredOwner      | reference_fixture::primaryTarget   | 1     | 1     |
    And direct calls and construction targets are not stored as Uses
    And template and nested callable Uses have canonical owners
    And body-nested declarations retain their specialized facts
    And every Uses site has an exact valid location with no duplicates
