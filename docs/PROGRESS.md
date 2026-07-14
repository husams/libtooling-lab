# Lab Progress

Mark each section `[x]` as you complete it.

## Part 1 — Setup & Your First Tool
- [ ] 1.1 — Clang's three interfaces, and where LibTooling fits
- [ ] 1.2 — Install the toolchain
- [ ] 1.3 — Tour the visitor-tool skeleton
- [ ] 1.4 — Build it
- [ ] 1.5 — Run it
- [ ] 1.6 — What the `--` actually does

## Part 2 — AST Fundamentals
- [ ] 2.1 — Seeing the AST: -ast-dump and friends
- [ ] 2.2 — ASTContext and TranslationUnitDecl
- [ ] 2.3 — Three hierarchies, no common base
- [ ] 2.4 — Node identity: kinds, casts, DynTypedNode
- [ ] 2.5 — A workflow for exploring any node

## Part 3 — Declarations I: The Decl Infrastructure
- [ ] 3.1 — The Decl base class
- [ ] 3.2 — Names: DeclarationName
- [ ] 3.3 — DeclContext: where declarations live
- [ ] 3.4 — Redeclaration chains
- [ ] 3.5 — The complete kind map: DeclNodes.inc

## Part 4 — Declarations II: Variables, Parameters & Fields
- [ ] 4.1 — VarDecl
- [ ] 4.2 — ParmVarDecl
- [ ] 4.3 — FieldDecl & IndirectFieldDecl
- [ ] 4.4 — Structured bindings: DecompositionDecl & BindingDecl
- [ ] 4.5 — The hidden parameters: ImplicitParamDecl & friends

## Part 5 — Declarations III: Functions & Methods
- [ ] 5.1 — FunctionDecl in depth
- [ ] 5.2 — CXXMethodDecl
- [ ] 5.3 — CXXConstructorDecl & the initializer list
- [ ] 5.4 — CXXDestructorDecl & CXXConversionDecl
- [ ] 5.5 — Implicit, defaulted, deleted
- [ ] 5.6 — CXXDeductionGuideDecl

## Part 6 — Declarations IV: Records
- [ ] 6.1 — RecordDecl & CXXRecordDecl
- [ ] 6.2 — Bases
- [ ] 6.3 — Access & AccessSpecDecl
- [ ] 6.4 — Friends: FriendDecl & FriendTemplateDecl
- [ ] 6.5 — Record layout

## Part 7 — Declarations V: Template Parameters & Function Templates
- [ ] 7.1 — The two-level model
- [ ] 7.2 — TemplateTypeParmDecl
- [ ] 7.3 — NonTypeTemplateParmDecl
- [ ] 7.4 — TemplateTemplateParmDecl & BuiltinTemplateDecl
- [ ] 7.5 — FunctionTemplateDecl

## Part 8 — Declarations VI: Class, Variable & Alias Templates, Concepts
- [ ] 8.1 — ClassTemplateDecl
- [ ] 8.2 — ClassTemplateSpecializationDecl & TemplateArgument
- [ ] 8.3 — ClassTemplatePartialSpecializationDecl
- [ ] 8.4 — VarTemplateDecl & its specializations
- [ ] 8.5 — TypeAliasTemplateDecl
- [ ] 8.6 — Concepts: ConceptDecl, ImplicitConceptSpecializationDecl, RequiresExprBodyDecl

## Part 9 — Declarations VII: Enums, Aliases & Scopes
- [ ] 9.1 — EnumDecl
- [ ] 9.2 — EnumConstantDecl
- [ ] 9.3 — TypedefDecl / TypeAliasDecl
- [ ] 9.4 — NamespaceDecl & NamespaceAliasDecl
- [ ] 9.5 — Using declarations & their shadows
- [ ] 9.6 — The C++20 & dependent corners

## Part 10 — Declarations VIII: The Long Tail, Complete
- [ ] 10.1 — The file-scope crowd
- [ ] 10.2 — Labels & outlined bodies
- [ ] 10.3 — Compiler-manufactured constants
- [ ] 10.4 — Pragma droppings
- [ ] 10.5 — The dialect tables: ObjC, OpenMP, OpenACC, HLSL
- [ ] 10.6 — Attributes: the other hierarchy on declarations

## Part 11 — Statements: Core & Selection
- [ ] 11.1 — The Stmt base
- [ ] 11.2 — CompoundStmt
- [ ] 11.3 — DeclStmt
- [ ] 11.4 — NullStmt, LabelStmt, AttributedStmt
- [ ] 11.5 — IfStmt
- [ ] 11.6 — SwitchStmt, CaseStmt, DefaultStmt

