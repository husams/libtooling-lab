#ifndef FACTS_TOOL_STORAGE_SEMANTICPROPERTIES_H
#define FACTS_TOOL_STORAGE_SEMANTICPROPERTIES_H

#include "model/Parameter.h"
#include "model/Relation.h"
#include "model/Symbol.h"

#include "clang/Basic/Version.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace facts::storage {

// The symbol table stores the LLVM 22 clang::index::SymbolKind numbering. LLVM
// 22 inserted IncludeDirective directly after Macro, so a tool built against an
// older Clang sees every kind from Enum on sitting one lower. Shifting at the
// storage boundary keeps a database saying the same thing whichever LLVM the
// tool was built with.
inline constexpr std::int64_t firstShiftedKind =
    static_cast<std::int64_t>(clang::index::SymbolKind::Enum);

constexpr std::int64_t storedSymbolKind(clang::index::SymbolKind kind) {
  const auto value = static_cast<std::int64_t>(kind);
#if CLANG_VERSION_MAJOR < 22
  return value >= firstShiftedKind ? value + 1 : value;
#else
  return value;
#endif
}

constexpr clang::index::SymbolKind symbolKindFromStored(std::int64_t value) {
#if CLANG_VERSION_MAJOR < 22
  const auto native = value > firstShiftedKind ? value - 1 : value;
#else
  const auto native = value;
#endif
  return static_cast<clang::index::SymbolKind>(native);
}

struct ParameterProperties {
  bool isPointer;
  bool isLValueReference;
  bool isRValueReference;
  bool isForwardingReference;
  bool isConst;
  bool isPack;
};

struct SymbolProperties {
  std::string_view access;
  bool isDefinition;
  bool isImplicit;
  bool isStatic;
  bool isVirtual;
  bool isConst;
  bool isInline;
  bool isPure;
  std::string_view refQualifier;
  bool isOverride;
  bool hasInternalLinkage;
  bool isExternal;
  bool isVariadic;
  bool isDeleted;
  bool isDefaulted;
  bool isExplicit;
  bool isFinal;
  bool isAbstract;
  bool isPolymorphic;
  bool hasExternStorage;
  std::string_view constantEvaluation;
  bool isNoexcept;
};

struct RelationProperties {
  std::string_view access;
  bool isVirtualBase;
  bool isImplicit;
  bool isLexical;
};

constexpr bool hasBit(std::uint32_t flags, std::size_t position) {
  return (flags & bit(position)) != 0;
}

constexpr std::uint32_t flagWhen(std::size_t position, bool enabled) {
  return enabled ? bit(position) : 0;
}

constexpr std::string_view accessName(std::uint32_t flags) {
  switch (flags & accessMask) {
  case clang::AS_public:
    return "public";
  case clang::AS_protected:
    return "protected";
  case clang::AS_private:
    return "private";
  default:
    return "none";
  }
}

constexpr std::uint32_t accessFlags(std::string_view access) {
  if (access == "public") {
    return clang::AS_public;
  }
  if (access == "protected") {
    return clang::AS_protected;
  }
  if (access == "private") {
    return clang::AS_private;
  }
  return clang::AS_none;
}

constexpr std::string_view refQualifierName(std::uint32_t flags) {
  switch ((flags >> refQualifierShift) & refQualifierMask) {
  case 1:
    return "lvalue";
  case 2:
    return "rvalue";
  default:
    return "none";
  }
}

constexpr std::uint32_t refQualifierFlags(std::string_view qualifier) {
  const auto value = qualifier == "lvalue"   ? 1U
                     : qualifier == "rvalue" ? 2U
                                             : 0U;
  return value << refQualifierShift;
}

constexpr std::string_view constantEvaluationName(std::uint32_t flags) {
  switch ((flags >> constexprShift) & constexprMask) {
  case 1:
    return "constexpr";
  case 2:
    return "consteval";
  case 3:
    return "constinit";
  default:
    return "none";
  }
}

constexpr std::uint32_t constantEvaluationFlags(std::string_view evaluation) {
  const auto value = evaluation == "constexpr"   ? 1U
                     : evaluation == "consteval" ? 2U
                     : evaluation == "constinit" ? 3U
                                                 : 0U;
  return value << constexprShift;
}

constexpr ParameterProperties parameterProperties(std::uint8_t flags) {
  return {
      .isPointer = (flags & bit(ParameterBit::PointerBit)) != 0,
      .isLValueReference = (flags & bit(ParameterBit::LValueReferenceBit)) != 0,
      .isRValueReference = (flags & bit(ParameterBit::RValueReferenceBit)) != 0,
      .isForwardingReference =
          (flags & bit(ParameterBit::ForwardingReferenceBit)) != 0,
      .isConst = (flags & bit(ParameterBit::ConstBit)) != 0,
      .isPack = (flags & bit(ParameterBit::PackBit)) != 0,
  };
}

