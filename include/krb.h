/*
    Korobok (.krb) format 
    A lightweight, header-only C++20 library for parsing and serializing
    simple key-value data files.
*/

#pragma once 
#include <optional>
#include <ranges>
#include <algorithm>
#include <variant>
#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <format>
#include <utility>

// NOTE: korobok uses std::format for logs 
#ifdef KRB_ENABLE_LOGS 
    #ifdef KRB_USE_EXTERNAL_LOG_FUNCS
        #if !defined(KRB_EXTERNAL_DEBUG) || !defined(KRB_EXTERNAL_WARN) || !defined(KRB_EXTERNAL_ERROR)
            #error "External debug macros are not defined!" 
        #endif

        #define KRB_DEBUG(fmt, ...) KRB_EXTERNAL_DEBUG("krb: " fmt, ##__VA_ARGS__)
        #define KRB_WARN(fmt, ...) KRB_EXTERNAL_WARN("krb: " fmt, ##__VA_ARGS__)
        #define KRB_ERROR(fmt, ...) KRB_EXTERNAL_ERROR("krb: " fmt, ##__VA_ARGS__)
    #else
        namespace korobok::logs {
            template <typename... Args>
            inline static void print(std::format_string<Args...> fmt, Args&&... args) {
                const auto text = std::format(fmt, std::forward<Args>(args)...);
                std::printf("%s\n", text.data());
            }
        }

        #define KRB_DEBUG(fmt, ...) korobok::logs::print("krb debug: " fmt, ##__VA_ARGS__)
        #define KRB_WARN(fmt, ...) korobok::logs::print("krb warn: " fmt, ##__VA_ARGS__)
        #define KRB_ERROR(fmt, ...) korobok::logs::print("krb error: " fmt, ##__VA_ARGS__)
    #endif
#else 
    #define KRB_DEBUG(fmt, ...) 
    #define KRB_WARN(fmt, ...) 
    #define KRB_ERROR(fmt, ...)
#endif

namespace korobok {
    namespace utils {
        static inline constexpr bool is_dot(char chr) {
            return chr == '.';
        }

        static inline constexpr bool is_digit(char chr)  {
            return chr >= '0' && chr <= '9';
        }

        static inline constexpr bool is_number(std::string_view text)  {
            return !text.empty() && std::ranges::all_of(text, [](char c) { return is_digit(c) || is_dot(c); });
        }

        static inline constexpr bool is_special_symbol(char chr) {
            constexpr std::array<char, 6> SPECIAL_CASES = {  '\f', '\n', '\r', '\t', '\v' };
            return std::ranges::any_of(SPECIAL_CASES, [chr](char c) { return chr == c; });
        }

        static inline constexpr bool is_white_space(char chr) {
            constexpr std::array<char, 6> WHITE_SPACE_CASES = { ' ', '\f', '\n', '\r', '\t', '\v' };
            return std::ranges::any_of(WHITE_SPACE_CASES, [chr](char c) { return chr == c; });
        }

        // Dummy function to make something like this: 1,2,3,4,5...
        template<typename T>
        static inline std::string join_with(std::span<const T> arr, std::string_view with) { 
            std::string finalStr { };
            for (const auto& element : arr) {
                finalStr += std::format("{}{}", element, with);
            }

            return finalStr.substr(0, finalStr.size() - with.size());
        }
    }

    struct token {
        enum class types {
            none, // Empty or undefined token
            string, 
            number,
            boolean,
            array_strings,
            array_numbers,
            comment, // Special token for comments
            group // If we need define something special
        };

        // The all valid value types for token
        using value_variants = std::variant<
            // For empty or undefined tokens
            std::monostate,

            // Simple types
            // TODO: support for std::int8_t, std::int16_t, std::int32_t, std::int64_t, double
            float, 
            std::string,
            const char*,
            bool,
            // Array types 
            // TODO: support for vector of std::int8_t, std::int16_t, std::int32_t, std::int64_t, double
            std::vector<float>,
            std::vector<std::string>
        >;

        template <typename T>
        static constexpr bool is_valid_value_type = 
            std::is_same_v<T, std::string> || 
            std::is_same_v<T, const char*> || 
            std::is_same_v<T, float> ||
            std::is_same_v<T, bool> ||
            std::is_same_v<T, std::vector<std::string>> ||
            std::is_same_v<T, std::vector<float>>;


        constexpr token() :
            m_name { },
            m_value { std::monostate { } },
            m_type { types::none }  { }

        explicit constexpr token(std::string_view name, value_variants value, types type) : 
            m_name { name }, 
            m_value { value }, 
            m_type { type } { }

        constexpr ~token() = default;

        // Get name
        [[nodiscard]] constexpr std::string_view name() const;
        
        // Get name ref
        [[nodiscard]] constexpr std::string& name(); 
        
