Feature: Native REST filesystem monitoring
  Linux inotify automatically reimports and reindexes active clones from the project database.

  Scenario Outline: Source and header saves publish new symbols and matcher results
    Given a native inotify server watching an indexed C++ project
    When I save an added function using "<save>"
    Then inotify automatically reimports and reindexes the new function

    Examples:
      | save               |
      | source edit        |
      | header edit        |
      | atomic replacement |

  Scenario: Newly created nested source directories are watched recursively
    Given a native inotify server watching an indexed C++ project
    When I add a nested translation unit to the compilation database
    And I edit that newly watched translation unit
    Then the nested function is updated without restarting the server

  Scenario: Failed reimports are observable without losing HTTP availability
    Given a native inotify server watching an indexed C++ project
    When I corrupt the watched compilation database
    Then watch status reports the reimport failure while HTTP stays available

  Scenario Outline: Inotify invalidates cached ASTs without a new Git commit
    Given a native inotify server watching an indexed project with a cached AST
    When I save an added function using "<save>"
    Then inotify automatically reimports and reindexes the new function
    And the project Git commit is unchanged

    Examples:
      | save        |
      | source edit |
      | header edit |
