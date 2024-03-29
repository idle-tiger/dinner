#include "booka.hpp"
#include "unpacked_booka.hpp"

#include "arg.hpp"
#include "error.hpp"
#include "fs.hpp"
#include "logging.hpp"
#include "overloaded.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <regex>
#include <string>

namespace fs = std::filesystem;

enum class Action {
    Encode,
    Decode,
};

std::istream& operator>>(std::istream& input, Action& action)
{
    static const std::map<std::string, Action> mapping {
        {"decode", Action::Decode},
        {"encode", Action::Encode},
    };

    std::string s;
    input >> s;
    if (auto i = mapping.find(s); i != mapping.end()) {
        action = i->second;
    } else {
        auto error = Error{};
        error << "unknown action; valid values:";
        if (auto j = mapping.begin(); j != mapping.end()) {
            error << " " << j->first;
            ++j;
            for (; j != mapping.end(); ++j) {
                error << ", " << j->first;
            }
        }
        throw std::move(error);
    }
    return input;
}

void encode(const fs::path& inputFilePath, const fs::path& outputFilePath)
{
    auto input = std::ifstream{inputFilePath};
    input.exceptions(std::ios::badbit);

    std::string character;
    std::map<std::string, uint32_t> imageIndices;
    std::map<std::string, uint32_t> musicIndices;

    auto unpackedBooka = booka::UnpackedBooka{};
    for (std::string line; std::getline(input, line); ) {
        auto match = std::smatch{};

        if (line.empty()) {
            character = "";
        } else if (line.starts_with("[")) {
            if (std::regex_match(
                    line,
                    match,
                    std::regex{R"_(\[фон "([^"\]]+)" ([^\]]+)\])_"})) {
                auto imageName = match[1];
                auto imagePath = inputFilePath.parent_path() / fs::path{match[2].str()};
                auto imageData = file::read(imagePath);
                std::cout << "image '" << imageName << "': " << Size{imageData.size()} << "\n";
                imageIndices[imageName] = (uint32_t)unpackedBooka.imageNames.size();
                unpackedBooka.imageNames.push_back(imageName);
                unpackedBooka.imageData.push_back(imageData);
            } else if (std::regex_match(
                    line,
                    match,
                    std::regex{R"_(\[музыка "([^"\]]+)" ([^\]]+)\])_"})) {
                auto musicName = match[1];
                auto musicPath = inputFilePath.parent_path() / fs::path{match[2].str()};
                auto musicData = file::read(musicPath);
                std::cout << "music '" << musicName << "': " << Size{musicData.size()} << "\n";
                musicIndices[musicName] = (uint32_t)unpackedBooka.musicNames.size();
                unpackedBooka.musicNames.push_back(musicName);
                unpackedBooka.musicData.push_back(musicData);
            } else {
                throw Error{} << "unknown directive: " << line;
            }
        } else if (std::regex_match(line, match, std::regex{"\\(фон: (.*)\\)"})) {
            auto imageName = match[1];
            auto imageIndex = imageIndices.at(imageName);
            unpackedBooka.actions.emplace_back(
                booka::UnpackedShowImageAction{.imageIndex = imageIndex});
        } else if (std::regex_match(line, match, std::regex{"\\(музыка: (.*)\\)"})) {
            auto musicName = match[1];
            auto musicIndex = musicIndices.at(musicName);
            unpackedBooka.actions.emplace_back(
                booka::UnpackedPlayMusicAction{.musicIndex = musicIndex});
        } else if (std::regex_match(line, match, std::regex{"\\(.*\\)"})) {
            unpackedBooka.actions.emplace_back(
                booka::UnpackedShowTextAction{.character = "", .text = line});
            std::cout << "[] " << line << "\n";
        } else if (std::regex_match(line, match, std::regex{"(.+):\\s*(.*)"})) {
            character = match[1];
            const auto& phrase = match[2];
            unpackedBooka.actions.emplace_back(
                booka::UnpackedShowTextAction{
                    .character = character, .text = phrase});
            std::cout << "[" << character << "] " << phrase << "\n";
        } else {
            unpackedBooka.actions.emplace_back(
                booka::UnpackedShowTextAction{.character = character, .text = line});
            std::cout << "[" << character << "] " << line << "\n";
        }
    }

    unpackedBooka.pack(outputFilePath);
}

void decode(const fs::path& inputFilePath, const fs::path& outputDirectoryPath)
{
    if (fs::exists(outputDirectoryPath)) {
        fs::remove_all(outputDirectoryPath);
    }
    fs::create_directory(outputDirectoryPath);
    fs::create_directory(outputDirectoryPath / "images");

    auto booka = booka::Booka{inputFilePath};
    for (const auto& image : booka.images()) {
        auto baseName = std::regex_replace(
            std::string{image.name}, std::regex{"\\s+"}, "_");
        auto filePath = outputDirectoryPath / "images" / baseName;
        filePath.replace_extension(".png");

        auto output = std::ofstream{filePath, std::ios::binary};
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output.write(
            reinterpret_cast<const char*>(image.data.data()),
            static_cast<std::streamsize>(image.data.size()));
    }

    auto output = std::ofstream{outputDirectoryPath / "script.txt"};
    output.exceptions(std::ios::badbit | std::ios::failbit);
    for (const auto& action : booka.actions()) {
        std::visit(Overloaded{
            [&] (const booka::ShowImageAction& action) {
                output << "[" <<
                    booka.images()[action.imageIndex].name << "]\n";
            },
            [&] (const booka::PlayMusicAction& action) {
                output << "[" << booka.music()[action.musicIndex].name << "]\n";
            },
            [&] (const booka::ShowTextAction& action) {
                if (!action.character.empty()) {
                    output << action.character << ": ";
                }
                output << action.text << "\n";
            }
        }, action);
    }
}

int main(int argc, char* argv[]) try
{
    auto parser = arg::Parser{};
    auto action = parser.argument<Action>()
        .metavar("ACTION")
        .markRequired()
        .help("action to perform");
    auto input = parser.option<fs::path>()
        .keys("--input")
        .markRequired()
        .help("path to input file");
    auto output = parser.option<fs::path>()
        .keys("--output")
        .markRequired()
        .help("path to output file");
    parser.helpKeys("-h", "--help");
    parser.parse(argc, argv);

    switch (action) {
        case Action::Decode:
            decode(input, output);
            break;
        case Action::Encode:
            encode(input, output);
            break;
    }

} catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return EXIT_FAILURE;
}
