import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts
import Qt.labs.platform as LabsPlatform

ApplicationWindow {
    id: window
    width: 1440
    height: 940
    visible: true
    title: "Octra IDE"

    property color bgTop: "#050b16"
    property color bgBottom: "#0c1730"
    property color panelBg: "#101c33"
    property color panelAlt: "#15233d"
    property color ink: "#f5f7fb"
    property color muted: "#8ea4c4"
    property color accent: "#6ea8fe"
    property color accentStrong: "#4f7cff"
    property color accentSoft: "#263752"
    property color okColor: "#52c89b"
    property color warnColor: "#ffbf66"
    property color dangerColor: "#ff6b81"

    property string feedback: ""
    property string consoleText: ""
    property string abiText: ""
    property string disasmText: ""
    property string storageText: ""
    property string dictionaryText: ""
    property int outputTabIndex: 2
    property string currentProjectId: ""
    property string activeFilePath: ""
    property bool projectDirty: false
    property int editorErrorLine: -1
    property string editorStatus: ""
    property string lastTxHash: ""

    Material.theme: Material.Dark
    Material.primary: accent
    Material.accent: accentStrong
    Material.background: bgBottom
    Material.foreground: ink

    palette.window: bgBottom
    palette.base: panelBg
    palette.text: ink
    palette.button: accent
    palette.buttonText: "white"
    palette.highlight: accent
    palette.highlightedText: "white"
    palette.placeholderText: muted

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: bgTop }
            GradientStop { position: 1.0; color: bgBottom }
        }
    }

    ListModel { id: projectSummaryModel }
    ListModel { id: projectFileModel }
    ListModel { id: transactionModel }

    function logConsole(kind, message) {
        var stamp = new Date().toLocaleTimeString()
        consoleText += "[" + stamp + "][" + kind.toUpperCase() + "] " + message + "\n"
        outputTabIndex = 2
    }

    function setFeedback(result, successMsg, failureMsg) {
        feedback = result.ok ? JSON.stringify(result, null, 2) : (result.error || "Unknown error")
        if (result.ok && successMsg)
            logConsole("info", successMsg)
        else if (!result.ok)
            logConsole("error", failureMsg ? failureMsg + ": " + feedback : feedback)
    }

    function normalizeFilePath(path) {
        var normalized = (path || "").trim()
        normalized = normalized.replace(/\\/g, "/")
        if (normalized.startsWith("/"))
            normalized = normalized.substring(1)
        return normalized
    }

    function explorerBaseUrl() {
        var base = (octraBridge.explorerUrl || "").trim()
        if (!base.length) {
            if (octraBridge.networkName === "Mainnet")
                base = "https://octrascan.io"
            else
                base = "https://devnet.octrascan.io"
        }
        return base.replace(/\/+$/, "")
    }

    function txExplorerUrl(hash) {
        var txHash = (hash || "").trim()
        if (!txHash.length)
            return ""
        return explorerBaseUrl() + "/tx.html?hash=" + encodeURIComponent(txHash)
    }

    function formatTxTime(ts) {
        var value = Number(ts || 0)
        if (!value)
            return "pending"
        return new Date(value * 1000).toLocaleString()
    }

    function walletExplorerLabel() {
        return explorerBaseUrl().replace(/^https?:\/\//, "")
    }

    function refreshTransactions() {
        if (!octraBridge.loaded) {
            transactionModel.clear()
            return
        }
        var res = octraBridge.getHistory(12, 0)
        if (!res.ok) {
            setFeedback(res, "Transaction history loaded", "Transaction history failed")
            return
        }
        transactionModel.clear()
        var txs = res.transactions || []
        for (var i = 0; i < txs.length; ++i) {
            var tx = txs[i] || {}
            var hash = (tx.hash || tx.tx_hash || "").trim()
            transactionModel.append({
                hash: hash,
                from: tx.from || "",
                to_: tx.to_ || tx.to || "",
                amount_raw: tx.amount_raw || tx.amount || "0",
                op_type: tx.op_type || "standard",
                status: tx.status || "confirmed",
                timestamp: tx.timestamp || 0,
                reject_reason: tx.reject_reason || "",
                explorerUrl: txExplorerUrl(hash)
            })
        }
    }

    function submitTransaction(result, successLabel, failureLabel) {
        setFeedback(result, successLabel, failureLabel)
        if (result.ok) {
            if (result.tx_hash)
                lastTxHash = result.tx_hash
            if (result.tx_hash || result.hash)
                refreshTransactions()
        }
        return result
    }

    function refreshWorkspace(preferredProjectId) {
        var res = octraBridge.listProjects()
        if (!res.ok) {
            setFeedback(res)
            return
        }
        workspaceRootLabel.text = "Workspace: " + (res.workspaceRoot || "")
        projectSummaryModel.clear()
        for (var i = 0; i < res.projects.length; ++i)
            projectSummaryModel.append(res.projects[i])
        var target = preferredProjectId || currentProjectId || res.lastProjectId || ""
        if (target.length) {
            openProject(target, true)
        } else if (projectSummaryModel.count > 0) {
            openProject(projectSummaryModel.get(0).id, true)
        }
    }

    function currentFilesArray() {
        saveCurrentEditorFile()
        var files = []
        for (var i = 0; i < projectFileModel.count; ++i) {
            var file = projectFileModel.get(i)
            files.push({ path: file.path, content: file.content })
        }
        return files
    }

    function findFileIndex(path) {
        for (var i = 0; i < projectFileModel.count; ++i) {
            if (projectFileModel.get(i).path === path)
                return i
        }
        return -1
    }

    function findProjectIndex(projectId) {
        for (var i = 0; i < projectSummaryModel.count; ++i) {
            if (projectSummaryModel.get(i).id === projectId)
                return i
        }
        return -1
    }

    function applyProject(project) {
        currentProjectId = project.id || ""
        projectNameField.text = project.name || "Project"
        projectFileModel.clear()
        var files = project.files || []
        for (var i = 0; i < files.length; ++i)
            projectFileModel.append({ path: normalizeFilePath(files[i].path), content: files[i].content || "" })
        activeFilePath = normalizeFilePath(project.activeFile || (files.length > 0 ? files[0].path : ""))
        if (!activeFilePath.length && projectFileModel.count > 0)
            activeFilePath = projectFileModel.get(0).path
        selectFileByPath(activeFilePath)
        projectDirty = false
        editorStatus = "Project ready"
    }

    function openProject(projectId, silent) {
        if (!projectId || !projectId.length)
            return
        var res = octraBridge.loadProject(projectId)
        if (!res.ok) {
            setFeedback(res)
            return
        }
        applyProject(res.project)
        if (!silent)
            setFeedback(res, "Opened project " + res.project.name)
        var idx = findProjectIndex(projectId)
        if (idx >= 0)
            projectSelector.currentIndex = idx
    }

    function saveCurrentEditorFile() {
        if (!activeFilePath.length)
            return
        var idx = findFileIndex(activeFilePath)
        if (idx >= 0)
            projectFileModel.setProperty(idx, "content", projectEditor.text)
    }

    function selectFileByPath(path) {
        var normalized = normalizeFilePath(path)
        var idx = findFileIndex(normalized)
        if (idx < 0 && projectFileModel.count > 0)
            idx = 0
        if (idx < 0) {
            activeFilePath = ""
            projectEditor.text = ""
            filePathField.text = ""
            fileList.currentIndex = -1
            return
        }
        saveCurrentEditorFile()
        activeFilePath = projectFileModel.get(idx).path
        filePathField.text = activeFilePath
        projectEditor.text = projectFileModel.get(idx).content
        fileList.currentIndex = idx
        editorErrorLine = -1
    }

    function addOrUpdateFile() {
        var path = normalizeFilePath(filePathField.text)
        if (!path.length) {
            editorStatus = "File path required"
            return
        }
        var idx = findFileIndex(path)
        if (idx >= 0) {
            projectFileModel.setProperty(idx, "content", projectEditor.text)
            projectFileModel.setProperty(idx, "path", path)
        } else {
            projectFileModel.append({ path: path, content: projectEditor.text })
            idx = projectFileModel.count - 1
        }
        activeFilePath = path
        fileList.currentIndex = idx
        projectDirty = true
        autosaveTimer.restart()
        editorStatus = "File staged for save"
    }

    function deleteActiveFile() {
        if (!activeFilePath.length)
            return
        var idx = findFileIndex(activeFilePath)
        if (idx < 0)
            return
        projectFileModel.remove(idx)
        projectDirty = true
        if (projectFileModel.count > 0) {
            selectFileByPath(projectFileModel.get(Math.max(0, idx - 1)).path)
        } else {
            activeFilePath = ""
            filePathField.text = ""
            projectEditor.text = ""
            fileList.currentIndex = -1
        }
        autosaveTimer.restart()
        editorStatus = "File removed"
    }

    function persistProject(silent) {
        if (!currentProjectId.length) {
            if (!silent)
                editorStatus = "Create or open a project first"
            return false
        }
        saveCurrentEditorFile()
        var res = octraBridge.saveProject(currentProjectId, projectNameField.text, JSON.stringify(currentFilesArray()), activeFilePath)
        if (!res.ok) {
            setFeedback(res)
            return false
        }
        projectDirty = false
        applyProject(res.project)
        refreshWorkspace(currentProjectId)
        if (!silent)
            setFeedback(res, "Saved project " + res.project.name)
        return true
    }

    function createNewProject() {
        var res = octraBridge.createProject(newProjectName.text, newProjectTemplate.currentValue)
        if (!res.ok) {
            setFeedback(res)
            return
        }
        newProjectDialog.close()
        refreshWorkspace(res.project.id)
        setFeedback(res, "Created project " + res.project.name)
    }

    function duplicateCurrentProject() {
        if (!currentProjectId.length)
            return
        var copyName = projectNameField.text + " Copy"
        var res = octraBridge.createProject(copyName, "")
        if (!res.ok) {
            setFeedback(res)
            return
        }
        currentProjectId = res.project.id
        projectNameField.text = copyName
        persistProject(true)
        refreshWorkspace(currentProjectId)
        setFeedback({ok: true, project: {name: copyName}}, "Duplicated project to " + copyName)
    }

    function exportCurrentProjectJson(path) {
        if (!persistProject(true))
            return
        setFeedback(octraBridge.exportProjectJson(currentProjectId, path), "Exported project JSON")
    }

    function exportCurrentProjectFolder(path) {
        if (!persistProject(true))
            return
        setFeedback(octraBridge.exportProjectFolder(currentProjectId, path), "Exported project folder")
    }

    function exportCurrentProjectZip(path) {
        if (!persistProject(true))
            return
        setFeedback(octraBridge.exportProjectZip(currentProjectId, path), "Exported project zip")
    }

    function importProjectResult(result) {
        if (!result.ok) {
            setFeedback(result)
            return
        }
        refreshWorkspace(result.project.id)
        setFeedback(result, "Imported project " + result.project.name)
    }

    function projectSourceForMain() {
        var files = currentFilesArray()
        for (var i = 0; i < files.length; ++i) {
            if (files[i].path === "main.aml")
                return files[i].content
        }
        return projectEditor.text
    }

    function projectDependencyFiles() {
        var files = currentFilesArray()
        var deps = []
        for (var i = 0; i < files.length; ++i) {
            if (files[i].path !== "main.aml")
                deps.push({ path: files[i].path, source: files[i].content })
        }
        return deps
    }

    function selectEditorLine(lineNumber) {
        if (lineNumber <= 0)
            return
        var text = projectEditor.text
        var start = 0
        var currentLine = 1
        while (currentLine < lineNumber && start < text.length) {
            var nl = text.indexOf("\n", start)
            if (nl < 0) break
            start = nl + 1
            currentLine += 1
        }
        var end = text.indexOf("\n", start)
        if (end < 0) end = text.length
        projectEditor.select(start, end)
        projectEditor.cursorPosition = start
        editorErrorLine = lineNumber
    }

    function captureCompileError(result) {
        if (result.ok) {
            editorErrorLine = -1
            editorStatus = "Compile successful"
            return
        }
        var match = /line\s+(\d+)/i.exec(result.error || "")
        if (match && match.length > 1)
            selectEditorLine(Number(match[1]))
        editorStatus = result.error || "Compile failed"
    }

    function applyCompileResult(result, label) {
        setFeedback(result, label ? label + " succeeded" : "Compile succeeded", label ? label + " failed" : "Compile failed")
        captureCompileError(result)
        if (!result.ok)
            return
        if (result.bytecode !== undefined)
            bytecodeOutput.text = result.bytecode
        abiText = result.abi !== undefined ? (typeof result.abi === "string" ? result.abi : JSON.stringify(result.abi, null, 2)) : ""
        disasmText = result.disasm !== undefined ? result.disasm : ""
        outputTabIndex = abiText.length ? 0 : 1
    }

    function compileCurrentEditor() {
        saveCurrentEditorFile()
        if (languageSelector.currentIndex === 0)
            applyCompileResult(octraBridge.compileAml(projectEditor.text), "AML compile")
        else
            applyCompileResult(octraBridge.compileAssembly(projectEditor.text), "Assembly compile")
    }

    function compileCurrentProject() {
        if (!persistProject(true))
            return
        var files = []
        var current = currentFilesArray()
        for (var i = 0; i < current.length; ++i)
            files.push({ path: current[i].path, source: current[i].content })
        applyCompileResult(octraBridge.compileProject(JSON.stringify(files), "main.aml"), "Project compile")
    }

    function fillRecommendedFees() {
        var res = octraBridge.getFees()
        if (!res.ok) {
            setFeedback(res)
            return
        }
        if (res.deploy && res.deploy.recommended !== undefined)
            deployFee.text = String(res.deploy.recommended)
        if (res.deploy && res.deploy.recommended !== undefined)
            dictionaryDeployFee.text = String(res.deploy.recommended)
        if (res.call && res.call.recommended !== undefined)
            contractFee.text = String(res.call.recommended)
        if (res.call && res.call.recommended !== undefined)
            dictionaryFee.text = String(res.call.recommended)
        if (res.standard && res.standard.recommended !== undefined)
            sendFee.text = String(res.standard.recommended)
        logConsole("info", "Recommended fees loaded")
    }

    function previewDeployAddress() {
        setFeedback(octraBridge.previewContractAddress(bytecodeOutput.text), "Computed contract address")
    }

    function deployCurrentContract() {
        var res = octraBridge.deployContract(bytecodeOutput.text, deployParams.text, deployFee.text)
        submitTransaction(res, "Deploy transaction submitted", "Deploy failed")
    }

    function callCurrentContract() {
        var res = octraBridge.callContract(contractAddr.text, contractMethod.text, contractParams.text, contractAmount.text, contractFee.text)
        submitTransaction(res, "Call transaction submitted", "Call failed")
    }

    function sendCurrentTransfer() {
        var res = octraBridge.send(sendTo.text, sendAmount.text, sendFee.text, sendMessage.text)
        submitTransaction(res, "Transfer submitted", "Send failed")
    }

    function viewCurrentContract() {
        var res = octraBridge.viewContract(contractAddr.text, contractMethod.text, contractParams.text)
        setFeedback(res, "View call completed", "View failed")
        if (res.ok) {
            storageText = JSON.stringify(res, null, 2)
            outputTabIndex = 3
        }
    }

    function dictionaryParams(values) {
        return JSON.stringify(values)
    }

    function dictionaryCall(method, values, amountRaw) {
        return octraBridge.callContract(dictionaryAddress.text, method, dictionaryParams(values), amountRaw || "", dictionaryFee.text)
    }

    function dictionaryView(method, values) {
        return octraBridge.viewContract(dictionaryAddress.text, method, dictionaryParams(values))
    }

    function refreshDictionaryOutput(result, successLabel, failureLabel) {
        setFeedback(result, successLabel, failureLabel)
        if (result.ok) {
            dictionaryText = JSON.stringify(result, null, 2)
            outputTabIndex = 3
        }
    }

    function deployDictionaryContract() {
        var res = octraBridge.deployTemplate("dictionary", "[]", dictionaryDeployFee.text)
        submitTransaction(res, "Dictionary deployed", "Dictionary deploy failed")
        refreshDictionaryOutput(res, "Dictionary deployed", "Dictionary deploy failed")
        if (res.ok && res.contract_address)
            dictionaryAddress.text = res.contract_address
    }

    function loadDictionaryStats() {
        var total = dictionaryView("total_terms", [])
        if (total.ok && total.total_terms !== undefined)
            dictionaryTotal.text = String(total.total_terms)
        var owner = dictionaryView("owner_address", [])
        if (owner.ok && owner.owner_address !== undefined)
            dictionaryOwner.text = owner.owner_address
        refreshDictionaryOutput(total.ok ? total : owner, "Dictionary stats loaded", "Dictionary stats failed")
    }

    function lookupDictionaryTerm() {
        var term = dictionaryTerm.text.trim()
        var definition = dictionaryView("definition_of", [term])
        var example = dictionaryView("example_of", [term])
        var category = dictionaryView("category_of", [term])
        var pronunciation = dictionaryView("pronunciation_of", [term])
        var synonym = dictionaryView("synonym_of", [term])
        var antonym = dictionaryView("antonym_of", [term])
        var exists = dictionaryView("exists", [term])
        var revision = dictionaryView("revision_of", [term])
        var translation = dictionaryView("translation_of", [term, dictionaryLanguage.text.trim()])
        dictionaryText = JSON.stringify({
            exists: exists.ok ? exists.exists : exists.error,
            definition: definition.ok ? definition.definition_of : definition.error,
            example: example.ok ? example.example_of : example.error,
            category: category.ok ? category.category_of : category.error,
            pronunciation: pronunciation.ok ? pronunciation.pronunciation_of : pronunciation.error,
            synonym: synonym.ok ? synonym.synonym_of : synonym.error,
            antonym: antonym.ok ? antonym.antonym_of : antonym.error,
            revision: revision.ok ? revision.revision_of : revision.error,
            translation: translation.ok ? translation.translation_of : translation.error
        }, null, 2)
        outputTabIndex = 3
        setFeedback(exists.ok ? exists : definition, "Dictionary lookup loaded", "Dictionary lookup failed")
    }

    function updateDictionary(action, values) {
        var res = dictionaryCall(action, values, "")
        submitTransaction(res, action + " submitted", action + " failed")
        refreshDictionaryOutput(res, action + " submitted", action + " failed")
    }

    function verifyCurrentSource() {
        var res = octraBridge.verifyContract(verifyAddr.text, projectEditor.text)
        setFeedback(res, "Source verified", "Verify failed")
    }

    function verifyCurrentProject() {
        if (!persistProject(true))
            return
        var res = octraBridge.verifyContract(verifyAddr.text, projectSourceForMain(), JSON.stringify(projectDependencyFiles()))
        setFeedback(res, "Project verified", "Verify failed")
    }

    function loadContractInfo() {
        var res = octraBridge.contractInfo(infoAddr.text)
        setFeedback(res, "Contract info loaded", "Info lookup failed")
        if (res.ok) {
            storageText = JSON.stringify(res, null, 2)
            outputTabIndex = 3
        }
    }

    function loadContractReceipt() {
        var res = octraBridge.contractReceipt(receiptHash.text)
        setFeedback(res, "Receipt loaded", "Receipt lookup failed")
        if (res.ok) {
            storageText = JSON.stringify(res, null, 2)
            outputTabIndex = 3
        }
    }

    function loadContractStorage() {
        var res = octraBridge.contractStorage(infoAddr.text, storageKey.text)
        setFeedback(res, "Storage loaded", "Storage lookup failed")
        if (res.ok) {
            storageText = JSON.stringify(res, null, 2)
            outputTabIndex = 3
        }
    }

    function encryptFheValue() {
        var res = octraBridge.fheEncrypt(Number(fheValue.text))
        setFeedback(res, "FHE value encrypted", "FHE encrypt failed")
        if (res.ok && res.ciphertext !== undefined)
            fheCipher.text = res.ciphertext
    }

    function decryptFheValue() {
        setFeedback(octraBridge.fheDecrypt(fheCipher.text), "FHE value decrypted", "FHE decrypt failed")
    }

    function applyNetworkPreset(name) {
        var preset = octraBridge.networkPreset(name)
        setFeedback(preset)
        if (!preset.ok)
            return
        rpcUrl.text = preset.rpcUrl
        explorerUrl.text = preset.explorerUrl
        signerUrl.text = preset.bridgeSignerUrl
    }

    Component.onCompleted: {
        refreshWorkspace("")
        fillRecommendedFees()
        refreshTransactions()
    }

    Connections {
        target: octraBridge
        function onStateChanged() {
            if (octraBridge.loaded)
                refreshTransactions()
            else
                transactionModel.clear()
        }
    }

    Timer {
        id: autosaveTimer
        interval: 900
        repeat: false
        onTriggered: persistProject(true)
    }

    Dialog {
        id: newProjectDialog
        modal: true
        title: "New Project"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: createNewProject()

        contentItem: ColumnLayout {
            spacing: 12
            TextField {
                id: newProjectName
                Layout.fillWidth: true
                placeholderText: "Project name"
                text: "Blue Project"
            }
            ComboBox {
                id: newProjectTemplate
                Layout.fillWidth: true
                model: [
                    { text: "Empty", value: "empty" },
                    { text: "Token", value: "token" },
                    { text: "Vault", value: "vault" },
                    { text: "AMM", value: "amm" },
                    { text: "Escrow", value: "escrow" },
                    { text: "Multisig", value: "multisig" }
                ]
                textRole: "text"
                valueRole: "value"
            }
        }
    }

    FileDialog {
        id: importFileDialog
        title: "Import Project File"
        nameFilters: ["Project files (*.zip *.aml-project.json *.json *.aml *.oasm)"]
        onAccepted: importProjectResult(octraBridge.importProjectFile(selectedFile.toString()))
    }

    LabsPlatform.FolderDialog {
        id: importFolderDialog
        title: "Import Project Folder"
        onAccepted: importProjectResult(octraBridge.importProjectFolder(folder.toString()))
    }

    FileDialog {
        id: exportJsonDialog
        title: "Export Project JSON"
        fileMode: FileDialog.SaveFile
        nameFilters: ["AML Project JSON (*.aml-project.json)"]
        onAccepted: exportCurrentProjectJson(selectedFile.toString())
    }

    LabsPlatform.FolderDialog {
        id: exportFolderDialog
        title: "Export Project Folder"
        onAccepted: exportCurrentProjectFolder(folder.toString())
    }

    FileDialog {
        id: exportZipDialog
        title: "Export Project Zip"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Zip Archive (*.zip)"]
        onAccepted: exportCurrentProjectZip(selectedFile.toString())
    }

    header: ToolBar {
        background: Rectangle {
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#0d1730" }
                GradientStop { position: 1.0; color: "#0a1324" }
            }
            border.color: "#233554"
        }
        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label {
                    text: "Octra IDE"
                    font.pixelSize: 20
                    font.bold: true
                    color: ink
                }
                Label {
                    text: octraBridge.loaded ? "Wallet " + octraBridge.address : "Wallet locked"
                    color: muted
                    elide: Label.ElideRight
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                spacing: 8
                Rectangle {
                    radius: 999
                    color: octraBridge.loaded ? "#143b32" : "#3b2c14"
                    border.color: octraBridge.loaded ? okColor : warnColor
                    border.width: 1
                    implicitHeight: 30
                    implicitWidth: 106
                    Label {
                        anchors.centerIn: parent
                        text: octraBridge.loaded ? "Unlocked" : "Locked"
                        color: ink
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
                    Rectangle {
                        radius: 999
                        color: "#11213c"
                        border.color: accentSoft
                        border.width: 1
                        implicitHeight: 30
                        implicitWidth: 180
                        Label {
                            anchors.centerIn: parent
                            text: octraBridge.networkName + " • " + octraBridge.rpcUrl
                            color: ink
                            font.pixelSize: 11
                            font.bold: true
                            width: parent.width - 14
                            horizontalAlignment: Text.AlignHCenter
                            elide: Label.ElideRight
                        }
                    }
            }
        }
    }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            Frame {
                Layout.fillWidth: true
                background: Rectangle {
                    radius: 18
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#11203b" }
                        GradientStop { position: 1.0; color: "#0f1a30" }
                    }
                    border.color: accentSoft
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 14
                        color: "#13233f"
                        border.color: accentSoft
                        border.width: 1
                        implicitHeight: 64
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Label { text: "Workspace"; color: muted; font.pixelSize: 11; font.capitalization: Font.AllUppercase; font.letterSpacing: 1.1 }
                            Label {
                                text: currentProjectId.length ? projectNameField.text : "No project open"
                                color: ink
                                font.pixelSize: 16
                                font.bold: true
                                elide: Label.ElideRight
                                width: parent.width
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 14
                        color: "#13233f"
                        border.color: accentSoft
                        border.width: 1
                        implicitHeight: 64
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Label { text: "Network"; color: muted; font.pixelSize: 11; font.capitalization: Font.AllUppercase; font.letterSpacing: 1.1 }
                            Label {
                                text: octraBridge.networkName
                                color: ink
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 14
                        color: "#13233f"
                        border.color: accentSoft
                        border.width: 1
                        implicitHeight: 64
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Label { text: "Active File"; color: muted; font.pixelSize: 11; font.capitalization: Font.AllUppercase; font.letterSpacing: 1.1 }
                            Label {
                                text: activeFilePath.length ? activeFilePath : "main.aml"
                                color: ink
                                font.pixelSize: 16
                                font.bold: true
                                elide: Label.ElideRight
                                width: parent.width
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: 14
                        color: projectDirty ? "#342312" : "#12382f"
                        border.color: projectDirty ? warnColor : okColor
                        border.width: 1
                        implicitHeight: 64
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Label { text: "Save State"; color: muted; font.pixelSize: 11; font.capitalization: Font.AllUppercase; font.letterSpacing: 1.1 }
                            Label {
                                text: projectDirty ? "Autosave pending" : "All changes saved"
                                color: ink
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }
                    }
                }
            }

            TabBar {
                id: mainTabs
                Layout.fillWidth: true
                background: Rectangle {
                    radius: 18
                    color: "#0f1a30"
                    border.color: accentSoft
                }
                TabButton { text: "Home" }
                TabButton { text: "Dashboard" }
                TabButton { text: "Dev Tools IDE" }
                TabButton { text: "Dictionary DApp" }
                TabButton { text: "Settings" }
            }

        StackLayout {
            currentIndex: mainTabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: Rectangle { color: "transparent" }
                SplitView {
                    anchors.fill: parent
                    spacing: 12

                    ColumnLayout {
                        SplitView.preferredWidth: 520
                        spacing: 12

                        Frame {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            background: Rectangle {
                                radius: 18
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "#162b52" }
                                    GradientStop { position: 1.0; color: "#0f1a30" }
                                }
                                border.color: accentSoft
                            }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 12

                                Label {
                                    text: "Octra IDE"
                                    color: accent
                                    font.pixelSize: 12
                                    font.bold: true
                                    font.capitalization: Font.AllUppercase
                                    font.letterSpacing: 1.2
                                }
                                Label {
                                    text: "Premium Octra workspace for wallets, contract deployment, and on-chain dictionary apps."
                                    color: ink
                                    font.pixelSize: 24
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: "Devnet-ready, template-aware, and tuned for fast create, deploy, inspect, and verify flows."
                                    color: muted
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }

                                RowLayout {
                                    spacing: 8
                                    Rectangle {
                                        radius: 999
                                        color: "#143b32"
                                        border.color: okColor
                                        border.width: 1
                                        implicitHeight: 28
                                        implicitWidth: 108
                                        Label { anchors.centerIn: parent; text: "Devnet"; color: ink; font.pixelSize: 11; font.bold: true }
                                    }
                                    Rectangle {
                                        radius: 999
                                        color: "#11213c"
                                        border.color: accentSoft
                                        border.width: 1
                                        implicitHeight: 28
                                        implicitWidth: 122
                                        Label { anchors.centerIn: parent; text: "AML + DApps"; color: ink; font.pixelSize: 11; font.bold: true }
                                    }
                                    Rectangle {
                                        radius: 999
                                        color: "#342312"
                                        border.color: warnColor
                                        border.width: 1
                                        implicitHeight: 28
                                        implicitWidth: 126
                                        Label { anchors.centerIn: parent; text: "Explorer links"; color: ink; font.pixelSize: 11; font.bold: true }
                                    }
                                }

                                RowLayout {
                                    spacing: 10
                                    Button { text: "Open Dashboard"; onClicked: mainTabs.currentIndex = 1 }
                                    Button { text: "Open IDE"; onClicked: mainTabs.currentIndex = 2 }
                                    Button { text: "Open Dictionary"; onClicked: mainTabs.currentIndex = 3 }
                                }
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                Label { text: "Wallet Access"; font.bold: true; color: ink }
                                Label { text: octraBridge.loaded ? "Wallet ready: " + octraBridge.address : "Create or unlock a wallet to start."; color: muted; wrapMode: Text.Wrap }
                                TextField { id: unlockPin; placeholderText: "6-digit PIN"; echoMode: TextInput.Password; Layout.fillWidth: true }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Button {
                                        text: "Unlock"
                                        Layout.fillWidth: true
                                        onClicked: setFeedback(octraBridge.unlockWallet(unlockPin.text), "Wallet unlocked", "Unlock failed")
                                    }
                                    Button {
                                        text: "Create Wallet"
                                        Layout.fillWidth: true
                                        onClicked: setFeedback(octraBridge.createWallet(unlockPin.text), "Wallet created", "Create failed")
                                    }
                                }
                                Label { text: "Import Private Key"; font.bold: true; color: ink }
                                TextArea {
                                    id: privateKey
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 120
                                    wrapMode: TextEdit.WrapAnywhere
                                    placeholderText: "Private key (base64)"
                                    background: Rectangle { color: panelAlt; radius: 12; border.color: "#d7e6fb" }
                                }
                                Button {
                                    text: "Import Private Key"
                                    Layout.fillWidth: true
                                    onClicked: setFeedback(octraBridge.importPrivateKey(privateKey.text, unlockPin.text), "Private key imported", "Import failed")
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        SplitView.fillWidth: true
                        spacing: 12

                        Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label { text: "Recent Transactions"; font.bold: true; color: ink }
                                        Label {
                                            text: octraBridge.loaded ? ("Explorer: " + walletExplorerLabel()) : "Unlock a wallet to load activity."
                                            color: muted
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                    Button {
                                        text: "Refresh"
                                        onClicked: refreshTransactions()
                                    }
                                }

                                ListView {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    model: transactionModel
                                    clip: true
                                    spacing: 8
                                    delegate: Frame {
                                        width: ListView.view.width
                                        background: Rectangle {
                                            radius: 14
                                            color: panelAlt
                                            border.color: accentSoft
                                        }
                                        ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 6

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Label {
                                                    text: hash.length ? hash.slice(0, 16) + "..." : "Transaction"
                                                    color: ink
                                                    font.bold: true
                                                    elide: Label.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                                Rectangle {
                                                    radius: 999
                                                    color: status === "confirmed" ? "#12382f" : "#3b2c14"
                                                    border.color: status === "confirmed" ? okColor : warnColor
                                                    border.width: 1
                                                    implicitHeight: 24
                                                    implicitWidth: 92
                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: status
                                                        color: ink
                                                        font.pixelSize: 10
                                                        font.bold: true
                                                    }
                                                }
                                            }

                                            Label {
                                                text: op_type + " • " + (amount_raw || "0") + " raw • " + formatTxTime(timestamp)
                                                color: muted
                                                wrapMode: Text.Wrap
                                                Layout.fillWidth: true
                                            }

                                            Label {
                                                text: "from " + (from || "n/a") + " to " + (to_ || "n/a")
                                                color: muted
                                                font.pixelSize: 11
                                                elide: Label.ElideMiddle
                                                Layout.fillWidth: true
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Label {
                                                    text: "<a href='" + explorerUrl + "'>Open in explorer</a>"
                                                    textFormat: Text.RichText
                                                    color: accent
                                                    onLinkActivated: Qt.openUrlExternally(link)
                                                    Layout.fillWidth: true
                                                }
                                                Button {
                                                    text: "Inspect"
                                                    onClicked: {
                                                        receiptHash.text = hash
                                                        loadContractReceipt()
                                                        mainTabs.currentIndex = 2
                                                        actionTabs.currentIndex = 4
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: Rectangle { color: "transparent" }

                SplitView {
                    anchors.fill: parent
                    spacing: 12

                    Frame {
                        SplitView.preferredWidth: 420
                        background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Label { text: "Dashboard"; font.bold: true; color: ink }
                            Label { text: octraBridge.loaded ? ("Balance for " + octraBridge.address) : "Unlock a wallet to send funds."; color: muted; wrapMode: Text.Wrap }
                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: "Refresh Balance"
                                    Layout.fillWidth: true
                                    onClicked: {
                                        setFeedback(octraBridge.getBalance(), "Balance refreshed", "Balance fetch failed")
                                        refreshTransactions()
                                    }
                                }
                                Button {
                                    text: "Refresh History"
                                    Layout.fillWidth: true
                                    onClicked: refreshTransactions()
                                }
                            }
                            TextField { id: sendTo; placeholderText: "Recipient oct..."; Layout.fillWidth: true }
                            TextField { id: sendAmount; placeholderText: "Amount in OCT"; Layout.fillWidth: true }
                            TextField { id: sendFee; placeholderText: "Fee OU"; Layout.fillWidth: true }
                            TextField { id: sendMessage; placeholderText: "Message"; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: "Use Recommended Fee"
                                    Layout.fillWidth: true
                                    onClicked: fillRecommendedFees()
                                }
                                Button {
                                    text: "Send"
                                    Layout.fillWidth: true
                                    onClicked: sendCurrentTransfer()
                                }
                            }
                        }
                    }

                    Frame {
                        SplitView.fillWidth: true
                        background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Network / Send Details"; font.bold: true; color: ink; Layout.fillWidth: true }
                                Label { text: walletExplorerLabel(); color: muted }
                            }

                            TextArea {
                                id: dashboardConsole
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                readOnly: true
                                text: feedback
                                wrapMode: TextEdit.WrapAnywhere
                                background: Rectangle { color: panelAlt; radius: 12; border.color: "#d7e6fb" }
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: Rectangle { color: "transparent" }

                SplitView {
                    anchors.fill: parent
                    spacing: 12

                    Frame {
                        SplitView.preferredWidth: 320
                        background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            Label { text: "Workspace"; font.bold: true; color: ink }
                            Label { id: workspaceRootLabel; text: "Workspace"; color: muted; wrapMode: Text.Wrap }
                            ComboBox {
                                id: projectSelector
                                Layout.fillWidth: true
                                model: projectSummaryModel
                                textRole: "name"
                                onActivated: {
                                    if (currentIndex >= 0 && currentIndex < projectSummaryModel.count)
                                        openProject(projectSummaryModel.get(currentIndex).id, false)
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 8
                                columnSpacing: 8

                                Button { text: "New"; Layout.fillWidth: true; onClicked: newProjectDialog.open() }
                                Button { text: "Duplicate"; Layout.fillWidth: true; onClicked: duplicateCurrentProject() }
                                Button {
                                    text: "Delete"
                                    Layout.fillWidth: true
                                    onClicked: {
                                        setFeedback(octraBridge.deleteProject(currentProjectId), "Project deleted", "Delete failed")
                                        refreshWorkspace("")
                                    }
                                }
                                Button { text: "Save"; Layout.fillWidth: true; onClicked: persistProject(false) }
                                Button { text: "Import File"; Layout.fillWidth: true; onClicked: importFileDialog.open() }
                                Button { text: "Import Folder"; Layout.fillWidth: true; onClicked: importFolderDialog.open() }
                                Button { text: "Export JSON"; Layout.fillWidth: true; onClicked: exportJsonDialog.open() }
                                Button { text: "Export Folder"; Layout.fillWidth: true; onClicked: exportFolderDialog.open() }
                                Button { text: "Export Zip"; Layout.fillWidth: true; onClicked: exportZipDialog.open() }
                                Button { text: "Fees"; Layout.fillWidth: true; onClicked: fillRecommendedFees() }
                            }

                            Label { text: "Project Name"; color: muted }
                            TextField {
                                id: projectNameField
                                Layout.fillWidth: true
                                onTextChanged: {
                                    projectDirty = true
                                    autosaveTimer.restart()
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: panelAlt
                                radius: 14
                                border.color: "#d7e6fb"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: "Files"; font.bold: true; color: ink; Layout.fillWidth: true }
                                        Label {
                                            text: projectDirty ? "Autosave pending" : "Saved"
                                            color: projectDirty ? warnColor : okColor
                                        }
                                    }

                                    ListView {
                                        id: fileList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        model: projectFileModel
                                        clip: true
                                        spacing: 4
                                        delegate: ItemDelegate {
                                            width: fileList.width
                                            text: model.path
                                            highlighted: model.path === activeFilePath
                                            onClicked: selectFileByPath(model.path)
                                        }
                                    }

                                    TextField {
                                        id: filePathField
                                        Layout.fillWidth: true
                                        placeholderText: "File path (e.g. main.aml)"
                                        onTextChanged: {
                                            activeFilePath = normalizeFilePath(text)
                                            projectDirty = true
                                            autosaveTimer.restart()
                                        }
                                    }
                                    RowLayout {
                                        Button { text: "Add / Update"; onClicked: addOrUpdateFile() }
                                        Button { text: "Remove"; onClicked: deleteActiveFile() }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        SplitView.fillWidth: true
                        SplitView.fillHeight: true
                        spacing: 12

                        Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label { text: activeFilePath.length ? activeFilePath : "Editor"; font.bold: true; color: ink; Layout.fillWidth: true }
                                    Label {
                                        text: editorErrorLine > 0 ? "Error line " + editorErrorLine : editorStatus
                                        color: editorErrorLine > 0 ? dangerColor : muted
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    ComboBox {
                                        id: languageSelector
                                        model: ["AppliedML", "Assembly"]
                                    }
                                    Button { text: "Compile Current"; onClicked: compileCurrentEditor() }
                                    Button { text: "Compile Project"; onClicked: compileCurrentProject() }
                                    Button { text: "Preview Address"; onClicked: previewDeployAddress() }
                                }

                                TextArea {
                                    id: projectEditor
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    wrapMode: TextEdit.NoWrap
                                    selectByKeyboard: true
                                    selectByMouse: true
                                    placeholderText: "Project source"
                                    background: Rectangle {
                                        radius: 12
                                        color: panelAlt
                                        border.color: editorErrorLine > 0 ? "#fca5a5" : "#d7e6fb"
                                        border.width: editorErrorLine > 0 ? 2 : 1
                                    }
                                    onTextChanged: {
                                        projectDirty = true
                                        autosaveTimer.restart()
                                        editorStatus = "Editing"
                                    }
                                }
                            }
                        }

                        SplitView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 360
                            orientation: Qt.Horizontal
                            spacing: 12

                            Frame {
                                SplitView.preferredWidth: 520
                                background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    TabBar {
                                        id: actionTabs
                                        Layout.fillWidth: true
                                        TabButton { text: "Compile" }
                                        TabButton { text: "Deploy" }
                                        TabButton { text: "Call / View" }
                                        TabButton { text: "Verify" }
                                        TabButton { text: "Inspect" }
                                        TabButton { text: "FHE" }
                                    }

                                    StackLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        currentIndex: actionTabs.currentIndex

                                        ColumnLayout {
                                            spacing: 8
                                            Label { text: "Use current file or full project compile actions above."; color: muted; wrapMode: Text.Wrap }
                                            Button { text: "Load Recommended Fees"; onClicked: fillRecommendedFees() }
                                        }

                                        ColumnLayout {
                                            spacing: 8
                                            TextArea {
                                                id: bytecodeOutput
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 120
                                                wrapMode: TextEdit.WrapAnywhere
                                                placeholderText: "Compiled bytecode"
                                            }
                                            TextField { id: deployParams; placeholderText: "Constructor params JSON array"; Layout.fillWidth: true }
                                            TextField { id: deployFee; placeholderText: "Deploy fee OU"; Layout.fillWidth: true }
                                            RowLayout {
                                                Button { text: "Use Deploy Fee"; onClicked: fillRecommendedFees() }
                                                Button { text: "Preview Address"; onClicked: previewDeployAddress() }
                                                Button { text: "Deploy"; onClicked: deployCurrentContract() }
                                            }
                                        }

                                        ColumnLayout {
                                            spacing: 8
                                            TextField { id: contractAddr; placeholderText: "Contract address"; Layout.fillWidth: true }
                                            TextField { id: contractMethod; placeholderText: "Method name"; Layout.fillWidth: true }
                                            TextField { id: contractParams; placeholderText: "Params JSON array"; Layout.fillWidth: true }
                                            TextField { id: contractAmount; placeholderText: "Amount raw"; Layout.fillWidth: true }
                                            TextField { id: contractFee; placeholderText: "Call fee OU"; Layout.fillWidth: true }
                                            RowLayout {
                                                Button { text: "Use Call Fee"; onClicked: fillRecommendedFees() }
                                                Button { text: "Send Call"; onClicked: callCurrentContract() }
                                                Button { text: "View"; onClicked: viewCurrentContract() }
                                            }
                                        }

                                        ColumnLayout {
                                            spacing: 8
                                            TextField { id: verifyAddr; placeholderText: "Contract address"; Layout.fillWidth: true }
                                            RowLayout {
                                                Button { text: "Verify Current File"; onClicked: verifyCurrentSource() }
                                                Button { text: "Verify Project"; onClicked: verifyCurrentProject() }
                                            }
                                            Label { text: "Project verification sends main.aml plus dependency files."; color: muted; wrapMode: Text.Wrap }
                                        }

                                        ColumnLayout {
                                            spacing: 8
                                            TextField { id: infoAddr; placeholderText: "Contract address"; Layout.fillWidth: true }
                                            TextField { id: receiptHash; placeholderText: "Receipt tx hash"; Layout.fillWidth: true }
                                            RowLayout {
                                                Button { text: "Contract Info"; onClicked: loadContractInfo() }
                                                Button { text: "Receipt"; onClicked: loadContractReceipt() }
                                            }
                                            RowLayout {
                                                TextField { id: storageKey; placeholderText: "Storage key"; Layout.fillWidth: true }
                                                Button { text: "Storage"; onClicked: loadContractStorage() }
                                            }
                                        }

                                        ColumnLayout {
                                            spacing: 8
                                            TextField { id: fheValue; placeholderText: "Integer value"; Layout.fillWidth: true }
                                            Button { text: "Encrypt"; onClicked: encryptFheValue() }
                                            TextArea {
                                                id: fheCipher
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 120
                                                wrapMode: TextEdit.WrapAnywhere
                                                placeholderText: "Ciphertext (base64)"
                                            }
                                            Button { text: "Decrypt"; onClicked: decryptFheValue() }
                                        }
                                    }
                                }
                            }

                            Frame {
                                SplitView.fillWidth: true
                                background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    TabBar {
                                        id: outputTabs
                                        Layout.fillWidth: true
                                        currentIndex: outputTabIndex
                                        onCurrentIndexChanged: outputTabIndex = currentIndex
                                        TabButton { text: "ABI" }
                                        TabButton { text: "Assembly" }
                                        TabButton { text: "Console" }
                                        TabButton { text: "Storage" }
                                    }

                                    StackLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        currentIndex: outputTabs.currentIndex

                                        TextArea {
                                            readOnly: true
                                            text: abiText
                                            wrapMode: TextEdit.WrapAnywhere
                                            placeholderText: "ABI output"
                                        }
                                        TextArea {
                                            readOnly: true
                                            text: disasmText
                                            wrapMode: TextEdit.NoWrap
                                            placeholderText: "Assembly / disassembly"
                                        }
                                        TextArea {
                                            readOnly: true
                                            text: consoleText
                                            wrapMode: TextEdit.WrapAnywhere
                                            placeholderText: "Console output"
                                        }
                                        TextArea {
                                            readOnly: true
                                            text: storageText
                                            wrapMode: TextEdit.WrapAnywhere
                                            placeholderText: "Storage / inspect output"
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: Rectangle { color: "transparent" }

                SplitView {
                    anchors.fill: parent
                    spacing: 12

                    Frame {
                        SplitView.preferredWidth: 380
                        background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                        ScrollView {
                            anchors.fill: parent
                            contentWidth: availableWidth
                            ColumnLayout {
                                width: parent.width
                                spacing: 10

                                Label { text: "Dictionary DApp"; font.bold: true; color: ink }
                                Label { text: "Deploy, curate, and query an on-chain dictionary from one screen."; color: muted; wrapMode: Text.Wrap }

                                TextField { id: dictionaryAddress; placeholderText: "Dictionary contract address"; Layout.fillWidth: true }
                                RowLayout {
                                    Button { text: "Deploy Dictionary"; onClicked: deployDictionaryContract() }
                                    Button { text: "Load Stats"; onClicked: loadDictionaryStats() }
                                }
                                TextField { id: dictionaryDeployFee; placeholderText: "Deploy fee OU"; Layout.fillWidth: true }
                                TextField { id: dictionaryFee; placeholderText: "Call fee OU"; Layout.fillWidth: true }
                                Button { text: "Use Recommended Fees"; onClicked: fillRecommendedFees() }

                                Rectangle {
                                    Layout.fillWidth: true
                                    radius: 14
                                    color: panelAlt
                                    border.color: "#d7e6fb"
                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 8
                                        Label { text: "Contract Summary"; font.bold: true; color: ink }
                                        TextField { id: dictionaryOwner; readOnly: true; placeholderText: "Owner"; Layout.fillWidth: true }
                                        TextField { id: dictionaryTotal; readOnly: true; placeholderText: "Total terms"; Layout.fillWidth: true }
                                    }
                                }

                                Label { text: "Lookup"; font.bold: true; color: ink }
                                TextField { id: dictionaryTerm; placeholderText: "Term"; Layout.fillWidth: true }
                                TextField { id: dictionaryLanguage; placeholderText: "Language code for translation, e.g. es"; Layout.fillWidth: true }
                                RowLayout {
                                    Button { text: "Lookup Term"; onClicked: lookupDictionaryTerm() }
                                    Button { text: "Remove Term"; onClicked: updateDictionary("remove_term", [dictionaryTerm.text.trim()]) }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        SplitView.fillWidth: true
                        SplitView.fillHeight: true
                        spacing: 12

                        Frame {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                            ScrollView {
                                anchors.fill: parent
                                contentWidth: availableWidth
                                ColumnLayout {
                                    width: parent.width
                                    spacing: 12

                                    Label { text: "Entry Editor"; font.bold: true; color: ink }
                                    TextField { id: dictionaryDefinition; placeholderText: "Definition"; Layout.fillWidth: true }
                                    TextField { id: dictionaryExample; placeholderText: "Example sentence"; Layout.fillWidth: true }
                                    TextField { id: dictionaryCategory; placeholderText: "Category"; Layout.fillWidth: true }
                                    TextField { id: dictionaryPronunciation; placeholderText: "Pronunciation"; Layout.fillWidth: true }
                                    TextField { id: dictionarySynonym; placeholderText: "Synonym"; Layout.fillWidth: true }
                                    TextField { id: dictionaryAntonym; placeholderText: "Antonym"; Layout.fillWidth: true }
                                    TextField { id: dictionaryTranslation; placeholderText: "Translation text"; Layout.fillWidth: true }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        rowSpacing: 8
                                        columnSpacing: 8

                                        Button { text: "Set Entry"; Layout.fillWidth: true; onClicked: updateDictionary("set_entry", [dictionaryTerm.text.trim(), dictionaryDefinition.text.trim(), dictionaryExample.text.trim(), dictionaryCategory.text.trim(), dictionaryPronunciation.text.trim()]) }
                                        Button { text: "Set Definition"; Layout.fillWidth: true; onClicked: updateDictionary("set_definition", [dictionaryTerm.text.trim(), dictionaryDefinition.text.trim()]) }
                                        Button { text: "Set Example"; Layout.fillWidth: true; onClicked: updateDictionary("set_example", [dictionaryTerm.text.trim(), dictionaryExample.text.trim()]) }
                                        Button { text: "Set Category"; Layout.fillWidth: true; onClicked: updateDictionary("set_category", [dictionaryTerm.text.trim(), dictionaryCategory.text.trim()]) }
                                        Button { text: "Set Pronunciation"; Layout.fillWidth: true; onClicked: updateDictionary("set_pronunciation", [dictionaryTerm.text.trim(), dictionaryPronunciation.text.trim()]) }
                                        Button { text: "Set Synonym"; Layout.fillWidth: true; onClicked: updateDictionary("set_synonym", [dictionaryTerm.text.trim(), dictionarySynonym.text.trim()]) }
                                        Button { text: "Set Antonym"; Layout.fillWidth: true; onClicked: updateDictionary("set_antonym", [dictionaryTerm.text.trim(), dictionaryAntonym.text.trim()]) }
                                        Button { text: "Set Translation"; Layout.fillWidth: true; onClicked: updateDictionary("set_translation", [dictionaryTerm.text.trim(), dictionaryLanguage.text.trim(), dictionaryTranslation.text.trim()]) }
                                        Button { text: "Clear Translation"; Layout.fillWidth: true; onClicked: updateDictionary("clear_translation", [dictionaryTerm.text.trim(), dictionaryLanguage.text.trim()]) }
                                    }
                                }
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8
                                Label { text: "Output"; font.bold: true; color: ink }
                                TextArea {
                                    readOnly: true
                                    text: dictionaryText
                                    wrapMode: TextEdit.WrapAnywhere
                                    placeholderText: "Dictionary results and responses"
                                    background: Rectangle { color: panelAlt; radius: 12; border.color: "#d7e6fb" }
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: Rectangle { color: panelBg; radius: 16; border.color: accentSoft }
                ScrollView {
                    anchors.fill: parent
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 12

                        Label { text: "Network Settings"; font.bold: true; color: ink }
                        RowLayout {
                            Button { text: "Use Mainnet"; onClicked: applyNetworkPreset("mainnet") }
                            Button { text: "Use Devnet"; onClicked: applyNetworkPreset("devnet") }
                        }
                        TextField { id: rpcUrl; placeholderText: "RPC URL"; Layout.fillWidth: true }
                        TextField { id: explorerUrl; placeholderText: "Explorer URL"; Layout.fillWidth: true }
                        TextField { id: signerUrl; placeholderText: "Bridge signer URL"; Layout.fillWidth: true }
                        Button {
                            text: "Save Settings"
                            onClicked: setFeedback(octraBridge.saveSettings(rpcUrl.text, explorerUrl.text, signerUrl.text), "Settings saved", "Save settings failed")
                        }
                    }
                }
            }
        }
    }

    footer: Frame {
        background: Rectangle { color: panelBg; border.color: accentSoft }
        width: parent.width
        height: 210
        TextArea {
            anchors.fill: parent
            anchors.margins: 8
            readOnly: true
            text: feedback
            wrapMode: TextEdit.WrapAnywhere
            background: Rectangle { color: panelAlt; radius: 12; border.color: "#d7e6fb" }
        }
    }
}
