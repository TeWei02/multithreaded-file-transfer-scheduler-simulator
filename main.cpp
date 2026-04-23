#include "simulation.h"
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstring>

static void usage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " [OPTIONS]\n"
        << "\nScheduler options:\n"
        << "  --jobs <n>          Number of jobs (default: 20)\n"
        << "  --workers <n>       Worker threads (default: 4)\n"
        << "  --bandwidth <mbps>  Bandwidth in MB/s (default: 80)\n"
        << "  --policy <name>     fcfs|sjf|priority|rr|wfq|all (default: all)\n"
        << "  --quantum <ms>      Round Robin quantum in sim-ms (default: 50)\n"
        << "  --seed <n>          Random seed (default: 42)\n"
        << "  --speed <n>         Simulation speed factor (default: 100)\n"
        << "\nExtension options:\n"
        << "  --engine <name>     sim|socket (default: sim)\n"
        << "  --congestion        Enable AIMD congestion model\n"
        << "  --packet-level      Enable packet-level scheduling\n"
        << "  --packet-scheduler  drr|htb (default: drr)\n"
        << "  --packet-size <mb>  Packet size in MB (default: 1.0)\n"
        << "  --qos               Enable DiffServ QoS model\n"
        << "  --trace <file>      Replay jobs from CSV trace file\n"
        << "  --pcap <file>       Replay jobs from PCAP file (requires libpcap)\n"
        << "  --web [port]        Enable web dashboard (default port: 8080)\n"
        << "  --no-qmodel         Disable formal queuing model output\n";
}

int main(int argc, char* argv[]) {
    SimConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing argument for " << arg << "\n";
                std::exit(1);
            }
            return argv[++i];
        };
        auto nextInt = [&]() -> int {
            return std::stoi(next());
        };
        auto nextDouble = [&]() -> double {
            return std::stod(next());
        };

        if      (arg == "--jobs")             cfg.num_jobs        = nextInt();
        else if (arg == "--workers")          cfg.num_workers     = nextInt();
        else if (arg == "--bandwidth")        cfg.bandwidth       = nextDouble();
        else if (arg == "--policy")           cfg.policy_filter   = next();
        else if (arg == "--quantum")          cfg.rr_quantum_ms   = nextDouble();
        else if (arg == "--seed")             cfg.seed            = nextInt();
        else if (arg == "--speed")            cfg.speed_factor    = nextInt();
        else if (arg == "--engine")           cfg.engine          = next();
        else if (arg == "--congestion")       cfg.congestion      = true;
        else if (arg == "--packet-level")     cfg.packet_level    = true;
        else if (arg == "--packet-scheduler") cfg.pkt_scheduler   = next();
        else if (arg == "--packet-size")      cfg.packet_size_mb  = nextDouble();
        else if (arg == "--qos")              cfg.qos             = true;
        else if (arg == "--trace")            cfg.trace_file      = next();
        else if (arg == "--pcap")             cfg.pcap_file       = next();
        else if (arg == "--no-qmodel")        cfg.show_queueing_models = false;
        else if (arg == "--web") {
            cfg.web_dashboard = true;
            // Optional port argument
            if (i + 1 < argc && argv[i+1][0] != '-') {
                try {
                    cfg.web_port = std::stoi(argv[++i]);
                } catch (...) {
                    std::cerr << "Invalid --web port '" << argv[i]
                              << "', using default 8080\n";
                    --i;
                }
            }
        }
        else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    try {
        Simulation sim(cfg);
        sim.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
