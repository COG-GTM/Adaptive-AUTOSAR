"use strict";

const cMaxLogEntries = 200;
const cMaxSomeIpEntries = 60;
const cMaxTransitions = 12;

const state = {
    logs: [],
    someIpMessages: [],
    transitions: [],
    functionGroupStates: {},
    applicationStates: {}
};

function formatTime(timestampMs) {
    const date = new Date(timestampMs);
    const pad = (value, width) => String(value).padStart(width, "0");

    return `${pad(date.getHours(), 2)}:${pad(date.getMinutes(), 2)}:` +
        `${pad(date.getSeconds(), 2)}.${pad(date.getMilliseconds(), 3)}`;
}

function formatUptime(uptimeMs) {
    const totalSeconds = Math.floor(uptimeMs / 1000);
    const pad = (value) => String(value).padStart(2, "0");

    return `${pad(Math.floor(totalSeconds / 3600))}:` +
        `${pad(Math.floor(totalSeconds / 60) % 60)}:${pad(totalSeconds % 60)}`;
}

function formatId(id) {
    return `0x${id.toString(16).padStart(4, "0")}`;
}

function statusClass(status) {
    return status ? status.toLowerCase() : "kdeactivated";
}

function setLink(online) {
    const link = document.getElementById("link");
    link.textContent = online ? "LIVE" : "OFFLINE";
    link.classList.toggle("online", online);
}

function renderKeyedTable(tableId, rows, previousStates) {
    const body = document.querySelector(`#${tableId} tbody`);
    body.innerHTML = "";

    rows.forEach((row) => {
        const tableRow = document.createElement("tr");
        if (previousStates[row.key] !== undefined &&
            previousStates[row.key] !== row.value) {
            tableRow.className = "changed";
        }
        previousStates[row.key] = row.value;

        const keyCell = document.createElement("td");
        keyCell.textContent = row.key;

        const valueCell = document.createElement("td");
        const badge = document.createElement("span");
        badge.className = `state-badge ${statusClass(row.value)}`;
        badge.textContent = row.value;
        valueCell.appendChild(badge);

        tableRow.appendChild(keyCell);
        tableRow.appendChild(valueCell);
        body.appendChild(tableRow);
    });
}

function renderGlobalSupervision(snapshot) {
    const global = snapshot.globalSupervision || {};
    const status = global.status || "kDeactivated";
    const element = document.getElementById("global-status");

    element.textContent = status.replace(/^k/, "").toUpperCase();
    element.className = `global-status ${statusClass(status)}`;

    document.getElementById("global-dominant").textContent =
        global.dominantType
            ? `dominant: ${global.dominantType}`
            : "no dominant supervision";

    const list = document.getElementById("supervisions");
    list.innerHTML = "";

    (snapshot.supervisions || []).forEach((supervision) => {
        const item = document.createElement("li");

        const name = document.createElement("span");
        name.textContent = supervision.name;

        const badge = document.createElement("span");
        badge.className = `state-badge ${statusClass(supervision.status)}`;
        badge.textContent = supervision.status;

        item.appendChild(name);
        item.appendChild(badge);
        list.appendChild(item);
    });
}

function renderCheckpoints(snapshot) {
    const body = document.querySelector("#checkpoints tbody");
    body.innerHTML = "";

    (snapshot.checkpoints || []).forEach((checkpoint) => {
        const row = document.createElement("tr");
        const cells = [
            String(checkpoint.id),
            checkpoint.name || "(unnamed)",
            String(checkpoint.reports),
            checkpoint.lastReportMs ? formatTime(checkpoint.lastReportMs) : "--"
        ];

        cells.forEach((text) => {
            const cell = document.createElement("td");
            cell.textContent = text;
            row.appendChild(cell);
        });

        body.appendChild(row);
    });
}

