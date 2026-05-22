#include "qt_app_bridge.hpp"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>

namespace {

using nlohmann::json;

QVariant jsonToVariant(const json& value) {
    auto bytes = QByteArray::fromStdString(value.dump());
    auto doc = QJsonDocument::fromJson(bytes);
    if (value.is_object()) return doc.object().toVariantMap();
    if (value.is_array()) return doc.array().toVariantList();
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number_integer()) return static_cast<qlonglong>(value.get<long long>());
    if (value.is_number_float()) return value.get<double>();
    if (value.is_string()) return QString::fromStdString(value.get<std::string>());
    return {};
}

json parseJsonText(const QString& text, const json& fallback, QString* error = nullptr) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return fallback;
    try {
        return json::parse(trimmed.toStdString());
    } catch (const std::exception& ex) {
        if (error) *error = ex.what();
        return json();
    }
}

QVariantMap errorResult(const QString& message) {
    return QVariantMap{{"ok", false}, {"error", message}};
}

QVariantMap successResult() {
    return QVariantMap{{"ok", true}};
}

QString networkNameForRpc(const QString& rpcUrl) {
    if (rpcUrl.contains("46.101.86.250:8080"))
        return "Mainnet";
    if (rpcUrl.contains("165.227.225.79:8080"))
        return "Devnet";
    if (rpcUrl.contains("127.0.0.1") || rpcUrl.contains("localhost"))
        return "Local";
    return "Custom";
}

QHash<QString, QStringList> templateMap() {
    return {
        {"empty", {"main.aml"}},
        {"token", {"main.aml", "interfaces/IOCS01.aml"}},
        {"vault", {"main.aml"}},
        {"amm", {"main.aml"}},
        {"escrow", {"main.aml"}},
        {"multisig", {"main.aml"}},
        {"dictionary", {"main.aml"}}
    };
}

QString normalizePathOrUrl(const QString& pathOrUrl) {
    const QString trimmed = pathOrUrl.trimmed();
    if (trimmed.startsWith("file:")) {
        const QUrl url(trimmed);
        if (url.isLocalFile()) return url.toLocalFile();
    }
    return trimmed;
}

QString sanitizeName(const QString& name) {
    QString out = name.trimmed();
    if (out.isEmpty()) out = "Project";
    static const QString forbidden = "\\/:*?\"<>|";
    for (const QChar ch : forbidden) out.replace(ch, '_');
    return out;
}

QString appDataRoot() {
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) root = QDir::homePath() + "/.octra_wallet_native";
    QDir().mkpath(root);
    return root;
}

QString workspaceRoot() {
    const QString path = appDataRoot() + "/workspace";
    QDir().mkpath(path);
    return path;
}

QString projectsRoot() {
    const QString path = workspaceRoot() + "/projects";
    QDir().mkpath(path);
    return path;
}

QString projectRoot(const QString& id) {
    return projectsRoot() + "/" + id;
}

QString projectFilesRoot(const QString& id) {
    return projectRoot(id) + "/files";
}

QString projectMetaPath(const QString& id) {
    return projectRoot(id) + "/project.json";
}

QString timestampIso() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

bool writeTextFile(const QString& path, const QByteArray& bytes, QString* error) {
    QFileInfo info(path);
    QDir().mkpath(info.path());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QByteArray readTextFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return {};
    }
    return file.readAll();
}

QVariantList loadTemplateFiles(const QString& key, QString* error) {
    const auto templates = templateMap();
    if (!templates.contains(key)) {
        if (error) *error = "Unknown template";
        return {};
    }
    QVariantList files;
    for (const QString& path : templates.value(key)) {
        QFile file(QString(":/octra_templates/%1/%2").arg(key, path));
        if (!file.open(QIODevice::ReadOnly)) {
            if (error) *error = QString("Failed to open template file: %1").arg(path);
            return {};
        }
        files.push_back(QVariantMap{
            {"path", path},
            {"content", QString::fromUtf8(file.readAll())}
        });
    }
    return files;
}

QSettings workspaceSettings() {
    return QSettings("OctraLabs", "OctraWalletNative");
}

