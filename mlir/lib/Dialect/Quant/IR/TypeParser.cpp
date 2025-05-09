//===- TypeParser.h - Quantization Type Parser ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "mlir/Dialect/Quant/QuantOps.h"
#include "mlir/Dialect/Quant/QuantTypes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/Types.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;
using namespace quant;

static Type parseStorageType(DialectAsmParser &parser, bool &isSigned) {
  auto typeLoc = parser.getCurrentLocation();
  Type type;

  // Parse storage type (alpha_ident, integer_literal).
  StringRef identifier;
  unsigned storageTypeWidth = 0;
  OptionalParseResult result = parser.parseOptionalType(type);
  if (result.has_value()) {
    if (!succeeded(*result))
      return nullptr;
    if (auto intType = llvm::dyn_cast<IntegerType>(type)) {
      isSigned = !intType.isUnsigned();
      storageTypeWidth = intType.getWidth();
    } else if (llvm::dyn_cast<Float8E5M2Type>(type) ||
               llvm::dyn_cast<Float8E4M3FNType>(type)) {
      storageTypeWidth = 8;
      isSigned = true;
    } else {
      parser.emitError(typeLoc, "illegal quantized storage type alias");
      return nullptr;
    }
  } else if (succeeded(parser.parseKeyword(&identifier))) {
    // Otherwise, this must be an unsigned integer (`u` integer-literal)
    if (identifier.consume_front("u")) {
      if (identifier.getAsInteger(10, storageTypeWidth)) {
        parser.emitError(typeLoc, "expected storage type width");
        return nullptr;
      }
      isSigned = false;
      type = parser.getBuilder().getIntegerType(storageTypeWidth);

    } else {
      parser.emitError(typeLoc, "illegal quantized storage type alias");
      return nullptr;
    }

  } else {
    return nullptr;
  }

  if (storageTypeWidth == 0 ||
      storageTypeWidth > QuantizedType::MaxStorageBits) {
    parser.emitError(typeLoc, "illegal storage type size: ")
        << storageTypeWidth;
    return nullptr;
  }

  return type;
}

static Type parseQuantileType(DialectAsmParser &parser) {
  auto typeLoc = parser.getCurrentLocation();
  Type type;

  // Parse storage type (alpha_ident, integer_literal).
  StringRef identifier;
  unsigned storageTypeWidth = 0;
  OptionalParseResult result = parser.parseOptionalType(type);
  if (result.has_value()) {
    if (!succeeded(*result))
      return nullptr;

    if (!type.isa<IntegerType>() && !type.isa<FloatType>()) {
      parser.emitError(typeLoc, "illegal quantile type alias");
      return nullptr;
    }
  } else if (succeeded(parser.parseKeyword(&identifier))) {
    // Otherwise, this must be an unsigned integer (`u` integer-literal)
    if (identifier.consume_front("u")) {
      if (identifier.getAsInteger(10, storageTypeWidth)) {
        parser.emitError(typeLoc, "expected quantile type width");
        return nullptr;
      }
      constexpr bool isSigned = false;
      type = parser.getBuilder().getIntegerType(storageTypeWidth, isSigned);

    } else {
      parser.emitError(typeLoc, "illegal quantile type alias");
      return nullptr;
    }
  } else {
    return nullptr;
  }

  return type;
}

static ParseResult
checkStorageRange(DialectAsmParser &parser, int64_t storageTypeMin,
                  int64_t storageTypeMax, int64_t defaultStorageTypeMin,
                  int64_t defaultStorageTypeMax, SMLoc minLoc, SMLoc maxLoc) {
  if (storageTypeMin < defaultStorageTypeMin) {
    return parser.emitError(minLoc, "illegal storage type minimum: ")
           << storageTypeMin;
  }
  if (storageTypeMax > defaultStorageTypeMax) {
    return parser.emitError(maxLoc, "illegal storage type maximum: ")
           << storageTypeMax;
  }
  return success();
}

