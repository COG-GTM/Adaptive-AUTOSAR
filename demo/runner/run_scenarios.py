#!/usr/bin/env python3
"""Execute real verification scenarios against this Adaptive AUTOSAR checkout.

The runner builds the platform, runs the unit suite, boots the simulator and
then exercises it over its real sockets (SOME/IP RPC on TCP, SOME/IP Service
Discovery on UDP multicast, DoIP/UDS on TCP). Everything it observes is written
to ``demo/data/runs/<UTC timestamp>.json`` and indexed in
``demo/data/runs/index.json``; the dashboard derives its badge states from that
artifact joined with the statically parsed ``demo/data/model.json``.

Python 3 standard library only. No scenario may hang: every step is bounded by
a timeout and the simulator is always torn down by killing its process group.
"""

import argparse
import errno
import json
import os
import pty
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import threading
import time

from datetime import datetime, timezone

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

BUILD_DIR = "build"
BINARY = "build/bin/adaptive_autosar"
MANIFESTS = [
    "./configuration/execution_manifest.arxml",
    "./configuration/extended_vehicle_manifest.arxml",
    "./configuration/diagnostic_manager_manifest.arxml",
    "./configuration/health_monitoring_manifest.arxml",
]

API_KEY_ENV = "VCC_API_KEY"
BEARER_TOKEN_ENV = "BEARER_TOKEN"

# Ports and addresses declared in configuration/*.arxml.
RPC_HOST, RPC_PORT = "127.0.0.1", 8080          # execution_manifest.arxml, RpcServerEP
DOIP_HOST, DOIP_PORT = "127.0.0.1", 8081        # extended_vehicle_manifest.arxml, ExtendedVehicleEP
SD_GROUP, SD_PORT = "239.0.0.1", 5555           # diagnostic_manager_manifest.arxml, MutlicastLocalhost

# SOME/IP RPC service and method identifiers implemented in src/ara/exec.
EXEC_SERVICE_ID, EXEC_REPORT_STATE_METHOD = 0x0001, 0x0001   # src/ara/exec/execution_server.h
STATE_SERVICE_ID, STATE_SET_STATE_METHOD = 0x0003, 0x0001    # src/ara/exec/state_server.h
STATE_TRANSITION_METHOD = 0x0002
SOMEIP_PROTOCOL_VERSION = 1                                   # PROTOCOL-VERSION in execution_manifest.arxml
SOMEIP_INTERFACE_VERSION = 1                                  # SocketRpcServer default

# UDS data identifiers implemented in src/application/helper/read_data_by_identifier.h.
UDS_READ_DATA_BY_IDENTIFIER = 0x22
UDS_DIDS = [
    (0xF50D, "average speed"),
    (0xF52F, "fuel amount"),
    (0xF546, "external temperature"),
    (0xF55E, "average fuel consumption"),
    (0xF505, "engine coolant temperature"),
    (0xF5A6, "odometer value"),
]

# Capability identifiers shared with demo/data/model.json (matrix.rows[].id).
CAP_SOMEIP_SD = "someip_sd"
CAP_SOMEIP_PUBSUB = "someip_pubsub"
CAP_SOMEIP_RPC = "someip_rpc"
CAP_EXEC = "exec_management"
CAP_STATE = "state_management"
CAP_PHM = "phm_supervision"
CAP_UDS_ROUTING = "uds_routing"
CAP_UDS_READ_DID = "uds_read_did"
CAP_UDS_SECURITY = "uds_security"
CAP_UDS_ECU_RESET = "uds_ecu_reset"
CAP_UDS_TRANSFER = "uds_transfer"
CAP_DOIP = "doip"
CAP_DTC = "dtc_reporting"
CAP_E2E = "e2e_protection"
CAP_LOGGING = "logging"

# Unit-test suites (gtest suite name prefixes in test/) per platform capability.
CAPABILITY_TEST_SUITES = {
    CAP_SOMEIP_SD: ["SomeIpSdTest", "SomeIpSdMessageTest", "ServiceEntryTest", "EventgroupEntryTest",
                    "Ipv4EndpointOptionTest", "LoadBalancingOptionTest", "TtlTimerTest", "DelayTimerTest"],
    CAP_SOMEIP_PUBSUB: ["SomeIpPubSubTest", "PubSubStateTest"],
    CAP_SOMEIP_RPC: ["SomeIpRpcMessageTest", "RpcServerTest", "RpcClientTest", "MessageTest"],
    CAP_EXEC: ["ExecutionServerTest", "ExecutionClientTest", "ModelledProcessTest", "DeterministicClientTest",
               "ExecExceptionTest", "WorkerThreadTest", "WorkerRunnableTest"],
    CAP_STATE: ["StateServerTest", "StateServerCtorTest", "StateClientTest", "FunctionGroupTest",
                "FunctionGroupStateTest", "MachineStateTest", "SMTriggerInTest", "SMTriggerOutTest",
                "TriggerInOutTest"],
    CAP_PHM: ["SupervisedEntityTest", "AliveSupervisionTest", "DeadlineSupervisionTest",
              "ElementarySupervisionTest", "GlobalSupervisionTest", "RecoveryActionTest"],
    CAP_UDS_ROUTING: ["UdsServiceRouterTest", "GenericUdsServiceTest", "NrcExceptionTest", "MetaInfoTest",
                      "ConversationTest", "CancellationHandlerTest"],
    CAP_UDS_READ_DID: [],
    CAP_UDS_SECURITY: ["SecurityAccessTest"],
    CAP_UDS_ECU_RESET: ["EcuResetRequestTest"],
    CAP_UDS_TRANSFER: ["RequestTransferTest", "RequestTransferExitTest", "TransferDataTest",
                       "DummyRequestTransfer"],
    CAP_DOIP: ["DoipControllerTest", "DoipLibTest", "DiagMessageTest", "DiagMessageAckTest",
               "DiagMessageNackTest", "VehicleIdRequestTest", "VehicleIdResponeTest",
               "EidVehicleIdRequestTest", "VinVehicleIdRequestTest", "RoutingActivationRequestTest",
               "RoutingActivationResponseTest", "GenericNackTest", "AnnouncementTimerTest",
               "PowerModeRequestTest", "PowerModeResponseTest", "PowerModeTest",
               "EntityStatusRequestTest", "EntityStatusResponseTest", "AliveCheckRequestTest",
               "AliveCheckResponseTest"],
    CAP_DTC: ["DtcInformationTest", "EventTest", "MonitorTest", "OperationCycleTest", "ConditionTest",
              "CounterBasedDebouncerTest", "TimerBasedDebouncerTest", "DiagErrorDomainTest"],
    CAP_E2E: ["Profile11Test"],
    CAP_LOGGING: ["LoggerTest", "LogStreamTest", "LogArgumentTest", "LoggingFrameworkTest"],
}

