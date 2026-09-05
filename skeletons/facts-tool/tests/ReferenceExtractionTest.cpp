#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Reference.h"
#include "model/Function.h"
#include "model/Variable.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/Casting.h>

#include <sqlite3.h>

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

void verifyExtractorSkips(const std::filesystem::path &outputRoot) {
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      "int target; int owner() { return target; }", {"-std=c++23"},
      "reference_skip.cpp");
  assert(ast);
  clang::FunctionDecl *owner = nullptr;
  clang::VarDecl *target = nullptr;
  for (auto *decl : ast->getASTContext().getTranslationUnitDecl()->decls()) {
    if (auto *function = llvm::dyn_cast<clang::FunctionDecl>(decl);
        function != nullptr && function->getName() == "owner") {
      owner = function;
    }
    if (auto *variable = llvm::dyn_cast<clang::VarDecl>(decl);
        variable != nullptr && variable->getName() == "target") {
      target = variable;
    }
  }
  assert(owner != nullptr && target != nullptr);

  std::filesystem::remove_all(outputRoot);
  std::filesystem::create_directories(outputRoot);
  facts::FileManager files((outputRoot / "files.sqlite").string());
  facts::FactStore store((outputRoot / "facts.sqlite").string());
  const auto ownerUsr = facts::extractUsr(*owner);
  const auto targetUsr = facts::extractUsr(*target);
  assert(ownerUsr && targetUsr);
  facts::Function ownerSymbol{};
  ownerSymbol.usr = *ownerUsr;
  ownerSymbol.qualifiedName = "owner";
  assert(store.save(1, ownerSymbol));

  const auto unresolved =
      facts::extractUseReference(*owner, *target, target->getLocation(),
                                 ast->getSourceManager(), files, store);
  // A valid DB-miss USR now uses canonical external-target extraction; an
  // unregistered in-memory source cannot supply its required file identity.
  assert(!unresolved && unresolved.error() == facts::ExtractionError::RelationTarget);

  facts::Variable targetSymbol{};
  targetSymbol.usr = *targetUsr;
  targetSymbol.qualifiedName = "target";
  assert(store.save(1, targetSymbol));
  const auto invalidLocation =
      facts::extractUseReference(*owner, *target, clang::SourceLocation{},
                                 ast->getSourceManager(), files, store);
  assert(invalidLocation && !*invalidLocation);
  std::filesystem::remove_all(outputRoot);
}

int scalar(sqlite3 *database, std::string_view sql) {
  sqlite3_stmt *statement = nullptr;
  assert(sqlite3_prepare_v2(database, sql.data(), static_cast<int>(sql.size()),
                            &statement, nullptr) == SQLITE_OK);
  assert(sqlite3_step(statement) == SQLITE_ROW);
  const int value = sqlite3_column_int(statement, 0);
  sqlite3_finalize(statement);
  return value;
}