QString lastProjectId() {
    QSettings settings("OctraLabs", "OctraWalletNative");
    return settings.value("workspace/lastProjectId").toString();
}

void setLastProjectId(const QString& id) {
    QSettings settings("OctraLabs", "OctraWalletNative");
    settings.setValue("workspace/lastProjectId", id);
}

QString createProjectId() {
    return QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
           QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QVariantList normalizeFiles(const QVariantList& input) {
    QVariantList files;
    for (const QVariant& item : input) {
        const QVariantMap map = item.toMap();
        QString path = map.value("path").toString().trimmed();
        if (path.isEmpty()) continue;
        path.replace('\\', '/');
        if (path.startsWith('/')) path.remove(0, 1);
        files.push_back(QVariantMap{
            {"path", path},
            {"content", map.value("content").toString()}
        });
    }
    return files;
}

QVariantMap loadProjectById(const QString& id, bool withFiles, QString* error) {
    const QString metaPath = projectMetaPath(id);
    const QByteArray raw = readTextFile(metaPath, error);
    if (raw.isEmpty() && !QFileInfo::exists(metaPath)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
        if (error) *error = "Invalid project metadata";
        return {};
    }
    const QJsonObject meta = doc.object();
    QVariantMap project;
    project.insert("id", meta.value("id").toString(id));
    project.insert("name", meta.value("name").toString("Project"));
    project.insert("createdAt", meta.value("createdAt").toString());
    project.insert("updatedAt", meta.value("updatedAt").toString());
    project.insert("activeFile", meta.value("activeFile").toString("main.aml"));
    project.insert("template", meta.value("template").toString());

    if (!withFiles) return project;

    QVariantList files;
    QStringList known;
    const QString filesRoot = projectFilesRoot(id);
    const QJsonArray order = meta.value("fileOrder").toArray();
    for (const QJsonValue& value : order) {
        const QString rel = value.toString();
        if (rel.isEmpty()) continue;
        const QByteArray content = readTextFile(filesRoot + "/" + rel, nullptr);
        files.push_back(QVariantMap{{"path", rel}, {"content", QString::fromUtf8(content)}});
        known.push_back(rel);
    }

    QDirIterator it(filesRoot, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString abs = it.next();
        const QString rel = QDir(filesRoot).relativeFilePath(abs);
        if (known.contains(rel)) continue;
        const QByteArray content = readTextFile(abs, nullptr);
        files.push_back(QVariantMap{{"path", rel}, {"content", QString::fromUtf8(content)}});
    }

    project.insert("files", files);
    return project;
}

QVariantMap saveProjectById(const QString& id,
                            const QString& name,
                            const QVariantList& rawFiles,
                            const QString& activeFile,
                            const QString& templateKey,
                            QString* error) {
    const QVariantMap existing = loadProjectById(id, false, nullptr);
    const QString createdAt = existing.value("createdAt").toString().isEmpty()
        ? timestampIso()
        : existing.value("createdAt").toString();

    const QVariantList files = normalizeFiles(rawFiles);
    if (files.isEmpty()) {
        if (error) *error = "Project must contain at least one file";
        return {};
    }

    const QString root = projectRoot(id);
    const QString filesRoot = projectFilesRoot(id);
    QDir().mkpath(root);
    QDir dir(filesRoot);
    if (dir.exists() && !dir.removeRecursively()) {
        if (error) *error = "Failed to clear previous project files";
        return {};
    }
    QDir().mkpath(filesRoot);

    QJsonArray fileOrder;
    for (const QVariant& entryVar : files) {
        const QVariantMap entry = entryVar.toMap();
        const QString rel = entry.value("path").toString();
        fileOrder.push_back(rel);
        if (!writeTextFile(filesRoot + "/" + rel, entry.value("content").toString().toUtf8(), error)) {
            return {};
        }
    }

    QJsonObject meta;
    meta.insert("id", id);
    meta.insert("name", sanitizeName(name));
    meta.insert("createdAt", createdAt);
    meta.insert("updatedAt", timestampIso());
    meta.insert("activeFile", activeFile.isEmpty() ? files.first().toMap().value("path").toString() : activeFile);
    meta.insert("template", templateKey);
    meta.insert("fileOrder", fileOrder);
    if (!writeTextFile(projectMetaPath(id), QJsonDocument(meta).toJson(QJsonDocument::Indented), error)) {
        return {};
    }
    return loadProjectById(id, true, error);
}

QVariantMap createProjectFromFiles(const QString& name,
                                   const QVariantList& files,
                                   const QString& activeFile,
                                   const QString& templateKey,
                                   QString* error) {
    return saveProjectById(createProjectId(), name, files, activeFile, templateKey, error);
}

QVariantMap importFolderAsProject(const QString& folderPath, QString* error) {
    QDir root(folderPath);
    if (!root.exists()) {
        if (error) *error = "Folder not found";
        return {};
    }
    QVariantList files;
    QDirIterator it(folderPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString abs = it.next();
        const QString rel = root.relativeFilePath(abs);
        const QByteArray content = readTextFile(abs, nullptr);
        files.push_back(QVariantMap{
            {"path", rel},
            {"content", QString::fromUtf8(content)}
        });
    }
    if (files.isEmpty()) {
        if (error) *error = "No files found in folder";
        return {};
    }
    return createProjectFromFiles(root.dirName(), files, files.first().toMap().value("path").toString(), "", error);
}

QVariantMap exportProjectFilesToFolder(const QVariantMap& project,
                                       const QString& destinationDir,
                                       QString* error) {
    QDir base(destinationDir);
    if (!base.exists() && !QDir().mkpath(destinationDir)) {
        if (error) *error = "Failed to create export folder";
        return {};
    }
    const QString targetRoot = base.filePath(sanitizeName(project.value("name").toString()));
    QDir target(targetRoot);
    if (target.exists() && !target.removeRecursively()) {
        if (error) *error = "Failed to clear target export folder";
        return {};
    }
    QDir().mkpath(targetRoot);
    const QVariantList files = project.value("files").toList();
    for (const QVariant& fileVar : files) {
        const QVariantMap file = fileVar.toMap();
        if (!writeTextFile(targetRoot + "/" + file.value("path").toString(),
                           file.value("content").toString().toUtf8(),
                           error)) {
            return {};
        }
    }
    QVariantMap out = successResult();
    out.insert("path", targetRoot);
    return out;
}

bool runProcess(const QString& program,
                const QStringList& args,
                const QString& workingDir,
                QString* error) {
    QProcess process;
    if (!workingDir.isEmpty()) process.setWorkingDirectory(workingDir);
    process.start(program, args);
    if (!process.waitForStarted()) {
        if (error) *error = process.errorString();
        return false;
    }
    if (!process.waitForFinished(-1)) {
        if (error) *error = process.errorString();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (stderrText.isEmpty()) stderrText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            *error = stderrText.isEmpty() ? "External tool failed" : stderrText;
        }
        return false;
    }
    return true;
}

