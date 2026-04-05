#pragma once
// Extension 5: Live traffic trace replay.
// Supports CSV traces and optionally PCAP files (requires libpcap).

#include "request.h"
#include <string>
#include <vector>

// Load jobs from a CSV file.
// Expected columns: flow_id,size_mb,arrival_ms,priority
// The first line whose first field is non-numeric is treated as a header.
std::vector<TransferJob> loadTraceCSV(const std::string& path);

#ifdef HAVE_LIBPCAP
// Load jobs from a PCAP file using libpcap.
// Groups packets by 5-tuple (src_ip, dst_ip, protocol, src_port, dst_port)
// and converts each flow into a TransferJob.
std::vector<TransferJob> loadTracePCAP(const std::string& path);
#endif
