#include <krb.h>

#include <fstream>
#include <vector>
#include <optional>
#include <sstream>

#define KRB_TEST(func, ...) \
KRB_DEBUG("RUN TEST: {}\n", #func); \
if (func(__VA_ARGS__)) { \
    KRB_DEBUG("PASSED TEST: {}\n", #func); \
} else { \
    KRB_ERROR("FAILED TEST: {}\n", #func); \
} \



namespace korobok::io {
    inline static std::optional<std::string> load_file(std::string_view path) {
        if (path.empty()) {
            KRB_ERROR("path is empty");
            return std::nullopt;
        }
        std::vector<char> buffer;
        std::ifstream file;
        file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            file.open(path.data(), std::iostream::ate | std::ios::binary);
        } catch (const std::ios_base::failure& ex) {
            KRB_ERROR("can't open file: {}", ex.what());
            return std::nullopt;
        }
        // Disable exceptions 
        file.exceptions(std::ifstream::goodbit);
        const auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        buffer.resize(size);
        if (!file.read(buffer.data(), size)) {
            KRB_ERROR("failed to read file");
            file.close();
            return std::nullopt;
        }
        const auto data = std::string(buffer.begin(), buffer.end());
        file.close();  
        return data;
    }

    inline static bool save_file(std::string_view path, std::span<const char> buffer) {
        if (path.empty()) {
            KRB_ERROR("can't save file because path is empty");
            return false;
        }
        std::ofstream file;
        file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        try {
            file.open(path.data(), std::ios_base::out);
        } catch (const std::ios_base::failure& ex) {
            KRB_ERROR("can't save file: {}", ex.what());
            return false;
        }
                    
        file.write(buffer.data(), buffer.size());
        file.close();
        return true;
    }
}

static bool test_parse_tokens(std::string_view source) {
    korobok::krb data { };
    const auto& result = data.from(source);
    if (!result.has_value()) {
        KRB_ERROR("failed to read sources");
        return false;
    }

    const auto& tokens = result.value();
    for (const auto& token : tokens) {
        std::stringstream stream { };
        stream << "token: " << token.name() << " ";

        switch (token.type())
        {
            case korobok::token::types::group: {
                stream << "is group token";
                break;
            }
            case korobok::token::types::array_numbers: { 
                const auto& resultF = token.value<std::vector<float>>();
                if (resultF.has_value()) {
                    const auto& vecValue = resultF.value().get();
                    if (vecValue.empty()) {
                        stream << "has empty array!\n";
                        continue;
                    }

                    stream << "[";
                    for (auto i : vecValue) {
                        stream << i;
                        if (i != vecValue.back()) 
                            stream << " ";
                    }                    
                    stream << "]";
                    break;
                } 

                const auto& resultI32 = token.value<std::vector<std::int32_t>>();
                if (resultI32.has_value()) {
                    const auto& vecValue = resultI32.value().get();
                    if (vecValue.empty()) {
                        stream << "has empty array!\n";
                        continue;
                    }

                    stream << "[";
                    for (auto i : vecValue) {
                        stream << i;
                        if (i != vecValue.back()) 
                            stream << " ";
                    }                    
                    stream << "]";
                    break;
                }             
                
                return false;
            }
            case korobok::token::types::number: {
                const auto& resultFloat = token.value<float>();
                if (resultFloat.has_value()) {
                    const auto value = resultFloat.value().get(); 
                    stream << "value:" << value;
                    break;
                }                

                const auto& resultInt32 = token.value<std::int32_t>();
                if (resultInt32.has_value()) {
                    const auto value = resultInt32.value().get();
                    stream << "value:" << value;
                    break;
                }  

                const auto& resultInt8 = token.value<std::int8_t>();
                if (resultInt8.has_value()) {
                    const auto value = resultInt8.value().get();
                    stream << "value:" << value;
                    break;
                }                


                return false;
            }
            case korobok::token::types::string: {
                const auto result = token.value<std::string>();
                if (result.has_value()) {
                    stream << "value:" << result.value().get();
                } else {
                    return false;
                }                
                break;
            }
            case korobok::token::types::boolean: {
                const auto result = token.value<bool>();
                if (result.has_value()) {
                    stream << "value:" << (result.value().get() ? "true" : "false");
                } else {
                    return false;
                }                      
                break;
            }
            default: {
                KRB_WARN("Unused value type {}", token.name());
                break;
            }
            stream << "\n";
        }

        KRB_DEBUG("- {}", stream.str());
    }

    return true;
} 

template <typename T>
bool test_get_token_value(std::string_view source, std::string_view name) {    
    korobok::krb data { };
    if (!data.from(source).has_value()) {
        KRB_ERROR("failed to parse source");
        return false;
    }

    const T value = data[name];
    KRB_DEBUG("get token; name: {} value: {}", name, value);

    return true;
}

bool test_dump(std::string_view source) {
    korobok::krb data { };
    if (!data.from(source).has_value()) {
        KRB_ERROR("failed to parse source");
        return false;
    }

    std::string stringValue = data["string_value"];
    if (stringValue.empty()) {
        KRB_ERROR("string_value is empty");
        return false;
    }

    KRB_DEBUG("get 1: {}", stringValue);
    // Change value 
    stringValue = "bye bye world!";
    KRB_DEBUG("get 2: {}", stringValue);
    data["string_value"] = stringValue;

    if (stringValue != std::string(data["string_value"])) { // TODO: check without explicit cast
        KRB_ERROR("wrong value setted for string_value");
        return false;
    }

    // Raw string test
    data["new_value_string"] = "hello";
    const auto saved_type = korobok::token::type_str(data["new_value_string"].type());
    const auto value_type = korobok::token::type_str(korobok::token::value_type(data["new_value_string"].raw_value()));
    KRB_DEBUG("string, saved_type: {}, value_type: {}, value: {}", saved_type, value_type, std::string { data["new_value_string"] });
    if (value_type != saved_type) {
        KRB_ERROR("value_type != saved_type");
        return false;
    }

    data["new_value_init_list_str"] = { "hello", "hi" };
    data["new_value_init_list_num"] = { 1.0f, 2.0f };

    static constexpr auto pi = 3.14f; 
    data["new_value_number"] = pi;
    if (data["new_value_number"] != pi) {
        KRB_ERROR("new_value_number != pi");
        return false;
    }

    std::vector<float> vec = data["number_dec_array_value"];
    for (auto i : vec) {
        KRB_DEBUG("{}", i);
    }

    const auto& dumpData = data.dump();
    KRB_DEBUG("data:\n{}", dumpData);
    if (!dumpData.empty()) {
        korobok::io::save_file("test2.krb" , std::vector<char> { dumpData.begin(), dumpData.end() });
    } else {
        KRB_ERROR("data dump is empty");
        return false;
    }

    return true;
}

int main() {
    try {
        const auto& fileResult = korobok::io::load_file(KRB_TEST_SOURCE_FILENAME);
        if (!fileResult.has_value()) {
            return 0;
        }
        const auto& source = fileResult.value();
        
        KRB_TEST(test_parse_tokens, source);

        KRB_TEST(test_get_token_value<std::int8_t>, source, "number_value_int8");
        KRB_TEST(test_get_token_value<std::int32_t>, source, "number_value");
        KRB_TEST(test_get_token_value<float>, source, "number_dec_value");
        
        KRB_TEST(test_dump, source);

    } catch (const std::bad_variant_access& ex) {
        KRB_ERROR("test failed, got fatal error:{}", ex.what());
    } catch (const std::invalid_argument& ex) {
        KRB_ERROR("test failed, got fatal error:{}", ex.what());
    }
    
    return 1;
}