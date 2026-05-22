#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "octra_core.hpp"

class OctraAppBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged)
    Q_PROPERTY(QString address READ address NOTIFY stateChanged)
    Q_PROPERTY(QString rpcUrl READ rpcUrl NOTIFY stateChanged)
    Q_PROPERTY(QString explorerUrl READ explorerUrl NOTIFY stateChanged)
    Q_PROPERTY(QString networkName READ networkName NOTIFY stateChanged)

public:
    explicit OctraAppBridge(QObject* parent = nullptr);

    bool loaded() const;
    QString address() const;
    QString rpcUrl() const;
    QString explorerUrl() const;
    QString networkName() const;

    Q_INVOKABLE QVariantMap walletStatus();
    Q_INVOKABLE QVariantMap getHistory(int limit = 20, int offset = 0);
    Q_INVOKABLE QVariantMap unlockWallet(const QString& pin, const QString& path = QString());
    Q_INVOKABLE QVariantMap createWallet(const QString& pin);
    Q_INVOKABLE QVariantMap importPrivateKey(const QString& privateKeyB64, const QString& pin);
    Q_INVOKABLE QVariantMap getBalance();
    Q_INVOKABLE QVariantMap getFees();
    Q_INVOKABLE QVariantMap send(const QString& to, const QString& amount, const QString& fee, const QString& message);
    Q_INVOKABLE QVariantMap listProjects() const;
    Q_INVOKABLE QVariantMap createProject(const QString& name, const QString& templateKey = QString());
    Q_INVOKABLE QVariantMap loadProject(const QString& id) const;
    Q_INVOKABLE QVariantMap saveProject(const QString& id, const QString& name, const QString& filesJson, const QString& activeFile);
    Q_INVOKABLE QVariantMap deleteProject(const QString& id);
    Q_INVOKABLE QVariantMap importProjectFile(const QString& pathOrUrl);
    Q_INVOKABLE QVariantMap importProjectFolder(const QString& pathOrUrl);
    Q_INVOKABLE QVariantMap exportProjectJson(const QString& id, const QString& pathOrUrl) const;
    Q_INVOKABLE QVariantMap exportProjectFolder(const QString& id, const QString& pathOrUrl) const;
    Q_INVOKABLE QVariantMap exportProjectZip(const QString& id, const QString& pathOrUrl) const;
    Q_INVOKABLE QVariantMap compileAssembly(const QString& source);
    Q_INVOKABLE QVariantMap compileAml(const QString& source);
    Q_INVOKABLE QVariantMap compileProject(const QString& filesJson, const QString& mainPath);
    Q_INVOKABLE QVariantMap deployTemplate(const QString& templateKey, const QString& paramsJson, const QString& fee);
    Q_INVOKABLE QVariantMap previewContractAddress(const QString& bytecode);
    Q_INVOKABLE QVariantMap deployContract(const QString& bytecode, const QString& paramsJson, const QString& fee);
    Q_INVOKABLE QVariantMap callContract(const QString& address, const QString& method, const QString& paramsJson, const QString& amountRaw, const QString& fee);
    Q_INVOKABLE QVariantMap viewContract(const QString& address, const QString& method, const QString& paramsJson);
    Q_INVOKABLE QVariantMap verifyContract(const QString& address, const QString& source, const QString& filesJson = QString());
    Q_INVOKABLE QVariantMap contractInfo(const QString& address);
    Q_INVOKABLE QVariantMap contractReceipt(const QString& hash);
    Q_INVOKABLE QVariantMap contractStorage(const QString& address, const QString& key);
    Q_INVOKABLE QVariantMap loadTemplate(const QString& key);
    Q_INVOKABLE QVariantMap networkPreset(const QString& network) const;
    Q_INVOKABLE QVariantMap fheEncrypt(qint64 value);
    Q_INVOKABLE QVariantMap fheDecrypt(const QString& ciphertextB64);
    Q_INVOKABLE QVariantMap saveSettings(const QString& rpcUrl, const QString& explorerUrl, const QString& bridgeSignerUrl);

signals:
    void stateChanged();

private:
    octra::native::OctraCore core_;
    QVariantMap toVariant(const octra::native::CoreResult& result);
    void syncState();

    bool loaded_ = false;
    QString address_;
    QString rpcUrl_;
    QString explorerUrl_;
    QString networkName_;
};