bool extractZipToFolder(const QString& zipPath, const QString& destination, QString* error) {
#ifdef _WIN32
    return runProcess("powershell",
                      {"-NoProfile", "-Command",
                       QString("Expand-Archive -Force -Path '%1' -DestinationPath '%2'")
                           .arg(zipPath, destination)},
                      QString(), error);
#else
    const QString unzip = QStandardPaths::findExecutable("unzip");
    if (unzip.isEmpty()) {
        if (error) *error = "unzip is not installed";
        return false;
    }
    return runProcess(unzip, {"-qq", zipPath, "-d", destination}, QString(), error);
#endif
}

bool createZipFromFolder(const QString& sourceDir, const QString& zipPath, QString* error) {
#ifdef _WIN32
    return runProcess("powershell",
                      {"-NoProfile", "-Command",
                       QString("Compress-Archive -Force -Path '%1\\*' -DestinationPath '%2'")
                           .arg(QDir::toNativeSeparators(sourceDir), QDir::toNativeSeparators(zipPath))},
                      QString(), error);
#else
    const QString zip = QStandardPaths::findExecutable("zip");
    if (zip.isEmpty()) {
        if (error) *error = "zip is not installed";
        return false;
    }
    return runProcess(zip, {"-qr", zipPath, "."}, sourceDir, error);
#endif
}

} // namespace

