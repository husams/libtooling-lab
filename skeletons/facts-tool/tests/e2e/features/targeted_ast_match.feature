Feature: Targeted dynamic AST matching
  Dynamic matchers persist only explicitly bound symbols and relations.

  Scenario Outline: Persist each supported symbol binding
    Given the targeted matcher corpus is imported
    When match runs twice with matcher "<matcher>"
    Then match succeeds and reports symbol kind "<kind>"
    And the selected symbol "<name>" is stored once as kind <stored_kind> with properties <properties>

    Examples:
      | kind       | stored_kind | properties | name                            | matcher |
      | record     | 7           | 0          | targeted_match::Record          | cxxRecordDecl(hasName("targeted_match::Record"),isDefinition()).bind("symbol") |
      | function   | 13          | 0          | targeted_match::caller          | functionDecl(hasName("targeted_match::caller")).bind("symbol") |
      | method     | 17          | 0          | targeted_match::Record::run     | cxxMethodDecl(hasName("targeted_match::Record::run"),isDefinition()).bind("symbol") |
      | field      | 15          | 0          | targeted_match::Record::field   | fieldDecl(hasName("targeted_match::Record::field")).bind("symbol") |
      | variable   | 14          | 0          | targeted_match::variable        | varDecl(hasName("targeted_match::variable"),hasType(qualType(hasCanonicalType(asString("int"))))).bind("symbol") |
      | enum       | 6           | 0          | targeted_match::Colour          | enumDecl(hasName("targeted_match::Colour")).bind("symbol") |
      | enumerator | 16          | 0          | targeted_match::Red             | enumConstantDecl(hasName("targeted_match::Red")).bind("symbol") |
      | template   | 7           | 1          | targeted_match::Box             | classTemplateDecl(hasName("targeted_match::Box")).bind("symbol") |

  Scenario: Repeating a record match is idempotent
    Given the targeted matcher corpus is imported
    When the Record symbol matcher runs twice
    Then match succeeds and reports symbol kind "record"
    And the selected symbol "targeted_match::Record" is stored once as kind 7 with properties 0

  Scenario Outline: Persist each explicit relation contract
    Given the targeted matcher corpus is imported
    When match runs twice with relation "<relation>" and matcher "<matcher>"
    Then match succeeds and stores one "<relation>" relation
    And "<sites>" relation sites are stored

    Examples:
      | relation  | sites | matcher |
      | Inherits  | 0 | cxxRecordDecl(hasName("targeted_match::Record"),isDerivedFrom(cxxRecordDecl(hasName("targeted_match::Base")).bind("target"))).bind("source") |
      | Contains  | 0 | enumDecl(hasName("targeted_match::Colour"),has(enumConstantDecl(hasName("targeted_match::Red")).bind("target"))).bind("source") |
      | Overrides | 1 | cxxMethodDecl(hasName("targeted_match::Record::run"),forEachOverridden(cxxMethodDecl().bind("target"))).bind("source") |
      | Uses      | 1 | declRefExpr(to(varDecl(hasName("targeted_match::variable")).bind("target")),hasAncestor(functionDecl(hasName("targeted_match::caller")).bind("source"))).bind("site") |
      | FieldOf   | 0 | fieldDecl(hasName("targeted_match::Record::field"),hasParent(cxxRecordDecl().bind("target"))).bind("source") |
      | MethodOf  | 0 | cxxMethodDecl(hasName("targeted_match::Record::run"),hasParent(cxxRecordDecl().bind("target"))).bind("source") |

  Scenario: Persist a direct call and print only argument facts
    Given the targeted matcher corpus is imported
    When match runs with matcher "callExpr(callee(functionDecl(hasName(\"targeted_match::print\")).bind(\"callee\"))).bind(\"call\")"
    Then match succeeds and stores one "Calls" relation
    And one Calls relation site is stored
    And the string lvalue argument is reported

  Scenario: Derive each nearest callable owner
    Given the targeted matcher corpus is imported
    When match runs with matcher "callExpr(callee(functionDecl(hasName(\"targeted_match::sink\")).bind(\"callee\"))).bind(\"call\")"
    Then function method constructor and lambda callers are stored

  Scenario: Print an evaluable direct-call argument
    Given the targeted matcher corpus is imported
    When match runs with matcher "callExpr(callee(functionDecl(hasName(\"targeted_match::number\")).bind(\"callee\"))).bind(\"call\")"
    Then the constant argument value is reported

  Scenario: Reject an indirect call without guessing a target
    Given the targeted matcher corpus is imported
    When match runs with matcher "callExpr(callee(expr(ignoringParenImpCasts(declRefExpr(to(varDecl(hasName(\"function\")))))).bind(\"callee\"))).bind(\"call\")"
    Then match fails with "callee must bind FunctionDecl"
    And no targeted facts are stored

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
      | Inherits | Inherits has incompatible source/target declarations | functionDecl(hasName("targeted_match::caller"),hasAncestor(namespaceDecl(hasDescendant(cxxRecordDecl(hasName("targeted_match::Base")).bind("target"))))).bind("source") |
      | Contains | Contains has incompatible source/target declarations | functionDecl(hasName("targeted_match::caller"),hasAncestor(namespaceDecl(hasDescendant(cxxRecordDecl(hasName("targeted_match::Record")).bind("target"))))).bind("source") |
      | Overrides | Overrides has incompatible source/target declarations | cxxRecordDecl(hasName("targeted_match::Record"),isDerivedFrom(cxxRecordDecl(hasName("targeted_match::Base")).bind("target"))).bind("source") |
      | Uses | Uses has incompatible source/target declarations | namespaceDecl(hasName("targeted_match"),has(cxxRecordDecl(hasName("Record")).bind("source")),hasDescendant(declRefExpr(to(varDecl(hasName("targeted_match::variable")).bind("target"))).bind("site"))) |
      | FieldOf | FieldOf forbids site binding | fieldDecl(hasName("targeted_match::Record::field"),hasInClassInitializer(integerLiteral().bind("site")),hasParent(cxxRecordDecl().bind("target"))).bind("source") |
      | FieldOf | FieldOf has incompatible source/target declarations | functionDecl(hasName("targeted_match::caller"),hasAncestor(namespaceDecl(hasDescendant(cxxRecordDecl(hasName("targeted_match::Record")).bind("target"))))).bind("source") |
      | MethodOf | MethodOf has incompatible source/target declarations | functionDecl(hasName("targeted_match::caller"),hasAncestor(namespaceDecl(hasDescendant(cxxRecordDecl(hasName("targeted_match::Record")).bind("target"))))).bind("source") |
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

  Scenario: Explicit sources preserve requested order
    Given two targeted matcher translation units are imported
    When match runs with both sources in reverse order
    Then both translation units match in reverse order

  Scenario: Reject a source that was not imported
    Given the targeted matcher corpus is imported
    When match runs for an unknown source
    Then match fails with "requested source is not imported"

  Scenario: Reject an imported source without a compile command
    Given two targeted matcher translation units are imported
    When match runs for the second source after its compile command is removed
    Then match fails with "no stored compile command for requested source"

  Scenario: Roll back matches when a later translation unit fails
    Given a valid then invalid targeted translation unit are imported
    When match runs without source arguments
    Then translation unit failure rolls back every fact

  Scenario: Preserve the existing schema
    Given the targeted matcher corpus is imported and its facts schema exists
    When match runs with matcher "functionDecl(hasName(\"targeted_match::caller\")).bind(\"symbol\")"
    Then the database schema is unchanged
