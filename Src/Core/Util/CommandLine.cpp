#include "Core/Util/CommandLine.h"

#include <sstream>

namespace
{
Util::ParsedCommandLine g_ProcessCommandLine;

bool IsOptionVisible(Util::CommandLineOptionVisibility visibility, Util::CommandLineParseMode mode)
{
    if (mode == Util::CommandLineParseMode::Development)
        return true;

    return visibility == Util::CommandLineOptionVisibility::Always;
}

void SetError(std::string* errorMessage, std::string message)
{
    if (errorMessage != nullptr)
        *errorMessage = std::move(message);
}

bool StoreParsedOption(const Util::CommandLineOption& option,
                       std::optional<std::string> value,
                       Util::ParsedCommandLine& parsed)
{
    parsed.m_Options[option.m_LongName] = std::move(value);
    return true;
}

bool ParseLongOptionArgument(std::string_view argument,
                             const Util::CommandLineSpec& spec,
                             int argc,
                             char** argv,
                             int& index,
                             Util::ParsedCommandLine& parsed,
                             std::string* errorMessage,
                             Util::CommandLineParseMode mode)
{
    const size_t equalsPos = argument.find('=');
    const std::string_view optionName =
        argument.substr(2, equalsPos == std::string_view::npos ? std::string_view::npos : equalsPos - 2);
    const Util::CommandLineOption* option = spec.FindLongOption(optionName);
    if (option == nullptr)
    {
        SetError(errorMessage, "Unknown argument: " + std::string(argument));
        return false;
    }

    const bool isOptionVisible = IsOptionVisible(option->m_Visibility, mode);
    const bool hasInlineValue = equalsPos != std::string_view::npos;

    if (option->m_Kind == Util::CommandLineOptionKind::Flag)
    {
        if (hasInlineValue)
        {
            SetError(errorMessage, "Unexpected value for flag: --" + std::string(optionName));
            return false;
        }

        if (isOptionVisible)
            return StoreParsedOption(*option, std::nullopt, parsed);

        return true;
    }

    std::string value;
    if (hasInlineValue)
    {
        value = std::string(argument.substr(equalsPos + 1));
    }
    else
    {
        if (index + 1 >= argc)
        {
            SetError(errorMessage, "Missing value for --" + std::string(optionName));
            return false;
        }

        value = argv[++index];
    }

    if (isOptionVisible)
        return StoreParsedOption(*option, std::move(value), parsed);

    return true;
}

bool ParseShortOptionArgument(std::string_view argument,
                              const Util::CommandLineSpec& spec,
                              int argc,
                              char** argv,
                              int& index,
                              Util::ParsedCommandLine& parsed,
                              std::string* errorMessage,
                              Util::CommandLineParseMode mode)
{
    if (argument.size() < 2)
    {
        SetError(errorMessage, "Unknown argument: " + std::string(argument));
        return false;
    }

    const char shortName = argument[1];
    const Util::CommandLineOption* option = spec.FindShortOption(shortName);
    if (option == nullptr)
    {
        SetError(errorMessage, "Unknown argument: " + std::string(argument));
        return false;
    }

    const bool isOptionVisible = IsOptionVisible(option->m_Visibility, mode);
    const std::string_view remainder = argument.substr(2);

    if (option->m_Kind == Util::CommandLineOptionKind::Flag)
    {
        if (!remainder.empty())
        {
            SetError(errorMessage, "Unexpected value for flag: -" + std::string(1, shortName));
            return false;
        }

        if (isOptionVisible)
            return StoreParsedOption(*option, std::nullopt, parsed);

        return true;
    }

    std::string value;
    if (!remainder.empty())
    {
        if (remainder.front() == '=')
            value = std::string(remainder.substr(1));
        else
            value = std::string(remainder);
    }
    else
    {
        if (index + 1 >= argc)
        {
            SetError(errorMessage, "Missing value for -" + std::string(1, shortName));
            return false;
        }

        value = argv[++index];
    }

    if (isOptionVisible)
        return StoreParsedOption(*option, std::move(value), parsed);

    return true;
}
} // namespace

