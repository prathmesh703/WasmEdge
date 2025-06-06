// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2019-2024 Second State INC

#include "loader/loader.h"

namespace WasmEdge {
namespace Loader {

// Load binary and decode HeapType. See "include/loader/loader.h".
Expect<ValType> Loader::loadHeapType(TypeCode TC, ASTNodeAttr From) {
  if (auto Res = FMgr.readS33()) {
    if (*Res < 0) {
      if (*Res < -64) {
        // For checking the invalid s33 value which is larger than 1 byte.
        return logLoadError(ErrCode::Value::MalformedRefType,
                            FMgr.getLastOffset(), From);
      }
      TypeCode HTCode =
          static_cast<TypeCode>(static_cast<uint8_t>((*Res) & INT64_C(0x7F)));
      switch (HTCode) {
      case TypeCode::ExternRef:
        // For the ref.func instruction, the immediate changed to store the heap
        // type directly instead of the reference type after applying the
        // typed function reference proposal. Therefore the reference-types
        // proposal should be checked here.
        if (!Conf.hasProposal(Proposal::ReferenceTypes)) {
          return logNeedProposal(ErrCode::Value::MalformedElemType,
                                 Proposal::ReferenceTypes, FMgr.getLastOffset(),
                                 From);
        }
        [[fallthrough]];
      case TypeCode::FuncRef:
        return ValType(TC, HTCode);
      case TypeCode::NullFuncRef:
      case TypeCode::NullExternRef:
      case TypeCode::NullRef:
      case TypeCode::AnyRef:
      case TypeCode::EqRef:
      case TypeCode::I31Ref:
      case TypeCode::StructRef:
      case TypeCode::ArrayRef:
        if (!Conf.hasProposal(Proposal::GC)) {
          return logNeedProposal(ErrCode::Value::MalformedRefType, Proposal::GC,
                                 FMgr.getLastOffset(), From);
        }
        return ValType(TC, HTCode);
      case TypeCode::ExnRef:
        if (!Conf.hasProposal(Proposal::ExceptionHandling)) {
          return logNeedProposal(ErrCode::Value::MalformedValType,
                                 Proposal::ExceptionHandling,
                                 FMgr.getLastOffset(), From);
        }
        return ValType(TC, HTCode);
      default:
        return logLoadError(ErrCode::Value::MalformedRefType,
                            FMgr.getLastOffset(), From);
      }
    } else {
      // Type index case. Legal if the function reference proposal is enabled.
      if (!Conf.hasProposal(Proposal::FunctionReferences)) {
        return logNeedProposal(ErrCode::Value::MalformedRefType,
                               Proposal::FunctionReferences,
                               FMgr.getLastOffset(), From);
      }
      return ValType(TC, static_cast<uint32_t>(*Res));
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(), From);
  }
}

// Load binary and decode RefType. See "include/loader/loader.h".
Expect<ValType> Loader::loadRefType(ASTNodeAttr From) {
  if (auto Res = FMgr.readByte()) {
    // The error code is different when the reference-types proposal turned off.
    ErrCode::Value FailCode = Conf.hasProposal(Proposal::ReferenceTypes)
                                  ? ErrCode::Value::MalformedRefType
                                  : ErrCode::Value::MalformedElemType;
    TypeCode Code = static_cast<TypeCode>(*Res);
    switch (Code) {
    case TypeCode::ExternRef:
      if (!Conf.hasProposal(Proposal::ReferenceTypes)) {
        return logNeedProposal(FailCode, Proposal::ReferenceTypes,
                               FMgr.getLastOffset(), From);
      }
      [[fallthrough]];
    case TypeCode::FuncRef:
      // The FuncRef (0x70) is always allowed in the RefType even if the
      // reference-types proposal not enabled.
      return ValType(Code);
    case TypeCode::NullFuncRef:
    case TypeCode::NullExternRef:
    case TypeCode::NullRef:
    case TypeCode::AnyRef:
    case TypeCode::EqRef:
    case TypeCode::I31Ref:
    case TypeCode::StructRef:
    case TypeCode::ArrayRef:
      if (!Conf.hasProposal(Proposal::GC)) {
        return logNeedProposal(FailCode, Proposal::GC, FMgr.getLastOffset(),
                               From);
      }
      return ValType(Code);
    case TypeCode::Ref:
    case TypeCode::RefNull: {
      if (!Conf.hasProposal(Proposal::FunctionReferences)) {
        return logNeedProposal(FailCode, Proposal::FunctionReferences,
                               FMgr.getLastOffset(), From);
      }
      return loadHeapType(Code, From);
    }
    default:
      return logLoadError(FailCode, FMgr.getLastOffset(), From);
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(), From);
  }
}

// Load binary and decode ValType. See "include/loader/loader.h".
Expect<ValType> Loader::loadValType(ASTNodeAttr From, bool IsStorageType) {
  if (auto Res = FMgr.readByte()) {
    TypeCode Code = static_cast<TypeCode>(*Res);
    switch (Code) {
    case TypeCode::V128:
      if (!Conf.hasProposal(Proposal::SIMD)) {
        return logNeedProposal(ErrCode::Value::MalformedValType, Proposal::SIMD,
                               FMgr.getLastOffset(), From);
      }
      [[fallthrough]];
    case TypeCode::I32:
    case TypeCode::I64:
    case TypeCode::F32:
    case TypeCode::F64:
      return ValType(Code);
    case TypeCode::I8:
    case TypeCode::I16:
      if (!IsStorageType) {
        break;
      }
      if (!Conf.hasProposal(Proposal::GC)) {
        return logNeedProposal(ErrCode::Value::MalformedValType, Proposal::GC,
                               FMgr.getLastOffset(), From);
      }
      return ValType(Code);
    case TypeCode::FuncRef:
      if (!Conf.hasProposal(Proposal::ReferenceTypes) &&
          !Conf.hasProposal(Proposal::BulkMemoryOperations)) {
        return logNeedProposal(ErrCode::Value::MalformedElemType,
                               Proposal::ReferenceTypes, FMgr.getLastOffset(),
                               From);
      }
      return ValType(Code);
    case TypeCode::ExternRef:
      if (!Conf.hasProposal(Proposal::ReferenceTypes)) {
        return logNeedProposal(ErrCode::Value::MalformedElemType,
                               Proposal::ReferenceTypes, FMgr.getLastOffset(),
                               From);
      }
      return ValType(Code);
    case TypeCode::NullFuncRef:
    case TypeCode::NullExternRef:
    case TypeCode::NullRef:
    case TypeCode::AnyRef:
    case TypeCode::EqRef:
    case TypeCode::I31Ref:
    case TypeCode::StructRef:
    case TypeCode::ArrayRef:
      if (!Conf.hasProposal(Proposal::GC)) {
        return logNeedProposal(ErrCode::Value::MalformedValType, Proposal::GC,
                               FMgr.getLastOffset(), From);
      }
      return ValType(Code);
    case TypeCode::ExnRef:
      if (!Conf.hasProposal(Proposal::ExceptionHandling)) {
        return logNeedProposal(ErrCode::Value::MalformedValType,
                               Proposal::ExceptionHandling,
                               FMgr.getLastOffset(), From);
      }
      return ValType(Code);
    case TypeCode::Ref:
    case TypeCode::RefNull:
      if (!Conf.hasProposal(Proposal::FunctionReferences)) {
        return logNeedProposal(ErrCode::Value::MalformedValType,
                               Proposal::FunctionReferences,
                               FMgr.getLastOffset(), From);
      }
      return loadHeapType(Code, From);
    default:
      break;
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(), From);
  }
  return logLoadError(ErrCode::Value::MalformedValType, FMgr.getLastOffset(),
                      From);
}

Expect<ValMut> Loader::loadMutability(ASTNodeAttr From) {
  if (auto Res = FMgr.readByte()) {
    switch (static_cast<ValMut>(*Res)) {
    case ValMut::Const:
    case ValMut::Var:
      return static_cast<ValMut>(*Res);
    default:
      return logLoadError(ErrCode::Value::InvalidMut, FMgr.getLastOffset(),
                          From);
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(), From);
  }
}

Expect<void> Loader::loadFieldType(AST::FieldType &FType) {
  // The error code logging is handled.
  EXPECTED_TRY(auto Type, loadValType(ASTNodeAttr::Type_Rec, true));
  FType.setStorageType(Type);
  // The error code logging is handled.
  EXPECTED_TRY(auto Mut, loadMutability(ASTNodeAttr::Type_Rec));
  FType.setValMut(Mut);
  return {};
}

Expect<void> Loader::loadCompositeType(AST::CompositeType &CType) {
  if (auto CodeByte = FMgr.readByte()) {
    switch (static_cast<TypeCode>(*CodeByte)) {
    case TypeCode::Array: {
      AST::FieldType FType;
      EXPECTED_TRY(loadFieldType(FType));
      CType.setArrayType(std::move(FType));
      return {};
    }
    case TypeCode::Struct: {
      std::vector<AST::FieldType> FList;
      EXPECTED_TRY(loadVec<AST::SubType>(
          FList, [this](AST::FieldType &FType) -> Expect<void> {
            // The error code logging is handled.
            return loadFieldType(FType);
          }));
      CType.setStructType(std::move(FList));
      return {};
    }
    case TypeCode::Func: {
      AST::FunctionType FuncType;
      EXPECTED_TRY(loadType(FuncType));
      CType.setFunctionType(std::move(FuncType));
      return {};
    }
    default:
      return logLoadError(ErrCode::Value::IntegerTooLong, FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Rec);
    }
  } else {
    return logLoadError(CodeByte.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Rec);
  }
}

// Load binary to construct Limit node. See "include/loader/loader.h".
Expect<void> Loader::loadLimit(AST::Limit &Lim) {
  // Read limit.
  if (auto Res = FMgr.readByte()) {
    switch (static_cast<AST::Limit::LimitType>(*Res)) {
    case AST::Limit::LimitType::SharedNoMax:
    case AST::Limit::LimitType::Shared:
      if (!Conf.hasProposal(Proposal::Threads)) {
        return logLoadError(ErrCode::Value::IntegerTooLarge,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      }
      [[fallthrough]];
    case AST::Limit::LimitType::HasMin:
    case AST::Limit::LimitType::HasMinMax:
      Lim.setType(static_cast<AST::Limit::LimitType>(*Res));
      break;
    default:
      if (*Res == 0x80 || *Res == 0x81) {
        // LEB128 cases will fail.
        return logLoadError(ErrCode::Value::IntegerTooLong,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      } else {
        return logLoadError(ErrCode::Value::IntegerTooLarge,
                            FMgr.getLastOffset(), ASTNodeAttr::Type_Limit);
      }
    }
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Limit);
  }

  // Read min and max number.
  if (auto Res = FMgr.readU32()) {
    Lim.setMin(*Res);
    Lim.setMax(*Res);
  } else {
    return logLoadError(Res.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Limit);
  }
  if (Lim.hasMax()) {
    if (auto Res = FMgr.readU32()) {
      Lim.setMax(*Res);
    } else {
      return logLoadError(Res.error(), FMgr.getLastOffset(),
                          ASTNodeAttr::Type_Limit);
    }
  }
  return {};
}

// Load binary to construct SubType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::SubType &SType) {
  if (auto CodeByte = FMgr.peekByte()) {
    switch (static_cast<TypeCode>(*CodeByte)) {
    default:
      // Case: comptype.
      SType.setFinal(true);
      return loadCompositeType(SType.getCompositeType());
    case TypeCode::Sub:
      // Case: 0x50 vec(typeidx) comptype.
      SType.setFinal(false);
      break;
    case TypeCode::SubFinal:
      // Case: 0x4F vec(typeidx) comptype.
      SType.setFinal(true);
      break;
    }
    FMgr.readByte();
    EXPECTED_TRY(loadVec<AST::SubType>(
        SType.getSuperTypeIndices(), [this](uint32_t &Idx) -> Expect<void> {
          if (auto Num = FMgr.readU32()) {
            Idx = *Num;
          } else {
            return logLoadError(Num.error(), FMgr.getLastOffset(),
                                ASTNodeAttr::Type_Sub);
          }
          return {};
        }));
    return loadCompositeType(SType.getCompositeType());
  } else {
    return logLoadError(CodeByte.error(), FMgr.getLastOffset(),
                        ASTNodeAttr::Type_Rec);
  }
}

// Load binary to construct FunctionType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::FunctionType &FuncType) {
  // Read type of Func (0x60). Moved into the composite type.
  auto LoadValType = [this](ValType &VT) -> Expect<void> {
    // The error code logging is handled.
    EXPECTED_TRY(VT, loadValType(ASTNodeAttr::Type_Function));
    return {};
  };
  // Read vector of parameter types.
  EXPECTED_TRY(
      loadVec<AST::FunctionType>(FuncType.getParamTypes(), LoadValType));