OctraAppBridge::OctraAppBridge(QObject* parent) : QObject(parent) {
    syncState();
}

bool OctraAppBridge::loaded() const { return loaded_; }
QString OctraAppBridge::address() const { return address_; }
QString OctraAppBridge::rpcUrl() const { return rpcUrl_; }
QString OctraAppBridge::explorerUrl() const { return explorerUrl_; }
QString OctraAppBridge::networkName() const { return networkName_; }

QVariantMap OctraAppBridge::toVariant(const octra::native::CoreResult& result) {
    QVariantMap out;
    out.insert("ok", result.ok);
    if (!result.error.empty()) out.insert("error", QString::fromStdString(result.error));
    if (!result.data.is_null() && !result.data.empty()) {
        if (result.data.is_object()) {
            const auto data = jsonToVariant(result.data).toMap();
            for (auto it = data.begin(); it != data.end(); ++it) out.insert(it.key(), it.value());
        } else {
            out.insert("data", jsonToVariant(result.data));
        }
    }
    return out;
}

void OctraAppBridge::syncState() {
    const auto state = core_.current_wallet();
    loaded_ = state.ok;
    if (state.ok && state.data.contains("wallet")) {
        const auto wallet = state.data["wallet"];
        address_ = QString::fromStdString(wallet.value("addr", ""));
        rpcUrl_ = QString::fromStdString(wallet.value("rpc_url", ""));
        explorerUrl_ = QString::fromStdString(wallet.value("explorer_url", ""));
        networkName_ = networkNameForRpc(rpcUrl_);
    } else {
        address_.clear();
        rpcUrl_.clear();
        explorerUrl_.clear();
        networkName_ = "Locked";
    }
    emit stateChanged();
}

QVariantMap OctraAppBridge::walletStatus() { return toVariant(core_.wallet_status()); }

QVariantMap OctraAppBridge::getHistory(int limit, int offset) {
    return toVariant(core_.get_history(limit, offset));
}

QVariantMap OctraAppBridge::unlockWallet(const QString& pin, const QString& path) {
    const auto result = core_.unlock_wallet(pin.toStdString(), path.toStdString());
    syncState();
    return toVariant(result);
}

QVariantMap OctraAppBridge::createWallet(const QString& pin) {
    const auto result = core_.create_wallet(pin.toStdString());
    syncState();
    return toVariant(result);
}

QVariantMap OctraAppBridge::importPrivateKey(const QString& privateKeyB64, const QString& pin) {
    const auto result = core_.import_wallet_private_key(privateKeyB64.toStdString(), pin.toStdString());
    syncState();
    return toVariant(result);
}

QVariantMap OctraAppBridge::getBalance() { return toVariant(core_.get_balance()); }
QVariantMap OctraAppBridge::getFees() { return toVariant(core_.get_fees()); }

QVariantMap OctraAppBridge::send(const QString& to, const QString& amount, const QString& fee, const QString& message) {
    return toVariant(core_.send(to.toStdString(), amount.toStdString(), fee.toStdString(), message.toStdString()));
}

QVariantMap OctraAppBridge::listProjects() const {
    QVariantList projects;
    QDir root(projectsRoot());
    const QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QString& id : dirs) {
        QVariantMap project = loadProjectById(id, false, nullptr);
        if (!project.isEmpty()) projects.push_back(project);
    }
    QVariantMap out = successResult();
    out.insert("projects", projects);
    out.insert("lastProjectId", lastProjectId());
    out.insert("workspaceRoot", workspaceRoot());
    return out;
}

QVariantMap OctraAppBridge::createProject(const QString& name, const QString& templateKey) {
    QString error;
    QVariantList files;
    QString activeFile = "main.aml";
    QString templateName = templateKey.trimmed();
    if (!templateName.isEmpty()) {
        files = loadTemplateFiles(templateName, &error);
        if (!error.isEmpty()) return errorResult(error);
        if (!files.isEmpty()) activeFile = files.first().toMap().value("path").toString();
    } else {
        files.push_back(QVariantMap{{"path", "main.aml"}, {"content", ""}});
    }
    QVariantMap project = createProjectFromFiles(sanitizeName(name), files, activeFile, templateName, &error);
    if (!error.isEmpty()) return errorResult(error);
    setLastProjectId(project.value("id").toString());
    QVariantMap out = successResult();
    out.insert("project", project);
    return out;
}

