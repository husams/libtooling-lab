#include "ast/extractors/TemplateSpecialization.h"

#include "ast/StoreExtracted.h"
#include "ast/extractors/NamedDecl.h"
#include "ast/extractors/Type.h"
#include "model/Parameter.h"
#include "model/Relation.h"
#include "storage/FactStore.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/TemplateBase.h>
#include <clang/AST/Type.h>
#include <llvm/Support/raw_ostream.h>

#include <cstddef>
#include <ranges>
#include <string>
#include <utility>

namespace facts {
namespace {

using ParametersResult = ExtractionResult<std::vector<TemplateParameter>>;

std::uint32_t flagWhen(ParameterBit flag, bool condition) {
  return condition ? bit(flag) : 0;
}

bool isConstQualified(clang::QualType type) {
  if (type.isConstQualified()) {
    return true;
  }
  if (!type->isPointerType() && !type->isReferenceType()) {
    return false;
  }
  return type->getPointeeType().isConstQualified();
}

std::uint32_t typeFlags(clang::QualType type, bool isPack) {
  return flagWhen(ParameterBit::PointerBit, type->isPointerType()) |
         flagWhen(ParameterBit::LValueReferenceBit,
                  type->isLValueReferenceType()) |
         flagWhen(ParameterBit::RValueReferenceBit,
                  type->isRValueReferenceType()) |
         flagWhen(ParameterBit::ConstBit, isConstQualified(type)) |
         flagWhen(ParameterBit::PackBit, isPack);
}

ExtractionResult<SymbolId> resolveType(clang::QualType type,
                                       const clang::ASTContext &context,
                                       FileManager &files, FactStore &store) {
  if (type.isNull()) {
    return std::unexpected(ExtractionError::InvalidType);
  }
  if (type->isDependentType()) {
    return SymbolId{};
  }
  return extractType(type, context.getSourceManager(), files, store)
      .transform_error(
          [](TypeResolutionError) { return ExtractionError::InvalidType; });
}

ExtractionResult<SymbolId>
resolveDeclaration(const clang::NamedDecl &declaration, FactStore &store) {
  return extractUsr(declaration)
      .and_then([&](std::string usr) -> ExtractionResult<SymbolId> {
        auto id = store.findId(usr);
        if (!id || !*id) {
          return std::unexpected(ExtractionError::InvalidType);
        }
        return **id;
      });
}

std::string printArgument(const clang::TemplateArgument &argument,
                          const clang::ASTContext &context) {
  std::string text;
  llvm::raw_string_ostream stream{text};
  argument.print(context.getPrintingPolicy(), stream, false);
  return text;
}

ExtractionResult<TemplateParameter>
extractLeafParameter(const clang::TemplateArgument &argument,
                     const clang::ASTContext &context, FileManager &files,
                     FactStore &store) {
  const auto nonType = [&](clang::QualType type) {
    return resolveType(type, context, files, store).transform([&](SymbolId id) {
      return TemplateParameter{
          .value = printArgument(argument, context),
          .type = id,
          .flags = typeFlags(type, argument.isPackExpansion()),
          .kind = TemplateParameterKind::NonType,
      };
    });
  };

  switch (argument.getKind()) {
  case clang::TemplateArgument::Type: {
    const auto type = argument.getAsType();
    return resolveType(type, context, files, store).transform([&](SymbolId id) {
      return TemplateParameter{
          .type = id,
          .flags = typeFlags(type, argument.isPackExpansion()),
          .kind = TemplateParameterKind::Type,
      };
    });
  }
  case clang::TemplateArgument::Declaration:
    return nonType(argument.getParamTypeForDecl());
  case clang::TemplateArgument::NullPtr:
    return nonType(argument.getNullPtrType());
  case clang::TemplateArgument::Integral:
    return nonType(argument.getIntegralType());
  case clang::TemplateArgument::StructuralValue:
    return nonType(argument.getStructuralValueType());
  case clang::TemplateArgument::Expression:
    return nonType(argument.getAsExpr()->getType());
  case clang::TemplateArgument::Template:
  case clang::TemplateArgument::TemplateExpansion: {
    const auto name = argument.getAsTemplateOrTemplatePattern();
    const auto *declaration = name.getAsTemplateDecl();
    if (declaration == nullptr) {
      return std::unexpected(ExtractionError::InvalidType);
    }
    return resolveDeclaration(*declaration, store).transform([&](SymbolId id) {
      return TemplateParameter{
          .type = id,
          .flags = flagWhen(ParameterBit::PackBit, argument.isPackExpansion()),
          .kind = TemplateParameterKind::Template,
      };
    });
  }
  case clang::TemplateArgument::Null:
  case clang::TemplateArgument::Pack:
    return std::unexpected(ExtractionError::InvalidType);
  }
  return std::unexpected(ExtractionError::InvalidType);
}

ParametersResult extractParameter(const clang::TemplateArgument &argument,
                                  const clang::ASTContext &context,
                                  FileManager &files, FactStore &store) {
  if (argument.getKind() != clang::TemplateArgument::Pack) {
    return extractLeafParameter(argument, context, files, store)
        .transform([](TemplateParameter parameter) {
          return std::vector<TemplateParameter>{std::move(parameter)};
        });
  }

  const auto append = [&](ParametersResult result, auto indexedArgument) {
    auto [packIndex, element] = indexedArgument;
    return std::move(result).and_then(
        [&](std::vector<TemplateParameter> parameters) {
          return extractParameter(element, context, files, store)
              .transform([parameters = std::move(parameters), packIndex](
                             std::vector<TemplateParameter> elements) mutable {
                std::ranges::for_each(elements, [packIndex](auto &parameter) {
                  parameter.kind = TemplateParameterKind::Pack;
                  parameter.packIndex = static_cast<std::int16_t>(packIndex);
                  parameter.flags |= bit(ParameterBit::PackBit);
                });
                std::ranges::move(elements, std::back_inserter(parameters));
                return parameters;
              });
        });
  };

  return std::ranges::fold_left(
      std::views::zip(std::views::iota(std::size_t{0}),
                      argument.pack_elements()),
      ParametersResult{std::vector<TemplateParameter>{}}, append);
}

std::expected<RelationKind, std::error_code>
relationKind(clang::TemplateSpecializationKind specializationKind) {
  if (specializationKind == clang::TSK_ExplicitSpecialization) {
    return RelationKind::Specializes;
  }
  if (clang::isTemplateInstantiation(specializationKind)) {
    return RelationKind::Instantiates;
  }
  return std::unexpected(std::make_error_code(std::errc::invalid_argument));
}

struct PatternTarget {
  SymbolId id;
  std::string usr;
};

std::string_view relationName(RelationKind kind) {
  return kind == RelationKind::Specializes ? "specializes" : "instantiates";
}

std::expected<PatternTarget, IndexingError>
findPattern(std::string_view source, const clang::NamedDecl &pattern,
            RelationKind kind, FactStore &store) {
  const auto target = pattern.getQualifiedNameAsString();
  const auto failure = [&](std::string_view usr, std::string_view detail) {
    return relationFailure(relationName(kind), "source", source, "target",
                           target, usr, detail);
  };

  return extractUsr(pattern)
      .transform_error([&](ExtractionError error) {
        return failure("<unavailable>", extractionErrorName(error));
      })
      .and_then(
          [&](std::string usr) -> std::expected<PatternTarget, IndexingError> {
            return store.findId(usr)
                .transform_error([&](std::error_code error) {
                  return failure(usr, error.message());
                })
                .and_then([&](std::optional<SymbolId> id)
                              -> std::expected<PatternTarget, IndexingError> {
                  if (!id) {
                    return std::unexpected(
                        failure(usr, "target symbol is not persisted"));
                  }
                  return PatternTarget{.id = *id, .usr = std::move(usr)};
                });
          });
}

} // namespace

ParametersResult
extractTemplateParameters(llvm::ArrayRef<clang::TemplateArgument> arguments,
                          const clang::ASTContext &context, FileManager &files,
                          FactStore &store) {
  const auto append = [&](ParametersResult result,
                          const clang::TemplateArgument &argument) {
    return std::move(result).and_then(
        [&](std::vector<TemplateParameter> parameters) {
          return extractParameter(argument, context, files, store)
              .transform([parameters = std::move(parameters)](
                             std::vector<TemplateParameter> extracted) mutable {
                std::ranges::move(extracted, std::back_inserter(parameters));
                return parameters;
              });
        });
  };

  return std::ranges::fold_left(
      arguments, ParametersResult{std::vector<TemplateParameter>{}}, append);
}

IndexingResult storeTemplateInstanceRelations(
    SymbolId instance, std::string_view instanceName,
    const clang::NamedDecl &pattern,
    clang::TemplateSpecializationKind specializationKind,
    llvm::ArrayRef<clang::TemplateArgument> arguments,
    const clang::ASTContext &context, FileManager &files, FactStore &store) {
  const auto target = pattern.getQualifiedNameAsString();
  return relationKind(specializationKind)
      .transform_error([&](std::error_code error) {
        return relationFailure("template_instance", "source", instanceName,
                               "target", target, "<unavailable>",
                               error.message());
      })
      .and_then([&](RelationKind kind) {
        return findPattern(instanceName, pattern, kind, store)
            .transform([kind](PatternTarget destination) {
              return std::pair{
                  Relation{.destination = destination.id, .kind = kind},
                  std::move(destination.usr)};
            });
      })
      .and_then([&](auto pattern) {
        auto [patternRelation, usr] = std::move(pattern);
        return extractTemplateParameters(arguments, context, files, store)
            .transform_error([&](ExtractionError error) {
              return relationFailure(relationName(patternRelation.kind),
                                     "source", instanceName, "target", target,
                                     usr, extractionErrorName(error));
            })
            .transform([&, usr = std::move(usr)](
                           std::vector<TemplateParameter> parameters) mutable {
              patternRelation.source = instance;
              auto relations =
                  std::views::zip(std::views::iota(std::size_t{0}),
                                  parameters) |
                  std::views::filter([](const auto &indexed) {
                    const auto &[position, parameter] = indexed;
                    return parameter.type.file != builtinFileId;
                  }) |
                  std::views::transform([instance](const auto &indexed) {
                    const auto &[position, parameter] = indexed;
                    return Relation{
                        .source = instance,
                        .destination = parameter.type,
                        .kind = RelationKind::TemplateArgumentType,
                        .position = static_cast<std::uint16_t>(position),
                    };
                  }) |
                  std::ranges::to<std::vector>();
              relations.insert(relations.begin(), patternRelation);
              return std::pair{std::move(relations), std::move(usr)};
            });
      })
      .and_then([&](auto result) {
        auto [relations, usr] = std::move(result);
        return store.addRelations(relations).transform_error(
            [&](std::error_code error) {
              return relationFailure(relationName(relations.front().kind),
                                     "source", instanceName, "target", target,
                                     usr, error.message());
            });
      });
}

} // namespace facts
