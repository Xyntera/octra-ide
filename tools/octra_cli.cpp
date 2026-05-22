#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "native/octra_core.hpp"

using octra::native::CoreResult;
using octra::native::OctraCore;

namespace {

struct Options {
    bool json = false;
    bool preview = false;
    std::string wallet_path = "data/wallet.oct";
    std::string pin;
    std::string private_key;
    std::string rpc_url;
    std::string explorer_url;
    std::string bridge_signer_url;
    std::string source_path;
    std::string bytecode;
    std::string params_json = "[]";
    std::string fee_ou;
    std::string language = "aml";
    std::string network = "devnet";
    std::string template_name;
};

std::string read_text_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open file: " + path);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string trim_copy(std::string value) {
    const char* ws = " \t\r\n";
    const auto start = value.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    const auto end = value.find_last_not_of(ws);
    return value.substr(start, end - start + 1);
}

void print_json(const CoreResult& result, const std::string& command) {
    nlohmann::json out;
    out["command"] = command;
    out["ok"] = result.ok;
    if (!result.error.empty()) out["error"] = result.error;
    if (!result.data.is_null() && !result.data.empty()) out["data"] = result.data;
    std::cout << out.dump(2) << '\n';
}

void print_plain(const CoreResult& result, const std::string& success_label) {
    if (!result.ok) {
        std::cerr << "error: " << result.error << '\n';
        return;
    }
    std::cout << success_label << '\n';
    if (!result.data.is_null() && !result.data.empty()) {
        std::cout << result.data.dump(2) << '\n';
    }
}

void print_result(const CoreResult& result, const std::string& command, const Options& opts, const std::string& success_label) {
    if (opts.json) {
        print_json(result, command);
    } else {
        print_plain(result, success_label);
    }
}

struct NetworkPreset {
    std::string rpc_url;
    std::string explorer_url;
    std::string bridge_signer_url;
};

NetworkPreset preset_for(const std::string& network) {
    if (network == "mainnet") {
        return {
            "http://46.101.86.250:8080",
            "https://octrascan.io",
            "https://relayer-002838819188.octra.network"
        };
    }
    return {
        "http://165.227.225.79:8080",
        "https://devnet.octrascan.io",
        ""
    };
}

std::string template_source_path(const std::string& template_name) {
    if (template_name == "vault") return "static/templates/vault/main.aml";
    if (template_name == "empty") return "static/templates/empty/main.aml";
    if (template_name == "amm") return "static/templates/amm/main.aml";
    if (template_name == "escrow") return "static/templates/escrow/main.aml";
    if (template_name == "multisig") return "static/templates/multisig/main.aml";
    if (template_name == "token") return "static/templates/token/main.aml";
    if (template_name == "dictionary") return "static/templates/dictionary/main.aml";
    return "";
}

std::string detect_language(const Options& opts) {
    if (!opts.language.empty()) return opts.language;
    if (!opts.source_path.empty()) {
        if (ends_with(opts.source_path, ".asm") || ends_with(opts.source_path, ".s")) return "assembly";
    }
    return "aml";
}

void usage() {
    std::cout <<
        "octra-cli - Octra contract deployment helper\n\n"
        "Usage:\n"
        "  octra-cli doctor [options]\n"
        "  octra-cli compile --source FILE [options]\n"
        "  octra-cli deploy  --source FILE | --bytecode BYTECODE [options]\n\n"
        "Core options:\n"
        "  --wallet PATH        Wallet file path (default: data/wallet.oct)\n"
        "  --pin PIN            6-digit wallet PIN\n"
        "  --private-key B64    Ephemeral base64 private key for one-shot deploys\n"
        "  --network NAME       devnet or mainnet (default: devnet)\n"
        "  --rpc-url URL        Custom RPC endpoint\n"
        "  --json               Machine-readable output\n\n"
        "Compile options:\n"
        "  --source FILE        AML or assembly source file\n"
        "  --language aml|assembly\n"
        "  --template NAME      Use a local example template such as vault or dictionary\n\n"
        "Deploy options:\n"
        "  --bytecode VALUE     Precompiled bytecode\n"
        "  --params JSON        Constructor params JSON array (default: [])\n"
        "  --fee OU             Deploy fee in OU\n"
        "  --preview            Print computed contract address only\n";
}