static ParseResult parseStorageRange(DialectAsmParser &parser, Type storageType,
                                     bool isSigned, int64_t &storageTypeMin,
                                     int64_t &storageTypeMax) {
  int64_t defaultMin, defaultMax;
  if (storageType.isa<IntegerType>()) {
    const auto width = llvm::dyn_cast<IntegerType>(storageType).getWidth();
    defaultMin = QuantizedType::getDefaultMinimumForInteger(isSigned, width);
    defaultMax = QuantizedType::getDefaultMaximumForInteger(isSigned, width);
  } else if (storageType.isa<Float8E5M2Type>()) {
    defaultMin = QuantizedType::getDefaultMinimumForF8E5M2();
    defaultMax = QuantizedType::getDefaultMaximumForF8E5M2();
  } else if (storageType.isa<Float8E4M3FNType>()) {
    defaultMin = QuantizedType::getDefaultMinimumForF8E4M3FN();
    defaultMax = QuantizedType::getDefaultMaximumForF8E4M3FN();
  } else {
    defaultMin = std::numeric_limits<int64_t>::max();
    defaultMax = std::numeric_limits<int64_t>::min();
  }

  if (failed(parser.parseOptionalLess())) {
    storageTypeMin = defaultMin;
    storageTypeMax = defaultMax;
    return success();
  }

  // Explicit storage min and storage max.
  // F8 min and max values are integers, so parseInteger() is used.
  SMLoc minLoc = parser.getCurrentLocation(), maxLoc;
  if (parser.parseInteger(storageTypeMin) || parser.parseColon() ||
      parser.getCurrentLocation(&maxLoc) ||
      parser.parseInteger(storageTypeMax) || parser.parseGreater())
    return failure();

  return checkStorageRange(parser, storageTypeMin, storageTypeMax, defaultMin,
                           defaultMax, minLoc, maxLoc);
}

static FloatType parseExpressedTypeAndRange(DialectAsmParser &parser,
                                            double &min, double &max) {
  auto typeLoc = parser.getCurrentLocation();
  FloatType type;

  if (failed(parser.parseType(type))) {
    parser.emitError(typeLoc, "expecting float expressed type");
    return nullptr;
  }

  // Calibrated min and max values.
  if (parser.parseLess() || parser.parseFloat(min) || parser.parseColon() ||
      parser.parseFloat(max) || parser.parseGreater()) {
    parser.emitError(typeLoc, "calibrated values must be present");
    return nullptr;
  }
  return type;
}

/// Parses an AnyQuantizedType.
///
///   any ::= `any<` storage-spec (expressed-type-spec)?`>`
///   storage-spec ::= storage-type (`<` storage-range `>`)?
///   storage-range ::= integer-literal `:` integer-literal
///   storage-type ::= (`i` | `u`) integer-literal
///   expressed-type-spec ::= `:` `f` integer-literal
static Type parseAnyType(DialectAsmParser &parser) {
  Type storageType;
  FloatType expressedType;
  unsigned typeFlags = 0;
  int64_t storageTypeMin;
  int64_t storageTypeMax;

  // Type specification.
  if (parser.parseLess())
    return nullptr;

  // Storage type.
  bool isSigned = false;
  storageType = parseStorageType(parser, isSigned);
  if (!storageType) {
    return nullptr;
  }
  if (isSigned) {
    typeFlags |= QuantizationFlags::Signed;
  }

  // Storage type range.
  if (parseStorageRange(parser, storageType, isSigned, storageTypeMin,
                        storageTypeMax)) {
    return nullptr;
  }

  // Optional expressed type.
  if (succeeded(parser.parseOptionalColon())) {
    if (parser.parseType(expressedType)) {
      return nullptr;
    }
  }

  if (parser.parseGreater()) {
    return nullptr;
  }

  return parser.getChecked<AnyQuantizedType>(
      typeFlags, storageType, expressedType, storageTypeMin, storageTypeMax);
}

