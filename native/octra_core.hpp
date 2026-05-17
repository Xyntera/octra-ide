#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "../lib/json.hpp"
#include "../lib/tx_builder.hpp"
#include "../wallet.hpp"
#include "../rpc_client.hpp"
#include "../lib/pvac_bridge.hpp"

namespace octra::native {

using json = nlohmann::json;

struct CoreResult {
    bool ok = false;
    std::string error;
    json data = json::object();

    static CoreResult success(json payload = json::object()) {
        return {true, "", std::move(payload)};
    }

    static CoreResult failure(std::string message) {
        return {false, std::move(message), json::object()};
    }
};

class OctraCore {
public:
    OctraCore();
    ~OctraCore();

    CoreResult wallet_status() const;
    CoreResult create_wallet(const std::string& pin);
    CoreResult import_wallet_private_key(const std::string& private_key_b64, const std::string& pin);
    CoreResult import_wallet_mnemonic(const std::string& mnemonic, const std::string& pin, int hd_version = 2);
    CoreResult unlock_wallet(const std::string& pin, const std::string& wallet_path = "");
    CoreResult lock_wallet();
    CoreResult current_wallet() const;
    CoreResult list_wallet_accounts() const;
    CoreResult switch_wallet(const std::string& address, const std::string& pin);
    CoreResult save_settings(const std::string& rpc_url,
                             const std::string& explorer_url,
                             const std::string& bridge_signer_url);
    CoreResult change_pin(const std::string& current_pin, const std::string& new_pin);

    CoreResult get_balance();
    CoreResult get_history(int limit = 20, int offset = 0);
    CoreResult get_tokens();
    CoreResult get_fees();
    CoreResult send(const std::string& to,
                    const std::string& amount,
                    const std::string& fee_ou = "",
                    const std::string& message = "");

    CoreResult compile_assembly(const std::string& source);
    CoreResult compile_aml(const std::string& source);
    CoreResult compile_project(const json& files, const std::string& main_path);
    CoreResult compute_contract_address(const std::string& bytecode);
    CoreResult deploy_contract(const std::string& bytecode,
                               const std::string& params_json,
                               const std::string& fee_ou,
                               const std::string& source = "",
                               const json& abi = json());
    CoreResult call_contract(const std::string& address,
                             const std::string& method,
                             const json& params,
                             const std::string& amount_raw,
                             const std::string& fee_ou);
    CoreResult view_contract(const std::string& address,
                             const std::string& method,
                             const json& params);
    CoreResult verify_contract(const std::string& address,
                               const std::string& source,
                               const json& files = json::array());
    CoreResult contract_info(const std::string& address);
    CoreResult contract_receipt(const std::string& hash);
    CoreResult contract_storage(const std::string& address, const std::string& key);

    CoreResult fhe_encrypt(std::int64_t value);
    CoreResult fhe_decrypt(const std::string& ciphertext_b64);

private:
    mutable std::mutex mutex_;
    octra::Wallet wallet_{};
    octra::RpcClient rpc_{};
    octra::PvacBridge pvac_{};
    bool wallet_loaded_ = false;
    bool pvac_ok_ = false;
    std::string wallet_path_ = octra::WALLET_FILE;
    std::string pin_;

    static double now_ts();
    static std::string default_rpc_url();
    static std::string parse_fee_ou(const std::string& fee_ou, const std::string& fallback);
    static std::int64_t parse_amount_raw(const std::string& amount);
    static json manifest_entry_to_json(const octra::ManifestEntry& entry);
    static json wallet_to_json(const octra::Wallet& wallet, const std::string& path);

    void reset_session_locked();
    void init_wallet_subsystems_locked();
    CoreResult require_wallet_locked() const;
    CoreResult submit_transaction_locked(octra::Transaction& tx);
    std::pair<int, std::string> nonce_and_balance_locked();
    void sign_tx_locked(octra::Transaction& tx);
};

} // namespace octra::native
