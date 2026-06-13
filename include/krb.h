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
#include <sstream>
#include <vector>
#include <span>
#include <format>
#include <utility>
#include <tuple>
#include <concepts>

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
            constexpr std::array<char, 5> SPECIAL_CASES = {  '\f', '\n', '\r', '\t', '\v' };
            return std::ranges::find(SPECIAL_CASES, chr) != SPECIAL_CASES.end();
        }

        static inline constexpr bool is_white_space(char chr) {
            constexpr std::array<char, 6> WHITE_SPACE_CASES = { ' ', '\f', '\n', '\r', '\t', '\v' };
            return std::ranges::find(WHITE_SPACE_CASES, chr) != WHITE_SPACE_CASES.end();
        }

        // Dummy function to make something like this: 1,2,3,4,5...
        template<typename T>
        static inline std::string join_with(std::span<const T> arr, std::string_view with) { 
            std::stringstream stream;
            for (const auto& element : arr) {
                stream << element << with;
            }

            const auto finalStr = stream.str();
            return finalStr.substr(0, finalStr.size() - with.size());
        }
        
        template <typename T>
        concept is_numeric = std::floating_point<T> || std::integral<T>;

        template <is_numeric T>
        static inline std::optional<T> get_number_from_string(std::string_view str) {
            T numberValue { 0 };
            const auto& result = std::from_chars(str.data(), str.data() + str.size(), numberValue).ec;
            if (result != std::errc()) {
                KRB_ERROR("invalid number convert {}", str);
                return std::nullopt;
            }

            return numberValue;
        }
    }
    namespace detail {
        using numbers_types_tuple = std::tuple<
            std::int8_t,
            std::uint8_t, 
            std::int16_t,
            std::uint16_t, 
            std::int32_t,
            std::uint32_t, 
            std::int64_t,
            std::uint64_t,
            float, 
            double>;

        using strings_types_tuple = std::tuple<
            std::string,
            const char*>;
        
        template <typename T>
        struct tuple_vec_variants_convert_trait;

        template<typename... T>
        struct tuple_vec_variants_convert_trait<std::tuple<T...>> {
            using type = std::variant<std::vector<T>...>;  
        };

        template <typename T>
        struct tuple_variants_convert_trait;

        template<typename... T>
        struct tuple_variants_convert_trait<std::tuple<T...>> {
            using type = std::variant<T...>;  
        };
        
        template <typename T>
        struct vec_tuple_value_traits;

        template<typename... T>
        struct vec_tuple_value_traits<std::tuple<T...>> {
            using type = std::tuple<std::vector<T>...>;  
        };

        using numbers_types_variants = tuple_variants_convert_trait<numbers_types_tuple>::type;
        using strings_types_variants = tuple_variants_convert_trait<strings_types_tuple>::type;
        
        using numbers_vec_types_variants = tuple_vec_variants_convert_trait<numbers_types_tuple>::type;
        using strings_vec_types_variants = tuple_vec_variants_convert_trait<strings_types_tuple>::type;

        template <typename... T>
        struct variant_cat_trait;

        template <typename... T>
        struct variant_cat_trait<std::variant<T...>> {
            using type = std::variant<T...>;
        };

        template <typename... T, typename... U, typename... N>
        struct variant_cat_trait<std::variant<T...>, std::variant<U...>, N...> {
            using type = variant_cat_trait<std::variant<T..., U...>, N...>::type;
        };

        template <typename T, typename U>
        struct is_type_in_tuple_trait : std::false_type { };

        template <typename T, typename... U>
        struct is_type_in_tuple_trait<T, std::tuple<U...>> : std::bool_constant<(std::is_same<T, U>::value || ...)> { };
        
        template <typename T, typename... U>
        using is_type_in_tuple = is_type_in_tuple_trait<T, U...>;

        using numbers_vec_types_tuple = vec_tuple_value_traits<numbers_types_tuple>::type;
        using strings_vec_types_tuple = vec_tuple_value_traits<strings_types_tuple>::type;

        template <typename T>
        static constexpr bool is_supported_type = 
            std::is_same<T, bool>::value || 
            is_type_in_tuple<T, numbers_types_tuple>::value ||
            is_type_in_tuple<T, strings_types_tuple>::value ||
            std::is_same<T, std::vector<bool>>::value || 
            is_type_in_tuple<T, numbers_vec_types_tuple>::value ||
            is_type_in_tuple<T, strings_vec_types_tuple>::value;

        using numbers_type_entry = std::tuple<std::string_view, numbers_types_variants>;
        static constexpr numbers_type_entry numbers_type_map[] = {
            { "i8",   std::int8_t { } },
            { "i16",  std::int16_t { } },
            { "i32",  std::int32_t { } },
            { "i64",  std::int64_t { } },
            { "u8",   std::uint8_t { } },
            { "u16",  std::uint16_t { } },
            { "u32",  std::uint32_t { } },
            { "u64",  std::uint64_t { } },
            { "f",    float { } },
            { "d",    double { } }
        };
    }

    struct token {
        enum class types : std::int8_t {
            none, // Empty or undefined token
            string, 
            number,
            boolean,
            array_strings,
            array_numbers,
            array_booleans,
            comment, // Special token for comments
            group // If we need define something special
        };

        // The all valid value types for token
        using value_variants = detail::variant_cat_trait<
            std::variant<std::monostate, bool>, 
            detail::numbers_types_variants,
            detail::strings_types_variants,
            detail::numbers_vec_types_variants,
            detail::strings_vec_types_variants>::type;
        
        token() :
            m_name { },
            m_value { std::monostate { } },
            m_type { types::none }  { }

        explicit token(std::string_view name, value_variants value, types type) : 
            m_name { name }, 
            m_value { value }, 
            m_type { type } { }

        ~token() = default;

        // Get name
        [[nodiscard]] std::string_view name() const;
        
        // Get name ref
        [[nodiscard]] std::string& name(); 
        
        // Get type 
        [[nodiscard]] types type() const;
        
        // Get type ref
        [[nodiscard]] types& type();
        
        // Get value ref 
        template <typename T>
        requires detail::is_supported_type<T>
        [[nodiscard]] std::optional<std::reference_wrapper<T>> value();

        // Get value 
        template <typename T>
        requires detail::is_supported_type<T>
        [[nodiscard]] std::optional<std::reference_wrapper<const T>> value() const;

        [[nodiscard]] const value_variants& raw_value() const;

        template <typename T>
        requires detail::is_supported_type<T>
        operator T() const;
        
        template <typename T>
        requires detail::is_supported_type<T>
        token& operator=(const T& right);

        template <typename T>
        requires detail::is_supported_type<std::decay_t<T>>
        token& operator=(T&& right);

        template <typename T>
        requires detail::is_supported_type<std::decay_t<T>>
        bool operator !=(const T& right);

        template <typename T>
        requires detail::is_supported_type<std::decay_t<T>>
        bool operator ==(const T& right);

        template <typename T>
        requires detail::is_supported_type<std::decay_t<T>>
        token& operator=(std::initializer_list<T> right);

        template <typename T>
        requires detail::is_supported_type<std::decay_t<T>>
        token& operator=(std::vector<T> right);

        [[nodiscard]] static constexpr types value_type(const value_variants& value);

        [[nodiscard]] static std::string type_str(types type);
        private:
            std::string m_name;
            value_variants m_value;
            types m_type;
    };

    inline const token::value_variants& token::raw_value() const {
        return m_value;
    }

    inline constexpr token::types token::value_type(const value_variants& value){
        return std::visit([]<typename T>(const T&) {
            if constexpr (detail::is_type_in_tuple<T, detail::numbers_types_tuple>::value) {
                return token::types::number;
            }
            
            if constexpr (detail::is_type_in_tuple<T, detail::strings_types_tuple>::value) {
                return token::types::string;
            }

            if constexpr (detail::is_type_in_tuple<T, detail::numbers_vec_types_tuple>::value) {
                return token::types::array_numbers;
            }

            if constexpr (detail::is_type_in_tuple<T, detail::strings_vec_types_tuple>::value) {
                return token::types::array_strings;
            }

            if constexpr (std::is_same<T, std::vector<bool>>::value) {
                return token::types::array_booleans;
            }

            if constexpr (std::is_same<T, bool>::value) {
                return token::types::boolean;
            }

            if constexpr (std::is_same<T, std::monostate>::value) {
                return token::types::none;
            } 

            return token::types::none; 
        }, value);
    }

    inline std::string token::type_str(token::types type) {
        switch (type) {
            case token::types::none: 
                return "None";
            case token::types::string: 
                return "String";
            case token::types::number: 
                return "Number";
            case token::types::boolean: 
                return "Boolean";
            case token::types::array_booleans: 
                return "Array Booleans";
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
    requires detail::is_supported_type<std::decay_t<T>>
    inline bool token::operator !=(const T& right) {
        auto result = value<T>();
        if (!result.has_value()) {
            return false;
        }

        return result.value() != right;
    }

    template <typename T>
    requires detail::is_supported_type<std::decay_t<T>>
    inline bool token::operator ==(const T& right) {
        auto result = value<T>();
        if (!result.has_value()) {
            return false;
        }

        return result.value() == right;
    }
    template <typename T>
    requires detail::is_supported_type<std::decay_t<T>>
    inline token& token::operator=(std::vector<T> right) {
        m_value = right; 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T>
    requires detail::is_supported_type<std::decay_t<T>>
    inline token& token::operator=(std::initializer_list<T> right) {
        return operator=(std::vector<T> { right.begin(), right.end() });
    }

    template <typename T>
    requires detail::is_supported_type<std::decay_t<T>>
    inline token& token::operator=(T&& right) {
        m_value = std::forward<T>(right); 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T>
    requires detail::is_supported_type<T>
    inline token& token::operator=(const T& right) {
        m_value = right; 
        m_type = value_type(m_value);    
        return *this;
    }

    template <typename T> 
    requires detail::is_supported_type<T>
    inline token::operator T() const {
        auto result = value<T>();
        if (!result.has_value()) {
            throw std::bad_variant_access(); // TODO: custom exception with better info about invalid type
        }

        return result.value();
    }

    template <typename T>
    requires detail::is_supported_type<T>
    inline std::optional<std::reference_wrapper<T>> token::value() {
        if (auto* value = std::get_if<T>(&m_value)) {
            return *value;
        }

        return std::nullopt;
    }

    template <typename T>
    requires detail::is_supported_type<T>
    inline std::optional<std::reference_wrapper<const T>> token::value() const {
        if (const auto* value = std::get_if<T>(&m_value)) {
            return *value;
        }

        return std::nullopt;
    }
  
    inline token::types& token::type() {
        return m_type;
    }
    
    inline token::types token::type() const {
        return m_type;
    }

    inline std::string& token::name() {
        return m_name;
    }
    
    inline std::string_view token::name() const {
        return m_name;
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
            [[nodiscard]] bool empty() const;
            
            // Get token from list
            [[nodiscard]] std::optional<std::reference_wrapper<token>> get(std::string_view name);
            [[nodiscard]] std::optional<std::reference_wrapper<const token>> get(std::string_view name) const;

            [[nodiscard]] token at(std::string_view name);
            [[nodiscard]] const token& at(std::string_view name) const;

            [[nodiscard]] token& operator[] (std::string_view name);
        private:
            [[nodiscard]] std::optional<token> parse_line(std::string_view line);
            
            // TODO: better name
            template <typename ValueTuple, typename ValueVariants> // TODO: check is tuple
            [[nodiscard]] std::optional<ValueVariants> find_value_type(const token& token) const;

            template<utils::is_numeric T>
            [[nodiscard]] std::optional<token> get_num_token(std::string_view name, std::string_view line);

            template<utils::is_numeric T>
            [[nodiscard]] std::optional<token> get_vec_num_token(std::string_view name, std::span<const std::string> elements);

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

    inline std::optional<std::reference_wrapper<const token>> krb::get(std::string_view name) const {
        const auto it = std::ranges::find_if(m_tokens, [name](const auto& token) { return token.name() == name; });
        if (it == m_tokens.end()) {
            return std::nullopt;
        }

        return *it;
    }

    inline std::optional<std::reference_wrapper<token>> krb::get(std::string_view name) {
        auto result = std::as_const(*this).get(name);
        if (!result) {
            return std::nullopt;
        } 
        return const_cast<token&>(result->get());
    }

    inline bool krb::empty() const {
        return m_tokens.empty();
    }

    inline std::span<const token> krb::tokens() const {
        return m_tokens;
    }
    
    template <typename ValueTuple, typename ValueVariants>
    inline std::optional<ValueVariants> krb::find_value_type(const token& token) const {
        std::optional<ValueVariants> applyResult { std::nullopt };
        std::apply([&](auto... types) {
            return (... || [&]() {
                using type = decltype(types); 
                const auto& result = token.value<type>();
                if (result.has_value()) {
                    applyResult = result.value().get();
                    return true;
                }

                return false;
            }());
        }, ValueTuple { });

        if (!applyResult.has_value()) {
            KRB_ERROR("failed to write {} token, wrong value type", token::type_str(token.type()));
        }

        return applyResult;
    }

    inline std::string krb::dump() const {
        if (m_tokens.empty()) {
            return "";
        }

        std::stringstream stream { };
        for (const auto& token : m_tokens) {
            switch (token.type()) {
                case token::types::string: {
                    const auto& result = find_value_type<detail::strings_types_tuple, detail::strings_types_variants>(token);
                    if (result.has_value()) {
                        std::visit([&](const auto& v) {
                            stream << token.name() << ":" << "\"" << v << "\"\n";
                        }, result.value());
                    }

                    break;
                }
                case token::types::number: {
                    const auto& result = find_value_type<detail::numbers_types_tuple, detail::numbers_types_variants>(token);
                    if (result.has_value()) {
                        std::visit([&](const auto& v) {
                            using type = std::decay_t<decltype(v)>;
                            if constexpr (std::is_same<type, std::int8_t>::value 
                                || std::is_same<type, std::uint8_t>::value
                                || std::is_same<type, char>::value) { // Convert int8 to int32 because stream are convert it to character
                                stream << token.name() << ":" << static_cast<std::int32_t>(v) << "\n";
                            } else {
                                stream << token.name() << ":" << v << "\n";
                            }
                        }, result.value());
                    }
                    break;
                }
                case token::types::boolean: {
                    const auto& result = token.value<bool>();
                    if (result.has_value()) {
                        stream << token.name() << ":" << (result.value().get() ? "true" : "false") << "\n";
                    } else {
                        KRB_ERROR("failed to write boolean token, wrong value type");
                    }
                    break;
                }                 
                case token::types::array_strings: {
                    const auto& result = find_value_type<detail::strings_vec_types_tuple, detail::strings_vec_types_variants>(token);
                    if (result.has_value()) {
                        std::visit([&](const auto& v) {
                            if (v.empty()) {
                                // TODO: 
                                return;
                            }
                            
                            using element_type = typename std::decay_t<decltype(v)>::value_type;
                            const auto& arrayInString = utils::join_with<element_type>(v, ",");                    
                            stream << token.name() << ":[" << std::string { arrayInString.begin(), arrayInString.end() } << "]\n";
                        }, result.value());
                    } else {
                        stream << token.name() << ":[]\n";
                    }
                    break;
                }                
                case token::types::array_booleans: {
                    // TODO:
                    break;
                }
                case token::types::array_numbers: {
                    const auto& result = find_value_type<detail::numbers_vec_types_tuple, detail::numbers_vec_types_variants>(token);
                    if (result.has_value()) {
                        std::visit([&](const auto& v) {
                            if (v.empty()) {
                                // TODO: 
                                return;
                            }

                            using element_type = typename std::decay_t<decltype(v)>::value_type;
                            const auto& arrayInString = utils::join_with<element_type>(v, ",");                    
                            stream << token.name() << ":[" << std::string { arrayInString.begin(), arrayInString.end() } << "]\n";
                        }, result.value());
                    } else {
                        stream << token.name() << ":[]\n";
                    }
                    break;
                }
                case token::types::group: {
                    stream <<"$:" << token.name() << "\n";
                    break;
                }
                case token::types::comment:
                case token::types::none: {
                    break;
                }
            }
        }

        return stream.str();
    }

    static constexpr auto COMMENT_TOKEN_SYMBOL = '#';
    static constexpr auto GROUP_TOKEN_SYMBOL = '$'; 
    
    template<utils::is_numeric T>
    inline std::optional<token> krb::get_num_token(std::string_view name, std::string_view line) {
        const auto& convertResult = utils::get_number_from_string<T>(line);
        if (!convertResult.has_value()) {
            return std::nullopt;
        }
        return token {
            name, 
            convertResult.value(),
            token::types::number
        };
    }

    template<utils::is_numeric T>
    inline std::optional<token> krb::get_vec_num_token(std::string_view name, std::span<const std::string> elements) {
        std::vector<T> numberVector { };
        numberVector.reserve(elements.size());
        for (const auto& str : elements) {
            const auto& convertResult = utils::get_number_from_string<T>(str);
            if (!convertResult.has_value()) {
                return std::nullopt;
            }
            numberVector.push_back(convertResult.value());
        }
        return token {
            name, 
            numberVector,
            token::types::array_numbers
        };
    }

    inline std::optional<token> krb::parse_line(std::string_view line) {  
        // Skip comments
        if (line.front() == COMMENT_TOKEN_SYMBOL) {
            return token {
                "", // FIXME ?
                std::monostate { },
                token::types::comment
            };
        }

        // Split two values: name and value. Or on three values: name; type; value 
        auto splitValues = line | std::views::split(':') | std::views::transform([](const auto& sub) {             
            return sub | std::views::split(';') | std::views::transform([](const auto& sub) { 
                return std::string { sub.begin(), sub.end() };
            }); 
        });

        std::vector<std::string> splitValuesVector { };

        for (auto&& vec : splitValues) {
            for (auto&& sub : vec) {
                splitValuesVector.push_back(std::move(sub));
            }
        }
        std::string name { };
        std::string value { };
        std::string type { };
        
        if (splitValuesVector.size() == 2) {
            name = splitValuesVector[0];
            value = splitValuesVector[1];
        } else if (splitValuesVector.size() == 3) {
            name = splitValuesVector[0];
            type = splitValuesVector[1];
            value = splitValuesVector[2];
        } else {
            KRB_ERROR("wrong size of line args, line:{} size:{}", line, splitValuesVector.size());
            return std::nullopt;
        }
        
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
                /*
                    All number cases: 

                    value:32 <- ! default is int32_t
                    value:u8;32 <- use uint8_t
                    value:i16;32 <- use int16_t
                    value:32.5 <- ! default is float
                    value:f;32.5 <- again float
                    value:d;32.5 <- double
                */

                if (!type.empty()) {
                    for (const auto& [entryName, entryType] : detail::numbers_type_map) {
                        if (entryName == type) {
                            return std::visit([this, name, value](auto val) {
                                return get_num_token<decltype(val)>(name, value);     
                            }, entryType);
                        }
                    }
                }

                // If type are missing 
                if (value.find('.') == std::string::npos) {
                    return get_num_token<std::int32_t>(name, value);
                } else {
                    return get_num_token<float>(name, value);
                }

                return std::nullopt;                
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
                const auto firstNumberElement = elementsVector.front();
                if (utils::is_number(firstNumberElement)) {
                    /*
                        value: [0, 1, 2, 3] <- std::vector<std::int32_t>
                        value:i8;[0, 1, 2, 3]
                        value:u8;[0, 1, 2, 3]
                        value: [0.5, 1.5, 2.5, 3.5] <- std::vector<float>
                        value:f;[0.5, 1.5, 2.5, 3.5] <- std::vector<float>
                        value:d;[0.5, 1.5, 2.5, 3.5] <- std::vector<double>
                    */
                    if (!type.empty()) {
                        for (const auto& [entryName, entryType] : detail::numbers_type_map) {
                            if (entryName == type) {
                                return std::visit([this, name, elementsVector](auto val) {
                                    return get_vec_num_token<decltype(val)>(name, elementsVector);     
                                }, entryType);
                            }
                        }
                    }

                    // If type are missing 
                    if (firstNumberElement.find('.') == std::string::npos) {
                        return get_vec_num_token<std::int32_t>(name, elementsVector);
                    } else {
                        return get_vec_num_token<float>(name, elementsVector);
                    }
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

                // TODO: vector bool
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