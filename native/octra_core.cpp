#include "octra_core.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>

#include "../crypto_utils.hpp"
#include "../lib/tx_builder.hpp"

extern "C" {
#include "../pvac/pvac_c_api.h"
}

namespace octra::native {

namespace {

json error_json(const std::string& error) {
    return json{{"error", error}};
}

std::string sanitize_error(std::string error) {
    for (char& ch : error) {
        if (static_cast<unsigned char>(ch) > 127) ch = '?';
    }
    return error;
}

} // namespace

OctraCore::OctraCore() = default;

OctraCore::~OctraCore() {
    std::lock_guard<std::mutex> lock(mutex_);
    reset_session_locked();
}

double OctraCore::now_ts() {
    auto d = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>(d).count();
}

std::string OctraCore::default_rpc_url() {
    const char* env_rpc = std::getenv("OCTRA_RPC_URL");
    if (env_rpc && *env_rpc) return env_rpc;
    return "http://127.0.0.1:8080";
}

std::string OctraCore::parse_fee_ou(const std::string& fee_ou, const std::string& fallback) {
    if (fee_ou.empty()) return fallback;
    if (!std::all_of(fee_ou.begin(), fee_ou.end(), ::isdigit)) return fallback;
    return fee_ou;
}

std::int64_t OctraCore::parse_amount_raw(const std::string& amount) {
    if (amount.empty()) return -1;
    const std::int64_t max_raw = 1000000000LL * 1000000LL;
    auto dot = amount.find('.');
    if (dot == std::string::npos) {
        if (!std::all_of(amount.begin(), amount.end(), ::isdigit)) return -1;
        std::int64_t v = std::stoll(amount);
        if (v > max_raw / 1000000) return -1;
        return v * 1000000;
    }
    std::string integer_part = amount.substr(0, dot);
    std::string frac_part = amount.substr(dot + 1);
    if (integer_part.empty() && frac_part.empty()) return -1;
    if ((!integer_part.empty() && !std::all_of(integer_part.begin(), integer_part.end(), ::isdigit)) ||
        (!frac_part.empty() && !std::all_of(frac_part.begin(), frac_part.end(), ::isdigit))) {
        return -1;
    }
    if (frac_part.size() > 6) frac_part = frac_part.substr(0, 6);
    while (frac_part.size() < 6) frac_part.push_back('0');
    std::int64_t ip = integer_part.empty() ? 0 : std::stoll(integer_part);
    if (ip > max_raw / 1000000) return -1;
    std::int64_t fp = frac_part.empty() ? 0 : std::stoll(frac_part);
    return ip * 1000000 + fp;
}

json OctraCore::manifest_entry_to_json(const octra::ManifestEntry& entry) {
    json out{
        {"name", entry.name},
        {"file", entry.file},
        {"addr", entry.addr},
        {"hd", entry.hd},
        {"hd_version", entry.hd_version},
        {"hd_index", entry.hd_index}
    };
    if (!entry.parent_addr.empty()) out["parent_addr"] = entry.parent_addr;
    if (!entry.master_seed_hash.empty()) out["seed_hash"] = entry.master_seed_hash;
    return out;
}

json OctraCore::wallet_to_json(const octra::Wallet& wallet, const std::string& path) {
    return json{
        {"addr", wallet.addr},
        {"rpc_url", wallet.rpc_url},
        {"explorer_url", wallet.explorer_url},
        {"bridge_signer_url", wallet.bridge_signer_url},
        {"pub_b64", wallet.pub_b64},
        {"hd_index", wallet.hd_index},
        {"hd_version", wallet.hd_version},
        {"has_master_seed", wallet.has_master_seed()},
        {"wallet_path", path}
    };
}

void OctraCore::reset_session_locked() {
    wallet_loaded_ = false;
    pvac_ok_ = false;
    pvac_.reset();
    if (!pin_.empty()) octra::secure_zero(pin_.data(), pin_.size());
    pin_.clear();
    octra::secure_zero(wallet_.sk, 64);
    octra::secure_zero(wallet_.pk, 32);
    wallet_ = octra::Wallet{};
    wallet_path_ = octra::WALLET_FILE;
}

void OctraCore::init_wallet_subsystems_locked() {
    if (!wallet_.rpc_url.empty()) rpc_.set_url(wallet_.rpc_url);
    else rpc_.set_url(default_rpc_url());
    pvac_ok_ = pvac_.init(wallet_.priv_b64);
    wallet_loaded_ = true;
}

CoreResult OctraCore::require_wallet_locked() const {
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    return CoreResult::success();
}

std::pair<int, std::string> OctraCore::nonce_and_balance_locked() {
    auto r = rpc_.get_balance(wallet_.addr);
    if (!r.ok) return {0, "0"};
    int nonce = r.result.value("pending_nonce", r.result.value("nonce", 0));
    std::string raw = "0";
    if (r.result.contains("balance_raw")) {
        const auto& value = r.result["balance_raw"];
        raw = value.is_string() ? value.get<std::string>() : std::to_string(value.get<long long>());
    }
    return {nonce, raw};
}

void OctraCore::sign_tx_locked(octra::Transaction& tx) {
    std::string msg = octra::canonical_json(tx);
    tx.signature = octra::ed25519_sign_detached(
        reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), wallet_.sk);
    tx.public_key = wallet_.pub_b64;
}