constexpr std::uint8_t parameterFlags(ParameterProperties properties) {
  return static_cast<std::uint8_t>(
      flagWhen(static_cast<std::size_t>(ParameterBit::PointerBit),
               properties.isPointer) |
      flagWhen(static_cast<std::size_t>(ParameterBit::LValueReferenceBit),
               properties.isLValueReference) |
      flagWhen(static_cast<std::size_t>(ParameterBit::RValueReferenceBit),
               properties.isRValueReference) |
      flagWhen(static_cast<std::size_t>(ParameterBit::ForwardingReferenceBit),
               properties.isForwardingReference) |
      flagWhen(static_cast<std::size_t>(ParameterBit::ConstBit),
               properties.isConst) |
      flagWhen(static_cast<std::size_t>(ParameterBit::PackBit),
               properties.isPack));
}

constexpr SymbolProperties symbolProperties(std::uint32_t flags) {
  return {
      .access = accessName(flags),
      .isDefinition = hasBit(flags, DefinitionBit),
      .isImplicit = hasBit(flags, ImplicitBit),
      .isStatic = hasBit(flags, StaticBit),
      .isVirtual = hasBit(flags, VirtualBit),
      .isConst = hasBit(flags, ConstBit),
      .isInline = hasBit(flags, InlineBit),
      .isPure = hasBit(flags, PureBit),
      .refQualifier = refQualifierName(flags),
      .isOverride = hasBit(flags, OverrideBit),
      .hasInternalLinkage = hasBit(flags, InternalLinkageBit),
      .isExternal = hasBit(flags, ExternalBit),
      .isVariadic = hasBit(flags, VariadicBit),
      .isDeleted = hasBit(flags, DeletedBit),
      .isDefaulted = hasBit(flags, DefaultedBit),
      .isExplicit = hasBit(flags, ExplicitBit),
      .isFinal = hasBit(flags, FinalBit),
      .isAbstract = hasBit(flags, AbstractBit),
      .isPolymorphic = hasBit(flags, PolymorphicBit),
      .hasExternStorage = hasBit(flags, ExternStorageBit),
      .constantEvaluation = constantEvaluationName(flags),
      .isNoexcept = hasBit(flags, NoexceptBit),
  };
}

constexpr std::uint32_t symbolFlags(SymbolProperties properties) {
  return accessFlags(properties.access) |
         flagWhen(DefinitionBit, properties.isDefinition) |
         flagWhen(ImplicitBit, properties.isImplicit) |
         flagWhen(StaticBit, properties.isStatic) |
         flagWhen(VirtualBit, properties.isVirtual) |
         flagWhen(ConstBit, properties.isConst) |
         flagWhen(InlineBit, properties.isInline) |
         flagWhen(PureBit, properties.isPure) |
         refQualifierFlags(properties.refQualifier) |
         flagWhen(OverrideBit, properties.isOverride) |
         flagWhen(InternalLinkageBit, properties.hasInternalLinkage) |
         flagWhen(ExternalBit, properties.isExternal) |
         flagWhen(VariadicBit, properties.isVariadic) |
         flagWhen(DeletedBit, properties.isDeleted) |
         flagWhen(DefaultedBit, properties.isDefaulted) |
         flagWhen(ExplicitBit, properties.isExplicit) |
         flagWhen(FinalBit, properties.isFinal) |
         flagWhen(AbstractBit, properties.isAbstract) |
         flagWhen(PolymorphicBit, properties.isPolymorphic) |
         flagWhen(ExternStorageBit, properties.hasExternStorage) |
         constantEvaluationFlags(properties.constantEvaluation) |
         flagWhen(NoexceptBit, properties.isNoexcept);
}

constexpr RelationProperties relationProperties(std::uint16_t flags) {
  return {
      .access = accessName(flags),
      .isVirtualBase = hasBit(flags, VirtualBaseBit),
      .isImplicit = hasBit(flags, ImplicitEdgeBit),
      .isLexical = hasBit(flags, LexicalBit),
  };
}

constexpr std::uint16_t relationFlags(RelationProperties properties) {
  return static_cast<std::uint16_t>(
      accessFlags(properties.access) |
      flagWhen(VirtualBaseBit, properties.isVirtualBase) |
      flagWhen(ImplicitEdgeBit, properties.isImplicit) |
      flagWhen(LexicalBit, properties.isLexical));
}

} // namespace facts::storage

#endif // FACTS_TOOL_STORAGE_SEMANTICPROPERTIES_H