QVariantMap OctraAppBridge::loadProject(const QString& id) const {
    QString error;
    QVariantMap project = loadProjectById(id, true, &error);
    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Project not found" : error);
    setLastProjectId(id);
    QVariantMap out = successResult();
    out.insert("project", project);
    return out;
}

QVariantMap OctraAppBridge::saveProject(const QString& id, const QString& name, const QString& filesJson, const QString& activeFile) {
    QString parseError;
    const json parsed = parseJsonText(filesJson, json::array(), &parseError);
    if (!parseError.isEmpty()) return errorResult("Invalid files JSON: " + parseError);
    const QVariantList files = jsonToVariant(parsed).toList();
    QString error;
    QVariantMap project = saveProjectById(id, sanitizeName(name), files, activeFile, QString(), &error);
    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Failed to save project" : error);
    setLastProjectId(id);
    QVariantMap out = successResult();
    out.insert("project", project);
    return out;
}

QVariantMap OctraAppBridge::deleteProject(const QString& id) {
    if (id.trimmed().isEmpty()) return errorResult("Project id required");
    QDir root(projectRoot(id));
    if (!root.exists()) return errorResult("Project not found");
    if (!root.removeRecursively()) return errorResult("Failed to delete project");
    if (lastProjectId() == id) setLastProjectId(QString());
    QVariantMap out = successResult();
    out.insert("deletedId", id);
    return out;
}

QVariantMap OctraAppBridge::importProjectFile(const QString& pathOrUrl) {
    const QString path = normalizePathOrUrl(pathOrUrl);
    QFileInfo info(path);
    if (!info.exists()) return errorResult("Import file not found");

    QString error;
    QVariantMap project;
    const QString suffix = info.suffix().toLower();
    if (suffix == "zip") {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) return errorResult("Failed to create temporary directory");
        if (!extractZipToFolder(path, tempDir.path(), &error)) return errorResult(error);
        QDir temp(tempDir.path());
        const QStringList entries = temp.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        QString importRoot = tempDir.path();
        if (entries.size() == 1) {
            const QFileInfo entryInfo(temp.filePath(entries.first()));
            if (entryInfo.isDir()) importRoot = entryInfo.absoluteFilePath();
        }
        project = importFolderAsProject(importRoot, &error);
    } else if (suffix == "json" || path.endsWith(".aml-project.json")) {
        const QByteArray raw = readTextFile(path, &error);
        if (!error.isEmpty()) return errorResult(error);
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) return errorResult("Invalid project JSON");
        const QJsonObject obj = doc.object();
        QVariantList files;
        const QJsonArray arr = obj.value("files").toArray();
        for (const QJsonValue& value : arr) {
            const QJsonObject file = value.toObject();
            files.push_back(QVariantMap{
                {"path", file.value("path").toString()},
                {"content", file.value("content").toString(file.value("source").toString())}
            });
        }
        if (files.isEmpty()) return errorResult("Project JSON contains no files");
        project = createProjectFromFiles(obj.value("name").toString(info.completeBaseName()),
                                         files,
                                         obj.value("activeFile").toString(files.first().toMap().value("path").toString()),
                                         QString(),
                                         &error);
    } else {
        const QByteArray raw = readTextFile(path, &error);
        if (!error.isEmpty()) return errorResult(error);
        const QString projectName = info.completeBaseName().isEmpty() ? "Imported Project" : info.completeBaseName();
        const QString fileName = (suffix == "aml" && info.fileName() != "main.aml")
            ? QString("main.aml")
            : info.fileName();
        QVariantList files{QVariantMap{{"path", fileName}, {"content", QString::fromUtf8(raw)}}};
        project = createProjectFromFiles(projectName, files, fileName, QString(), &error);
    }

    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Failed to import project" : error);
    setLastProjectId(project.value("id").toString());
    QVariantMap out = successResult();
    out.insert("project", project);
    return out;
}