        // Get type 
        [[nodiscard]] constexpr types type() const;
        
        // Get type ref
        [[nodiscard]] constexpr types& type();
        
        // Get value ref 
        template <typename T>
        requires token::is_valid_value_type<T>
        [[nodiscard]] constexpr std::optional<std::reference_wrapper<T>> value();

        // Get value 
        template <typename T>
        requires token::is_valid_value_type<T>
        [[nodiscard]] constexpr std::optional<std::reference_wrapper<const T>> value() const;
        
        [[nodiscard]] const value_variants& raw_variant() const;

        template <typename T>
        requires token::is_valid_value_type<T>
        operator T() const;
        
        template <typename T>
        requires token::is_valid_value_type<T>
        token& operator=(const T& right);

        template <typename T>
        requires token::is_valid_value_type<std::decay_t<T>>
        token& operator=(T&& right);


        [[nodiscard]] static constexpr types value_type(const value_variants& value);

        [[nodiscard]] static constexpr std::string type_str(types type);
        private:
            std::string m_name;
            value_variants m_value;
            types m_type;
    };

    inline const token::value_variants& token::raw_variant() const {
        return m_value;
    }

    // TODO: idk can be improved ?
    inline constexpr token::types token::value_type(const value_variants& value){
        return std::visit([]<typename T>(const T&) -> token::types {
            if constexpr (std::is_same_v<T, std::string>) {
                return token::types::string;
            }    
            else if constexpr (std::is_same_v<T, const char*>) {
                return token::types::string;
            }            
            else if constexpr (std::is_same_v<T, float>) {
                return token::types::number;
            }                    
            else if constexpr (std::is_same_v<T, bool>) {
                return token::types::boolean;
            }                     
            else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return token::types::array_strings;
            } 
            else if constexpr (std::is_same_v<T, std::vector<float>>) {
                return token::types::array_numbers;
            }       
            else if constexpr (std::is_same_v<T, std::monostate>) {
                return token::types::none;
            } 

            return token::types::none; 
        }, value);
    }

    inline constexpr std::string token::type_str(token::types type) {
        switch (type) {
            case token::types::none: 
                return "None";
            case token::types::string: 
                return "String";
            case token::types::number: 
                return "Number";
            case token::types::boolean: 
                return "Boolean";
            case token::types::array_strings: 
                return "Array Strings";
            case token::types::array_numbers: 
                return "Array Numbers";
            case token::types::comment: 
                return "Comment";
            case token::types::group: 
                return "Group";
        }

        return "UNK";
    }
    
    template <typename T>
    requires token::is_valid_value_type<std::decay_t<T>>
    token& token::operator=(T&& right) {
        m_value = std::forward<T>(right); 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T>
    requires token::is_valid_value_type<T>
    inline token& token::operator=(const T& right) {
        m_value = right; 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T> 
    requires token::is_valid_value_type<T>
    inline token::operator T() const {
        auto result = value<T>();
        if (!result.has_value()) {
            throw std::bad_variant_access();
        }

        return result.value();
    }

    
    inline constexpr token::types& token::type() {
        return m_type;
    }
    
    inline constexpr token::types token::type() const {
        return m_type;
    }

    inline constexpr std::string& token::name() {
        return m_name;
    }
    
    inline constexpr std::string_view token::name() const {
        return m_name;
    }

    template <typename T>
    requires token::is_valid_value_type<T>
    inline constexpr std::optional<std::reference_wrapper<T>> token::value() {
        if (auto* value = std::get_if<T>(&m_value)) {
            return *value;
        }

        return std::nullopt;
    }

    template <typename T>
    requires token::is_valid_value_type<T>
    inline constexpr std::optional<std::reference_wrapper<const T>> token::value() const {
        if (const auto* value = std::get_if<T>(&m_value)) {
            return *value;
        }

        return std::nullopt;
    }

    class krb {
        public:
            krb() = default;            
            ~krb() = default;

            // Convert text to tokens 
            [[nodiscard]] std::optional<std::span<const token>> from(std::string_view source);

            // Convert tokens to text 
            [[nodiscard]] std::string dump() const;
            
            // Get tokens
            [[nodiscard]] std::span<const token> tokens() const;
            
            // Is token list are empty
            [[nodiscard]] constexpr bool empty() const;
            
            // Get token from list
            [[nodiscard]] constexpr std::optional<std::reference_wrapper<token>> get(std::string_view name);
            [[nodiscard]] constexpr std::optional<std::reference_wrapper<const token>> get(std::string_view name) const;

            [[nodiscard]] token at(std::string_view name);
            [[nodiscard]] const token& at(std::string_view name) const;

            [[nodiscard]] token& operator[] (std::string_view name);
        private:
            [[nodiscard]] std::optional<token> parse_line(std::string_view line);
            std::vector<token> m_tokens;
    };

    inline token& krb::operator[] (std::string_view name) {
        const auto& result = get(name);
        if (!result.has_value()) {
            m_tokens.push_back(token {
                name,
                std::monostate { },
                token::types::none
            });
            return m_tokens.back();
        }

        return result.value();
    }

    inline const token& krb::at(std::string_view name) const {
        const auto& result = get(name);
        if (!result.has_value()) {
            throw std::invalid_argument("can't find token");
        }

        return result.value();
    }

    inline token krb::at(std::string_view name) {
        return const_cast<token&>(std::as_const(*this).at(name));
    }

    inline constexpr std::optional<std::reference_wrapper<const token>> krb::get(std::string_view name) const {
        const auto it = std::ranges::find_if(m_tokens, [name](const auto& token) { return token.name() == name; });
        if (it == m_tokens.end()) {
            return std::nullopt;
        }

        return *it;
    }

    inline constexpr std::optional<std::reference_wrapper<token>> krb::get(std::string_view name) {
        auto result = std::as_const(*this).get(name);
        if (!result) {
            return std::nullopt;
        } 
        return const_cast<token&>(result->get());
    }

    inline constexpr bool krb::empty() const {
        return m_tokens.empty();
    }

    inline std::span<const token> krb::tokens() const {
        return m_tokens;
    }

    inline std::string krb::dump() const {
        if (m_tokens.empty()) {
            return "";
        }

        std::string source { };
        for (const auto& token : m_tokens) {
            switch (token.type()) {
                case token::types::string: {
                    const auto& resultString = token.value<std::string>();
                    if (resultString.has_value()) {
                        source += std::format("{}:\"{}\"\n", token.name(), resultString.value().get());
                    } else {
                        const auto& resultCStr = token.value<const char*>();
                        if (resultCStr.has_value()) {
                            source += std::format("{}:\"{}\"\n", token.name(), resultCStr.value().get());
                        }
                    }
                    break;
                }
                case token::types::number: {
                    // TODO: Number type ?
                    const auto& result = token.value<float>();
                    if (result.has_value()) {
                        source += std::format("{}:{}\n", token.name(), result.value().get());
                    } else {
                        KRB_ERROR("failed to write number, wrong value type");
                    }
                    break;
                }
                case token::types::boolean: {
                    const auto& result = token.value<bool>();
                    if (result.has_value()) {
                        source += std::format("{}:{}\n", token.name(), ((result.value().get()) ? "true" : "false"));
                    } else {
                        KRB_ERROR("failed to write bool, wrong value type");
                    }
                    break;
                }                 
                case token::types::array_strings: {
                    const auto& result = token.value<std::vector<std::string>>();
                    if (result.has_value()) {
                        const auto& stringsVector = result.value().get();
                        if (stringsVector.empty()) {
                            source += std::format("{}:[]\n", token.name());
                            break;
                        }

                        const auto& arrayInString = utils::join_with<std::string>(stringsVector, ",");                    
                        source += std::format("{}:[{}]\n", token.name(), std::string { arrayInString.begin(), arrayInString.end() });
                    } else {
                        KRB_ERROR("failed to write strings array, wrong value type");
                    }
                    break;
                }                
                case token::types::array_numbers: {
                    // TODO: Number type ?
                    const auto& result = token.value<std::vector<float>>();
                    if (result.has_value()) {
                        const auto& numbersVector = result.value().get();
                        if (numbersVector.empty()) {
                            source += std::format("{}:[]\n", token.name());
                            break;
                        }

                        const auto& arrayInString = utils::join_with<float>(numbersVector, ","); 
                        source += std::format("{}:[{}]\n", token.name(), std::string { arrayInString.begin(), arrayInString.end() });
                    } else {
                        KRB_ERROR("failed to write numbers array, wrong value type");
                    }
                    break;
                }
                case token::types::group: {
                    source += std::format("$:{}\n", token.name());
                    break;
                }
                case token::types::comment:
                case token::types::none: {
                    break;
                }
            }
        }

        return source;
    }

    static constexpr auto COMMENT_TOKEN_SYMBOL = '#';
    static constexpr auto GROUP_TOKEN_SYMBOL = '$'; 
    
    inline std::optional<token> krb::parse_line(std::string_view line) {  
        // Skip comments
        if (line.front() == COMMENT_TOKEN_SYMBOL) {
            return token {
                "", // FIXME ?
                std::monostate { },
                token::types::comment
            };
        }
        // Slice two values to name and value 
        auto splitValues = line | std::views::split(':') | std::views::transform([](const auto& sub) { return std::string { sub.begin(), sub.end() }; });        
        std::vector<std::string> splitValuesVector { splitValues.begin(), splitValues.end() };

        if (splitValuesVector.size() != 2) {
            KRB_ERROR("wrong size of line args, line:{} size:{}", line, splitValuesVector.size());
            return std::nullopt;
        }
        const auto& name = splitValuesVector[0];
        auto value = splitValuesVector[1];

        if (name.size() == 1 && name.front() == GROUP_TOKEN_SYMBOL) {                
            return token {
                value, 
                std::monostate { },
                token::types::group
            };
        } else { // Else we are parse name:value aka variable
            // If first character is whitespace 
            if (utils::is_white_space(value.front())) {
                value = value.substr(1, value.size());
            }

            if (utils::is_number(value)) {
                // TODO: Detect which type of number we are use float or int
                float numberValue = 0.0f;
                const auto& result = std::from_chars(value.data(), value.data() + value.size(), numberValue).ec;
                if (result != std::errc()) {
                    KRB_ERROR("invalid number convert {}", value);
                    return std::nullopt;
                }
                
                return token {
                    name, 
                    numberValue,
                    token::types::number
                };
            } else if (value.front() == '"' && value.back() == '"') {
                // Remove quotes
                const auto& inner = value.substr(1, value.size() - 2);
                return token {
                    name, 
                    inner,
                    token::types::string
                };
            } else if (value.front() == '[' && value.back() == ']') {
                // Remove whitespaces / special symbols / brackets
                auto filtered = value | std::views::filter([](char c) { return c != '[' && c != ']' && !utils::is_white_space(c);});
                const auto& inner = std::string { filtered.begin(), filtered.end() };
                
                auto elements = inner
                    | std::views::split(',') 
                    | std::views::transform([](const auto& range) {
                        return std::string(range.begin(), range.end());
                });

                std::vector<std::string> elementsVector { elements.begin(), elements.end() };
                if (elementsVector.empty()) {
                    return token {
                        name, 
                        std::vector<float> { }, // FIXME: Not optimal 
                        token::types::array_numbers
                    };
                }

                // Convert to std::vector<std::int32_t>/std::vector<float>
                if (utils::is_number(elementsVector.front())) {
                    // TODO: Convert by number type
                    std::vector<float> numberVector { };
                    numberVector.reserve(elementsVector.size());

                    for (const auto& str : elementsVector) {
                        float numberValue = 0.0f;
                        const auto& result = std::from_chars(str.data(), str.data() + str.size(), numberValue).ec;
                        if (result != std::errc()) {
                            KRB_ERROR("invalid number convert for array {}", str);
                            return std::nullopt;
                        }                            
                        numberVector.push_back(numberValue);
                    }

                    return token {
                        name, 
                        numberVector,
                        token::types::array_numbers
                    };
                } else { // Else, convert to std::vector<std::string>
                    std::vector<std::string> stringsVector { };
                    for (const auto& str : elementsVector) {
                        if (str.front() == '"' && str.back() == '"') {
                            stringsVector.push_back(str);
                        }
                    }

                    return token {
                        name, 
                        stringsVector,
                        token::types::array_strings
                    }; 
                }

                KRB_ERROR("not supported array type!");
                return std::nullopt;
            } else if (value == "true" || value == "false") {
                return token {
                    name, 
                    value == "true",
                    token::types::boolean
                };
            }
        }

        KRB_ERROR("unknown type for parse or syntax error, name:{} value:{}", name, value);
        return std::nullopt;
    }

    inline std::optional<std::span<const token>> krb::from(std::string_view source) {
        if (!m_tokens.empty()) {
            m_tokens.clear();
        }
        
        if (source.empty()) {
            KRB_ERROR("source is empty, abort");   
            return std::nullopt;
        }

        auto linesToParse = source 
                | std::views::split('\n') // Split line by /n 
                | std::views::transform([](const auto& sub) { return std::string { sub.begin(), sub.end() }; })
                | std::views::filter([](const auto& line) { return !line.empty(); });

        std::vector<std::string> linesPool { linesToParse.begin(), linesToParse.end() };
        if (linesPool.empty()) {
            KRB_ERROR("lines pool is empty, abort");
            return std::nullopt;
        }

        std::vector<token> tokens { };
        for (const auto& line : linesPool) {
            if (utils::is_white_space(line.front())) {
                continue;
            }
            
            auto filtered = line | std::views::filter([](char c) { return !utils::is_special_symbol(c);});
            auto filteredString = std::string { filtered.begin(), filtered.end() };            
            const auto& parsedLineResult = parse_line(filteredString);
            if (!parsedLineResult.has_value()) {     
                KRB_ERROR("failed to parse line, abort");           
                return std::nullopt;
            }
            const auto& token = parsedLineResult.value();
            if (token.type() == token::types::comment) {
                continue;
            }

            tokens.push_back(token);
        }

        m_tokens = std::move(tokens);
        return m_tokens;
    }
}