## Part 12 — Statements: Loops & Jumps
- [ ] 12.1 — WhileStmt and DoStmt
- [ ] 12.2 — ForStmt
- [ ] 12.3 — CXXForRangeStmt
- [ ] 12.4 — ReturnStmt
- [ ] 12.5 — BreakStmt and ContinueStmt
- [ ] 12.6 — GotoStmt, LabelStmt, IndirectGotoStmt

## Part 13 — Statements: The Long Tail
- [ ] 13.1 — CXXTryStmt and CXXCatchStmt
- [ ] 13.2 — SEH: the Windows exception statements
- [ ] 13.3 — Inline assembly: GCCAsmStmt and MSAsmStmt
- [ ] 13.4 — Coroutine statements
- [ ] 13.5 — Outlined and dialect-specific statements
- [ ] 13.6 — Objective-C statements (complete table)
- [ ] 13.7 — OpenMP directives (complete table)
- [ ] 13.8 — OpenACC constructs (complete table)

## Part 14 — Expressions: The Expr Base
- [ ] 14.1 — Expr in the hierarchy
- [ ] 14.2 — Value categories
- [ ] 14.3 — Object kinds
- [ ] 14.4 — ParenExpr
- [ ] 14.5 — Dependence

## Part 15 — Expressions: Literals
- [ ] 15.1 — Numbers: IntegerLiteral and FloatingLiteral
- [ ] 15.2 — Characters and strings
- [ ] 15.3 — The keyword literals
- [ ] 15.4 — UserDefinedLiteral
- [ ] 15.5 — Compound and embedded literals

## Part 16 — Expressions: Names & Members
- [ ] 16.1 — DeclRefExpr
- [ ] 16.2 — MemberExpr
- [ ] 16.3 — PredefinedExpr
- [ ] 16.4 — SourceLocExpr
- [ ] 16.5 — The Microsoft and vector tail

## Part 17 — Expressions: Operators
- [ ] 17.1 — UnaryOperator
- [ ] 17.2 — BinaryOperator and CompoundAssignOperator
- [ ] 17.3 — Conditionals
- [ ] 17.4 — CXXRewrittenBinaryOperator
- [ ] 17.5 — Subscripts
- [ ] 17.6 — sizeof, alignof, offsetof
- [ ] 17.7 — The GNU and builtin operator tail

## Part 18 — Expressions: Calls
- [ ] 18.1 — CallExpr
- [ ] 18.2 — CXXMemberCallExpr
- [ ] 18.3 — CXXOperatorCallExpr
- [ ] 18.4 — UserDefinedLiteral, revisited as a call
- [ ] 18.5 — The call tail

## Part 19 — Expressions: Construction & Initialization
- [ ] 19.1 — CXXConstructExpr: where objects come from
- [ ] 19.2 — InitListExpr: one node, two forms
- [ ] 19.3 — The paren-shaped relatives
- [ ] 19.4 — std::initializer_list and its hidden array
- [ ] 19.5 — Defaults: borrowed expressions

## Part 20 — Expressions: Casts
- [ ] 20.1 — The CastExpr machinery
- [ ] 20.2 — ImplicitCastExpr and the CastKind taxonomy
- [ ] 20.3 — The explicit casts, one by one
- [ ] 20.4 — A worked modernizer

## Part 21 — Expressions: C++ Objects & Introspection
- [ ] 21.1 — CXXThisExpr
- [ ] 21.2 — CXXNewExpr and CXXDeleteExpr
- [ ] 21.3 — Exceptions: CXXThrowExpr and CXXNoexceptExpr
- [ ] 21.4 — Runtime introspection: typeid and friends
- [ ] 21.5 — LambdaExpr
- [ ] 21.6 — Compiler trait intrinsics

## Part 22 — Expressions: The Invisible AST
- [ ] 22.1 — Temporaries: the lifetime trio
- [ ] 22.2 — Placeholders: OpaqueValueExpr and PseudoObjectExpr
- [ ] 22.3 — ConstantExpr and RecoveryExpr: the boundary markers
- [ ] 22.4 — The Ignore* helpers
- [ ] 22.5 — Reading a full sandwich

## Part 23 — Expressions: Dependence, Packs & Concepts
- [ ] 23.1 — Dependent trees: what changes
- [ ] 23.2 — Unresolved names and members
- [ ] 23.3 — Parameter packs
- [ ] 23.4 — Concepts expressions

## Part 24 — Expressions: Constant Evaluation & the Complete Tail
- [ ] 24.1 — Constant evaluation
- [ ] 24.2 — CXXRewrittenBinaryOperator: C++20's ghost writer
- [ ] 24.3 — Coroutine expressions
- [ ] 24.4 — _Generic
- [ ] 24.5 — The complete remaining tail

## Part 25 — Types I: QualType & Builtins
- [ ] 25.1 — QualType: a Type* plus qualifiers
- [ ] 25.2 — The full qualifier model
- [ ] 25.3 — BuiltinType: the complete zoo
- [ ] 25.4 — PredefinedSugarType