CoreResult OctraCore::submit_transaction_locked(octra::Transaction& tx) {
    sign_tx_locked(tx);
    auto r = rpc_.submit_tx(octra::build_tx_json(tx));
    if (!r.ok) return CoreResult::failure(r.error);
    json result{{"tx_hash", r.result.value("tx_hash", "")}};
    return CoreResult::success(result);
}

CoreResult OctraCore::wallet_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto wallets = octra::scan_and_merge_oct_files();
    json found = json::array();
    for (const auto& entry : wallets) found.push_back(manifest_entry_to_json(entry));
    return CoreResult::success({
        {"loaded", wallet_loaded_},
        {"needs_pin", !wallets.empty() || octra::has_legacy_wallet()},
        {"needs_create", wallets.empty() && !octra::has_legacy_wallet()},
        {"wallets", found}
    });
}

CoreResult OctraCore::create_wallet(const std::string& pin) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pin.size() != 6 || !std::all_of(pin.begin(), pin.end(), ::isdigit)) {
        return CoreResult::failure("PIN must be 6 digits");
    }
    octra::ensure_data_dir();
    auto created = octra::create_wallet(octra::WALLET_FILE, pin);
    wallet_ = created.first;
    wallet_path_ = octra::WALLET_FILE;
    pin_ = pin;
    init_wallet_subsystems_locked();
    octra::manifest_upsert({ "Primary", wallet_path_, wallet_.addr, true, wallet_.hd_version, wallet_.hd_index, "", octra::compute_seed_hash(wallet_.master_seed_b64) });
    return CoreResult::success({
        {"wallet", wallet_to_json(wallet_, wallet_path_)},
        {"mnemonic", created.second}
    });
}

CoreResult OctraCore::import_wallet_private_key(const std::string& private_key_b64, const std::string& pin) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pin.size() != 6 || !std::all_of(pin.begin(), pin.end(), ::isdigit)) {
        return CoreResult::failure("PIN must be 6 digits");
    }
    wallet_ = octra::import_wallet(octra::WALLET_FILE, private_key_b64, pin);
    wallet_path_ = octra::WALLET_FILE;
    pin_ = pin;
    init_wallet_subsystems_locked();
    octra::manifest_upsert({"Imported", wallet_path_, wallet_.addr, false, 1, 0, "", ""});
    return CoreResult::success({{"wallet", wallet_to_json(wallet_, wallet_path_)}});
}

CoreResult OctraCore::import_wallet_mnemonic(const std::string& mnemonic, const std::string& pin, int hd_version) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pin.size() != 6 || !std::all_of(pin.begin(), pin.end(), ::isdigit)) {
        return CoreResult::failure("PIN must be 6 digits");
    }
    wallet_ = octra::import_wallet_mnemonic(octra::WALLET_FILE, mnemonic, pin, hd_version);
    wallet_path_ = octra::WALLET_FILE;
    pin_ = pin;
    init_wallet_subsystems_locked();
    octra::manifest_upsert({"Imported Seed", wallet_path_, wallet_.addr, true, wallet_.hd_version, wallet_.hd_index, "", octra::compute_seed_hash(wallet_.master_seed_b64)});
    return CoreResult::success({{"wallet", wallet_to_json(wallet_, wallet_path_)}});
}

