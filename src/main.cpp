#include "audiogenerator_alsa.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <algorithm>

namespace {

struct TDatasetEntry {
    std::string SignalId;
    std::string Type;                 // "multitone", "sine" и т.п.
    std::string FrequenciesHz;        // список частот через запятую (строка)
    unsigned int NumTones = 0;
    double AmplitudePeak = 0.0;       // пиковая амплитуда
    unsigned int SampleRateHz = 0;
    unsigned int SampleCount = 0;
    std::string RelativePath;
};

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string ToLower(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(), ::tolower);
    return res;
}

std::vector<std::string> SplitCSV(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ','))
        fields.push_back(Trim(field));
    return fields;
}

// Вспомогательная функция для безопасного чтения опционального целочисленного поля
bool TryGetUInt(const std::map<std::string, int>& colIdx,
                const std::vector<std::string>& fields,
                const std::string& key, unsigned int& out) {
    auto it = colIdx.find(key);
    if (it == colIdx.end() || static_cast<size_t>(it->second) >= fields.size())
        return false;
    try {
        out = static_cast<unsigned int>(std::stoul(fields[it->second]));
        return true;
    } catch (...) {
        return false;
    }
}

// Вспомогательная функция для безопасного чтения опционального double поля
bool TryGetDouble(const std::map<std::string, int>& colIdx,
                  const std::vector<std::string>& fields,
                  const std::string& key, double& out) {
    auto it = colIdx.find(key);
    if (it == colIdx.end() || static_cast<size_t>(it->second) >= fields.size())
        return false;
    try {
        out = std::stod(fields[it->second]);
        return true;
    } catch (...) {
        return false;
    }
}

// Вспомогательная функция для безопасного чтения опциональной строки
bool TryGetString(const std::map<std::string, int>& colIdx,
                  const std::vector<std::string>& fields,
                  const std::string& key, std::string& out) {
    auto it = colIdx.find(key);
    if (it == colIdx.end() || static_cast<size_t>(it->second) >= fields.size())
        return false;
    out = fields[it->second];
    return true;
}

bool LoadDatasetManifest(const std::string& manifest_path,
                         std::vector<TDatasetEntry>& entries) {
    std::ifstream manifest(manifest_path);
    if (!manifest.is_open()) {
        std::cerr << "Cannot open manifest: " << manifest_path << std::endl;
        return false;
    }

    std::string header_line;
    if (!std::getline(manifest, header_line)) {
        std::cerr << "Empty manifest file." << std::endl;
        return false;
    }
    header_line = Trim(header_line);
    if (header_line.empty()) {
        std::cerr << "Manifest header is empty." << std::endl;
        return false;
    }

    std::vector<std::string> header = SplitCSV(header_line);
    std::map<std::string, int> col_idx;
    for (size_t i = 0; i < header.size(); ++i) {
        std::string key = ToLower(header[i]);
        col_idx[key] = static_cast<int>(i);
    }

    // Обязательные колонки (имена в нижнем регистре)
    const std::vector<std::string> required = {
        "signal_id", "sample_rate_hz", "sample_count", "relative_path"
    };
    for (const auto& req : required) {
        if (col_idx.find(req) == col_idx.end()) {
            std::cerr << "Manifest missing required column: " << req << std::endl;
            return false;
        }
    }

    std::string line;
    int line_num = 1;
    while (std::getline(manifest, line)) {
        ++line_num;
        line = Trim(line);
        if (line.empty())
            continue;

        std::vector<std::string> fields = SplitCSV(line);
        if (fields.size() <= static_cast<size_t>(col_idx["relative_path"])) {
            std::cerr << "Skipping malformed line " << line_num << ": too few fields" << std::endl;
            continue;
        }

        TDatasetEntry entry;
        entry.SignalId = fields[col_idx["signal_id"]];
        entry.SampleRateHz = static_cast<unsigned int>(
            std::stoul(fields[col_idx["sample_rate_hz"]]));
        entry.SampleCount = static_cast<unsigned int>(
            std::stoul(fields[col_idx["sample_count"]]));
        entry.RelativePath = fields[col_idx["relative_path"]];

        // Опциональные поля (без них запись остаётся валидной)
        TryGetString(col_idx, fields, "type", entry.Type);
        TryGetString(col_idx, fields, "frequencies_hz", entry.FrequenciesHz);
        TryGetUInt(col_idx, fields, "num_tones", entry.NumTones);
        TryGetDouble(col_idx, fields, "amplitude_peak", entry.AmplitudePeak);

        entries.push_back(entry);
    }

    if (entries.empty()) {
        std::cerr << "No valid entries found in manifest." << std::endl;
        return false;
    }
    return true;
}

