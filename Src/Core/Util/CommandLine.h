#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Util
{
enum class CommandLineOptionKind
{
    Flag,
    Value,
};

enum class CommandLineOptionVisibility
{
    Always,
    DevelopmentOnly,
};

enum class CommandLineParseMode
{
    Development,
    Shipping,
};

struct CommandLineOption
{
    std::string longName;
    std::optional<char> shortName;
    CommandLineOptionKind kind = CommandLineOptionKind::Flag;
    CommandLineOptionVisibility visibility = CommandLineOptionVisibility::Always;
    std::string valueName;
    std::string description;
};

struct ParsedCommandLine
{
    bool HasOption(std::string_view name) const;
    std::optional<std::string_view> GetOptionValue(std::string_view name) const;

    std::unordered_map<std::string, std::optional<std::string>> options;
    std::vector<std::string> positionals;
};

class CommandLineSpec
{
public:
    CommandLineSpec& AddFlag(std::string_view longName,
                             std::optional<char> shortName,
                             std::string_view description,
                             CommandLineOptionVisibility visibility = CommandLineOptionVisibility::Always);

    CommandLineSpec& AddValueOption(std::string_view longName,
                                    std::optional<char> shortName,
                                    std::string_view valueName,
                                    std::string_view description,
                                    CommandLineOptionVisibility visibility = CommandLineOptionVisibility::Always);

    const std::vector<CommandLineOption>& GetOptions() const;
    std::string BuildUsage(std::string_view programName,
                           CommandLineParseMode mode = CommandLineParseMode::Development) const;
    const CommandLineOption* FindLongOption(std::string_view longName) const;
    const CommandLineOption* FindShortOption(char shortName) const;

private:
    std::vector<CommandLineOption> m_Options;
};

bool ParseCommandLine(int argc,
                      char** argv,
                      const CommandLineSpec& spec,
                      ParsedCommandLine& parsed,
                      std::string* errorMessage = nullptr,
                      CommandLineParseMode mode = CommandLineParseMode::Development);

bool HasRawCommandLineFlag(int argc, char** argv, std::string_view longName);
bool ProcessHasOption(std::string_view name);
std::optional<std::string_view> GetProcessOptionValue(std::string_view name);
void SetProcessCommandLine(ParsedCommandLine commandLine);
const ParsedCommandLine& GetProcessCommandLine();
void ClearProcessCommandLine();
} // namespace Util
