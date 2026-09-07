#!/usr/bin/env python3
"""Generate demo/data/model.json from the Adaptive-AUTOSAR checkout.

Walks configuration/*.arxml, src/ and test/ and emits a single JSON model that
drives the static demo under demo/. Standard library only; idempotent.

Usage:  python3 demo/generate_data.py
"""

import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG_DIR = os.path.join(REPO_ROOT, "configuration")
SRC_DIR = os.path.join(REPO_ROOT, "src")
TEST_DIR = os.path.join(REPO_ROOT, "test")
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")

SOURCE_EXTS = (".cpp", ".h", ".hpp")
VENDORED = ("src/arxml/pugixml.cpp", "src/arxml/pugixml.hpp", "src/arxml/pugiconfig.hpp")

CLASS_RE = re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_]\w*)\b(?!\s*;)")
NAMESPACE_RE = re.compile(r"^\s*namespace\s+([A-Za-z_]\w*)")
DID_RE = re.compile(r"static const uint16_t c(\w+?)Did\{0x([0-9a-fA-F]+)\}")
SID_RE = re.compile(r"static const uint8_t cSid\{0x([0-9a-fA-F]+)\}")


def rel(path):
    return os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")


def read_lines(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read().splitlines()


def strip_ns(tag):
    return tag.split("}", 1)[1] if "}" in tag else tag


# --------------------------------------------------------------------------
# ARXML parsing
# --------------------------------------------------------------------------

INTERESTING = {
    "FUNCTION-GROUP": "functionGroup",
    "MODE-DECLARATION": "machineState",
    "PROCESS": "process",
    "EXECUTABLE": "executable",
    "DIAGNOSTIC-EVENT-INTERFACE": "diagnosticEvent",
    "DIAGNOSTIC-MONITOR-INTERFACE": "diagnosticMonitor",
    "PROVIDED-SOMEIP-SERVICE-INSTANCE": "providedService",
    "REQUIRED-SOMEIP-SERVICE-INSTANCE": "requiredService",
    "DO-IP-INSTANTIATION": "doipInstance",
    "GLOBAL-SUPERVISION": "supervision",
    "SUPERVISION-CHECKPOINT": "checkpoint",
    "NETWORK-ENDPOINT": "endpoint",
    "AP-APPLICATION-ENDPOINT": "applicationEndpoint",
}


def line_index(lines):
    """Map (tag, short-name) and raw tags to 1-based line numbers, in order."""
    index = []
    for number, text in enumerate(lines, start=1):
        for match in re.finditer(r"<([A-Z0-9\-]+)>", text):
            index.append((match.group(1), number))
    return index


def find_line(index, tag, occurrence):
    seen = 0
    for name, number in index:
        if name == tag:
            seen += 1
            if seen == occurrence:
                return number
    return 1


def child_text(node, tag):
    for child in node:
        if strip_ns(child.tag) == tag:
            return (child.text or "").strip()
    return None


def deep_text(node, tag):
    for child in node.iter():
        if strip_ns(child.tag) == tag:
            return (child.text or "").strip()
    return None


def parse_arxml(path):
    lines = read_lines(path)
    index = line_index(lines)
    tree = ET.parse(path)
    root = tree.getroot()

    package = None
    for node in root.iter():
        if strip_ns(node.tag) == "AR-PACKAGE":
            package = child_text(node, "SHORT-NAME")
            break

    counts = {}
    elements = []
    for node in root.iter():
        tag = strip_ns(node.tag)
        kind = INTERESTING.get(tag)
        if kind is None:
            continue
        counts[tag] = counts.get(tag, 0) + 1
        line = find_line(index, tag, counts[tag])
        short_name = child_text(node, "SHORT-NAME")
        detail = OrderedDict()

        if tag in ("PROVIDED-SOMEIP-SERVICE-INSTANCE", "REQUIRED-SOMEIP-SERVICE-INSTANCE"):
            service_id = deep_text(node, "SERVICE-INTERFACE-ID")
            if service_id:
                detail["serviceInterfaceId"] = int(service_id)
            major = deep_text(node, "MAJOR-VERSION")
            minor = deep_text(node, "MINOR-VERSION")
            if major is not None and minor is not None:
                detail["version"] = "%s.%s" % (major, minor)
            instance = deep_text(node, "SERVICE-INSTANCE-ID")
            if instance is not None:
                detail["instanceId"] = int(instance)
            if short_name is None:
                short_name = "SomeIpServiceInstance_0x%02X" % int(service_id or 0)
        elif tag == "DIAGNOSTIC-EVENT-INTERFACE":
            dtc = deep_text(node, "DTC-NUMBER")
            if dtc:
                detail["dtcNumber"] = int(dtc)
                detail["dtcHex"] = "0x%04X" % int(dtc)
        elif tag == "DIAGNOSTIC-MONITOR-INTERFACE":
            for field in ("TIME-PASSED-THRESHOLD", "TIME-FAILED-THRESHOLD"):
                value = deep_text(node, field)
                if value:
                    detail[field.lower().replace("-", "_")] = int(value)
        elif tag == "NETWORK-ENDPOINT":
            address = deep_text(node, "IPV-4-ADDRESS")
            if address:
                detail["ipv4"] = address
        elif tag == "AP-APPLICATION-ENDPOINT":
            port = deep_text(node, "PORT-NUMBER")
            if port:
                detail["port"] = int(port)
                detail["transport"] = "TCP" if node.find(".//{*}TCP-TP") is not None else "UDP"
        elif tag == "DO-IP-INSTANTIATION":
            for field in ("EID", "GID", "LOGICAL-ADDRESS", "VEHICLE-ANNOUNCEMENT-COUNT"):
                value = deep_text(node, field)
                if value:
                    detail[field.lower().replace("-", "_")] = int(value)
            short_name = short_name or "DoIpInstantiation"
        elif tag == "GLOBAL-SUPERVISION":
            for field in ("ALIVE-REFERENCE-CYCLE", "EXPECTED-ALIVE-INDICATIONS",
                          "MIN-DEADLINE", "MAX-DEADLINE"):
                value = deep_text(node, field)
                if value:
                    detail[field.lower().replace("-", "_")] = int(value)
            short_name = short_name or "GlobalSupervision"
        elif tag == "SUPERVISION-CHECKPOINT":
            checkpoint = child_text(node, "CHECKPOINT-ID")
            if checkpoint is not None:
                detail["checkpointId"] = int(checkpoint)

        elements.append(OrderedDict([
            ("id", "%s::%s" % (os.path.basename(path), short_name or tag)),
            ("kind", kind),
            ("tag", tag),
            ("shortName", short_name or tag),
            ("file", rel(path)),
            ("line", line),
            ("detail", detail),
            ("excerpt", "\n".join(lines[max(0, line - 1):line + 11])),
        ]))

    return OrderedDict([
        ("file", rel(path)),
        ("package", package),
        ("lines", len(lines)),
        ("elements", elements),
    ])


# --------------------------------------------------------------------------
# Source / test walking
# --------------------------------------------------------------------------

def namespaces_of(lines):
    found = []
    for text in lines[:80]:
        match = NAMESPACE_RE.match(text)
        if match:
            found.append(match.group(1))
    return found


def collect_sources():
    sources = OrderedDict()
    for base, _dirs, files in os.walk(SRC_DIR):
        for name in sorted(files):
            if not name.endswith(SOURCE_EXTS):
                continue
            path = os.path.join(base, name)
            relative = rel(path)
            if relative in VENDORED:
                continue
            lines = read_lines(path)
            classes = []
            for number, text in enumerate(lines, start=1):
                match = CLASS_RE.match(text)
                if match and match.group(1) not in [c["name"] for c in classes]:
                    classes.append({"name": match.group(1), "line": number})
            spaces = namespaces_of(lines)
            parts = relative.split("/")[1:-1]
            module = "/".join(parts[:2]) if parts else "src (root)"
            sources[relative] = OrderedDict([
                ("path", relative),
                ("module", module),
                ("namespace", "::".join(spaces) if spaces else ""),
                ("loc", len(lines)),
                ("classes", classes),
                ("test", None),
            ])
    return sources


def collect_tests(sources):
    tests = []
    for base, _dirs, files in os.walk(TEST_DIR):
        for name in sorted(files):
            if not name.endswith(SOURCE_EXTS):
                continue
            path = os.path.join(base, name)
            relative = rel(path)
            lines = read_lines(path)
            cases = re.findall(r"TEST(?:_F)?\(\s*(\w+)\s*,\s*(\w+)\s*\)", "\n".join(lines))
            stem = os.path.splitext(os.path.basename(relative))[0]
            if stem.endswith("_test"):
                stem = stem[: -len("_test")]
            candidates = [
                "src/" + relative[len("test/"):].replace(os.path.basename(relative), stem + ext)
                for ext in (".cpp", ".h")
            ]
            covered = []
            for candidate in candidates:
                if candidate in sources:
                    covered.append(candidate)
            if not covered:
                for key in sources:
                    if os.path.splitext(os.path.basename(key))[0] == stem:
                        covered.append(key)
            for key in covered:
                sources[key]["test"] = relative
            tests.append(OrderedDict([
                ("path", relative),
                ("loc", len(lines)),
                ("cases", len(cases)),
                ("suites", sorted(set(suite for suite, _case in cases))),
                ("covers", covered),
            ]))
    return tests


def module_stats(sources):
    modules = OrderedDict()
    for source in sources.values():
        entry = modules.setdefault(source["module"], {
            "module": source["module"],
            "files": 0,
            "tested": 0,
            "loc": 0,
            "classes": 0,
        })
        entry["files"] += 1
        entry["loc"] += source["loc"]
        entry["classes"] += len(source["classes"])
        if source["test"]:
            entry["tested"] += 1
    for entry in modules.values():
        entry["coverage"] = round(entry["tested"] / entry["files"], 4) if entry["files"] else 0.0
    return sorted(modules.values(), key=lambda item: item["module"])


# --------------------------------------------------------------------------
# UDS services / DIDs harvested from real source
# --------------------------------------------------------------------------

UDS_NAMES = {
    "0x11": "ECUReset",
    "0x22": "ReadDataByIdentifier",
    "0x27": "SecurityAccess",
    "0x31": "RoutineControl",
    "0x34": "RequestDownload",
    "0x35": "RequestUpload",
    "0x36": "TransferData",
    "0x37": "RequestTransferExit",
}


def collect_uds(sources):
    services = OrderedDict()
    dids = []
    for relative, source in sources.items():
        lines = read_lines(os.path.join(REPO_ROOT, relative))
        for number, text in enumerate(lines, start=1):
            sid = SID_RE.search(text)
            if sid:
                key = "0x%s" % sid.group(1).lower()
                entry = services.setdefault(key, OrderedDict([
                    ("sid", key),
                    ("name", UDS_NAMES.get(key, "UdsService")),
                    ("files", []),
                    ("line", number),
                ]))
                entry["files"].append(relative)
            did = DID_RE.search(text)
            if did:
                label = re.sub(r"(?<!^)(?=[A-Z])", " ", did.group(1))
                dids.append(OrderedDict([
                    ("did", "0x%s" % did.group(2).upper()),
                    ("name", label),
                    ("constant", "c%sDid" % did.group(1)),
                    ("file", relative),
                    ("line", number),
                ]))
    for entry in services.values():
        entry["files"] = sorted(set(entry["files"]))
        entry["tested"] = any(sources[f]["test"] for f in entry["files"] if f in sources)
        entry["tests"] = sorted(set(
            sources[f]["test"] for f in entry["files"] if f in sources and sources[f]["test"]
        ))
    return (
        sorted(services.values(), key=lambda item: item["sid"]),
        sorted(dids, key=lambda item: item["did"]),
    )


# --------------------------------------------------------------------------
# Requirements, traceability, variant matrix
# --------------------------------------------------------------------------

def load_requirements():
    packages = {}
    for version in ("v1", "v2"):
        path = os.path.join(DATA_DIR, "oem_reqs_%s.json" % version)
        with open(path, "r", encoding="utf-8") as handle:
            packages[version] = json.load(handle)
    return packages


def resolve_anchor(anchor, arxml, sources, uds_services, dids):
    """Resolve a requirement anchor to real repo artifacts."""
    kind = anchor.get("type")
    value = anchor.get("value")
    if kind == "arxml":
        for manifest in arxml:
            for element in manifest["elements"]:
                if element["shortName"] == value:
                    return element
    elif kind == "uds":
        for service in uds_services:
            if service["sid"] == value:
                return service
    elif kind == "did":
        for entry in dids:
            if entry["did"].lower() == value.lower():
                return entry
    elif kind == "source":
        return sources.get(value)
    return None


def build_diff(packages, arxml, sources, uds_services, dids):
    old = {item["id"]: item for item in packages["v1"]["requirements"]}
    new = {item["id"]: item for item in packages["v2"]["requirements"]}
    changes = []

    def impacts(requirement):
        entries = []
        for anchor in requirement.get("anchors", []):
            resolved = resolve_anchor(anchor, arxml, sources, uds_services, dids)
            if resolved is None:
                entries.append(OrderedDict([
                    ("type", anchor["type"]),
                    ("label", anchor["value"]),
                    ("file", None),
                    ("resolved", False),
                ]))
                continue
            entries.append(OrderedDict([
                ("type", anchor["type"]),
                ("label", anchor["value"]),
                ("file", resolved.get("file") or resolved.get("path") or
                 (resolved.get("files") or [None])[0]),
                ("line", resolved.get("line")),
                ("resolved", True),
            ]))
        for path in requirement.get("sources", []):
            source = sources.get(path)
            entries.append(OrderedDict([
                ("type", "source"),
                ("label", path.split("/")[-1]),
                ("file", path),
                ("line", 1),
                ("resolved", source is not None),
                ("test", source["test"] if source else None),
            ]))
        return entries

    for req_id in sorted(set(list(old) + list(new))):
        before = old.get(req_id)
        after = new.get(req_id)
        if before and not after:
            status = "removed"
            payload = before
        elif after and not before:
            status = "added"
            payload = after
        elif before["text"] != after["text"] or before.get("anchors") != after.get("anchors") \
                or before.get("asil") != after.get("asil"):
            status = "changed"
            payload = after
        else:
            status = "unchanged"
            payload = after
        changes.append(OrderedDict([
            ("id", req_id),
            ("status", status),
            ("title", payload["title"]),
            ("before", before["text"] if before else None),
            ("after", after["text"] if after else None),
            ("asilBefore", before.get("asil") if before else None),
            ("asilAfter", after.get("asil") if after else None),
            ("capability", payload.get("capability")),
            ("impacts", impacts(payload)),
        ]))
    return changes


def build_traceability(packages, arxml, sources, uds_services, dids):
    chains = []
    for requirement in packages["v2"]["requirements"]:
        arxml_nodes = []
        service_nodes = []
        for anchor in requirement.get("anchors", []):
            resolved = resolve_anchor(anchor, arxml, sources, uds_services, dids)
            if resolved is None:
                continue
            if anchor["type"] == "arxml":
                arxml_nodes.append(OrderedDict([
                    ("label", resolved["shortName"]),
                    ("sub", resolved["tag"]),
                    ("file", resolved["file"]),
                    ("line", resolved["line"]),
                    ("excerpt", resolved["excerpt"]),
                ]))
            elif anchor["type"] == "uds":
                service_nodes.append(OrderedDict([
                    ("label", "UDS %s %s" % (resolved["sid"], resolved["name"])),
                    ("sub", "ara::diag::routing"),
                    ("file", resolved["files"][0] if resolved["files"] else None),
                    ("line", resolved.get("line", 1)),
                ]))
            elif anchor["type"] == "did":
                service_nodes.append(OrderedDict([
                    ("label", "DID %s" % resolved["did"]),
                    ("sub", resolved["name"].strip()),
                    ("file", resolved["file"]),
                    ("line", resolved["line"]),
                ]))

        source_nodes = []
        test_nodes = []
        for path in requirement.get("sources", []):
            source = sources.get(path)
            if source is None:
                continue
            excerpt_lines = read_lines(os.path.join(REPO_ROOT, path))
            head = source["classes"][0]["line"] if source["classes"] else 1
            source_nodes.append(OrderedDict([
                ("label", path.split("/")[-1]),
                ("sub", source["namespace"] or source["module"]),
                ("file", path),
                ("line", head),
                ("excerpt", "\n".join(excerpt_lines[max(0, head - 2):head + 10])),
            ]))
            if source["test"]:
                test_lines = read_lines(os.path.join(REPO_ROOT, source["test"]))
                first = next((n for n, t in enumerate(test_lines, start=1)
                              if t.startswith("TEST")), 1)
                test_nodes.append(OrderedDict([
                    ("label", source["test"].split("/")[-1]),
                    ("sub", "gtest"),
                    ("file", source["test"]),
                    ("line", first),
                    ("excerpt", "\n".join(test_lines[max(0, first - 1):first + 10])),
                ]))

        gaps = []
        if not arxml_nodes:
            gaps.append("no manifest element")
        if not source_nodes:
            gaps.append("no implementation owner")
        if not test_nodes:
            gaps.append("no unit test")
        chains.append(OrderedDict([
            ("id", requirement["id"]),
            ("title", requirement["title"]),
            ("asil", requirement.get("asil")),
            ("capability", requirement.get("capability")),
            ("stages", OrderedDict([
                ("requirement", [OrderedDict([
                    ("label", requirement["id"]),
                    ("sub", requirement["title"]),
                    ("file", "demo/data/oem_reqs_v2.json"),
                    ("line", 1),
                    ("excerpt", requirement["text"]),
                ])]),
                ("manifest", arxml_nodes),
                ("service", service_nodes),
                ("source", source_nodes),
                ("test", test_nodes),
            ])),
            ("gaps", gaps),
        ]))
    return chains


def build_matrix(packages, capabilities, sources, chains):
    oems = packages["v2"]["oems"]
    rows = []
    chain_by_capability = {}
    for chain in chains:
        chain_by_capability.setdefault(chain["capability"], []).append(chain)

    for capability in capabilities:
        cells = OrderedDict()
        related = chain_by_capability.get(capability["id"], [])
        implemented = [
            path for path in capability["sources"] if path in sources
        ]
        tested = [path for path in implemented if sources[path]["test"]]
        ratio = len(tested) / len(implemented) if implemented else 0.0
        blocking = [gap for chain in related for gap in chain["gaps"]
                    if gap in ("no unit test", "no implementation owner")]
        for oem in oems:
            required = capability["id"] in packages["v2"]["variants"][oem["id"]]
            if not required:
                state = "absent"
                note = "not in %s package" % oem["short"]
            elif not implemented:
                state = "absent"
                note = "no implementation found"
            elif blocking or ratio < 0.6:
                state = "partial"
                note = "%d/%d files under test%s" % (
                    len(tested), len(implemented),
                    "; " + blocking[0] if blocking else "")
            else:
                state = "supported"
                note = "%d/%d files under test" % (len(tested), len(implemented))
            cells[oem["id"]] = OrderedDict([("state", state), ("note", note)])
        rows.append(OrderedDict([
            ("id", capability["id"]),
            ("name", capability["name"]),
            ("evidence", capability["evidence"]),
            ("files", implemented),
            ("tested", len(tested)),
            ("cells", cells),
        ]))
    return OrderedDict([("oems", oems), ("rows", rows)])


def capabilities_from_repo(arxml, uds_services, sources):
    """Capabilities derived from real function groups, services and UDS handlers."""
    definitions = [
        ("someip_sd", "SOME/IP Service Discovery", "src/ara/com/someip/sd"),
        ("someip_pubsub", "SOME/IP Publish/Subscribe", "src/ara/com/someip/pubsub"),
        ("someip_rpc", "SOME/IP RPC over TCP", "src/ara/com/someip/rpc"),
        ("exec_management", "Execution Management & function groups", "src/ara/exec"),
        ("state_management", "Machine state management", "src/ara/sm"),
        ("phm_supervision", "Platform Health supervision", "src/ara/phm"),
        ("uds_routing", "UDS service routing", "src/ara/diag/routing"),
        ("uds_read_did", "UDS ReadDataByIdentifier (0x22)", "src/application/helper/read_data_by_identifier"),
        ("uds_security", "UDS SecurityAccess (0x27)", "src/ara/diag/security_access"),
        ("uds_ecu_reset", "UDS ECUReset (0x11)", "src/ara/diag/ecu_reset_request"),
        ("uds_transfer", "UDS Download/Upload/Transfer (0x34-0x37)", "src/ara/diag"),
        ("doip", "DoIP vehicle announcement & routing", "src/application/doip"),
        ("dtc_reporting", "DTC / event memory reporting", "src/ara/diag/dtc_information"),
        ("e2e_protection", "E2E Profile 11 protection", "src/ara/com/e2e"),
        ("logging", "ara::log logging framework", "src/ara/log"),
    ]
    capabilities = []
    for cap_id, name, prefix in definitions:
        files = sorted(path for path in sources if path.startswith(prefix))
        if cap_id == "uds_transfer":
            files = sorted(path for path in sources
                           if re.search(r"(download|upload|transfer_data|request_transfer)", path))
        evidence = []
        for service in uds_services:
            if service["files"] and any(f in files for f in service["files"]):
                evidence.append("UDS %s" % service["sid"])
        for manifest in arxml:
            for element in manifest["elements"]:
                if element["kind"] in ("functionGroup", "providedService", "requiredService",
                                       "supervision", "doipInstance", "diagnosticEvent"):
                    if cap_id.split("_")[0] in element["shortName"].lower() or \
                            (cap_id == "doip" and element["kind"] == "doipInstance") or \
                            (cap_id == "phm_supervision" and element["kind"] == "supervision") or \
                            (cap_id == "dtc_reporting" and element["kind"] == "diagnosticEvent") or \
                            (cap_id == "someip_sd" and element["kind"] in
                             ("providedService", "requiredService")) or \
                            (cap_id == "exec_management" and element["kind"] == "functionGroup"):
                        evidence.append("%s (%s)" % (element["shortName"], element["file"].split("/")[-1]))
        capabilities.append(OrderedDict([
            ("id", cap_id),
            ("name", name),
            ("sources", files),
            ("evidence", sorted(set(evidence))[:3]),
        ]))
    return capabilities


# --------------------------------------------------------------------------

def main():
    arxml = [parse_arxml(os.path.join(CONFIG_DIR, name))
             for name in sorted(os.listdir(CONFIG_DIR)) if name.endswith(".arxml")]
    sources = collect_sources()
    tests = collect_tests(sources)
    uds_services, dids = collect_uds(sources)
    modules = module_stats(sources)
    capabilities = capabilities_from_repo(arxml, uds_services, sources)
    packages = load_requirements()
    diff = build_diff(packages, arxml, sources, uds_services, dids)
    chains = build_traceability(packages, arxml, sources, uds_services, dids)
    matrix = build_matrix(packages, capabilities, sources, chains)

    all_elements = [element for manifest in arxml for element in manifest["elements"]]
    service_ids = sorted(set(
        element["detail"]["serviceInterfaceId"] for element in all_elements
        if "serviceInterfaceId" in element["detail"]
    ))
    dtcs = [OrderedDict([
        ("name", element["shortName"]),
        ("dtc", element["detail"]["dtcHex"]),
        ("decimal", element["detail"]["dtcNumber"]),
        ("file", element["file"]),
        ("line", element["line"]),
    ]) for element in all_elements if "dtcHex" in element["detail"]]

    untested = [source for source in sources.values() if not source["test"]]
    header_counts = OrderedDict([
        ("manifests", len(arxml)),
        ("manifestElements", len(all_elements)),
        ("functionGroups", sum(1 for e in all_elements if e["kind"] == "functionGroup")),
        ("machineStates", sum(1 for e in all_elements if e["kind"] == "machineState")),
        ("serviceInstances", sum(1 for e in all_elements
                                 if e["kind"] in ("providedService", "requiredService"))),
        ("serviceIds", service_ids),
        ("checkpoints", sum(1 for e in all_elements if e["kind"] == "checkpoint")),
        ("dtcs", len(dtcs)),
        ("dids", len(dids)),
        ("udsServices", len(uds_services)),
        ("sourceFiles", len(sources)),
        ("sourceLoc", sum(source["loc"] for source in sources.values())),
        ("classes", sum(len(source["classes"]) for source in sources.values())),
        ("testFiles", len(tests)),
        ("testCases", sum(test["cases"] for test in tests)),
        ("untestedFiles", len(untested)),
        ("coverageRatio", round(1 - len(untested) / len(sources), 4) if sources else 0.0),
        ("tracedRequirements", len(chains)),
        ("requirementGaps", sum(1 for chain in chains if chain["gaps"])),
        ("changedRequirements", sum(1 for change in diff if change["status"] != "unchanged")),
    ])

    model = OrderedDict([
        ("generatedFrom", "COG-GTM/Adaptive-AUTOSAR"),
        ("counts", header_counts),
        ("manifests", arxml),
        ("modules", modules),
        ("udsServices", uds_services),
        ("dids", dids),
        ("dtcs", dtcs),
        ("requirementPackages", OrderedDict([
            ("v1", packages["v1"]["meta"]),
            ("v2", packages["v2"]["meta"]),
        ])),
        ("requirementDiff", diff),
        ("traceability", chains),
        ("matrix", matrix),
        ("untested", [OrderedDict([("path", s["path"]), ("module", s["module"]), ("loc", s["loc"])])
                      for s in sorted(untested, key=lambda s: -s["loc"])[:40]]),
    ])

    os.makedirs(DATA_DIR, exist_ok=True)
    out_path = os.path.join(DATA_DIR, "model.json")
    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(model, handle, indent=2)
        handle.write("\n")
    print("wrote %s" % rel(out_path))
    for key, value in header_counts.items():
        print("  %-22s %s" % (key, value))
    return 0


if __name__ == "__main__":
    sys.exit(main())
