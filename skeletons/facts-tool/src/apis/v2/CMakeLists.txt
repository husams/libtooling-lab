target_sources(facts-apis PRIVATE
  JobCallGraph.cpp
  JobExecute.cpp
  JobImportScan.cpp
  JobPreparation.cpp
  JobSelection.cpp
  JobSymbols.cpp
  JobValidation.cpp
  JobVariableFlow.cpp)
add_subdirectory(catalog)
add_subdirectory(http)
add_subdirectory(symbols)
