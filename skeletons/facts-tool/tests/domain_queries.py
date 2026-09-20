"""Clang DSL examples whose binding names are entirely client controlled."""
GENERIC_MATCHERS = {
    "unbound": ('functionDecl(hasName("alpha::answer"))', {"root"}),
    "named": ('functionDecl(hasName("alpha::answer")).bind("chosen")', {"chosen"}),
    "multiple": ('functionDecl(hasName("alpha::answer"), '
                 'hasBody(compoundStmt().bind("body"))).bind("function")', {"body", "function"}),
    "namespace": ('namespaceDecl(hasName("alpha")).bind("namespace")', {"namespace"}),
    "statement": ('returnStmt(hasAncestor(functionDecl(hasName("alpha::answer"))))'
                  '.bind("statement")', {"statement"}),
    "type": ('qualType(isInteger()).bind("type")', {"type"}),
    "internal name": ('functionDecl(hasName("alpha::answer")).bind("__facts_root")',
                      {"__facts_root"}),
}