QVariantMap OctraAppBridge::importProjectFolder(const QString& pathOrUrl) {
    const QString path = normalizePathOrUrl(pathOrUrl);
    QString error;
    QVariantMap project = importFolderAsProject(path, &error);
    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Failed to import folder" : error);
    setLastProjectId(project.value("id").toString());
    QVariantMap out = successResult();
    out.insert("project", project);
    return out;
}

QVariantMap OctraAppBridge::exportProjectJson(const QString& id, const QString& pathOrUrl) const {
    QString error;
    QVariantMap project = loadProjectById(id, true, &error);
    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Project not found" : error);
    QJsonObject out;
    out.insert("name", project.value("name").toString());
    out.insert("activeFile", project.value("activeFile").toString());
    QJsonArray files;
    for (const QVariant& value : project.value("files").toList()) {
        const QVariantMap file = value.toMap();
        QJsonObject entry;
        entry.insert("path", file.value("path").toString());
        entry.insert("content", file.value("content").toString());
        files.push_back(entry);
    }
    out.insert("files", files);
    const QString target = normalizePathOrUrl(pathOrUrl);
    if (!writeTextFile(target, QJsonDocument(out).toJson(QJsonDocument::Indented), &error)) return errorResult(error);
    QVariantMap result = successResult();
    result.insert("path", target);
    return result;
}

QVariantMap OctraAppBridge::exportProjectFolder(const QString& id, const QString& pathOrUrl) const {
    QString error;
    QVariantMap project = loadProjectById(id, true, &error);
    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Project not found" : error);
    return exportProjectFilesToFolder(project, normalizePathOrUrl(pathOrUrl), &error);
}

QVariantMap OctraAppBridge::exportProjectZip(const QString& id, const QString& pathOrUrl) const {
    QString error;
    QVariantMap project = loadProjectById(id, true, &error);
    if (!error.isEmpty() || project.isEmpty()) return errorResult(error.isEmpty() ? "Project not found" : error);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) return errorResult("Failed to create temporary directory");
    QVariantMap exported = exportProjectFilesToFolder(project, tempDir.path(), &error);
    if (!error.isEmpty()) return errorResult(error);
    const QString sourceDir = exported.value("path").toString();
    const QString targetZip = normalizePathOrUrl(pathOrUrl);
    if (!createZipFromFolder(sourceDir, targetZip, &error)) return errorResult(error);
    QVariantMap out = successResult();
    out.insert("path", targetZip);
    return out;
}

QVariantMap OctraAppBridge::compileAssembly(const QString& source) {
    return toVariant(core_.compile_assembly(source.toStdString()));
}

QVariantMap OctraAppBridge::compileAml(const QString& source) {
    return toVariant(core_.compile_aml(source.toStdString()));
}

QVariantMap OctraAppBridge::compileProject(const QString& filesJson, const QString& mainPath) {
    QString parseError;
    const json files = parseJsonText(filesJson, json::array(), &parseError);
    if (!parseError.isEmpty()) return errorResult("Invalid files JSON: " + parseError);
    return toVariant(core_.compile_project(files, mainPath.toStdString()));
}

QVariantMap OctraAppBridge::deployTemplate(const QString& templateKey, const QString& paramsJson, const QString& fee) {
    QString error;
    QVariantList files = loadTemplateFiles(templateKey, &error);
    if (!error.isEmpty()) return errorResult(error);
    if (files.isEmpty()) return errorResult("Template is empty");

    json compiled;
    if (files.size() == 1) {
        const QVariantMap file = files.first().toMap();
        const auto compile = core_.compile_aml(file.value("content").toString().toStdString());
        if (!compile.ok) return toVariant(compile);
        compiled = compile.data;
    } else {
        json payload = json::array();
        for (const QVariant& item : files) {
            const QVariantMap file = item.toMap();
            payload.push_back({
                {"path", file.value("path").toString().toStdString()},
                {"source", file.value("content").toString().toStdString()}
            });
        }
        const auto compile = core_.compile_project(payload, "main.aml");
        if (!compile.ok) return toVariant(compile);
        compiled = compile.data;
    }

    const std::string bytecode = compiled.value("bytecode", "");
    const json abi = compiled.contains("abi") ? compiled["abi"] : json::object();
    return toVariant(core_.deploy_contract(
        bytecode,
        paramsJson.toStdString(),
        fee.toStdString(),
        templateKey.toStdString(),
        abi));
}

