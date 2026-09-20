#include "index/Fixture.h"
#include <iostream>

int main() {
  index_test::Fixture fixture;
  index_test::verifySnapshot(fixture);
  index_test::verifyFailures(fixture);
  index_test::verifyLongCursor();
  index_test::verifyPrefix();
  index_test::verifyGraphIdentity();
  index_test::verifyInvalidatedSymbols();
  index_test::verifySymbolResources();
  index_test::verifyMigration();
  index_test::verifyObsoleteSources();
  std::cout << "global index snapshots, indexed name lookup, cursor coherence, "
               "definition location, clone identity and failure preservation PASS\n";
}
