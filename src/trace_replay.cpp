#include "trace_replay.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>

static void ltrim(std::string& s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                         [](unsigned char c){ return !std::isspace(c); }));
}
static void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c){ return !std::isspace(c); }).base(),
            s.end());
}

std::vector<TransferJob> loadTraceCSV(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open trace file: " + path);

    std::vector<TransferJob> jobs;
    std::string line;
    int line_no = 0;

    while (std::getline(f, line)) {
        ++line_no;
        ltrim(line); rtrim(line);
        if (line.empty()) continue;

        // Detect header: first field must parse as an integer id.
        {
            std::istringstream probe(line);
            std::string first;
            std::getline(probe, first, ',');
            ltrim(first); rtrim(first);
            try {
                size_t pos = 0;
                (void)std::stoi(first, &pos);
                if (pos != first.size()) continue;
            } catch (...) {
                continue;
            }
        }

        std::istringstream ss(line);
        std::string tok;
        std::vector<std::string> fields;
        while (std::getline(ss, tok, ',')) {
            ltrim(tok); rtrim(tok);
            fields.push_back(tok);
        }
        if (fields.size() < 4) {
            std::cerr << "trace_replay: skipping malformed line " << line_no << "\n";
            continue;
        }

        TransferJob j;
        j.id                = std::stoi(fields[0]);
        j.size_mb           = std::stod(fields[1]);
        j.arrival_ms        = std::stod(fields[2]);
        j.priority          = std::stoi(fields[3]);
        j.user_id           = (fields.size() >= 5) ? std::stoi(fields[4]) : 1;
        j.direction         = TransferDirection::DOWNLOAD;
        j.start_ms          = j.arrival_ms;
        j.first_start_ms    = 0.0;
        j.remaining_size_mb = j.size_mb;
        jobs.push_back(j);
    }
    return jobs;
}

#ifdef HAVE_LIBPCAP
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>
#include <sstream>

struct FlowKey {
    uint32_t src_ip, dst_ip;
    uint16_t src_port, dst_port;
    uint8_t  proto;
    bool operator==(const FlowKey& o) const {
        return src_ip==o.src_ip && dst_ip==o.dst_ip &&
               src_port==o.src_port && dst_port==o.dst_port &&
               proto==o.proto;
    }
};
struct FlowKeyHash {
    size_t operator()(const FlowKey& k) const {
        size_t h = k.src_ip;
        h ^= (size_t)k.dst_ip  << 16;
        h ^= (size_t)k.src_port<< 32;
        h ^= (size_t)k.dst_port<< 48;
        h ^= k.proto;
        return h;
    }
};

struct FlowData {
    double first_ts_ms = 0.0;
    double last_ts_ms  = 0.0;
    double total_bytes = 0.0;
};

std::vector<TransferJob> loadTracePCAP(const std::string& path) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* pcap = pcap_open_offline(path.c_str(), errbuf);
    if (!pcap) throw std::runtime_error("Cannot open PCAP: " + std::string(errbuf));

    std::unordered_map<FlowKey, FlowData, FlowKeyHash> flows;
    struct pcap_pkthdr* hdr;
    const u_char* data;
    int res;

    while ((res = pcap_next_ex(pcap, &hdr, &data)) == 1) {
        double ts_ms = hdr->ts.tv_sec * 1000.0 + hdr->ts.tv_usec / 1000.0;
        const struct ether_header* eth = reinterpret_cast<const struct ether_header*>(data);
        if (ntohs(eth->ether_type) != ETHERTYPE_IP) continue;

        const struct ip* iph = reinterpret_cast<const struct ip*>(data + sizeof(ether_header));
        FlowKey key{};
        key.src_ip = ntohl(iph->ip_src.s_addr);
        key.dst_ip = ntohl(iph->ip_dst.s_addr);
        key.proto  = iph->ip_p;

        int ip_hlen = iph->ip_hl * 4;
        const u_char* transport = data + sizeof(ether_header) + ip_hlen;

        if (iph->ip_p == IPPROTO_TCP) {
            const struct tcphdr* th = reinterpret_cast<const struct tcphdr*>(transport);
            key.src_port = ntohs(th->th_sport);
            key.dst_port = ntohs(th->th_dport);
        } else if (iph->ip_p == IPPROTO_UDP) {
            const struct udphdr* uh = reinterpret_cast<const struct udphdr*>(transport);
            key.src_port = ntohs(uh->uh_sport);
            key.dst_port = ntohs(uh->uh_dport);
        }

        auto& flow = flows[key];
        if (flow.total_bytes == 0.0) flow.first_ts_ms = ts_ms;
        flow.last_ts_ms   = ts_ms;
        flow.total_bytes += hdr->caplen;
    }
    pcap_close(pcap);

    std::vector<TransferJob> jobs;
    jobs.reserve(flows.size());
    int idx = 1;
    for (auto& [key, flow] : flows) {
        TransferJob j;
        j.id                = idx++;
        j.user_id           = 1 + static_cast<int>(key.src_port % 10);
        j.size_mb           = flow.total_bytes / (1024.0 * 1024.0);
        j.arrival_ms        = flow.first_ts_ms;
        j.start_ms          = flow.first_ts_ms;
        j.direction         = TransferDirection::DOWNLOAD;
        j.priority          = 5;
        j.first_start_ms    = 0.0;
        j.remaining_size_mb = j.size_mb;
        jobs.push_back(j);
    }
    // Sort by arrival time.
    std::sort(jobs.begin(), jobs.end(),
              [](const TransferJob& a, const TransferJob& b){
                  return a.arrival_ms < b.arrival_ms; });
    return jobs;
}
#endif
