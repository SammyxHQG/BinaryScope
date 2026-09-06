#pragma once
#include "AnalysisResult.h"
#include <QPromise>
namespace bs {
class AnalysisService {
  public:
    // Worker entry point. Errors are values so QtConcurrent cannot obscure their messages.
    static void analyze(QPromise<std::shared_ptr<AnalysisResult>>& promise, const QString& path);
};
} // namespace bs