CoreResult OctraCore::unlock_wallet(const std::string& pin, const std::string& wallet_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = wallet_path.empty() ? octra::WALLET_FILE : wallet_path;
    try {
        if (octra::has_legacy_wallet() && path == octra::WALLET_FILE && !octra::has_encrypted_wallet()) {
            wallet_ = octra::migrate_wallet(pin);
        } else {
            wallet_ = octra::load_wallet_encrypted(path, pin);
        }
        wallet_path_ = path;
        pin_ = pin;
        init_wallet_subsystems_locked();
        return CoreResult::success({{"wallet", wallet_to_json(wallet_, wallet_path_)}});
    } catch (const std::exception& e) {
        return CoreResult::failure(e.what());
    }
}

CoreResult OctraCore::lock_wallet() {
    std::lock_guard<std::mutex> lock(mutex_);
    reset_session_locked();
    return CoreResult::success({{"locked", true}});
}

CoreResult OctraCore::current_wallet() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    return CoreResult::success({{"wallet", wallet_to_json(wallet_, wallet_path_)}});
}

CoreResult OctraCore::list_wallet_accounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entries = octra::scan_and_merge_oct_files();
    json wallets = json::array();
    for (const auto& entry : entries) wallets.push_back(manifest_entry_to_json(entry));
    return CoreResult::success({{"wallets", wallets}});
}

CoreResult OctraCore::switch_wallet(const std::string& address, const std::string& pin) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entries = octra::scan_and_merge_oct_files();
    auto it = std::find_if(entries.begin(), entries.end(), [&](const octra::ManifestEntry& entry) {
        return entry.addr == address;
    });
    if (it == entries.end()) return CoreResult::failure("wallet not found");
    try {
        wallet_ = octra::load_wallet_encrypted(it->file, pin);
        wallet_path_ = it->file;
        pin_ = pin;
        init_wallet_subsystems_locked();
        return CoreResult::success({{"wallet", wallet_to_json(wallet_, wallet_path_)}});
    } catch (const std::exception& e) {
        return CoreResult::failure(e.what());
    }
}

CoreResult OctraCore::save_settings(const std::string& rpc_url,
                                    const std::string& explorer_url,
                                    const std::string& bridge_signer_url) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (rpc_url.empty()) return CoreResult::failure("rpc_url required");
    wallet_.rpc_url = rpc_url;
    if (!explorer_url.empty()) wallet_.explorer_url = explorer_url;
    wallet_.bridge_signer_url = bridge_signer_url;
    octra::save_wallet_encrypted(wallet_path_, wallet_, pin_);
    rpc_.set_url(wallet_.rpc_url);
    return CoreResult::success({{"wallet", wallet_to_json(wallet_, wallet_path_)}});
}

CoreResult OctraCore::change_pin(const std::string& current_pin, const std::string& new_pin) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (current_pin != pin_) return CoreResult::failure("wrong current PIN");
    if (new_pin.size() != 6 || !std::all_of(new_pin.begin(), new_pin.end(), ::isdigit)) {
        return CoreResult::failure("new PIN must be 6 digits");
    }
    octra::save_wallet_encrypted(wallet_path_, wallet_, new_pin);
    pin_ = new_pin;
    return CoreResult::success({{"ok", true}});
}

CoreResult OctraCore::get_balance() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    auto [nonce, balance_raw] = nonce_and_balance_locked();
    json out{
        {"public_balance", balance_raw},
        {"nonce", nonce},
        {"encrypted_balance", "0"},
        {"pvac_available", pvac_ok_}
    };
    if (pvac_ok_) {
        std::string sig = octra::sign_balance_request(wallet_.addr, wallet_.sk);
        auto enc = rpc_.get_encrypted_balance(wallet_.addr, sig, wallet_.pub_b64);
        if (enc.ok && enc.result.is_object()) {
            std::string cipher = enc.result.value("cipher", "0");
            out["encrypted_balance"] = std::to_string(pvac_.get_balance(cipher));
        }
    }
    return CoreResult::success(out);
}

CoreResult OctraCore::get_history(int limit, int offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    auto r = rpc_.get_txs_by_address(wallet_.addr, limit, offset);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success(r.result);
}

CoreResult OctraCore::get_tokens() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    auto r = rpc_.tokens_by_address(wallet_.addr);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success(r.result);
}

CoreResult OctraCore::get_fees() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> ops = {"standard", "encrypt", "decrypt", "stealth", "claim", "deploy", "call"};
    std::vector<std::string> methods(ops.size(), "octra_recommendedFee");
    std::vector<json> params;
    for (const auto& op : ops) params.push_back(json::array({op}));
    auto results = rpc_.call_batch(methods, params, 10);
    json fees = json::object();
    for (size_t i = 0; i < ops.size(); ++i) {
        if (i < results.size() && results[i].ok) fees[ops[i]] = results[i].result;
        else fees[ops[i]] = {{"minimum", "1000"}, {"recommended", "1000"}, {"fast", "2000"}};
    }
    return CoreResult::success(fees);
}