namespace Util
{
bool ParsedCommandLine::HasOption(std::string_view name) const
{
    return m_Options.contains(std::string(name));
}

std::optional<std::string_view> ParsedCommandLine::GetOptionValue(std::string_view name) const
{
    const auto it = m_Options.find(std::string(name));
    if (it == m_Options.end() || !it->second.has_value())
        return std::nullopt;

    return *it->second;
}

CommandLineSpec& CommandLineSpec::AddFlag(std::string_view longName,
                                          std::optional<char> shortName,
                                          std::string_view description,
                                          CommandLineOptionVisibility visibility)
{
    m_Options.push_back(CommandLineOption{
        .m_LongName = std::string(longName),
        .m_ShortName = shortName,
        .m_Kind = CommandLineOptionKind::Flag,
        .m_Visibility = visibility,
        .m_ValueName = {},
        .m_Description = std::string(description),
    });
    return *this;
}

CommandLineSpec& CommandLineSpec::AddValueOption(std::string_view longName,
                                                 std::optional<char> shortName,
                                                 std::string_view valueName,
                                                 std::string_view description,
                                                 CommandLineOptionVisibility visibility)
{
    m_Options.push_back(CommandLineOption{
        .m_LongName = std::string(longName),
        .m_ShortName = shortName,
        .m_Kind = CommandLineOptionKind::Value,
        .m_Visibility = visibility,
        .m_ValueName = std::string(valueName),
        .m_Description = std::string(description),
    });
    return *this;
}

const std::vector<CommandLineOption>& CommandLineSpec::GetOptions() const
{
    return m_Options;
}

std::string CommandLineSpec::BuildUsage(std::string_view programName, CommandLineParseMode mode) const
{
    std::ostringstream stream;
    stream << "Usage: " << programName;
    for (const auto& option : m_Options)
    {
        if (!IsOptionVisible(option.m_Visibility, mode))
            continue;

        stream << " [--" << option.m_LongName;
        if (option.m_Kind == CommandLineOptionKind::Value)
            stream << " <" << option.m_ValueName << ">";
        stream << "]";
    }
    stream << "\n\nOptions:\n";

    for (const auto& option : m_Options)
    {
        if (!IsOptionVisible(option.m_Visibility, mode))
            continue;

        stream << "  ";
        if (option.m_ShortName.has_value())
            stream << "-" << *option.m_ShortName << ", ";
        else
            stream << "    ";

        stream << "--" << option.m_LongName;
        if (option.m_Kind == CommandLineOptionKind::Value)
            stream << " <" << option.m_ValueName << ">";
        if (!option.m_Description.empty())
            stream << "\n      " << option.m_Description;
        stream << "\n";
    }

    return stream.str();
}

const CommandLineOption* CommandLineSpec::FindLongOption(std::string_view longName) const
{
    for (const auto& option : m_Options)
    {
        if (option.m_LongName == longName)
            return &option;
    }

    return nullptr;
}

const CommandLineOption* CommandLineSpec::FindShortOption(char shortName) const
{
    for (const auto& option : m_Options)
    {
        if (option.m_ShortName.has_value() && *option.m_ShortName == shortName)
            return &option;
    }

    return nullptr;
}

bool ParseCommandLine(int argc,
                      char** argv,
                      const CommandLineSpec& spec,
                      ParsedCommandLine& parsed,
                      std::string* errorMessage,
                      CommandLineParseMode mode)
{
    parsed = ParsedCommandLine{};

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument = argv[i];
        if (argument == "--")
        {
            for (++i; i < argc; ++i)
                parsed.m_Positionals.emplace_back(argv[i]);
            return true;
        }

        if (argument.size() > 2 && argument.starts_with("--"))
        {
            if (!ParseLongOptionArgument(argument, spec, argc, argv, i, parsed, errorMessage, mode))
                return false;
            continue;
        }

        if (argument.size() > 1 && argument.front() == '-' && argument[1] != '-')
        {
            if (!ParseShortOptionArgument(argument, spec, argc, argv, i, parsed, errorMessage, mode))
                return false;
            continue;
        }

        parsed.m_Positionals.emplace_back(argument);
    }

    return true;
}

bool HasRawCommandLineFlag(int argc, char** argv, std::string_view longName)
{
    const std::string prefix = "--" + std::string(longName);
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument = argv[i];
        if (argument == prefix)
            return true;
        if (argument.starts_with(prefix) && argument.size() > prefix.size() && argument[prefix.size()] == '=')
            return true;
    }

    return false;
}

bool ProcessHasOption(std::string_view name)
{
    return g_ProcessCommandLine.HasOption(name);
}

std::optional<std::string_view> GetProcessOptionValue(std::string_view name)
{
    return g_ProcessCommandLine.GetOptionValue(name);
}

void SetProcessCommandLine(ParsedCommandLine commandLine)
{
    g_ProcessCommandLine = std::move(commandLine);
}

const ParsedCommandLine& GetProcessCommandLine()
{
    return g_ProcessCommandLine;
}

void ClearProcessCommandLine()
{
    g_ProcessCommandLine = ParsedCommandLine{};
}
} // namespace Util