function renderSomeIp(snapshot) {
    const someIp = snapshot.someip || {};

    document.getElementById("someip-rate").textContent =
        (someIp.ratePerSecond || 0).toFixed(1);
    document.getElementById("someip-total").textContent = someIp.total || 0;

    state.someIpMessages = state.someIpMessages
        .concat(someIp.messages || [])
        .slice(-cMaxSomeIpEntries);

    const list = document.getElementById("someip-messages");
    list.innerHTML = "";

    state.someIpMessages.slice().reverse().forEach((message) => {
        const item = document.createElement("li");

        const timestamp = document.createElement("span");
        timestamp.className = "timestamp";
        timestamp.textContent = formatTime(message.timestampMs);

        const direction = document.createElement("span");
        direction.className = `direction-${message.direction}`;
        direction.textContent = message.direction.toUpperCase();

        const identifiers = document.createElement("span");
        identifiers.textContent =
            `service ${formatId(message.serviceId)} · method ` +
            `${formatId(message.methodId)} · ${message.payloadLength} B`;

        item.appendChild(timestamp);
        item.appendChild(direction);
        item.appendChild(identifiers);
        list.appendChild(item);
    });
}

function renderTransitions(snapshot) {
    state.transitions = state.transitions
        .concat(snapshot.transitions || [])
        .slice(-cMaxTransitions);

    const list = document.getElementById("transitions");
    list.innerHTML = "";

    state.transitions.slice().reverse().forEach((transition) => {
        const item = document.createElement("li");

        const timestamp = document.createElement("span");
        timestamp.className = "timestamp";
        timestamp.textContent = formatTime(transition.timestampMs);

        const description = document.createElement("span");
        description.textContent =
            `${transition.functionGroup} → ${transition.state}`;

        item.appendChild(timestamp);
        item.appendChild(description);
        list.appendChild(item);
    });
}

function renderLogs(snapshot) {
    state.logs = state.logs
        .concat(snapshot.logs || [])
        .slice(-cMaxLogEntries);

    const list = document.getElementById("logs");
    list.innerHTML = "";

    state.logs.slice().reverse().forEach((log) => {
        const item = document.createElement("li");
        item.className = `level-${(log.level || "info").toLowerCase()}`;

        const timestamp = document.createElement("span");
        timestamp.className = "timestamp";
        timestamp.textContent = formatTime(log.timestampMs);

        const level = document.createElement("span");
        level.className = "log-level";
        level.textContent = (log.level || "INFO").toUpperCase();

        const context = document.createElement("span");
        context.className = "log-context";
        context.textContent = `${log.application}/${log.context}`;

        const message = document.createElement("span");
        message.className = "log-message";
        message.textContent = log.message;

        item.appendChild(timestamp);
        item.appendChild(level);
        item.appendChild(context);
        item.appendChild(message);
        list.appendChild(item);
    });
}

function render(snapshot) {
    document.getElementById("uptime").textContent =
        formatUptime(snapshot.uptimeMs || 0);

    renderKeyedTable(
        "applications",
        (snapshot.applications || []).map((application) => ({
            key: application.name,
            value: application.executionState
        })),
        state.applicationStates);

    renderKeyedTable(
        "function-groups",
        (snapshot.functionGroups || []).map((functionGroup) => ({
            key: functionGroup.name,
            value: functionGroup.state
        })),
        state.functionGroupStates);

    renderGlobalSupervision(snapshot);
    renderCheckpoints(snapshot);
    renderSomeIp(snapshot);
    renderTransitions(snapshot);
    renderLogs(snapshot);
}

function resetState() {
    state.logs = [];
    state.someIpMessages = [];
    state.transitions = [];
    state.functionGroupStates = {};
    state.applicationStates = {};
}

function connect() {
    const source = new EventSource("/api/stream");

    source.onopen = () => {
        resetState();
        setLink(true);
    };

    source.onmessage = (event) => {
        setLink(true);
        render(JSON.parse(event.data));
    };

    source.onerror = () => {
        setLink(false);
        source.close();
        window.setTimeout(connect, 1000);
    };
}

connect();