CoreResult OctraCore::send(const std::string& to,
                           const std::string& amount,
                           const std::string& fee_ou,
                           const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (to.size() != 47 || to.rfind("oct", 0) != 0) return CoreResult::failure("invalid address");
    std::int64_t raw = parse_amount_raw(amount);
    if (raw <= 0) return CoreResult::failure("invalid amount");
    auto [nonce, balance_raw] = nonce_and_balance_locked();
    (void)balance_raw;
    octra::Transaction tx;
    tx.from = wallet_.addr;
    tx.to_ = to;
    tx.amount = std::to_string(raw);
    tx.nonce = nonce + 1;
    tx.ou = parse_fee_ou(fee_ou, raw < 1000000000 ? "10000" : "30000");
    tx.timestamp = now_ts();
    tx.op_type = "standard";
    tx.message = message;
    return submit_transaction_locked(tx);
}

CoreResult OctraCore::compile_assembly(const std::string& source) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (source.empty()) return CoreResult::failure("source required");
    auto r = rpc_.compile_assembly(source);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success({
        {"bytecode", r.result.value("bytecode", "")},
        {"size", r.result.value("size", 0)},
        {"instructions", r.result.value("instructions", 0)}
    });
}

CoreResult OctraCore::compile_aml(const std::string& source) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (source.empty()) return CoreResult::failure("source required");
    auto r = rpc_.compile_aml(source);
    if (!r.ok) return CoreResult::failure(sanitize_error(r.error));
    json out{
        {"bytecode", r.result.value("bytecode", "")},
        {"size", r.result.value("size", 0)},
        {"instructions", r.result.value("instructions", 0)},
        {"version", r.result.value("version", "")}
    };
    if (r.result.contains("abi")) out["abi"] = r.result["abi"];
    if (r.result.contains("disasm")) out["disasm"] = r.result["disasm"];
    return CoreResult::success(out);
}

CoreResult OctraCore::compile_project(const json& files, const std::string& main_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!files.is_array() || files.empty()) return CoreResult::failure("files required");
    auto r = rpc_.compile_aml_multi(files, main_path);
    if (!r.ok) return CoreResult::failure(sanitize_error(r.error));
    json out{
        {"bytecode", r.result.value("bytecode", "")},
        {"size", r.result.value("size", 0)},
        {"instructions", r.result.value("instructions", 0)},
        {"version", r.result.value("version", "")}
    };
    if (r.result.contains("abi")) out["abi"] = r.result["abi"];
    if (r.result.contains("disasm")) out["disasm"] = r.result["disasm"];
    return CoreResult::success(out);
}

CoreResult OctraCore::compute_contract_address(const std::string& bytecode) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    auto [nonce, _] = nonce_and_balance_locked();
    auto r = rpc_.compute_contract_address(bytecode, wallet_.addr, nonce + 1);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success(r.result);
}

CoreResult OctraCore::deploy_contract(const std::string& bytecode,
                                      const std::string& params_json,
                                      const std::string& fee_ou,
                                      const std::string& source,
                                      const json& abi) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (bytecode.empty()) return CoreResult::failure("bytecode required");
    auto [nonce, _] = nonce_and_balance_locked();
    auto addr = rpc_.compute_contract_address(bytecode, wallet_.addr, nonce + 1);
    if (!addr.ok) return CoreResult::failure(addr.error);
    octra::Transaction tx;
    tx.from = wallet_.addr;
    tx.to_ = addr.result.value("address", "");
    tx.amount = "0";
    tx.nonce = nonce + 1;
    tx.ou = parse_fee_ou(fee_ou, "50000000");
    tx.timestamp = now_ts();
    tx.op_type = "deploy";
    tx.encrypted_data = bytecode;
    tx.message = params_json;
    auto submitted = submit_transaction_locked(tx);
    if (submitted.ok) {
        submitted.data["contract_address"] = tx.to_;
        if (!source.empty()) submitted.data["source"] = source;
        if (!abi.is_null() && !abi.empty()) submitted.data["abi"] = abi;
    }
    return submitted;
}

