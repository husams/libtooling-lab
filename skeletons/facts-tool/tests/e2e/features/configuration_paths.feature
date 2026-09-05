Feature: Canonical configuration identity and safe path rendering
  Scenario Outline: Project identity from <kind>
    Given a project identity fixture "<kind>"
    When I inspect the effective configuration twice
    Then the canonical project identity determines the database
    Examples:
      | kind |
      | unmarked |
      | nested |
      | symlink |
      | git-file |
      | yaml-marker |
      | filesystem-root |
      | unicode |

  Scenario Outline: Safe literal rendering <template>
    Given template "<template>" with expected relative target "<target>"
    When I inspect the effective configuration twice
    Then the rendered path is canonical and no storage was created
    Examples:
      | template | target |
      | {filename}.sqlite | project.v2.sqlite |
      | nested//./{filename}.custom | nested/project.v2.custom |
      | literal | literal |
      | {filename}-{filename}.db | project.v2-project.v2.db |
      | unicode 空間/{filename}.db | unicode 空間/project.v2.db |
      | $UNCHANGED/{filename}.db | $UNCHANGED/project.v2.db |

  Scenario Outline: Reject unsafe template <template>
    Given invalid template "<template>"
    When I attempt configuration inspection
    Then configuration fails with "conf_template"
    Examples:
      | template |
      | ../outside.db |
      | inside/../outside.db |
      | /outside.db |
      | {unknown}.db |
      | {}.db |
      | {filename |
      | filename} |
      | directory/ |
      | . |
      | bad\path.db |

  Scenario: Reject a canonical symlink escape
    Given a generated target symlink escapes storage
    When I attempt configuration inspection
    Then configuration fails with "conf_template escapes conf_root:"

  Scenario Outline: Missing HOME with sufficient explicit storage
    Given configuration environment case "<case>"
    When I inspect the effective configuration twice
    Then missing HOME remains symbolic in discovery
    Examples:
      | case |
      | xdg-data |
      | explicit-root |

  Scenario Outline: Reject unresolved consumed environment <case>
    Given configuration environment case "<case>"
    When I attempt configuration inspection
    Then configuration fails with "<message>"
    Examples:
      | case | message |
      | absent | unresolved conf; searched: |
      | missing-key | unresolved conf; searched: |
      | tilde | requires HOME |
      | relative-config | XDG_CONFIG_HOME must be absolute |
      | relative-data | XDG_DATA_HOME must be absolute |
      | relative-data-missing-key | XDG_DATA_HOME must be absolute |

  Scenario: A relative conf_root from the user file anchors to the project root
    Given a user file with a relative conf_root
    When I inspect the effective configuration twice
    Then conf is under the project root's generated storage

  Scenario Outline: New template placeholders <template>
    Given conf_template "<template>" with an environment token
    When I inspect the effective configuration twice
    Then conf matches the expected rendered path
    Examples:
      | template |
      | {project_name}/{filename}.db |
      | {user}/{filename}.db |
      | ${FACTS_TOOL_TEST_TOKEN}/{filename}.db |

  Scenario: An unset environment placeholder is a configuration error
    Given conf_template "${FACTS_TOOL_TEST_MISSING_TOKEN}/{filename}.db"
    When I attempt configuration inspection
    Then configuration fails with "unset environment variable"

  Scenario: facts_template supplies the default output for a single source
    Given a project facts_template using the source placeholder
    When I extract with no explicit output and one source
    Then extraction succeeds and writes the facts_template path

  Scenario: facts_template with more than one source is a usage error
    Given a project facts_template using the source placeholder
    When I extract with no explicit output and two sources
    Then extraction fails with a usage error asking for -o/--facts

  Scenario: An explicit -o always overrides facts_template
    Given a project facts_template using the source placeholder
    When I extract with an explicit output and one source
    Then extraction succeeds and writes the explicit output

  Scenario: An absolute conf_template via {project_root} is accepted like facts_template
    Given the acceptance-example conf_template and facts_template
    When I inspect the effective configuration twice
    Then conf is the project root's .index/project.db
    And extracting one source writes the project root's .index/src/a/b.db

  Scenario: A raw literal absolute conf_template is still rejected
    Given conf_template "/outside.db"
    When I attempt configuration inspection
    Then configuration fails with "conf_template"

  Scenario: A leading ~/ conf_template bypasses conf_root entirely, like conf_root's own ~/
    Given conf_template "~/project.db"
    When I inspect the effective configuration twice
    Then conf is directly under HOME

  Scenario: A symlink after a leading {project_root} in facts_template still escapes
    Given a project facts_template that escapes through a symlink after {project_root}
    When I extract with no explicit output and one source
    Then extraction fails with "escapes"

  Scenario: facts_template still supplies the default output under a direct --conf override
    Given a project facts_template using the source placeholder registered under a direct conf
    When I extract under the direct conf with no explicit output and one source
    Then extraction succeeds and writes the facts_template path

  Scenario: A facts_template default never creates its directory when extraction fails
    Given a project facts_template using the source placeholder
    When I extract with no explicit output and a source that was never imported
    Then extraction fails and no facts_template directory was created

  Scenario: An invalid lower-tier template is a configuration error even when a higher tier wins
    Given a user conf_template with an unknown placeholder and a valid project conf_template
    When I attempt configuration inspection
    Then configuration fails with "unknown placeholder"

  Scenario: Discovery is recorded for every tier even after an earlier one fails
    Given a malformed project file and a valid user file
    When I attempt configuration inspection
    Then discovery marks the project file invalid and the user file found

  Scenario: facts_template supplies the default output for dependency analysis
    Given a project facts_template using the source placeholder
    When I analyse dependencies with no explicit output and one source
    Then dependency analysis succeeds and writes the facts_template path

  Scenario: facts_template needing a source cannot default a symbol command's --facts
    Given a project facts_template using the source placeholder
    When I list symbols with no explicit --facts
    Then symbol listing fails with a usage error asking for -o/--facts

  Scenario: A project-scoped facts_template supplies the default --facts for symbol commands
    Given a project-scoped facts_template with no source placeholder
    When I list symbols with no explicit --facts
    Then symbol listing succeeds using the facts_template path
