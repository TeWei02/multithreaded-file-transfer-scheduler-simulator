#include <exception>
#include <iostream>
#include <optional>
#include <string>

#include "simulation_engine.h"

namespace {

void PrintUsage() {
    std::cout << "Multithreaded File Transfer and Scheduling Simulator\n\n";
    std::cout << "Usage:\n";
    std::cout << "  ./simulator --input data/requests.csv --scheduler fifo --threads 4\n";
    std::cout << "  ./simulator --input data/requests.csv --benchmark --threads 4\n";
    std::cout << "  ./simulator --generate-sample data/sample_requests.csv --n 100 --seed 42\n\n";
    std::cout << "Options:\n";
    std::cout << "  --input <path>             Input request CSV\n";
    std::cout << "  --scheduler <name>         fifo | sjf | priority | rr | wfq\n";
    std::cout << "  --threads <int>            Number of worker threads\n";
    std::cout << "  --benchmark                Run all schedulers on same input\n";
    std::cout << "  --generate-sample <path>   Generate sample requests CSV and exit\n";
    std::cout << "  --n <int>                  Number of generated sample requests\n";
    std::cout << "  --quantum <float>          RR time quantum in seconds\n";
    std::cout << "  --seed <int>               Random seed\n";
    std::cout << "  --output-dir <path>        Output directory (default: output)\n";
    std::cout << "  --overhead <float>         Per-dispatch overhead in seconds\n";
    std::cout << "  --help                     Show this message\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    using namespace simulator;

    SimulationConfig config;
    std::optional<std::string> generate_sample_path;
    std::size_t sample_n = 100;
    bool scheduler_set = false;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];

            auto require_value = [&](const std::string& name) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error("Missing value for " + name);
                }
                return argv[++i];
            };

            if (arg == "--help") {
                PrintUsage();
                return 0;
            }
            if (arg == "--input") {
                config.input_path = require_value(arg);
                continue;
            }
            if (arg == "--scheduler") {
                const std::string value = require_value(arg);
                auto parsed = ParseSchedulerType(value);
                if (!parsed.has_value()) {
                    throw std::runtime_error("Unsupported scheduler: " + value);
                }
                config.scheduler = *parsed;
                scheduler_set = true;
                continue;
            }
            if (arg == "--threads") {
                config.threads = std::stoi(require_value(arg));
                continue;
            }
            if (arg == "--benchmark") {
                config.benchmark = true;
                continue;
            }
            if (arg == "--generate-sample") {
                generate_sample_path = require_value(arg);
                continue;
            }
            if (arg == "--n") {
                sample_n = static_cast<std::size_t>(std::stoul(require_value(arg)));
                continue;
            }
            if (arg == "--quantum") {
                config.quantum_sec = std::stod(require_value(arg));
                continue;
            }
            if (arg == "--seed") {
                config.seed = static_cast<unsigned int>(std::stoul(require_value(arg)));
                continue;
            }
            if (arg == "--output-dir") {
                config.output_dir = require_value(arg);
                continue;
            }
            if (arg == "--overhead") {
                config.connection_overhead_sec = std::stod(require_value(arg));
                continue;
            }

            throw std::runtime_error("Unknown argument: " + arg);
        }

        if (generate_sample_path.has_value()) {
            return SimulationEngine::GenerateSample(*generate_sample_path, sample_n, config.seed);
        }

        if (config.input_path.empty()) {
            std::cerr << "--input is required unless --generate-sample is used\n";
            PrintUsage();
            return 1;
        }

        if (!config.benchmark && !scheduler_set) {
            std::cerr << "--scheduler is required in single run mode\n";
            PrintUsage();
            return 1;
        }

        SimulationEngine engine(config);
        return engine.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
