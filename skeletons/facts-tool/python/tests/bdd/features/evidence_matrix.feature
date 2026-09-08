Feature: Schema13 evidence access matrix

  Scenario: Every facade preserves access identity and ancestors
    Given a schema13 matrix pair
    When I query every evidence access class
    Then every matrix facade exposes owners targets files and real ancestors
