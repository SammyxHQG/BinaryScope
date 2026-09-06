#pragma once
#include "BinaryFile.h"
#include "HashCalculator.h"
#include "analysis/StringExtractor.h"
#include "pe/PEHeaders.h"
#include <memory>
namespace bs {
struct AnalysisResult {
    std::shared_ptr<const BinaryFile> file;
    PEImage pe;
    Hashes hashes;
    StringIndex strings;
    QString error;
};
} // namespace bs