static ParseResult parseQuantParams(DialectAsmParser &parser, double &scale,
                                    int64_t &zeroPoint) {
  // scale[:zeroPoint]?
  // scale.
  if (parser.parseFloat(scale)) {
    return failure();
  }

  // zero point.
  zeroPoint = 0;
  if (failed(parser.parseOptionalColon())) {
    // Default zero point.
    return success();
  }

  return parser.parseInteger(zeroPoint);
}

/// Parses a UniformQuantizedType or a QuantileQuantizedType.
///
///   uniform_type ::= uniform_per_layer
///                  | uniform_per_axis
///   uniform_per_layer ::= `uniform<` storage-spec expressed-type-spec
///                          `,` scale-zero `>`
///   uniform_per_axis ::= `uniform<` storage-spec expressed-type-spec
///                        axis-spec `,` scale-zero-list `>`
///   storage-spec ::= storage-type (`<` storage-range `>`)?
///   storage-range ::= integer-literal `:` integer-literal
///   storage-type ::= (`i` | `u`) integer-literal
///   expressed-type-spec ::= `:` `f` integer-literal
///   axis-spec ::= `:` integer-literal
///   scale-zero ::= float-literal `:` integer-literal
///   scale-zero-list ::= `{` scale-zero (`,` scale-zero)* `}`
///
///   quantile_type ::= quantile_per_layer
///                   | quantile_per_axis
///   quantile_per_layer ::= `quantile<` storage-spec quantile-type-spec
///                           expressed-type-spec `,` quantiles-list `,`
///                           scale-zero `>`
///   quantile_per_axis ::= `quantile<` storage-spec quantile-type-spec
///                          expressed-type-spec axis-spec `,` quantiles-list
///                          scale-zero-list `>`
///   storage-spec ::= storage-type (`<` storage-range `>`)?
///   storage-range ::= integer-literal `:` integer-literal
///   storage-type ::= (`i` | `u`) integer-literal
///   quantile-type-spec ::= `:` ((`i` | `u` | `f`) integer-literal | `f8E5M2` |
///                          `f8E4M3FN`)
///   expressed-type-spec ::= `:` `f` integer-literal axis-spec ::=
///   `:` integer-literal quantiles-list ::= `{` quantile (`,` quantile)* `}`
///   scale-zero ::= `:` float-literal `:` integer-literal
///   scale-zero-list ::= `:` `{` scale-zero (`,` scale-zero)* `}`
static Type parseUniformType(DialectAsmParser &parser, bool isQuantile) {
  Type storageType;
  Type quantileType;
  FloatType expressedType;
  unsigned typeFlags = 0;
  int64_t storageTypeMin;
  int64_t storageTypeMax;
  bool isPerAxis = false;
  int32_t quantizedDimension;
  SmallVector<double, 1> quantiles;
  SmallVector<double, 1> scales;
  SmallVector<int64_t, 1> zeroPoints;

  // Type specification.
  if (parser.parseLess()) {
    return nullptr;
  }

  // Storage type.
  bool isSigned = false;
  storageType = parseStorageType(parser, isSigned);
  if (!storageType) {
    return nullptr;
  }
  if (isSigned) {
    typeFlags |= QuantizationFlags::Signed;
  }

  // Storage type range.
  if (parseStorageRange(parser, storageType, isSigned, storageTypeMin,
                        storageTypeMax)) {
    return nullptr;
  }

  // quantile type.
  if (isQuantile) {
    if (parser.parseColon()) {
      return nullptr;
    }
    quantileType = parseQuantileType(parser);
    if (!quantileType) {
      return nullptr;
    }
  }

  // Expressed type.
  if (parser.parseColon() || parser.parseType(expressedType)) {
    return nullptr;
  }

  // Optionally parse quantized dimension for per-axis quantization.
  if (succeeded(parser.parseOptionalColon())) {
    if (parser.parseInteger(quantizedDimension))
      return nullptr;
    isPerAxis = true;
  }

  // Comma leading into range_spec.
  if (parser.parseComma()) {
    return nullptr;
  }

  // Quantile list
  if (isQuantile) {
    if (parser.parseLBrace()) {
      return nullptr;
    }

    do {
      quantiles.emplace_back();
      if (parser.parseFloat(quantiles.back())) {
        return nullptr;
      }
    } while (succeeded(parser.parseOptionalComma()));

    if (parser.parseRBrace()) {
      return nullptr;
    }

    if (parser.parseColon()) {
      return nullptr;
    }
  }

  // Parameter specification.
  // For per-axis, ranges are in a {} delimitted list.
  if (isPerAxis) {
    if (parser.parseLBrace()) {
      return nullptr;
    }
  }

  // Parse scales/zeroPoints.
  SMLoc scaleZPLoc = parser.getCurrentLocation();
  do {
    scales.resize(scales.size() + 1);
    zeroPoints.resize(zeroPoints.size() + 1);
    if (parseQuantParams(parser, scales.back(), zeroPoints.back())) {
      return nullptr;
    }
  } while (isPerAxis && succeeded(parser.parseOptionalComma()));

  if (isPerAxis) {
    if (parser.parseRBrace()) {
      return nullptr;
    }
  }

  if (parser.parseGreater()) {
    return nullptr;
  }

  if (!isPerAxis && scales.size() > 1) {
    return (parser.emitError(scaleZPLoc,
                             "multiple scales/zeroPoints provided, but "
                             "quantizedDimension wasn't specified"),
            nullptr);
  }

  if (isQuantile) {
    ArrayRef<double> quantilesRef(quantiles.begin(), quantiles.end());

    if (isPerAxis) {
      ArrayRef<double> scalesRef(scales.begin(), scales.end());
      ArrayRef<int64_t> zeroPointsRef(zeroPoints.begin(), zeroPoints.end());

      return parser.getChecked<QuantileQuantizedPerAxisType>(
          typeFlags, storageType, quantileType, expressedType, quantilesRef,
          scalesRef, zeroPointsRef, quantizedDimension, storageTypeMin,
          storageTypeMax);
    }

    return parser.getChecked<QuantileQuantizedType>(
        typeFlags, storageType, quantileType, expressedType, quantilesRef,
        scales.front(), zeroPoints.front(), storageTypeMin, storageTypeMax);
  }

  if (isPerAxis) {
    ArrayRef<double> scalesRef(scales.begin(), scales.end());
    ArrayRef<int64_t> zeroPointsRef(zeroPoints.begin(), zeroPoints.end());
    return parser.getChecked<UniformQuantizedPerAxisType>(
        typeFlags, storageType, expressedType, scalesRef, zeroPointsRef,
        quantizedDimension, storageTypeMin, storageTypeMax);
  }

  return parser.getChecked<UniformQuantizedType>(
      typeFlags, storageType, expressedType, scales.front(), zeroPoints.front(),
      storageTypeMin, storageTypeMax);
}

