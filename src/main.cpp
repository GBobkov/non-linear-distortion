#include "audiogenerator_alsa.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct TDatasetEntry
{
    std::string SignalId;
    int FrequencyHz = 0;
    unsigned int SampleRateHz = 0;
    unsigned int SampleCount = 0;
    std::string RelativePath;
};

bool SkipManifestHeader(const std::string& line)
{
    return line.rfind("signal_id,", 0) == 0;
}

std::string TrimRight(const std::string& value)
{
    std::string trimmed = value;
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n'))
        trimmed.pop_back();
    return trimmed;
}

bool ParseManifestLine(const std::string& line, TDatasetEntry& entry)
{
    std::stringstream line_stream(line);
    std::string field;
    std::vector<std::string> fields;

    while (std::getline(line_stream, field, ','))
        fields.push_back(TrimRight(field));

    if (fields.size() != 7)
        return false;

    entry.SignalId = fields[0];
    entry.FrequencyHz = std::stoi(fields[1]);
    entry.SampleRateHz = static_cast<unsigned int>(std::stoul(fields[2]));
    entry.SampleCount = static_cast<unsigned int>(std::stoul(fields[5]));
    entry.RelativePath = fields[6];
    return true;
}

bool LoadSignalSamples(const std::string& dataset_root,
                       const TDatasetEntry& entry,
                       std::vector<float>& samples)
{
    std::ifstream input(dataset_root + "/" + entry.RelativePath, std::ios::binary);
    if (!input.is_open())
        return false;

    samples.assign(entry.SampleCount, 0.0f);
    input.read(reinterpret_cast<char*>(samples.data()),
               static_cast<std::streamsize>(entry.SampleCount * sizeof(float)));

    const std::streamsize expected_size =
        static_cast<std::streamsize>(entry.SampleCount * sizeof(float));
    return input.gcount() == expected_size;
}

bool LoadDatasetManifest(const std::string& manifest_path,
                         std::vector<TDatasetEntry>& entries)
{
    std::ifstream manifest(manifest_path);
    if (!manifest.is_open())
        return false;

    std::string line;
    while (std::getline(manifest, line))
    {
        line = TrimRight(line);
        if (line.empty() || SkipManifestHeader(line))
            continue;

        TDatasetEntry entry;
        if (!ParseManifestLine(line, entry))
            return false;

        entries.push_back(entry);
    }

    return !entries.empty();
}

} // namespace


int main(int argc, char** argv) {
    const std::string dataset_root =
        argc > 1 ? argv[1] : "dataset/excitation_signals";
    const std::string manifest_path = dataset_root + "/manifest.csv";

    std::vector<TDatasetEntry> dataset_entries;
    if (!LoadDatasetManifest(manifest_path, dataset_entries)) {
        std::cerr << "Failed to load dataset manifest: " << manifest_path << std::endl;
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

    for (const TDatasetEntry& entry : dataset_entries) {
        if (entry.SampleRateHz != TAlsaGenerator::CSampleRate) {
            std::cerr << "Skipping " << entry.SignalId
                      << ": unsupported sample rate " << entry.SampleRateHz << std::endl;
            continue;
        }

        std::vector<float> samples;
        if (!LoadSignalSamples(dataset_root, entry, samples)) {
            std::cerr << "Failed to load samples for " << entry.SignalId << std::endl;
            return 1;
        }

        if (gen.LoadStreamBuffer(&samples) != TAlsaGenerator::LoadBufferOk) {
            std::cerr << "Failed to play " << entry.SignalId << std::endl;
            return 1;
        }

        std::cout << "Played " << entry.SignalId
                  << " (" << entry.FrequencyHz << " Hz)" << std::endl;

        std::cout << "Sleep... " << std::endl;
        sleep(1);
    }

    if (!gen.ReleaseStreamBuffer()) {
        std::cerr << "Failed to drain audio stream" << std::endl;
        return 1;
    }

    std::cout << "Finished playing " << dataset_entries.size() << " signals." << std::endl;
    return 0;
}
