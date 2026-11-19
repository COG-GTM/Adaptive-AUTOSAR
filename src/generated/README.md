# Generated Classic-to-Adaptive bridge output (AE-29, v0 prototype)

> **Prototype, not a certified toolchain.** These files demonstrate a Classic-to-Adaptive migration
> path. They are not qualified, the ARXML is not validated against the AUTOSAR XSD and the C++ API is
> only `ara::com`-like. Do not use them for production or safety-related software.

The files in the sub-directories are **generated; do not edit them**. They are the unmodified output of
`examples/generator/adaptive/classic_to_adaptive.py` in
[COG-GTM/autosar](https://github.com/COG-GTM/autosar), which reads a Classic SWC description and maps:

| Classic | Adaptive |
| --- | --- |
| Provided sender-receiver port / data element | Event |
| Provided client-server port / operation | Method |
| ImplementationDataType | C++ type alias |

| Directory | Classic input | Content |
| --- | --- | --- |
| `wheel_speed_sensor_service/` | `/ComponentTypes/WheelSpeedSensor` | Event `WheelSpeed` (`WheelSpeed_T` = `std::uint16_t`) |
| `timer_component_service/` | `/ComponentTypes/TimerComponent` | Methods `GetTime` and `IsTimerElapsed` |

Each directory holds the ServiceInterface description with its SOME/IP deployment (`.arxml`) and the
`_types.h`, `_common.h`, `_skeleton.h` and `_proxy.h` headers. The headers are bound to the existing
SOME/IP classes of this repository:

- events: `ara::com::helper::NetworkLayer<EventMessage>`, where `EventMessage` is a generated
  `ara::com::someip::SomeIpMessage` of type notification
- methods: `ara::com::someip::rpc::RpcServer` and `RpcClient`

To regenerate, run from the root of the autosar repository:

```bash
G=examples/template/generated
python examples/generator/adaptive/classic_to_adaptive.py --out-dir <this dir>/wheel_speed_sensor_service
python examples/generator/adaptive/classic_to_adaptive.py --swc TimerComponent --service-id 0x1001 \
    --out-dir <this dir>/timer_component_service \
    $G/AUTOSAR_Platform.arxml $G/PortInterfaces.arxml $G/TimerComponent.arxml
```

## What is hand-written

Everything under `test/generated/` is hand-written: the GoogleTests, the in-process RPC loopback
(`loopback_rpc.h`), the `TestTimer` service implementation and the SOME/IP-SD wiring in
`wheel_speed_sensor_service_sd_test.cpp`.

## Limitations

- `OfferService`, `Subscribe` and `Unsubscribe` are local switches. The generated code does not drive
  SOME/IP service discovery or eventgroup subscription; the SD test wires `SomeIpSdServer` by hand
  with the generated identifiers.
- The repository has no UDP transport for event notifications, so events are only exercised over the
  in-process `MockupNetworkLayer`. Methods are only exercised over the in-process RPC loopback, not
  over `SocketRpcServer`/`SocketRpcClient`.
- Methods report their result through a callback (no `ara::core::Future`), with a success flag instead
  of application errors. A proxy keeps one response handler per method, so overlapping calls of the
  same method are not supported.
- Values are the raw Classic internal values; CompuMethods and data constraints are not applied.
- Only fixed-width integers, `bool`, `float` and `double` are serialized (big-endian, no padding).
- The generated code is not wired into the `adaptive_autosar` executable or its manifests.