/// Parses an CalibratedQuantizedType.
///
///   calibrated ::= `calibrated<` expressed-spec `>`
///   expressed-spec ::= expressed-type `<` calibrated-range `>`
///   expressed-type ::= `f` integer-literal
///   calibrated-range ::= float-literal `:` float-literal
static Type parseCalibratedType(DialectAsmParser &parser) {
  FloatType expressedType;
  double min;
  double max;

  // Type specification.
  if (parser.parseLess())
    return nullptr;

  // Expressed type.
  expressedType = parseExpressedTypeAndRange(parser, min, max);
  if (!expressedType) {
    return nullptr;
  }

  if (parser.parseGreater()) {
    return nullptr;
  }

  return parser.getChecked<CalibratedQuantizedType>(expressedType, min, max);
}

/// Parse a type registered to this dialect.
Type QuantizationDialect::parseType(DialectAsmParser &parser) const {

  // All types start with an identifier that we switch on.
  StringRef typeNameSpelling;
  if (failed(parser.parseKeyword(&typeNameSpelling)))
    return nullptr;

  if (typeNameSpelling == "uniform")
    return parseUniformType(parser, false);
  if (typeNameSpelling == "quantile")
    return parseUniformType(parser, true);
  if (typeNameSpelling == "any")
    return parseAnyType(parser);
  if (typeNameSpelling == "calibrated")
    return parseCalibratedType(parser);

  parser.emitError(parser.getNameLoc(),
                   "unknown quantized type " + typeNameSpelling);
  return nullptr;
}

