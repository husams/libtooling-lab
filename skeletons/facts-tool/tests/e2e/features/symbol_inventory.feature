Feature: Extracted symbol and fact inventory
  Supported declarations, definitions, parameters, and references are captured together.

  Background:
    Given realistic shared-header declarations, definitions, parameters, and relations
    When the real facts-tool indexes both translation units using compile_commands.json

  Scenario: List every stored symbol with its concrete node kind
    Then the complete symbol inventory is
      | node | qualified_name          |
      | 1    | e2e::fun                |
      | 1    | e2e::functionTemplate   |
      | 1    | e2e::headerHelper       |
      | 1    | e2e::CompositeWidget::apply |
      | 1    | e2e::ConstClass::ConstClass |
      | 1    | e2e::defaultArguments   |
      | 1    | e2e::MethodFixture::deletedMethod |
      | 1    | e2e::MethodFixture::inlineMethod |
      | 1    | e2e::MethodFixture::operator== |
      | 1    | e2e::MethodFixture::outOfLineMethod |
      | 1    | e2e::MethodFixture::pureMethod |
      | 1    | e2e::MethodTemplateFixture::methodTemplate |
      | 1    | e2e::Policy::apply      |
      | 1    | e2e::primitiveTypes     |
      | 1    | e2e::transform          |
      | 1    | e2e::useOne             |
      | 1    | e2e::useTwo             |
      | 1    | e2e::userDefinedTypes   |
      | 2    | e2e::CompositeWidget    |
      | 2    | e2e::ClassTemplate      |
      | 2    | e2e::ConstClass         |
      | 2    | e2e::Deferred           |
      | 2    | e2e::MethodFixture      |
      | 2    | e2e::MethodTemplateFixture |
      | 2    | e2e::InitializerFixture |
      | 2    | e2e::MyRecord           |
      | 2    | e2e::Payload            |
      | 2    | e2e::Policy             |
      | 2    | e2e::PrivateWidget      |
      | 2    | e2e::PublicWidget       |
      | 2    | e2e::StructTemplate     |
      | 2    | e2e::UnionTemplate      |
      | 2    | e2e::Widget             |
      | 2    | e2e::X                  |
      | 3    | e2e::Mode               |
      | 4    | e2e::MyRecord::s        |
      | 4    | e2e::ConstClass::value  |
      | 4    | e2e::InitializerFixture::count |
      | 4    | e2e::InitializerFixture::enabled |
      | 4    | e2e::InitializerFixture::label |
      | 4    | e2e::InitializerFixture::limit |
      | 4    | e2e::InitializerFixture::name |
      | 4    | e2e::InitializerFixture::values |
      | 4    | e2e::Payload::fractional |
      | 4    | e2e::Payload::integral  |
      | 4    | e2e::Policy::multiplier |
      | 4    | e2e::Widget::value      |
      | 4    | e2e::constinitGlobal    |
      | 4    | e2e::constructedClass   |
      | 4    | e2e::constructedWidget  |
      | 4    | e2e::globalValues       |
      | 4    | e2e::inlineGlobal       |
      | 4    | e2e::internalGlobal     |
      | 4    | e2e::mergedGlobal       |
      | 4    | e2e::sharedCounter      |
      | 5    | e2e                     |
      | 5    | e2e::Count              |
      | 5    | e2e::StructAlias        |
      | 5    | e2e::WidgetAlias        |
      | 5    | e2e::WidgetTypedef      |
      | 6    | e2e::Mode::Fast         |
      | 6    | e2e::Mode::Later        |
      | 6    | e2e::Mode::Slow         |

  Scenario: Function definitions and parameter metadata are captured
    Then the defined functions include
      | qualified_name    |
      | e2e::fun          |
      | e2e::headerHelper |
      | e2e::transform    |
      | e2e::useOne       |
      | e2e::useTwo       |
    And the parameters for e2e::transform are
      | position | name   |
      | 0        | widget |
      | 1        | factor |
    And the parameters for e2e::headerHelper are
      | position | name  | has_default |
      | 0        | input | 0           |
      | 1        | delta | 1           |

  Scenario: Typed facts and relations retain referential integrity
    Then the facts database has no foreign-key violations
    And every relation references captured source and destination symbols
