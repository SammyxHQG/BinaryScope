#include "AnalysisService.h"
#include "analysis/EntropyAnalyzer.h"
#include "pe/PEParser.h"
namespace bs {
void AnalysisService::analyze(QPromise<std::shared_ptr<AnalysisResult>>& promise, const QString& path) {
    auto result = std::make_shared<AnalysisResult>();
    try {
        promise.setProgressRange(0, 5);
        promise.setProgressValueAndText(0, "Reading file snapshot...");
        result->file = std::make_shared<BinaryFile>(std::filesystem::path(path.toStdWString()));
        promise.setProgressValueAndText(1, "Parsing PE headers and symbols...");
        result->pe = PEParser::parse(result->file->bytes());
        promise.setProgressValueAndText(2, "Calculating SHA-256 and MD5...");
        result->hashes = HashCalculator::calculate(result->file->bytes());
        promise.setProgressValueAndText(3, "Measuring section entropy...");
        for (auto& section : result->pe.sections)
            section.entropy = EntropyAnalyzer::calculate(
                result->file->bytes().subspan(section.rawSize ? section.rawOffset : 0, section.rawSize));
        promise.setProgressValueAndText(4, "Indexing ASCII and UTF-16 strings...");
        result->strings = StringExtractor::extract(result->file->bytes(), 2);
        promise.setProgressValueAndText(5, "Analysis complete");
    } catch (const std::exception& e) {
        result->error = QString::fromUtf8(e.what());
    }
    promise.addResult(std::move(result));
}
} // namespace bs