static void printStorageType(QuantizedType type, DialectAsmPrinter &out) {
  // storage type
  unsigned storageWidth = type.getStorageTypeIntegralWidth();
  bool isSigned = type.isSigned();
  if (type.getStorageType().isa<Float8E5M2Type>()) {
    out << "f8E5M2";
  } else if (type.getStorageType().isa<Float8E4M3FNType>()) {
    out << "f8E4M3FN";
  } else if (isSigned) {
    out << "i" << storageWidth;
  } else {
    out << "u" << storageWidth;
  }

  // storageTypeMin and storageTypeMax if not default.
  int64_t defaultMin =
      type.getStorageType().isa<IntegerType>()
          ? QuantizedType::getDefaultMinimumForInteger(isSigned, storageWidth)
          : type.getStorageType().isa<Float8E5M2Type>()
                ? QuantizedType::getDefaultMinimumForF8E5M2()
                : type.getStorageType().isa<Float8E4M3FNType>()
                      ? QuantizedType::getDefaultMinimumForF8E4M3FN()
                      : std::numeric_limits<int64_t>::max();

  int64_t defaultMax =
      type.getStorageType().isa<IntegerType>()
          ? QuantizedType::getDefaultMaximumForInteger(isSigned, storageWidth)
          : type.getStorageType().isa<Float8E5M2Type>()
                ? QuantizedType::getDefaultMaximumForF8E5M2()
                : type.getStorageType().isa<Float8E4M3FNType>()
                      ? QuantizedType::getDefaultMaximumForF8E4M3FN()
                      : std::numeric_limits<int64_t>::min();

  if (defaultMin != type.getStorageTypeMin() ||
      defaultMax != type.getStorageTypeMax()) {
    out << "<" << type.getStorageTypeMin() << ":" << type.getStorageTypeMax()
        << ">";
  }
}

static void printQuantileType(Type quantileType, DialectAsmPrinter &out) {
  if (auto intType = llvm::dyn_cast<IntegerType>(quantileType)) {
    const unsigned storageTypeWidth = intType.getWidth();
    if (intType.isUnsigned()) {
      out << ":u" << storageTypeWidth;
    } else {
      out << ":i" << storageTypeWidth;
    }
  } else if (quantileType.isa<Float8E5M2Type>()) {
    out << ":f8E5M2";
  } else if (quantileType.isa<Float8E4M3FNType>()) {
    out << ":f8E4M3FN";
  } else {
    // Float types
    out << ":" << quantileType;
  }
}

static void printQuantParams(double scale, int64_t zeroPoint,
                             DialectAsmPrinter &out) {
  out << scale;
  if (zeroPoint != 0) {
    out << ":" << zeroPoint;
  }
}

/// Helper that prints a AnyQuantizedType.
static void printAnyQuantizedType(AnyQuantizedType type,
                                  DialectAsmPrinter &out) {
  out << "any<";
  printStorageType(type, out);
  if (Type expressedType = type.getExpressedType()) {
    out << ":" << expressedType;
  }
  out << ">";
}

/// Helper that prints a UniformQuantizedType.
static void printUniformQuantizedType(UniformQuantizedType type,
                                      DialectAsmPrinter &out) {
  out << "uniform<";
  printStorageType(type, out);
  out << ":" << type.getExpressedType() << ", ";

  // scheme specific parameters
  printQuantParams(type.getScale(), type.getZeroPoint(), out);
  out << ">";
}