int run(const std::vector<std::string> &arguments) {
  const auto child = fork();
  assert(child >= 0);
  if (child == 0) {
    std::vector<char *> raw;
    raw.reserve(arguments.size() + 1);
    for (const auto &argument : arguments) {
      raw.push_back(const_cast<char *>(argument.c_str()));
    }
    raw.push_back(nullptr);
    execv(raw.front(), raw.data());
    _exit(127);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}

void writeCompilationDatabase(const std::filesystem::path &path,
                              const std::filesystem::path &compiler,
                              const std::filesystem::path &fixtureRoot,
                              bool reverseOrder) {
  std::vector<std::filesystem::path> sources{
      fixtureRoot / "references_one.cpp",
      fixtureRoot / "references_two.cpp",
  };
  if (reverseOrder) {
    std::ranges::reverse(sources);
  }

  std::ofstream output(path / "compile_commands.json");
  assert(output);
  output << "[\n";
  for (std::size_t index = 0; index < sources.size(); ++index) {
    output << "  {\"directory\":\"" << fixtureRoot.string() << "\",\"file\":\""
           << sources[index].string() << "\",\"arguments\":[\""
           << compiler.string() << "\",\"-std=c++23\",\"-I"
           << fixtureRoot.string() << "\",\"-c\",\"" << sources[index].string()
           << "\"]}";
    output << (index + 1 == sources.size() ? "\n" : ",\n");
  }
  output << "]\n";
}

void verifyFacts(const std::filesystem::path &databasePath) {
  sqlite3 *database = nullptr;
  assert(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK);

  assert(scalar(database,
                "SELECT count FROM relation r JOIN symbol source ON "
                "source.id=r.source_id JOIN symbol destination ON "
                "destination.id=r.destination_id WHERE r.kind=7 AND "
                "source.qualified_name='reference_fixture::sharedInline' "
                "AND destination.qualified_name="
                "'reference_fixture::primaryTarget'") == 4);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation_site site JOIN symbol source "
                "ON source.id=site.source_id JOIN symbol destination ON "
                "destination.id=site.destination_id WHERE site.kind=7 AND "
                "source.qualified_name='reference_fixture::sharedInline' "
                "AND destination.qualified_name="
                "'reference_fixture::primaryTarget'") == 4);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation r JOIN symbol destination ON "
                "destination.id=r.destination_id WHERE r.kind=7 AND "
                "(destination.qualified_name LIKE '%helper' OR "
                "destination.qualified_name LIKE '%Constructed::Constructed' "
                "OR destination.qualified_name LIKE '%sharedInline')") == 0);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation r JOIN symbol source ON "
                "source.id=r.source_id WHERE r.kind=7 AND "
                "source.qualified_name='reference_fixture::templatedOwner'") ==
         2);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation r JOIN symbol source ON "
                "source.id=r.source_id JOIN symbol destination ON "
                "destination.id=r.destination_id WHERE r.kind=7 AND "
                "source.qualified_name LIKE '%Local::method' AND "
                "destination.qualified_name="
                "'reference_fixture::primaryTarget'") == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation r JOIN symbol source ON "
                "source.id=r.source_id JOIN symbol destination ON "
                "destination.id=r.destination_id WHERE r.kind=7 AND "
                "source.qualified_name LIKE '%operator()' AND "
                "destination.qualified_name="
                "'reference_fixture::secondaryTarget'") == 1);
  assert(scalar(database,
                "SELECT COUNT(*) FROM symbol WHERE qualified_name LIKE "
                "'%LocalEnum' OR qualified_name LIKE "
                "'%nestedDeclarations%LocalAlpha' OR qualified_name LIKE "
                "'%nestedDeclarations%LocalBeta'") == 3);
  assert(scalar(database,
                "SELECT COUNT(*) FROM symbol field JOIN relation r ON "
                "r.source_id=field.id JOIN symbol owner ON "
                "owner.id=r.destination_id WHERE r.kind=8 AND "
                "field.qualified_name LIKE '%nestedDeclarations%localField' "
                "AND field.is_definition=1 AND "
                "owner.qualified_name LIKE '%Local'") == 1);
  assert(scalar(database, "SELECT COUNT(*) FROM symbol WHERE qualified_name IN "
                          "('localValue','lambda')") == 0);
  assert(scalar(database,
                "SELECT COUNT(*) FROM relation_site WHERE file_id=0 OR "
                "line<1 OR col<1 OR offset<0") == 0);
  assert(scalar(database,
                "SELECT COUNT(*) FROM (SELECT source_id,destination_id,kind,"
                "position,file_id,offset,COUNT(*) occurrences FROM "
                "relation_site GROUP BY source_id,destination_id,kind,"
                "position,file_id,offset HAVING occurrences<>1)") == 0);
  assert(scalar(database,
                "SELECT COUNT(*) FROM (SELECT r.source_id,r.destination_id "
                "FROM relation r LEFT JOIN relation_site site ON "
                "site.source_id=r.source_id AND "
                "site.destination_id=r.destination_id AND site.kind=r.kind "
                "AND site.position=r.position WHERE r.kind=7 GROUP BY "
                "r.source_id,r.destination_id,r.kind,r.position HAVING "
                "r.count<>COUNT(site.offset))") == 0);
  sqlite3_close(database);
}

void verifyOrder(const std::filesystem::path &factsTool,
                 const std::filesystem::path &compiler,
                 const std::filesystem::path &fixtureRoot,
                 const std::filesystem::path &runRoot, bool reverseOrder) {
  std::filesystem::remove_all(runRoot);
  std::filesystem::create_directories(runRoot);
  writeCompilationDatabase(runRoot, compiler, fixtureRoot, reverseOrder);
  const auto files = runRoot / "files.sqlite";
  const auto facts = runRoot / "facts.sqlite";
  const auto one = fixtureRoot / "references_one.cpp";
  const auto two = fixtureRoot / "references_two.cpp";
  assert(run({factsTool.string(), "import", "--conf", files.string(),
              "--compilation-database", runRoot.string(), one.string(),
              two.string()}) == 0);
  assert(run({factsTool.string(), "extract", "--output", facts.string(),
              "--conf", files.string(), one.string(), two.string()}) == 0);
  verifyFacts(facts);
  std::filesystem::remove_all(runRoot);
}

} // namespace

int main(int argc, char **argv) {
  assert(argc == 5);
  const std::filesystem::path outputRoot = argv[4];
  verifyExtractorSkips(outputRoot / "extractor-skips");
  verifyOrder(argv[1], argv[2], argv[3], outputRoot / "forward", false);
  verifyOrder(argv[1], argv[2], argv[3], outputRoot / "reverse", true);
}