  // Read vector of result types.
  EXPECTED_TRY(
      loadVec<AST::FunctionType>(FuncType.getReturnTypes(), LoadValType));
  return {};
}

// Load binary to construct MemoryType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::MemoryType &MemType) {
  // Read limit.
  return loadLimit(MemType.getLimit()).map_error([](auto E) {
    spdlog::error(ErrInfo::InfoAST(ASTNodeAttr::Type_Memory));
    return E;
  });
}

// Load binary to construct TableType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::TableType &TabType) {
  // Read reference type.
  // The AST node information is handled.
  EXPECTED_TRY(auto Type, loadRefType(ASTNodeAttr::Type_Table));
  TabType.setRefType(Type);

  // Read limit.
  return loadLimit(TabType.getLimit()).map_error([](auto E) {
    spdlog::error(ErrInfo::InfoAST(ASTNodeAttr::Type_Table));
    return E;
  });
}

// Load binary to construct GlobalType node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::GlobalType &GlobType) {
  // Read value type.
  // The AST node information is handled.
  EXPECTED_TRY(auto Type, loadValType(ASTNodeAttr::Type_Global));
  GlobType.setValType(Type);

  // Read mutability.
  // The AST node information is handled.
  EXPECTED_TRY(auto Mut, loadMutability(ASTNodeAttr::Type_Global));
  GlobType.setValMut(Mut);
  return {};
}

// Load binary to construct Tag node. See "include/loader/loader.h".
Expect<void> Loader::loadType(AST::TagType &TgType) {
  EXPECTED_TRY(auto B, FMgr.readByte().map_error([this](auto E) {
    spdlog::error(E);
    spdlog::error(ErrInfo::InfoLoading(FMgr.getLastOffset()));
    return E;
  }));

  // The preserved byte for future extension possibility for tag
  // It supports only 0x00 currently, which is for exception handling.
  if (unlikely(B != 0x00)) {
    spdlog::error(ErrCode::Value::ExpectedZeroByte);
    spdlog::error(ErrInfo::InfoLoading(FMgr.getLastOffset()));
    return Unexpect(ErrCode::Value::ExpectedZeroByte);
  }
  EXPECTED_TRY(auto Idx, FMgr.readU32().map_error([this](auto E) {
    spdlog::error(E);
    spdlog::error(ErrInfo::InfoLoading(FMgr.getLastOffset()));
    return E;
  }));
  TgType.setTypeIdx(Idx);
  return {};
}

} // namespace Loader
} // namespace WasmEdge
