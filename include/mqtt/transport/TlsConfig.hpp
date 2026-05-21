#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mqtt {

enum class TlsVersion : uint8_t {
    TLS_1_2 = 0,
    TLS_1_3,
};

struct TlsConfig {
    bool        enabled{false};
    TlsVersion  minVersion{TlsVersion::TLS_1_2};

    // CA bundle for broker certificate verification
    std::string caCertFile;

    // Mutual TLS (client authentication)
    std::optional<std::string> clientCertFile;
    std::optional<std::string> clientKeyFile;
    std::optional<std::string> clientKeyPassword;

    // PKCS#12 bundle alternative
    std::optional<std::string> pkcs12File;
    std::optional<std::string> pkcs12Password;

    bool verifyPeer{true};     // MUST remain true in production
    bool verifyHostname{true};

    std::vector<std::string> cipherList;
    std::vector<std::string> cipherSuitesTls13;
    std::vector<std::string> alpnProtocols;
};

} // namespace mqtt
