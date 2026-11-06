# TODO

Gaps
- Also redeliver across a disconnect
- Allow two `CloseOk`s when client and server simultaneously `Close`

Enhancements
- Support N consumers on the same channel
- Support N channels on the same connection
- Support N connections in the same session
- Fully adhere to the negotiated capabilities
- Support anonymous consumer when the `Basic.Consume` is `no-wait`
- Extend table generator to produce nesting
- Add property-writing utilities for the common patterns
- Add heartbeat support (e.g. chance of delay)

Documentation
- Document interpretation of AMQP 0-9-1 ambiguities: "a client should not depend on this"

Maybe
- Allow requeued messages to disappear
