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
        namespace krb::logs {
            template <typename... Args>
            inline static void print(std::format_string<Args...> fmt, Args&&... args) {
                const auto text = std::format(fmt, std::forward<Args>(args)...);
                std::printf("%s\n", text.data());
            }
        }

        #define KRB_DEBUG(fmt, ...) krb::logs::print("krb debug: " fmt, ##__VA_ARGS__)
        #define KRB_WARN(fmt, ...) krb::logs::print("krb warn: " fmt, ##__VA_ARGS__)
        #define KRB_ERROR(fmt, ...) krb::logs::print("krb error: " fmt, ##__VA_ARGS__)
    #endif
#else 
    #define KRB_DEBUG(fmt, ...) 
    #define KRB_WARN(fmt, ...) 
    #define KRB_ERROR(fmt, ...)
#endif

namespace krb {
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

    struct Token {
        enum class Types {
            None, // Empty or undefined token
            String, 
            Number,
            Bool,
            ArrayStrings,
            ArrayNumbers,
            Comment, // Special token for comments
            Group // If we need define something special
        };

        // The all valid value types for token
        using ValueVariants = std::variant<
            // All valid simple types
            // TODO: support for std::int8_t, std::int16_t, std::int32_t, std::int64_t, double
            float, 
            std::string,
            const char*,
            bool,
            // For empty or undefined tokens
            std::monostate,
            // All valid array types 
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


        constexpr Token() :
            m_name { },
            m_value { std::monostate { } },
            m_type { Types::None }  { }

        explicit constexpr Token(std::string_view name, ValueVariants value, Types type) : 
            m_name { name }, 
            m_value { value }, 
            m_type { type } { }

        constexpr ~Token() = default;

        // Get name
        [[nodiscard]] constexpr std::string_view name() const;
        
        // Get name ref
        [[nodiscard]] constexpr std::string& name(); 
        
        // Get type 
        [[nodiscard]] constexpr Types type() const;
        
        // Get type ref
        [[nodiscard]] constexpr Types& type();
        
        // Get value ref 
        template <typename T>
        requires Token::is_valid_value_type<T>
        [[nodiscard]] constexpr std::optional<std::reference_wrapper<T>> value();

        // Get value 
        template <typename T>
        requires Token::is_valid_value_type<T>
        [[nodiscard]] constexpr std::optional<std::reference_wrapper<const T>> value() const;
        
        [[nodiscard]] const ValueVariants& raw_variant() const;

        template <typename T>
        requires Token::is_valid_value_type<T>
        operator T() const;
        
        template <typename T>
        requires Token::is_valid_value_type<T>
        Token& operator=(const T& right);

        template <typename T>
        requires Token::is_valid_value_type<std::decay_t<T>>
        Token& operator=(T&& right);


        [[nodiscard]] static constexpr Types value_type(const ValueVariants& value);

        [[nodiscard]] static constexpr std::string type_str(Types type);
        private:
            std::string m_name;
            ValueVariants m_value;
            Types m_type;
    };

    inline const Token::ValueVariants& Token::raw_variant() const {
        return m_value;
    }

