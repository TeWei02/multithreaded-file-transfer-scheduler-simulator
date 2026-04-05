#ifndef CSV_UTILS_H
#define CSV_UTILS_H

#include <cstddef>
#include <string>
#include <vector>

#include "request.h"

namespace simulator {

std::vector<Request> ReadRequestsCsv(const std::string& path);
void WriteRequestResultsCsv(const std::string& path, const std::vector<RequestResult>& rows);
void WriteSummaryCsv(const std::string& path, const std::vector<MetricsSummary>& rows);
void GenerateSampleRequestsCsv(const std::string& path, std::size_t n, unsigned int seed);

}  // namespace simulator

#endif