bool parse_args(int argc, char** argv, std::string& command, Options& opts) {
    if (argc < 2) return false;
    command = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        auto need_value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + name);
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            command = "help";
            return true;
        } else if (arg == "--json") {
            opts.json = true;
        } else if (arg == "--preview") {
            opts.preview = true;
        } else if (arg == "--wallet") {
            opts.wallet_path = need_value("--wallet");
        } else if (arg == "--pin") {
            opts.pin = need_value("--pin");
        } else if (arg == "--private-key") {
            opts.private_key = need_value("--private-key");
        } else if (arg == "--rpc-url") {
            opts.rpc_url = need_value("--rpc-url");
        } else if (arg == "--explorer-url") {
            opts.explorer_url = need_value("--explorer-url");
        } else if (arg == "--bridge-signer-url") {
            opts.bridge_signer_url = need_value("--bridge-signer-url");
        } else if (arg == "--source") {
            opts.source_path = need_value("--source");
        } else if (arg == "--bytecode") {
            opts.bytecode = need_value("--bytecode");
        } else if (arg == "--params") {
            opts.params_json = need_value("--params");
        } else if (arg == "--fee") {
            opts.fee_ou = need_value("--fee");
        } else if (arg == "--language") {
            opts.language = need_value("--language");
        } else if (arg == "--network") {
            opts.network = need_value("--network");
        } else if (arg == "--template") {
            opts.template_name = need_value("--template");
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    return true;
}

CoreResult unlock_and_configure(OctraCore& core, const Options& opts) {
    NetworkPreset preset = preset_for(opts.network);
    std::string rpc_url = opts.rpc_url.empty() ? preset.rpc_url : opts.rpc_url;
    std::string explorer_url = opts.explorer_url.empty() ? preset.explorer_url : opts.explorer_url;
    std::string bridge_signer_url = opts.bridge_signer_url.empty() ? preset.bridge_signer_url : opts.bridge_signer_url;

    if (!opts.private_key.empty()) {
        auto loaded = core.load_private_key_ephemeral(opts.private_key, rpc_url, explorer_url, bridge_signer_url);
        if (!loaded.ok) return loaded;
        return core.current_wallet();
    }

    if (opts.pin.empty()) return CoreResult::failure("missing --pin");
    auto result = core.unlock_wallet(opts.pin, opts.wallet_path);
    if (!result.ok) return result;

    if (!rpc_url.empty()) {
        auto saved = core.save_settings(rpc_url, explorer_url, bridge_signer_url);
        if (!saved.ok) return saved;
    }
    return core.current_wallet();
}

CoreResult compile_source(OctraCore& core, const Options& opts, const std::string& source_text) {
    const std::string language = detect_language(opts);
    if (language == "assembly") return core.compile_assembly(source_text);
    return core.compile_aml(source_text);
}

CoreResult compile_for_deploy(OctraCore& core, const Options& opts, std::string& bytecode_out, nlohmann::json& abi_out) {
    if (!opts.bytecode.empty()) {
        bytecode_out = opts.bytecode;
        abi_out = nlohmann::json::object();
        return CoreResult::success();
    }
    std::string source_path = opts.source_path;
    if (source_path.empty() && !opts.template_name.empty()) {
        source_path = template_source_path(opts.template_name);
    }
    if (source_path.empty()) return CoreResult::failure("provide --source, --template, or --bytecode");
    std::string source_text = read_text_file(source_path);
    auto compiled = compile_source(core, opts, source_text);
    if (!compiled.ok) return compiled;
    bytecode_out = compiled.data.value("bytecode", "");
    abi_out = compiled.data.contains("abi") ? compiled.data["abi"] : nlohmann::json::object();
    if (bytecode_out.empty()) return CoreResult::failure("compile did not return bytecode");
    return compiled;
}