## Part 26 — Types II: Derived Types
- [ ] 26.1 — Pointers and references
- [ ] 26.2 — Member pointers
- [ ] 26.3 — Arrays: all five kinds
- [ ] 26.4 — Vectors and matrices
- [ ] 26.5 — Complex, atomic, `_BitInt`, and the rest

## Part 27 — Types III: Function, Tag & Sugar Types
- [ ] 27.1 — Function types
- [ ] 27.2 — Tag types and the injected class name
- [ ] 27.3 — Sugar nodes: the complete set
- [ ] 27.4 — `typeof` and `__underlying_type`
- [ ] 27.5 — Canonical types: the rules that keep tools honest

## Part 28 — Types IV: Deduction & Dependence
- [ ] 28.1 — `auto` and CTAD
- [ ] 28.2 — `decltype` and pack indexing
- [ ] 28.3 — Template specialization and parameter types
- [ ] 28.4 — Dependent names and packs
- [ ] 28.5 — The Objective-C types

## Part 29 — Types V: TypeLoc
- [ ] 29.1 — TypeSourceInfo and TypeLoc values
- [ ] 29.2 — The mirror hierarchy
- [ ] 29.3 — The recipes
- [ ] 29.4 — TypeLocs in visitors and matchers

## Part 30 — Visitors
- [ ] 30.1 — Where a visitor plugs in
- [ ] 30.2 — RecursiveASTVisitor: Visit / Traverse / WalkUpFrom
- [ ] 30.3 — Hands-on: a declaration inventory
- [ ] 30.4 — Controlling the traversal
- [ ] 30.5 — Getting context: parents and DeclContext
- [ ] 30.6 — DynamicRecursiveASTVisitor

## Part 31 — SourceManager & Locations
- [ ] 31.1 — What a SourceLocation really is
- [ ] 31.2 — Decoding locations: file, line, column
- [ ] 31.3 — Macros: spelling vs expansion
- [ ] 31.4 — Ranges and extracting source text
- [ ] 31.5 — Filtering: main file, system headers, presumed locations

## Part 32 — Tokens & the Lexer
- [ ] 32.1 — Where tokens sit in the pipeline
- [ ] 32.2 — The Token class
- [ ] 32.3 — Raw lexing a file
- [ ] 32.4 — The Lexer's static helpers
- [ ] 32.5 — Hands-on: a token inventory

## Part 33 — AST Matchers
- [ ] 33.1 — Matchers vs visitors
- [ ] 33.2 — Prototyping in clang-query
- [ ] 33.3 — The matcher grammar
- [ ] 33.4 — MatchFinder, callbacks, and binding
- [ ] 33.5 — Composing matchers
- [ ] 33.6 — Traversal modes and implicit nodes
- [ ] 33.7 — Exercises

## Part 34 — Command-Line APIs
- [ ] 34.1 — llvm::cl in ten minutes
- [ ] 34.2 — Custom options for your tool
- [ ] 34.3 — Hands-on: --verbose and --name-filter
- [ ] 34.4 — Compilation databases

## Part 35 — Advanced Tooling
- [ ] 35.1 — ArgumentsAdjusters
- [ ] 35.2 — Virtual files and runToolOnCode
- [ ] 35.3 — ASTUnit: an AST without an action
- [ ] 35.4 — Running over many files
- [ ] 35.5 — Where to go next

## Part 36 — The Preprocessor: Includes, Macros & Callbacks
- [ ] 36.1 — Getting hold of the Preprocessor
- [ ] 36.2 — PPCallbacks: the hook surface
- [ ] 36.3 — Hands-on: an include lister
- [ ] 36.4 — Macros
- [ ] 36.5 — Skipped regions, the include stack, and friends

## Part 37 — The Index Library: USRs & Symbol Occurrences
- [ ] 37.1 — USRs: a name that outlives the parse
- [ ] 37.2 — Symbol info and roles
- [ ] 37.3 — The occurrence stream
- [ ] 37.4 — Hands-on: reading the cross-reference stream

## Part 38 — Rewriting & Edits: Rewriter, Replacements & the Edit Library
- [ ] 38.1 — The Rewriter
- [ ] 38.2 — Getting the result out
- [ ] 38.3 — Replacements: the serializable tier
- [ ] 38.4 — The Edit library
- [ ] 38.5 — Hands-on: an `override` inserter

## Part 39 — Analysis: the CFG, CallGraph & Flow Analyses
- [ ] 39.1 — Building a CFG
- [ ] 39.2 — Blocks, terminators, and edges
- [ ] 39.3 — CallGraph: who calls whom
- [ ] 39.4 — The prebuilt analyses
- [ ] 39.5 — Hands-on: an unreachable-block finder