/// Helper that prints a UniformQuantizedPerAxisType.
static void printUniformQuantizedPerAxisType(UniformQuantizedPerAxisType type,
                                             DialectAsmPrinter &out) {
  out << "uniform<";
  printStorageType(type, out);
  out << ":" << type.getExpressedType() << ":";
  out << type.getQuantizedDimension();
  out << ", ";

  // scheme specific parameters
  ArrayRef<double> scales = type.getScales();
  ArrayRef<int64_t> zeroPoints = type.getZeroPoints();
  out << "{";
  llvm::interleave(
      llvm::seq<size_t>(0, scales.size()), out,
      [&](size_t index) {
        printQuantParams(scales[index], zeroPoints[index], out);
      },
      ",");
  out << "}>";
}

/// Helper that prints a QuantileQuantizedType.
static void printQuantileQuantizedType(QuantileQuantizedType type,
                                       DialectAsmPrinter &out) {
  out << "quantile<";
  printStorageType(type, out);
  printQuantileType(type.getQuantileType(), out);
  out << ":" << type.getExpressedType() << ", ";

  // scheme specific parameters
  ArrayRef<double> quantiles = type.getQuantiles();
  out << "{";
  llvm::interleave(
      llvm::seq<size_t>(0, quantiles.size()), out,
      [&](size_t index) { out << quantiles[index]; }, ",");
  out << "}:";

  printQuantParams(type.getScale(), type.getZeroPoint(), out);
  out << ">";
}

/// Helper that prints a QuantileQuantizedPerAxisType.
static void printQuantileQuantizedPerAxisType(QuantileQuantizedPerAxisType type,
                                              DialectAsmPrinter &out) {
  out << "quantile<";
  printStorageType(type, out);
  printQuantileType(type.getQuantileType(), out);
  out << ":" << type.getExpressedType() << ":";
  out << type.getQuantizedDimension();
  out << ", ";

  // scheme specific parameters
  ArrayRef<double> quantiles = type.getQuantiles();
  out << "{";
  llvm::interleave(
      llvm::seq<size_t>(0, quantiles.size()), out,
      [&](size_t index) { out << quantiles[index]; }, ",");
  out << "}:";

  ArrayRef<double> scales = type.getScales();
  ArrayRef<int64_t> zeroPoints = type.getZeroPoints();
  out << "{";
  llvm::interleave(
      llvm::seq<size_t>(0, scales.size()), out,
      [&](size_t index) {
        printQuantParams(scales[index], zeroPoints[index], out);
      },
      ",");
  out << "}>";
}

/// Helper that prints a CalibratedQuantizedType.
static void printCalibratedQuantizedType(CalibratedQuantizedType type,
                                         DialectAsmPrinter &out) {
  out << "calibrated<" << type.getExpressedType();
  out << "<" << type.getMin() << ":" << type.getMax() << ">";
  out << ">";
}

/// Print a type registered to this dialect.
void QuantizationDialect::printType(Type type, DialectAsmPrinter &os) const {
  if (auto anyType = llvm::dyn_cast<AnyQuantizedType>(type))
    printAnyQuantizedType(anyType, os);
  else if (auto uniformType = llvm::dyn_cast<QuantileQuantizedType>(type))
    printQuantileQuantizedType(uniformType, os);
  else if (auto perAxisType =
               llvm::dyn_cast<QuantileQuantizedPerAxisType>(type))
    printQuantileQuantizedPerAxisType(perAxisType, os);
  else if (auto uniformType = llvm::dyn_cast<UniformQuantizedType>(type))
    printUniformQuantizedType(uniformType, os);
  else if (auto perAxisType = llvm::dyn_cast<UniformQuantizedPerAxisType>(type))
    printUniformQuantizedPerAxisType(perAxisType, os);
  else if (auto calibratedType = llvm::dyn_cast<CalibratedQuantizedType>(type))
    printCalibratedQuantizedType(calibratedType, os);
  else
    llvm_unreachable("Unhandled quantized type");
}