    // TODO: idk can be improved ?
    inline constexpr Token::Types Token::value_type(const ValueVariants& value){
        return std::visit([]<typename T>(const T&) -> Token::Types {
            if constexpr (std::is_same_v<T, std::string>) {
                return Token::Types::String;
            }    
            else if constexpr (std::is_same_v<T, const char*>) {
                return Token::Types::String;
            }            
            else if constexpr (std::is_same_v<T, float>) {
                return Token::Types::Number;
            }                    
            else if constexpr (std::is_same_v<T, bool>) {
                return Token::Types::Bool;
            }                     
            else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return Token::Types::ArrayStrings;
            } 
            else if constexpr (std::is_same_v<T, std::vector<float>>) {
                return Token::Types::ArrayNumbers;
            }       
            else if constexpr (std::is_same_v<T, std::monostate>) {
                return Token::Types::None;
            } 
            return Token::Types::None; 
        }, value);
    }

    inline constexpr std::string Token::type_str(Token::Types type) {
        switch (type) {
            case Token::Types::None: 
                return "None";
            case Token::Types::String: 
                return "String";
            case Token::Types::Number: 
                return "Number";
            case Token::Types::Bool: 
                return "Bool";
            case Token::Types::ArrayStrings: 
                return "ArrayStrings";
            case Token::Types::ArrayNumbers: 
                return "ArrayNumbers";
            case Token::Types::Comment: 
                return "Comment";
            case Token::Types::Group: 
                return "Group";
        }

        return "UNK";
    }
    
    template <typename T>
    requires Token::is_valid_value_type<std::decay_t<T>>
    Token& Token::operator=(T&& right) {
        m_value = std::forward<T>(right); 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T>
    requires Token::is_valid_value_type<T>
    inline Token& Token::operator=(const T& right) {
        m_value = right; 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T> 
    requires Token::is_valid_value_type<T>
    inline Token::operator T() const {
        auto result = value<T>();
        if (!result.has_value()) {
            throw std::bad_variant_access();
        }

        return result.value();
    }

    
    inline constexpr Token::Types& Token::type() {
        return m_type;
    }
    
    inline constexpr Token::Types Token::type() const {
        return m_type;
    }

    inline constexpr std::string& Token::name() {
        return m_name;
    }
    
    inline constexpr std::string_view Token::name() const {
        return m_name;
    }

    template <typename T>
    requires Token::is_valid_value_type<T>
    inline constexpr std::optional<std::reference_wrapper<T>> Token::value() {
        if (auto* value = std::get_if<T>(&m_value)) {
            return *value;
        }

        return std::nullopt;
    }

    template <typename T>
    requires Token::is_valid_value_type<T>
    inline constexpr std::optional<std::reference_wrapper<const T>> Token::value() const {
        if (const auto* value = std::get_if<T>(&m_value)) {
            return *value;
        }

        return std::nullopt;
    }

    class Krb {
        public:
            Krb() = default;            
            ~Krb() = default;

            // Convert text to tokens 
            [[nodiscard]] std::optional<std::span<const Token>> from(std::string_view source);

            // Convert tokens to text 
            [[nodiscard]] std::string dump() const;
            
            // Get tokens
            [[nodiscard]] std::span<const Token> tokens() const;
            
            // Is token list are empty
            [[nodiscard]] constexpr bool empty() const;
            
            // Get token from list
            [[nodiscard]] constexpr std::optional<std::reference_wrapper<Token>> token(std::string_view name);
            [[nodiscard]] constexpr std::optional<std::reference_wrapper<const Token>> token(std::string_view name) const;

            [[nodiscard]] Token at(std::string_view name);
            [[nodiscard]] const Token& at(std::string_view name) const;

            [[nodiscard]] Token& operator[] (std::string_view name);
        private:
            [[nodiscard]] std::optional<Token> parse_line(std::string_view line);
            std::vector<Token> m_tokens;
    };

    inline Token& Krb::operator[] (std::string_view name) {
        const auto& result = token(name);
        if (!result.has_value()) {
            auto token = Token {
                name,
                std::monostate { },
                Token::Types::None
            };

            m_tokens.push_back(token);
            return m_tokens.back();
        }

        return result.value();
    }

    inline const Token& Krb::at(std::string_view name) const {
        const auto& result = token(name);
        if (!result.has_value()) {
            throw std::invalid_argument("can't find token");
        }

        return result.value();
    }

    inline Token Krb::at(std::string_view name) {
        return const_cast<Token&>(std::as_const(*this).at(name));
    }

    inline constexpr std::optional<std::reference_wrapper<const Token>> Krb::token(std::string_view name) const {
        const auto it = std::ranges::find_if(m_tokens, [name](const auto& token) { return token.name() == name; });
        if (it == m_tokens.end()) {
            return std::nullopt;
        }

        return *it;
    }

    inline constexpr std::optional<std::reference_wrapper<Token>> Krb::token(std::string_view name) {
        auto result = std::as_const(*this).token(name);
        if (!result) {
            return std::nullopt;
        } 
        return const_cast<Token&>(result->get());
    }

    inline constexpr bool Krb::empty() const {
        return m_tokens.empty();
    }

    inline std::span<const Token> Krb::tokens() const {
        return m_tokens;
    }

    inline std::string Krb::dump() const {
        if (m_tokens.empty()) {
            return "";
        }

        std::string source { };
        for (const auto& token : m_tokens) {
            switch (token.type()) {
                case Token::Types::String: {
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
                case Token::Types::Number: {
                    // TODO: Number type ?
                    const auto& result = token.value<float>();
                    if (result.has_value()) {
                        source += std::format("{}:{}\n", token.name(), result.value().get());
                    } else {
                        KRB_ERROR("failed to write number, wrong value type");
                    }
                    break;
                }
                case Token::Types::Bool: {
                    const auto& result = token.value<bool>();
                    if (result.has_value()) {
                        source += std::format("{}:{}\n", token.name(), ((result.value().get()) ? "true" : "false"));
                    } else {
                        KRB_ERROR("failed to write bool, wrong value type");
                    }
                    break;
                }                 
                case Token::Types::ArrayStrings: {
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
                case Token::Types::ArrayNumbers: {
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
                case Token::Types::Group: {
                    source += std::format("$:{}\n", token.name());
                    break;
                }
                case Token::Types::Comment:
                case Token::Types::None: {
                    break;
                }
            }
        }

        return source;
    }

    static constexpr auto COMENT_TOKEN_SYMBOL = '#';
    static constexpr auto GROUP_TOKEN_SYMBOL = '$'; 
    
    inline std::optional<Token> Krb::parse_line(std::string_view line) {  
        // Skip comments
        if (line.front() == COMENT_TOKEN_SYMBOL) {
            return Token {
                "", // FIXME ?
                std::monostate { },
                Token::Types::Comment
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
            return Token {
                value, 
                std::monostate { },
                Token::Types::Group
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
                
                return Token {
                    name, 
                    numberValue,
                    Token::Types::Number
                };
            } else if (value.front() == '"' && value.back() == '"') {
                // Remove quotes
                const auto& inner = value.substr(1, value.size() - 2);
                return Token {
                    name, 
                    inner,
                    Token::Types::String
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
                    return Token {
                        name, 
                        std::vector<float> { }, // FIXME: Not optimal 
                        Token::Types::ArrayNumbers
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

                    return Token {
                        name, 
                        numberVector,
                        Token::Types::ArrayNumbers
                    };
                } else { // Else, convert to std::vector<std::string>
                    std::vector<std::string> stringsVector { };
                    for (const auto& str : elementsVector) {
                        if (str.front() == '"' && str.back() == '"') {
                            stringsVector.push_back(str);
                        }
                    }

                    return Token {
                        name, 
                        stringsVector,
                        Token::Types::ArrayStrings
                    }; 
                }

                KRB_ERROR("not supported array type!");
                return std::nullopt;
            } else if (value == "true" || value == "false") {
                return Token {
                    name, 
                    value == "true",
                    Token::Types::Bool
                };
            }
        }

        KRB_ERROR("unknown type for parse or syntax error, name:{} value:{}", name, value);
        return std::nullopt;
    }

    inline std::optional<std::span<const Token>> Krb::from(std::string_view source) {
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

        std::vector<Token> tokens { };
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
            if (token.type() == Token::Types::Comment) {
                continue;
            }

            tokens.push_back(token);
        }

        m_tokens = std::move(tokens);
        return m_tokens;
    }
}