int run_doctor(OctraCore& core, const Options& opts) {
    if (!opts.private_key.empty()) {
        auto preset = preset_for(opts.network);
        auto loaded = core.load_private_key_ephemeral(
            opts.private_key,
            opts.rpc_url.empty() ? preset.rpc_url : opts.rpc_url,
            opts.explorer_url.empty() ? preset.explorer_url : opts.explorer_url,
            opts.bridge_signer_url.empty() ? preset.bridge_signer_url : opts.bridge_signer_url);
        if (!loaded.ok) {
            if (opts.json) print_json(loaded, "doctor");
            else print_plain(loaded, "doctor failed");
            return 1;
        }
        auto fees = core.get_fees();
        nlohmann::json out{
            {"wallet", loaded.data.value("wallet", nlohmann::json::object())},
            {"fees_ok", fees.ok}
        };
        if (fees.ok) out["fees"] = fees.data;
        if (!fees.ok) out["fees_error"] = fees.error;
        CoreResult result = fees.ok ? CoreResult::success(out) : CoreResult::failure(fees.error);
        if (opts.json) print_json(result, "doctor");
        else print_plain(result, "doctor");
        return result.ok ? 0 : 1;
    }

    auto status = core.wallet_status();
    if (!opts.pin.empty()) {
        auto configured = unlock_and_configure(core, opts);
        if (!configured.ok) {
            if (opts.json) print_json(configured, "doctor");
            else print_plain(configured, "doctor failed");
            return 1;
        }

        auto fees = core.get_fees();
        nlohmann::json out{
            {"wallet", configured.data.value("wallet", nlohmann::json::object())},
            {"fees_ok", fees.ok}
        };
        if (fees.ok) out["fees"] = fees.data;
        if (!fees.ok) out["fees_error"] = fees.error;
        CoreResult result = fees.ok ? CoreResult::success(out) : CoreResult::failure(fees.error);
        if (opts.json) print_json(result, "doctor");
        else print_plain(result, "doctor");
        return result.ok ? 0 : 1;
    }

    if (!opts.json) {
        print_plain(status, "wallet status");
        return status.ok ? 0 : 1;
    }
    print_json(status, "doctor");
    return status.ok ? 0 : 1;
}

int run_compile(OctraCore& core, const Options& opts) {
    auto configured = unlock_and_configure(core, opts);
    if (!configured.ok) {
        if (opts.json) print_json(configured, "compile");
        else print_plain(configured, "compile failed");
        return 1;
    }
    std::string source_path = opts.source_path;
    if (source_path.empty() && !opts.template_name.empty()) {
        source_path = template_source_path(opts.template_name);
    }
    if (source_path.empty()) {
        std::cerr << "error: provide --source or --template\n";
        return 1;
    }
    std::string source_text = read_text_file(source_path);
    auto compiled = compile_source(core, opts, source_text);
    if (opts.json) {
        print_json(compiled, "compile");
    } else {
        print_plain(compiled, "compile succeeded");
    }
    return compiled.ok ? 0 : 1;
}

int run_deploy(OctraCore& core, const Options& opts) {
    auto configured = unlock_and_configure(core, opts);
    if (!configured.ok) {
        if (opts.json) print_json(configured, "deploy");
        else print_plain(configured, "deploy failed");
        return 1;
    }

    std::string bytecode;
    nlohmann::json abi = nlohmann::json::object();
    auto prepared = compile_for_deploy(core, opts, bytecode, abi);
    if (!prepared.ok) {
        if (opts.json) print_json(prepared, "deploy");
        else print_plain(prepared, "deploy failed");
        return 1;
    }

    auto preview = core.compute_contract_address(bytecode);
    if (!preview.ok) {
        if (opts.json) print_json(preview, "deploy");
        else print_plain(preview, "deploy failed");
        return 1;
    }
    const std::string contract_address = preview.data.value("address", "");

    if (opts.preview) {
        nlohmann::json out{{"contract_address", contract_address}, {"bytecode", bytecode}};
        CoreResult result = CoreResult::success(out);
        if (opts.json) print_json(result, "deploy");
        else print_plain(result, "deploy preview");
        return 0;
    }

    std::string deploy_fee = opts.fee_ou;
    if (deploy_fee.empty()) {
        auto fees = core.get_fees();
        if (fees.ok) deploy_fee = fees.data.value("deploy", nlohmann::json::object()).value("recommended", "50000000");
    }
    auto deployed = core.deploy_contract(bytecode, opts.params_json, deploy_fee, opts.source_path, abi);
    if (deployed.ok && !deployed.data.contains("contract_address")) {
        deployed.data["contract_address"] = contract_address;
    }
    if (opts.json) {
        print_json(deployed, "deploy");
    } else {
        print_plain(deployed, "deploy submitted");
    }
    return deployed.ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            usage();
            return 0;
        }
        const std::string first = argv[1];
        if (first == "--help" || first == "-h") {
            usage();
            return 0;
        }
        std::string command;
        Options opts;
        if (!parse_args(argc, argv, command, opts) || command == "help") {
            usage();
            return command == "help" ? 0 : 1;
        }

        OctraCore core;
        if (command == "doctor") return run_doctor(core, opts);
        if (command == "compile") return run_compile(core, opts);
        if (command == "deploy") return run_deploy(core, opts);

        std::cerr << "unknown command: " << command << "\n\n";
        usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