TIMEOUTS = {
    "build": 1800,
    "ctest": 1200,
    "non_interactive": 120,
    "boot": 40,
    "sd": 15,
    "rpc": 20,
    "doip": 20,
}

TAIL_CHARS = 1600


def utcnow():
    return datetime.now(timezone.utc)


def iso(moment):
    return moment.strftime("%Y-%m-%dT%H:%M:%SZ")


def tail(text, limit=TAIL_CHARS):
    text = text or ""
    return text if len(text) <= limit else "…" + text[-limit:]


def run_command(command, cwd, timeout, env=None):
    """Run a command with a hard timeout; never raises on failure."""
    started = time.monotonic()
    merged_env = dict(os.environ)
    if env:
        merged_env.update(env)
    try:
        completed = subprocess.run(
            command, cwd=cwd, timeout=timeout, env=merged_env,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        return {
            "exitCode": completed.returncode,
            "stdout": completed.stdout.decode("utf-8", "replace"),
            "stderr": completed.stderr.decode("utf-8", "replace"),
            "timedOut": False,
            "durationMs": int((time.monotonic() - started) * 1000),
        }
    except subprocess.TimeoutExpired as expired:
        return {
            "exitCode": None,
            "stdout": (expired.stdout or b"").decode("utf-8", "replace"),
            "stderr": (expired.stderr or b"").decode("utf-8", "replace"),
            "timedOut": True,
            "durationMs": int((time.monotonic() - started) * 1000),
        }


class Scenario:
    def __init__(self, identifier, name, category, capabilities, description):
        self.data = {
            "id": identifier,
            "name": name,
            "category": category,
            "capabilities": list(capabilities),
            "description": description,
            "command": "",
            "cwd": ".",
            "exitCode": None,
            "durationMs": 0,
            "status": "skipped",
            "assertions": [],
            "stdoutTail": "",
            "stderrTail": "",
            "logExcerpt": [],
            "notes": "",
        }
        self.skipped = False

    def skip(self, reason):
        self.skipped = True
        self.data["status"] = "skipped"
        self.data["notes"] = reason
        return self.data

    def assert_that(self, name, expected, actual, ok):
        self.data["assertions"].append(
            {"name": name, "expected": expected, "actual": actual, "ok": bool(ok)})
        return ok

    def finish(self):
        assertions = self.data["assertions"]
        if self.skipped:
            return self.data
        self.data["status"] = "pass" if assertions and all(a["ok"] for a in assertions) else "fail"
        return self.data


class Platform:
    """The simulator under test, started on a pseudo terminal.

    ``main.cpp`` treats the presence of both secret environment variables as
    "non-interactive" and then skips the two ``getchar()`` calls that keep the
    process alive, so an env-var start initialises every process and terminates
    within a second. To exercise a *running* platform the runner starts the
    binary on a PTY instead and feeds the same secret values into the prompts,
    which leaves the process running until it is torn down. Nothing is echoed:
    the binary disables terminal echo while reading the secrets, and the runner
    never stores or prints them.
    """

    def __init__(self, repo_root, api_key, bearer_token):
        self.repo_root = repo_root
        self.api_key = api_key
        self.bearer_token = bearer_token
        self.process = None
        self.master_fd = None
        self.buffer = ""
        self.command = "{} {}".format(BINARY, " ".join(MANIFESTS))

    def start(self):
        master_fd, slave_fd = pty.openpty()
        env = dict(os.environ)
        env.pop(API_KEY_ENV, None)
        env.pop(BEARER_TOKEN_ENV, None)
        self.process = subprocess.Popen(
            [os.path.join(self.repo_root, BINARY)] + MANIFESTS,
            cwd=self.repo_root, env=env,
            stdin=slave_fd, stdout=slave_fd, stderr=slave_fd,
            start_new_session=True)
        os.close(slave_fd)
        self.master_fd = master_fd
        self.pump(0.5)
        os.write(master_fd, (self.api_key + "\n").encode())
        self.pump(0.5)
        os.write(master_fd, (self.bearer_token + "\n").encode())

    def pump(self, seconds):
        """Drain the PTY for a while so the platform never blocks on output."""
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            readable, _, _ = select.select([self.master_fd], [], [], 0.2)
            if not readable:
                continue
            try:
                chunk = os.read(self.master_fd, 65536)
            except OSError as error:
                if error.errno in (errno.EIO, errno.EBADF):
                    break
                raise
            if not chunk:
                break
            self.buffer += chunk.decode("utf-8", "replace")
        return self.buffer

    def wait_for(self, needle, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if needle in self.buffer:
                return True
            self.pump(0.3)
        return needle in self.buffer

    def alive(self):
        return self.process is not None and self.process.poll() is None

    def log_lines(self):
        lines = []
        for raw in self.buffer.splitlines():
            line = raw.strip()
            if line and "Log Level:" in line:
                lines.append(re.sub(r"\s+", " ", line))
        return lines

    def excerpt(self, *needles):
        return [line for line in self.log_lines() if any(n in line for n in needles)]

    def stop(self):
        if self.process is None:
            return
        if self.process.poll() is None:
            try:
                os.killpg(os.getpgid(self.process.pid), signal.SIGTERM)
            except (ProcessLookupError, PermissionError):
                pass
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(os.getpgid(self.process.pid), signal.SIGKILL)
                except (ProcessLookupError, PermissionError):
                    pass
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    pass
        if self.master_fd is not None:
            try:
                os.close(self.master_fd)
            except OSError:
                pass
            self.master_fd = None


# --------------------------------------------------------------- SOME/IP RPC
def someip_request(service_id, method_id, payload, client_id=0x0DE0, session_id=0x0001,
                   message_type=0x00):
    message_id = (service_id << 16) | method_id
    header = struct.pack(
        ">IIHHBBBB", message_id, 8 + len(payload), client_id, session_id,
        SOMEIP_PROTOCOL_VERSION, SOMEIP_INTERFACE_VERSION, message_type, 0x00)
    return header + payload


def someip_parse(response):
    if len(response) < 16:
        return None
    message_id, length, client_id, session_id, protocol, interface, message_type, return_code = \
        struct.unpack(">IIHHBBBB", response[:16])
    rpc_payload = response[16:16 + max(0, length - 8)]
    return {
        "serviceId": message_id >> 16,
        "methodId": message_id & 0xFFFF,
        "clientId": client_id,
        "sessionId": session_id,
        "protocolVersion": protocol,
        "interfaceVersion": interface,
        "messageType": message_type,
        "returnCode": return_code,
        "rpcPayload": rpc_payload,
    }


def length_prefixed(text):
    encoded = text.encode()
    return struct.pack(">I", len(encoded)) + encoded


def rpc_call(payload, timeout=4.0):
    """One request/response exchange against the SOME/IP RPC server."""
    connection = socket.create_connection((RPC_HOST, RPC_PORT), timeout=timeout)
    try:
        connection.settimeout(timeout)
        connection.sendall(payload)
        response = connection.recv(1024)
    finally:
        connection.close()
    return someip_parse(response)


RETURN_CODES = {
    0x00: "eOK", 0x01: "eNotOk", 0x02: "eUnknownService", 0x03: "eUnknownMethod",
    0x04: "eNotReady", 0x05: "eNotReachable", 0x06: "eTimeout",
    0x07: "eWrongProtocolVersion", 0x08: "eWrongInterfaceVersion", 0x09: "eMalformedMessage",
    0x0A: "eWrongMessageType",
}


def describe_response(parsed):
    if parsed is None:
        return "no response"
    return "messageType=0x{:02X} returnCode=0x{:02X} ({})".format(
        parsed["messageType"], parsed["returnCode"],
        RETURN_CODES.get(parsed["returnCode"], "unknown"))


# ------------------------------------------------------------------ scenarios
def scenario_build(repo_root, skip_build):
    scenario = Scenario(
        "build", "CMake Debug build", "build", [],
        "Configures and compiles the platform and its unit tests from this checkout.")
    command = ("cmake -B build -DCMAKE_BUILD_TYPE=Debug && "
               "cmake --build build --config Debug -j{}".format(os.cpu_count() or 1))
    scenario.data["command"] = command
    if skip_build:
        return scenario.skip("Skipped with --skip-build; the existing build/ tree was reused.")

    configure = run_command(["cmake", "-B", BUILD_DIR, "-DCMAKE_BUILD_TYPE=Debug"],
                            repo_root, TIMEOUTS["build"])
    compile_result = {"exitCode": None, "stdout": "", "stderr": "", "durationMs": 0, "timedOut": False}
    if configure["exitCode"] == 0:
        compile_result = run_command(
            ["cmake", "--build", BUILD_DIR, "--config", "Debug", "-j{}".format(os.cpu_count() or 1)],
            repo_root, TIMEOUTS["build"])

    scenario.data["exitCode"] = compile_result["exitCode"]
    scenario.data["durationMs"] = configure["durationMs"] + compile_result["durationMs"]
    scenario.data["stdoutTail"] = tail(compile_result["stdout"])
    scenario.data["stderrTail"] = tail(configure["stderr"] + compile_result["stderr"])
    scenario.assert_that("cmake configure exits 0", "0", str(configure["exitCode"]),
                         configure["exitCode"] == 0)
    scenario.assert_that("cmake build exits 0", "0", str(compile_result["exitCode"]),
                         compile_result["exitCode"] == 0)
    scenario.assert_that("simulator binary exists", BINARY,
                         "present" if os.path.exists(os.path.join(repo_root, BINARY)) else "missing",
                         os.path.exists(os.path.join(repo_root, BINARY)))
    return scenario.finish()


CTEST_RESULT_RE = re.compile(r"^\s*\d+/\d+ Test\s+#\d+: (\S+) \.+\s+(\S+)")


def scenario_unit_tests(repo_root):
    scenario = Scenario(
        "unit_tests", "Unit suite (ctest)", "tests", sorted(CAPABILITY_TEST_SUITES),
        "Runs the repository's gtest suites through ctest and maps each suite to the "
        "platform capability it covers.")
    scenario.data["command"] = "cd build && ctest -C Debug --output-on-failure"
    scenario.data["cwd"] = "build"

    result = run_command(["ctest", "-C", "Debug", "--output-on-failure"],
                         os.path.join(repo_root, BUILD_DIR), TIMEOUTS["ctest"])
    scenario.data["exitCode"] = result["exitCode"]
    scenario.data["durationMs"] = result["durationMs"]
    scenario.data["stdoutTail"] = tail(result["stdout"])
    scenario.data["stderrTail"] = tail(result["stderr"])

    outcomes = {}
    for line in result["stdout"].splitlines():
        match = CTEST_RESULT_RE.match(line)
        if match:
            outcomes[match.group(1)] = match.group(2)
    passed = sum(1 for status in outcomes.values() if status == "Passed")
    failed = [name for name, status in outcomes.items() if status != "Passed"]

    per_capability = {}
    for capability, suites in CAPABILITY_TEST_SUITES.items():
        selected = [name for name in outcomes
                    if name.split(".")[0] in suites]
        per_capability[capability] = {
            "tests": len(selected),
            "passed": sum(1 for name in selected if outcomes[name] == "Passed"),
            "suites": sorted({name.split(".")[0] for name in selected}),
        }
    scenario.data["capabilityResults"] = per_capability
    scenario.data["capabilities"] = sorted(
        capability for capability, stats in per_capability.items()
        if stats["tests"] > 0 and stats["passed"] == stats["tests"])
    scenario.data["notes"] = "{} tests executed, {} passed.".format(len(outcomes), passed)

    scenario.assert_that("ctest exits 0", "0", str(result["exitCode"]), result["exitCode"] == 0)
    scenario.assert_that("no failing test", "0 failures",
                         "{} failures".format(len(failed)) + (": " + ", ".join(failed[:5]) if failed else ""),
                         not failed)
    scenario.assert_that("test suite is not empty", ">0 tests", "{} tests".format(len(outcomes)),
                         len(outcomes) > 0)
    return scenario.finish()


def scenario_non_interactive_startup(repo_root, secrets_present):
    scenario = Scenario(
        "non_interactive_startup", "Documented non-interactive startup", "platform",
        [CAP_EXEC, CAP_LOGGING],
        "Runs the documented env-var startup command and records the lifecycle it produces.")
    scenario.data["command"] = ("{}=*** {}=*** {} {}".format(
        API_KEY_ENV, BEARER_TOKEN_ENV, BINARY, " ".join(MANIFESTS)))
    if not secrets_present:
        return scenario.skip("Skipped: {} / {} are not available in this environment."
                             .format(API_KEY_ENV, BEARER_TOKEN_ENV))

    result = run_command([os.path.join(repo_root, BINARY)] + MANIFESTS, repo_root,
                         TIMEOUTS["non_interactive"])
    scenario.data["exitCode"] = result["exitCode"]
    scenario.data["durationMs"] = result["durationMs"]
    scenario.data["stdoutTail"] = tail(result["stdout"])
    scenario.data["stderrTail"] = tail(result["stderr"])
    scenario.data["logExcerpt"] = [
        re.sub(r"\s+", " ", line.strip()) for line in result["stdout"].splitlines()
        if "Log Level:" in line][:20]

    scenario.assert_that("start does not hang on a prompt", "exits without stdin input",
                         "timed out" if result["timedOut"] else "exited", not result["timedOut"])
    scenario.assert_that("exit code is 0", "0", str(result["exitCode"]), result["exitCode"] == 0)
    scenario.assert_that("secrets are read from the environment",
                         "'loaded from environment variable' twice",
                         "{} occurrences".format(result["stdout"].count("loaded from environment variable")),
                         result["stdout"].count("loaded from environment variable") == 2)
    scenario.assert_that("execution management initialises",
                         "Execution management has been initialized.",
                         "found" if "Execution management has been initialized." in result["stdout"] else "absent",
                         "Execution management has been initialized." in result["stdout"])
    scenario.data["notes"] = (
        "main.cpp skips its two getchar() calls when both secrets come from the environment, so "
        "this start path initialises every process and terminates immediately ({} ms here). The "
        "socket scenarios below therefore drive the binary on a PTY instead."
        .format(result["durationMs"]))
    return scenario.finish()


def scenario_platform_boot(platform):
    scenario = Scenario(
        "platform_boot", "Platform boot & process lifecycle", "platform",
        [CAP_EXEC, CAP_PHM, CAP_LOGGING],
        "Boots the simulator and waits for every modelled process to report initialisation.")
    scenario.data["command"] = platform.command + "  (secrets fed on a PTY)"
    started = time.monotonic()
    platform.start()
    expected = [
        ("execution management", "Execution management has been initialized."),
        ("state management", "State management has been initialized."),
        ("platform health management", "Plafrom health management has been initialized."),
        ("extended vehicle AA", "Extended Vehicle AA has been initialized."),
        ("diagnostic manager", "Diagnostic Manager has been initialized."),
    ]
    for _, needle in expected:
        platform.wait_for(needle, TIMEOUTS["boot"] / len(expected))
    scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
    scenario.data["exitCode"] = None if platform.alive() else platform.process.returncode
    scenario.data["stdoutTail"] = tail("\n".join(platform.log_lines()))
    scenario.data["logExcerpt"] = platform.excerpt(*[needle for _, needle in expected])

    for label, needle in expected:
        scenario.assert_that("{} initialises".format(label), needle,
                             "logged" if needle in platform.buffer else "not logged",
                             needle in platform.buffer)
    scenario.assert_that("platform stays running", "process alive after boot",
                         "alive" if platform.alive() else "exited", platform.alive())
    return scenario.finish()


def scenario_state_management(platform):
    scenario = Scenario(
        "exec_state_transitions", "Function group state transitions", "platform",
        [CAP_EXEC, CAP_STATE],
        "Verifies MachineFG reaches its StartUp state and that Execution Management accepts the "
        "reported execution state.")
    scenario.data["command"] = "(observed on the running platform started by platform_boot)"
    started = time.monotonic()
    expected = [
        ("MachineFG is configured", "Function group: MachineFG is configured."),
        ("Off state is configured", "State: Off of function group: MachineFG is configured."),
        ("StartUp state is configured", "State: StartUp of function group: MachineFG is configured."),
        ("initial machine state transition", "EM initial machine state transition is fetched successfully."),
        ("StartUp transition", "EM is transited to the start-up state successfully."),
        ("execution state report", "Execution state is reported successfully."),
    ]
    for _, needle in expected:
        platform.wait_for(needle, 5)
    scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
    scenario.data["logExcerpt"] = platform.excerpt(*[needle for _, needle in expected])
    for label, needle in expected:
        scenario.assert_that(label, needle, "logged" if needle in platform.buffer else "not logged",
                             needle in platform.buffer)
    return scenario.finish()


def scenario_someip_rpc(platform):
    scenario = Scenario(
        "someip_rpc", "SOME/IP RPC on TCP 8080", "someip", [CAP_SOMEIP_RPC, CAP_STATE, CAP_EXEC],
        "Sends real SOME/IP request messages to the Execution Management RPC server and asserts on "
        "the returned message type and return code.")
    scenario.data["command"] = ("SOME/IP requests to tcp://{}:{} (service 0x0003 SetState, "
                                "service 0x0001 ReportExecutionState)".format(RPC_HOST, RPC_PORT))
    started = time.monotonic()
    probes = []

    def probe(label, payload, expect_type, expect_code):
        try:
            parsed = rpc_call(payload, timeout=TIMEOUTS["rpc"] / 4)
        except OSError as error:
            probes.append((label, None, "socket error: {}".format(error)))
            scenario.assert_that(label, "messageType=0x{:02X} returnCode=0x{:02X}".format(
                expect_type, expect_code), "socket error: {}".format(error), False)
            return
        probes.append((label, parsed, describe_response(parsed)))
        ok = parsed is not None and parsed["messageType"] == expect_type \
            and parsed["returnCode"] == expect_code
        scenario.assert_that(
            label,
            "messageType=0x{:02X} returnCode=0x{:02X} ({})".format(
                expect_type, expect_code, RETURN_CODES.get(expect_code, "unknown")),
            describe_response(parsed), ok)

    # Unknown service identifier must be rejected with eUnknownService.
    probe("unknown service 0x00FF is rejected",
          someip_request(0x00FF, 0x0001, b""), 0x81, 0x02)
    # Undeclared function group state must be rejected by StateServer::handleSetState.
    probe("SetState MachineFG/DoesNotExist is rejected",
          someip_request(STATE_SERVICE_ID, STATE_SET_STATE_METHOD,
                         length_prefixed("MachineFG") + length_prefixed("DoesNotExist")),
          0x81, 0x01)
    # A second EM initialisation must be refused (StateServer::handleStateTransition).
    probe("repeated EM state transition is refused",
          someip_request(STATE_SERVICE_ID, STATE_TRANSITION_METHOD, b""), 0x81, 0x01)
    # Reporting a fresh instance specifier as kRunning (0x00) must be accepted.
    probe("ReportExecutionState(kRunning) is accepted",
          someip_request(EXEC_SERVICE_ID, EXEC_REPORT_STATE_METHOD,
                         length_prefixed("VerificationProbe") + bytes([0x00])), 0x80, 0x00)
    # A declared transition MachineFG StartUp -> Off must be accepted.
    probe("SetState MachineFG/Off is accepted",
          someip_request(STATE_SERVICE_ID, STATE_SET_STATE_METHOD,
                         length_prefixed("MachineFG") + length_prefixed("Off")), 0x80, 0x00)

    scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
    scenario.data["exitCode"] = 0 if all(a["ok"] for a in scenario.data["assertions"]) else 1
    scenario.data["stdoutTail"] = "\n".join(
        "{}: {}".format(label, description) for label, _, description in probes)
    scenario.data["logExcerpt"] = platform.excerpt("Execution management", "State management")
    return scenario.finish()


SD_ENTRY_TYPES = {0x00: "FindService", 0x01: "OfferService", 0x06: "SubscribeEventgroup",
                  0x07: "SubscribeEventgroupAck"}


def parse_sd_message(datagram):
    if len(datagram) < 24:
        return None
    message_id, length, client_id, session_id, protocol, interface, message_type, return_code = \
        struct.unpack(">IIHHBBBB", datagram[:16])
    if message_id != 0xFFFF8100:
        return None
    flags = struct.unpack(">I", datagram[16:20])[0]
    entries_length = struct.unpack(">I", datagram[20:24])[0]
    entries = []
    offset = 24
    end = min(len(datagram), 24 + entries_length)
    while offset + 16 <= end:
        entry = datagram[offset:offset + 16]
        entry_type = entry[0]
        service_id, instance_id = struct.unpack(">HH", entry[4:8])
        major_version = entry[8]
        ttl = int.from_bytes(entry[9:12], "big")
        entries.append({
            "type": entry_type,
            "typeName": SD_ENTRY_TYPES.get(entry_type, "0x{:02X}".format(entry_type)),
            "serviceId": service_id,
            "instanceId": instance_id,
            "majorVersion": major_version,
            "ttl": ttl,
        })
        offset += 16
    return {"sessionId": session_id, "flags": flags, "protocolVersion": protocol,
            "interfaceVersion": interface, "messageType": message_type,
            "returnCode": return_code, "entries": entries}


class SdCapture:
    """Background capture of the SD multicast group.

    The Diagnostic Manager's SD client sends its first FindService 100-200 ms
    after boot (INITIAL-DELAY-*-VALUE in diagnostic_manager_manifest.arxml), so
    the group has to be joined *before* the platform is started.
    """

    def __init__(self):
        self.messages = []
        self.error = None
        self.socket = None
        self.thread = None
        self.stop_event = threading.Event()
        self.command = ("join udp://{}:{} (IGMP on 127.0.0.1) and decode SOME/IP-SD messages"
                        .format(SD_GROUP, SD_PORT))

    def start(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind(("", SD_PORT))
            membership = struct.pack("4s4s", socket.inet_aton(SD_GROUP),
                                     socket.inet_aton("127.0.0.1"))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
            sock.settimeout(0.4)
        except OSError as error:
            self.error = "multicast join failed: {}".format(error)
            return
        self.socket = sock
        self.thread = threading.Thread(target=self._loop, daemon=True)
        self.thread.start()

    def _loop(self):
        while not self.stop_event.is_set():
            try:
                datagram, sender = self.socket.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError as error:
                self.error = str(error)
                return
            parsed = parse_sd_message(datagram)
            if parsed:
                parsed["from"] = "{}:{}".format(*sender)
                parsed["bytes"] = len(datagram)
                parsed["hex"] = datagram[:64].hex()
                self.messages.append(parsed)

    def stop(self):
        self.stop_event.set()
        if self.thread is not None:
            self.thread.join(timeout=3)
        if self.socket is not None:
            self.socket.close()
            self.socket = None

    def entries(self, type_name=None):
        found = [entry for message in self.messages for entry in message["entries"]]
        return [e for e in found if type_name is None or e["typeName"] == type_name]

    def transcript(self, limit=8):
        return "\n".join(
            "from {} session=0x{:04X} entries={}".format(
                message["from"], message["sessionId"],
                ", ".join("{}(service=0x{:04X}, instance=0x{:04X}, ttl={})".format(
                    entry["typeName"], entry["serviceId"], entry["instanceId"], entry["ttl"])
                    for entry in message["entries"]))
            for message in self.messages[:limit])


def scenario_someip_sd_find(platform, capture):
    scenario = Scenario(
        "someip_sd_find", "SOME/IP Service Discovery client FSM", "someip", [CAP_SOMEIP_SD],
        "Joins the SD multicast group declared in the manifests before boot and decodes the "
        "FindService entries the Diagnostic Manager's SD client actually puts on the wire.")
    scenario.data["command"] = capture.command
    started = time.monotonic()
    deadline = time.monotonic() + TIMEOUTS["sd"]
    while time.monotonic() < deadline and len(capture.entries("FindService")) < 2:
        platform.pump(0.3)

    captured = list(capture.messages)
    finds = capture.entries("FindService")
    scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
    scenario.data["exitCode"] = 0 if captured else 1
    scenario.data["stdoutTail"] = capture.transcript()
    scenario.data["capturedMessages"] = captured[:8]
    scenario.data["logExcerpt"] = platform.excerpt("Diagnostic Manager")
    if capture.error:
        scenario.data["notes"] = capture.error

    scenario.assert_that("SD messages observed on the multicast group", ">=1 SOME/IP-SD message",
                         "{} messages".format(len(captured)), len(captured) >= 1)
    scenario.assert_that("SD message ID 0xFFFF8100 with protocol/interface version 1/1",
                         "protocolVersion=1 interfaceVersion=1",
                         "protocolVersion={} interfaceVersion={}".format(
                             captured[0]["protocolVersion"] if captured else "-",
                             captured[0]["interfaceVersion"] if captured else "-"),
                         bool(captured) and captured[0]["protocolVersion"] == 1
                         and captured[0]["interfaceVersion"] == 1)
    scenario.assert_that("FindService for the manifested service 0x0005",
                         "FindService entry, serviceId=0x0005",
                         "{} FindService entries, service ids {}".format(
                             len(finds), sorted({"0x{:04X}".format(e["serviceId"]) for e in finds})),
                         any(entry["serviceId"] == 5 for entry in finds))
    scenario.assert_that("client FSM leaves the initial wait phase (repetition finds)",
                         ">=2 FindService entries", "{} FindService entries".format(len(finds)),
                         len(finds) >= 2)
    scenario.assert_that("session id increments across repetitions",
                         "strictly increasing session ids",
                         "session ids {}".format([m["sessionId"] for m in captured[:6]]),
                         len(captured) >= 2 and all(
                             captured[i + 1]["sessionId"] > captured[i]["sessionId"]
                             for i in range(min(len(captured), 6) - 1)))
    return scenario.finish()


def scenario_someip_sd_offer(platform, capture):
    scenario = Scenario(
        "someip_sd_offer", "SOME/IP service offering & subscription", "someip",
        [CAP_SOMEIP_PUBSUB],
        "Checks whether the Extended Vehicle AA answers the client's finds with an OfferService "
        "entry for service 0x0005 and whether an eventgroup subscription follows.")
    scenario.data["command"] = capture.command
    started = time.monotonic()
    offers = capture.entries("OfferService")
    subscribes = capture.entries("SubscribeEventgroup")
    scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
    scenario.data["exitCode"] = 0 if offers else 1
    scenario.data["stdoutTail"] = capture.transcript()
    scenario.data["logExcerpt"] = platform.excerpt("Extended Vehicle", "VIN")
    scenario.assert_that("OfferService for service 0x0005", "OfferService entry, serviceId=0x0005",
                         "{} OfferService entries".format(len(offers)),
                         any(entry["serviceId"] == 5 for entry in offers))
    scenario.assert_that("eventgroup subscription follows the offer",
                         ">=1 SubscribeEventgroup entry",
                         "{} SubscribeEventgroup entries".format(len(subscribes)),
                         len(subscribes) >= 1)
    if not offers:
        scenario.data["notes"] = (
            "ExtendedVehicle::Main only calls mSdServer->Start() after the VCC extended-vehicle "
            "REST call resolves a VIN; that call fails in this environment, so nothing is offered "
            "and no subscription can follow.")
    return scenario.finish()


def doip_message(protocol_version, payload_type, payload):
    return struct.pack(">BBHI", protocol_version, protocol_version ^ 0xFF, payload_type,
                       len(payload)) + payload


def scenario_uds_doip(platform):
    scenario = Scenario(
        "uds_doip", "UDS over DoIP on TCP 8081", "diagnostics",
        [CAP_DOIP, CAP_UDS_ROUTING, CAP_UDS_READ_DID],
        "Opens a DoIP (ISO 13400) connection to the Extended Vehicle AA and issues a vehicle "
        "identification request plus UDS ReadDataByIdentifier (0x22) requests for the DIDs "
        "implemented in src/application/helper/read_data_by_identifier.h.")
    scenario.data["command"] = (
        "DoIP VehicleIdRequest (0x0001) + UDS 0x22 on DIDs {} to tcp://{}:{}".format(
            ", ".join("0x{:04X}".format(did) for did, _ in UDS_DIDS), DOIP_HOST, DOIP_PORT))
    started = time.monotonic()
    transcript = []
    connection = None
    try:
        connection = socket.create_connection((DOIP_HOST, DOIP_PORT), timeout=4)
    except OSError as error:
        scenario.assert_that(
            "DoIP server accepts a TCP connection", "connected to {}:{}".format(DOIP_HOST, DOIP_PORT),
            "{}".format(error), False)
        scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
        scenario.data["exitCode"] = 1
        scenario.data["logExcerpt"] = platform.excerpt("Extended Vehicle", "VIN")
        scenario.data["stdoutTail"] = (
            "connect(tcp://{}:{}) -> {}".format(DOIP_HOST, DOIP_PORT, error))
        scenario.data["notes"] = (
            "ExtendedVehicle::Main only constructs the DoIP server after the VCC extended-vehicle "
            "REST call resolves a VIN, so no socket is listening when that call fails.")
        return scenario.finish()

    try:
        connection.settimeout(4)
        scenario.assert_that("DoIP server accepts a TCP connection",
                             "connected to {}:{}".format(DOIP_HOST, DOIP_PORT), "connected", True)

        request = doip_message(0x02, 0x0001, b"")
        connection.sendall(request)
        try:
            response = connection.recv(4096)
        except socket.timeout:
            response = b""
        transcript.append("VehicleIdRequest -> {}".format(response[:64].hex()))
        payload_type = struct.unpack(">H", response[2:4])[0] if len(response) >= 4 else None
        scenario.assert_that("vehicle identification response (0x0004)", "payloadType=0x0004",
                             "payloadType={}".format(
                                 "0x{:04X}".format(payload_type) if payload_type is not None else "none"),
                             payload_type == 0x0004)

        for did, label in UDS_DIDS:
            uds = bytes([UDS_READ_DATA_BY_IDENTIFIER, did >> 8, did & 0xFF])
            diag = struct.pack(">HH", 0x0E00, 0x0001) + uds
            connection.sendall(doip_message(0x02, 0x8001, diag))
            try:
                response = connection.recv(4096)
            except socket.timeout:
                response = b""
            transcript.append("0x22 {:04X} ({}) -> {}".format(did, label, response[:64].hex()))
            body = response[8:] if len(response) > 8 else b""
            uds_response = body[4:] if len(body) > 4 else b""
            positive = len(uds_response) >= 3 and uds_response[0] == 0x62
            negative = len(uds_response) >= 3 and uds_response[0] == 0x7F
            scenario.assert_that(
                "ReadDataByIdentifier 0x{:04X} ({})".format(did, label),
                "positive response SID 0x62",
                ("0x62 with DID 0x{:02X}{:02X}".format(uds_response[1], uds_response[2]) if positive
                 else "NRC 0x{:02X}".format(uds_response[2]) if negative else "no UDS response"),
                positive)
    except OSError as error:
        scenario.assert_that("DoIP exchange completes", "no socket error", str(error), False)
    finally:
        connection.close()

    scenario.data["durationMs"] = int((time.monotonic() - started) * 1000)
    scenario.data["exitCode"] = 0 if all(a["ok"] for a in scenario.data["assertions"]) else 1
    scenario.data["stdoutTail"] = "\n".join(transcript)
    scenario.data["logExcerpt"] = platform.excerpt("Extended Vehicle", "VIN")
    return scenario.finish()


# ---------------------------------------------------------------- run harness
def git_info(repo_root):
    def git(*args):
        result = run_command(["git"] + list(args), repo_root, 20)
        return result["stdout"].strip() if result["exitCode"] == 0 else ""

    return {
        "sha": git("rev-parse", "HEAD"),
        "shortSha": git("rev-parse", "--short", "HEAD"),
        "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
        "dirty": bool(git("status", "--porcelain")),
    }


def host_info():
    uname = os.uname()
    return {
        "hostname": uname.nodename,
        "os": "{} {}".format(uname.sysname, uname.release),
        "machine": uname.machine,
        "cpus": os.cpu_count(),
        "python": sys.version.split()[0],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=REPO_ROOT)
    parser.add_argument("--output-dir", default=None,
                        help="defaults to <repo-root>/demo/data/runs")
    parser.add_argument("--skip-build", action="store_true",
                        help="reuse the existing build/ tree instead of rebuilding")
    parser.add_argument("--skip-tests", action="store_true",
                        help="skip the ctest scenario (useful while iterating)")
    args = parser.parse_args()

    repo_root = os.path.abspath(args.repo_root)
    output_dir = args.output_dir or os.path.join(repo_root, "demo", "data", "runs")
    os.makedirs(output_dir, exist_ok=True)

    api_key = os.environ.get(API_KEY_ENV, "")
    bearer_token = os.environ.get(BEARER_TOKEN_ENV, "")
    secrets_present = bool(api_key) and bool(bearer_token)

    started_at = utcnow()
    started = time.monotonic()
    scenarios = []

    scenarios.append(scenario_build(repo_root, args.skip_build))
    if args.skip_tests:
        skipped = Scenario("unit_tests", "Unit suite (ctest)", "tests", [],
                           "Skipped with --skip-tests.")
        skipped.data["command"] = "cd build && ctest -C Debug --output-on-failure"
        scenarios.append(skipped.skip("Skipped with --skip-tests."))
    else:
        scenarios.append(scenario_unit_tests(repo_root))
    scenarios.append(scenario_non_interactive_startup(repo_root, secrets_present))

    platform = Platform(repo_root, api_key, bearer_token)
    socket_scenarios = ["platform_boot", "exec_state_transitions", "someip_sd_find",
                        "someip_sd_offer", "someip_rpc", "uds_doip"]
    if not secrets_present:
        for identifier in socket_scenarios:
            placeholder = Scenario(identifier, identifier, "platform", [],
                                   "Not executed: the platform cannot be started without secrets.")
            scenarios.append(placeholder.skip(
                "Degraded run: {} / {} were not available, so the simulator was never started."
                .format(API_KEY_ENV, BEARER_TOKEN_ENV)))
    else:
        capture = SdCapture()
        capture.start()
        try:
            scenarios.append(scenario_platform_boot(platform))
            scenarios.append(scenario_state_management(platform))
            scenarios.append(scenario_someip_sd_find(platform, capture))
            scenarios.append(scenario_someip_sd_offer(platform, capture))
            scenarios.append(scenario_someip_rpc(platform))
            scenarios.append(scenario_uds_doip(platform))
        finally:
            capture.stop()
            platform.stop()

    finished_at = utcnow()
    run_id = started_at.strftime("%Y-%m-%dT%H-%M-%SZ")
    artifact = {
        "schemaVersion": 1,
        "runId": run_id,
        "startedAt": iso(started_at),
        "finishedAt": iso(finished_at),
        "durationMs": int((time.monotonic() - started) * 1000),
        "host": host_info(),
        "commit": git_info(repo_root),
        "environment": {
            "secretsPresent": secrets_present,
            "degraded": not secrets_present,
            "startMode": "pty-interactive" if secrets_present else "not-started",
            "startModeReason": (
                "main.cpp skips its blocking getchar() calls when both secrets are set in the "
                "environment, so an env-var start terminates right after initialisation. The "
                "socket scenarios feed the same secret values into the binary's prompts on a "
                "pseudo terminal to keep a real platform running."),
            "restDependency": (
                "The DoIP server and the SD offer path are only constructed after "
                "https://api.volvocars.com/extended-vehicle/v1/vehicles resolves a VIN."),
        },
        "scenarios": scenarios,
        "summary": {
            "total": len(scenarios),
            "passed": sum(1 for s in scenarios if s["status"] == "pass"),
            "failed": sum(1 for s in scenarios if s["status"] == "fail"),
            "skipped": sum(1 for s in scenarios if s["status"] == "skipped"),
        },
    }

    artifact_path = os.path.join(output_dir, run_id + ".json")
    with open(artifact_path, "w") as handle:
        json.dump(artifact, handle, indent=2)
        handle.write("\n")

    index_path = os.path.join(output_dir, "index.json")
    entries = []
    if os.path.exists(index_path):
        try:
            with open(index_path) as handle:
                entries = json.load(handle).get("runs", [])
        except (ValueError, OSError):
            entries = []
    entries = [entry for entry in entries if entry.get("runId") != run_id]
    entries.append({
        "runId": run_id,
        "file": run_id + ".json",
        "startedAt": iso(started_at),
        "commit": artifact["commit"]["shortSha"],
        "branch": artifact["commit"]["branch"],
        "host": artifact["host"]["hostname"],
        "degraded": artifact["environment"]["degraded"],
        "summary": artifact["summary"],
    })
    entries.sort(key=lambda entry: entry["runId"], reverse=True)
    with open(index_path, "w") as handle:
        json.dump({"generatedAt": iso(utcnow()), "runs": entries}, handle, indent=2)
        handle.write("\n")

    print("run {}: {} passed, {} failed, {} skipped".format(
        run_id, artifact["summary"]["passed"], artifact["summary"]["failed"],
        artifact["summary"]["skipped"]))
    for scenario in scenarios:
        print("  [{}] {}".format(scenario["status"], scenario["name"]))
    print("artifact: {}".format(os.path.relpath(artifact_path, repo_root)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