QVariantMap OctraAppBridge::previewContractAddress(const QString& bytecode) {
    return toVariant(core_.compute_contract_address(bytecode.toStdString()));
}

QVariantMap OctraAppBridge::deployContract(const QString& bytecode, const QString& paramsJson, const QString& fee) {
    return toVariant(core_.deploy_contract(bytecode.toStdString(), paramsJson.toStdString(), fee.toStdString()));
}

QVariantMap OctraAppBridge::callContract(const QString& address, const QString& method, const QString& paramsJson, const QString& amountRaw, const QString& fee) {
    QString parseError;
    const json params = parseJsonText(paramsJson, json::array(), &parseError);
    if (!parseError.isEmpty()) return errorResult("Invalid params JSON: " + parseError);
    return toVariant(core_.call_contract(address.toStdString(), method.toStdString(), params, amountRaw.toStdString(), fee.toStdString()));
}

QVariantMap OctraAppBridge::viewContract(const QString& address, const QString& method, const QString& paramsJson) {
    QString parseError;
    const json params = parseJsonText(paramsJson, json::array(), &parseError);
    if (!parseError.isEmpty()) return errorResult("Invalid params JSON: " + parseError);
    return toVariant(core_.view_contract(address.toStdString(), method.toStdString(), params));
}

QVariantMap OctraAppBridge::verifyContract(const QString& address, const QString& source, const QString& filesJson) {
    QString parseError;
    const json files = parseJsonText(filesJson, json::array(), &parseError);
    if (!parseError.isEmpty()) return errorResult("Invalid files JSON: " + parseError);
    return toVariant(core_.verify_contract(address.toStdString(), source.toStdString(), files));
}

QVariantMap OctraAppBridge::contractInfo(const QString& address) {
    return toVariant(core_.contract_info(address.toStdString()));
}

QVariantMap OctraAppBridge::contractReceipt(const QString& hash) {
    return toVariant(core_.contract_receipt(hash.toStdString()));
}

QVariantMap OctraAppBridge::contractStorage(const QString& address, const QString& key) {
    return toVariant(core_.contract_storage(address.toStdString(), key.toStdString()));
}

QVariantMap OctraAppBridge::loadTemplate(const QString& key) {
    QString error;
    QVariantList files = loadTemplateFiles(key, &error);
    if (!error.isEmpty()) return errorResult(error);
    QVariantMap out = successResult();
    out.insert("template", key);
    out.insert("files", files);
    return out;
}

QVariantMap OctraAppBridge::networkPreset(const QString& network) const {
    QVariantMap out = successResult();
    if (network.compare("mainnet", Qt::CaseInsensitive) == 0) {
        out.insert("name", "Mainnet");
        out.insert("rpcUrl", "http://46.101.86.250:8080");
        out.insert("explorerUrl", "https://octrascan.io");
        out.insert("bridgeSignerUrl", "https://relayer-002838819188.octra.network");
        return out;
    }
    if (network.compare("devnet", Qt::CaseInsensitive) == 0) {
        out.insert("name", "Devnet");
        out.insert("rpcUrl", "http://165.227.225.79:8080");
        out.insert("explorerUrl", "https://devnet.octrascan.io");
        out.insert("bridgeSignerUrl", "");
        return out;
    }
    return errorResult("Unknown network preset");
}

QVariantMap OctraAppBridge::fheEncrypt(qint64 value) {
    return toVariant(core_.fhe_encrypt(value));
}

QVariantMap OctraAppBridge::fheDecrypt(const QString& ciphertextB64) {
    return toVariant(core_.fhe_decrypt(ciphertextB64.toStdString()));
}

QVariantMap OctraAppBridge::saveSettings(const QString& rpcUrl, const QString& explorerUrl, const QString& bridgeSignerUrl) {
    const auto result = core_.save_settings(rpcUrl.toStdString(), explorerUrl.toStdString(), bridgeSignerUrl.toStdString());
    syncState();
    return toVariant(result);
}