CoreResult OctraCore::call_contract(const std::string& address,
                                    const std::string& method,
                                    const json& params,
                                    const std::string& amount_raw,
                                    const std::string& fee_ou) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (address.empty() || method.empty()) return CoreResult::failure("address and method required");
    auto [nonce, _] = nonce_and_balance_locked();
    octra::Transaction tx;
    tx.from = wallet_.addr;
    tx.to_ = address;
    tx.amount = amount_raw.empty() ? "0" : amount_raw;
    tx.nonce = nonce + 1;
    tx.ou = parse_fee_ou(fee_ou, "1000");
    tx.timestamp = now_ts();
    tx.op_type = "call";
    tx.encrypted_data = method;
    tx.message = params.dump();
    return submit_transaction_locked(tx);
}

CoreResult OctraCore::view_contract(const std::string& address,
                                    const std::string& method,
                                    const json& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    auto r = rpc_.contract_call_view(address, method, params, wallet_.addr);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success(r.result);
}

CoreResult OctraCore::verify_contract(const std::string& address,
                                      const std::string& source,
                                      const json& files) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    json params = json::array({address, source});
    if (files.is_array() && !files.empty()) params.push_back(files);
    auto r = rpc_.call("contract_verify", params, 15);
    if (!r.ok) return CoreResult::failure(sanitize_error(r.error));
    return CoreResult::success(r.result);
}

CoreResult OctraCore::contract_info(const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto r = rpc_.vm_contract(address);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success(r.result);
}

CoreResult OctraCore::contract_receipt(const std::string& hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto r = rpc_.contract_receipt(hash);
    if (!r.ok) return CoreResult::failure(r.error);
    return CoreResult::success(r.result);
}

CoreResult OctraCore::contract_storage(const std::string& address, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (address.empty() || key.empty()) return CoreResult::failure("address and key required");
    auto r = rpc_.contract_storage(address, key);
    if (!r.ok) return CoreResult::failure(r.error);
    json out;
    if (r.result.contains("value") && !r.result["value"].is_null()) out["value"] = r.result["value"];
    else out["value"] = nullptr;
    return CoreResult::success(out);
}

CoreResult OctraCore::fhe_encrypt(std::int64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (!pvac_ok_) return CoreResult::failure("pvac not available");
    uint8_t seed[32];
    octra::random_bytes(seed, 32);
    pvac_cipher ct = pvac_.encrypt(static_cast<uint64_t>(value), seed);
    auto data = pvac_.serialize_cipher(ct);
    std::string ciphertext_b64 = octra::base64_encode(data.data(), data.size());
    uint8_t blinding[32];
    octra::random_bytes(blinding, 32);
    auto amount_commitment = pvac_.pedersen_commit(static_cast<uint64_t>(value), blinding);
    std::string amount_commitment_b64 = octra::base64_encode(amount_commitment.data(), amount_commitment.size());
    pvac_zero_proof proof = pvac_.make_zero_proof_bound(ct, static_cast<uint64_t>(value), blinding);
    std::string zero_proof = pvac_.encode_zero_proof(proof);
    pvac_.free_zero_proof(proof);
    pvac_.free_cipher(ct);
    return CoreResult::success({
        {"ciphertext", ciphertext_b64},
        {"amount_commitment", amount_commitment_b64},
        {"zero_proof", zero_proof},
        {"proof_kind", "bound_zero_v1"}
    });
}

CoreResult OctraCore::fhe_decrypt(const std::string& ciphertext_b64) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!wallet_loaded_) return CoreResult::failure("no wallet loaded");
    if (!pvac_ok_) return CoreResult::failure("pvac not available");
    auto raw = octra::base64_decode(ciphertext_b64);
    if (raw.empty()) return CoreResult::failure("invalid base64");
    pvac_cipher ct = pvac_.deserialize_cipher(raw.data(), raw.size());
    if (!ct) return CoreResult::failure("invalid ciphertext");
    uint64_t lo = 0, hi = 0;
    pvac_.decrypt_fp(ct, lo, hi);
    pvac_.free_cipher(ct);
    std::int64_t value = static_cast<std::int64_t>(lo);
    if (hi != 0) {
        __uint128_t p = (__uint128_t(1) << 127) - 1;
        __uint128_t full = (__uint128_t(hi) << 64) | lo;
        if (full > p / 2) value = -static_cast<std::int64_t>(p - full);
    }
    return CoreResult::success({{"value", value}});
}

} // namespace octra::native
