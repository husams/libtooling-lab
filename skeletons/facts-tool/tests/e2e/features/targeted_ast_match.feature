Feature: Targeted dynamic AST matching
  Dynamic matchers persist only explicitly bound symbols and relations.

  Scenario Outline: Persist each supported symbol binding
    Given the targeted matcher corpus is imported
    When match runs with matcher "<matcher>"
    Then match succeeds and reports symbol kind "<kind>"
    And the selected symbol "<name>" exists once

    Examples:
      | kind       | name                            | matcher |
      | record     | targeted_match::Record          | cxxRecordDecl(hasName("targeted_match::Record"),isDefinition()).bind("symbol") |
      | function   | targeted_match::caller          | functionDecl(hasName("targeted_match::caller")).bind("symbol") |
      | method     | targeted_match::Record::run     | cxxMethodDecl(hasName("targeted_match::Record::run"),isDefinition()).bind("symbol") |
      | field      | targeted_match::Record::field   | fieldDecl(hasName("targeted_match::Record::field")).bind("symbol") |
      | variable   | targeted_match::variable        | varDecl(hasName("targeted_match::variable")).bind("symbol") |
      | enum       | targeted_match::Colour          | enumDecl(hasName("targeted_match::Colour")).bind("symbol") |
      | enumerator | targeted_match::Red             | enumConstantDecl(hasName("targeted_match::Red")).bind("symbol") |
      | template   | targeted_match::Box             | classTemplateDecl(hasName("targeted_match::Box")).bind("symbol") |

  Scenario: Repeating a record match is idempotent
    Given the targeted matcher corpus is imported
    When the Record symbol matcher runs twice
    Then match succeeds and reports symbol kind "record"
    And the selected symbol "targeted_match::Record" exists once

  Scenario Outline: Persist each explicit relation contract
    Given the targeted matcher corpus is imported
    When match runs with relation "<relation>" and matcher "<matcher>"
    Then match succeeds and stores one "<relation>" relation
    And "<sites>" relation sites are stored

    Examples:
      | relation  | sites | matcher |
      | Inherits  | 0 | cxxRecordDecl(hasName("targeted_match::Record"),isDerivedFrom(cxxRecordDecl(hasName("targeted_match::Base")).bind("target"))).bind("source") |
      | Contains  | 0 | enumDecl(hasName("targeted_match::Colour"),has(enumConstantDecl(hasName("targeted_match::Red")).bind("target"))).bind("source") |
      | Overrides | 1 | cxxMethodDecl(cxxMethodDecl(hasName("targeted_match::Record::run"),forEachOverridden(cxxMethodDecl().bind("target"))).bind("source"),cxxMethodDecl().bind("site")) |
      | Uses      | 1 | declRefExpr(to(varDecl(hasName("targeted_match::variable")).bind("target")),hasAncestor(functionDecl(hasName("targeted_match::caller")).bind("source"))).bind("site") |
      | FieldOf   | 0 | fieldDecl(hasName("targeted_match::Record::field"),hasParent(cxxRecordDecl().bind("target"))).bind("source") |
      | MethodOf  | 0 | cxxMethodDecl(hasName("targeted_match::Record::run"),hasParent(cxxRecordDecl().bind("target"))).bind("source") |

  Scenario: Persist a direct call and print only argument facts
    Given the targeted matcher corpus is imported
    When match runs with matcher "callExpr(callee(functionDecl(hasName(\"targeted_match::print\")).bind(\"callee\"))).bind(\"call\")"
    Then match succeeds and stores one "Calls" relation
    And one Calls relation site is stored
    And the string lvalue argument is reported

  Scenario: Reject invalid bindings without partial writes
    Given the targeted matcher corpus is imported
    When match runs with matcher "functionDecl(hasName(\"targeted_match::caller\")).bind(\"wrong\")"
    Then match fails with "bindings must exactly match a supported contract"
    And no targeted facts are stored

  Scenario Outline: Reject invalid contracts without partial writes
    Given the targeted matcher corpus is imported
    When match runs with relation "<relation>" and matcher "<matcher>"
    Then match fails with "<message>"
    And no targeted facts are stored

    Examples:
      | relation | message | matcher |
      | Uses | Uses requires site binding | functionDecl(hasName("targeted_match::caller"),hasDescendant(declRefExpr(to(varDecl(hasName("targeted_match::variable")).bind("target"))))).bind("source") |
      | Overrides | Overrides requires site binding | cxxMethodDecl(hasName("targeted_match::Record::run"),forEachOverridden(cxxMethodDecl().bind("target"))).bind("source") |
      | FieldOf | FieldOf forbids site binding | fieldDecl(hasName("targeted_match::Record::field"),hasInClassInitializer(integerLiteral().bind("site")),hasParent(cxxRecordDecl().bind("target"))).bind("source") |
      | FieldOf | FieldOf has incompatible source/target declarations | functionDecl(hasName("targeted_match::caller"),hasAncestor(namespaceDecl(hasDescendant(cxxRecordDecl(hasName("targeted_match::Record")).bind("target"))))).bind("source") |
      | Unknown | unsupported relation kind | fieldDecl(hasName("targeted_match::Record::field"),hasParent(cxxRecordDecl().bind("target"))).bind("source") |

  Scenario: Reject an unsupported template declaration
    Given the targeted matcher corpus is imported
    When match runs with matcher "typeAliasTemplateDecl(hasName(\"targeted_match::Alias\")).bind(\"symbol\")"
    Then match fails with "unsupported TemplateDecl binding"
    And no targeted facts are stored

  Scenario: Match all imported translation units in stored order
    Given two targeted matcher translation units are imported
    When match runs without source arguments
    Then both translation units match in stored order

  Scenario: Roll back matches when a later translation unit fails
    Given a valid then invalid targeted translation unit are imported
    When match runs without source arguments
    Then translation unit failure rolls back every fact

  Scenario: Preserve the existing schema
    Given the targeted matcher corpus is imported and its facts schema exists
    When match runs with matcher "functionDecl(hasName(\"targeted_match::caller\")).bind(\"symbol\")"
    Then the database schema is unchanged
