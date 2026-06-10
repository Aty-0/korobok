#include <krb.h>

#include <fstream>
#include <vector>
#include <optional>
#include <sstream>

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

static void test_parse_tokens(std::string_view source) {
    korobok::krb data { };
    const auto& result = data.from(source);
    if (!result.has_value()) {
        KRB_ERROR("failed to read sources");
        return;
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
                const auto& result = token.value<std::vector<float>>();
                if (result.has_value()) {
                    const auto& vecValue = result.value().get();
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
                }
                
                break;
            }
            case korobok::token::types::number: {
                const auto result = token.value<float>();
                if (result.has_value()) {
                    stream << "value:" << result.value().get();
                }
                break;
            }
            case korobok::token::types::string: {
                const auto result = token.value<std::string>();
                if (result.has_value()) {
                    stream << "value:" << result.value().get();
                }
                break;
            }
            case korobok::token::types::boolean: {
                const auto result = token.value<bool>();
                if (result.has_value()) {
                    stream << "value:" << (result.value().get() ? "true" : "false");
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
} 

void test_get_token_value_float(std::string_view source, std::string_view name) {    
    korobok::krb data { };
    if (!data.from(source).has_value()) {
        KRB_ERROR("failed to parse source");
        return;
    }

    const float floatValue = data[name];
    KRB_DEBUG("get token; name: {} value: {}", name, floatValue);
}

void test_dump(std::string_view source) {
    korobok::krb data { };
    if (!data.from(source).has_value()) {
        KRB_ERROR("failed to parse source");
        return;
    }

    std::string stringValue = data["string_value"];
    KRB_DEBUG("get 1: {}", stringValue);
    // Change value 
    stringValue = "bye bye world!";
    KRB_DEBUG("get 2: {}", stringValue);
    data["string_value"] = stringValue;

    // Raw string test
    data["new_value_string"] = "hello";
    const auto saved_type = korobok::token::type_str(data["new_value_string"].type());
    const auto value_type = korobok::token::type_str(korobok::token::value_type(data["new_value_string"].raw_variant()));
    KRB_DEBUG("string, saved_type: {}, value_type: {}, value: {}", saved_type, value_type, static_cast<const char*>(data["new_value_string"]));
    
    data["new_value_number"] = 3.14f;
    
    std::vector<float> vec = data["number_array_value"];
    for (auto i : vec) {
        KRB_DEBUG("{}", i);
    }

    const auto& dumpData = data.dump();
    KRB_DEBUG("data:\n{}", dumpData);
    if (!dumpData.empty()) {
        korobok::io::save_file("test2.krb" , std::vector<char> { dumpData.begin(), dumpData.end() });
    }
}

int main() {
    try {
        const auto& fileResult = korobok::io::load_file(KRB_TEST_SOURCE_FILENAME);
        if (!fileResult.has_value()) {
            return 0;
        }
        const auto& source = fileResult.value();
        
        KRB_DEBUG("========================");
        KRB_DEBUG("test_parse_tokens");
        KRB_DEBUG("========================");

        test_parse_tokens(source);

        KRB_DEBUG("========================");
        KRB_DEBUG("test_get_token_value_float");
        KRB_DEBUG("========================");

        test_get_token_value_float(source, "number_value");
        test_get_token_value_float(source, "number_dec_value");

        KRB_DEBUG("========================");
        KRB_DEBUG("test_dump");
        KRB_DEBUG("========================");

        test_dump(source);

    } catch (const std::bad_variant_access& ex) {
        KRB_ERROR("fatal:{}", ex.what());
    } catch (const std::invalid_argument& ex) {
        KRB_ERROR("fatal:{}", ex.what());
    }
    
    return 1;
}