bool LoadSignalSamples(const std::string& dataset_root,
                       const TDatasetEntry& entry,
                       std::vector<float>& samples) {
    std::string full_path = dataset_root + "/" + entry.RelativePath;
    std::ifstream input(full_path, std::ios::binary);
    if (!input.is_open()) {
        std::cerr << "Failed to open: " << full_path << std::endl;
        return false;
    }
    samples.assign(entry.SampleCount, 0.0f);
    input.read(reinterpret_cast<char*>(samples.data()),
               static_cast<std::streamsize>(entry.SampleCount * sizeof(float)));
    return input.gcount() == static_cast<std::streamsize>(entry.SampleCount * sizeof(float));
}

bool PlayEntry(TAlsaGenerator& gen,
               const std::string& dataset_root,
               const TDatasetEntry& entry) {
    // Проверка частоты дискретизации
    if (entry.SampleRateHz != TAlsaGenerator::CSampleRate) {
        std::cerr << "Skipping " << entry.SignalId
                  << ": unsupported sample rate " << entry.SampleRateHz
                  << " (expected " << TAlsaGenerator::CSampleRate << ")" << std::endl;
        return false;
    }

    std::vector<float> samples;
    if (!LoadSignalSamples(dataset_root, entry, samples)) {
        std::cerr << "Failed to load samples for " << entry.SignalId << std::endl;
        return false;
    }

    if (gen.LoadStreamBuffer(&samples) != TAlsaGenerator::LoadBufferOk) {
        std::cerr << "Failed to load buffer for " << entry.SignalId << std::endl;
        return false;
    }

    if (!gen.ReleaseStreamBuffer()) {
        std::cerr << "Failed to release stream for " << entry.SignalId << std::endl;
        return false;
    }

    // Расширенный вывод информации о сигнале
    std::cout << "Played: " << entry.SignalId;
    if (!entry.Type.empty()) std::cout << " [" << entry.Type << "]";
    if (!entry.FrequenciesHz.empty()) std::cout << " freq=" << entry.FrequenciesHz;
    if (entry.AmplitudePeak > 0.0) std::cout << " amp=" << entry.AmplitudePeak;
    std::cout << std::endl;

    return true;
}

bool IsRepeatCommand(const std::string& cmd) {
    std::string c = Trim(cmd);
    return c == "repeat" || c == "r";
}

} // namespace

int main(int argc, char** argv) {
    const std::string dataset_root = (argc > 1) ? argv[1] : "dataset/ess_signals";
    const std::string manifest_path = dataset_root + "/manifest.csv";

    std::vector<TDatasetEntry> entries;
    if (!LoadDatasetManifest(manifest_path, entries)) {
        return 1;
    }

    TAlsaGenerator gen;
    std::cout << "Scanning audio interfaces..." << std::endl;
    gen.GetInterfaces();
    std::cout << "Looking for UMC202HD..." << std::endl;
    if (!gen.GetUMC202Interface()) {
        std::cerr << "Failed to find UMC202HD!" << std::endl;
        return 1;
    }

    std::cout << "Loaded " << entries.size() << " signals:" << std::endl;
    for (size_t i = 0; i < entries.size(); ++i) {
        std::cout << i << ": " << entries[i].SignalId;
        if (!entries[i].Type.empty()) std::cout << " [" << entries[i].Type << "]";
        if (!entries[i].FrequenciesHz.empty())
            std::cout << " (" << entries[i].FrequenciesHz << " Hz)";
        if (entries[i].AmplitudePeak > 0.0)
            std::cout << " peak=" << entries[i].AmplitudePeak;
        std::cout << std::endl;
    }

    size_t current = 0;
    size_t played = 0;
    while (current < entries.size()) {
        if (!PlayEntry(gen, dataset_root, entries[current])) {
            std::cerr << "Playback failed for " << entries[current].SignalId << ", stopping." << std::endl;
            return 1;
        }
        ++played;

        std::cout << "Command (repeat/r to replay, anything else for next): ";
        std::string cmd;
        if (!std::getline(std::cin, cmd)) {
            std::cout << std::endl;
            break;
        }
        if (!IsRepeatCommand(cmd))
            ++current;
    }

    std::cout << "Finished playing " << played << " signal(s)." << std::endl;
    return 0;
}