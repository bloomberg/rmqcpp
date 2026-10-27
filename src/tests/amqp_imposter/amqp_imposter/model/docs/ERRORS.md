Some errors need certain circumstances to arise in order to be sendable;
e.g. for a method to be received with some bad fields (think double `channel.open`).
Let's call these "situational".

Other errors are "always-on"; think an operator decides to close a connection via management UI.

Here are the forced situational errors we've specified:

- PRECONDITION_FAILED / `channel.close` (406)
    - Acknowledgement for delivery_tag = 0 with multiple = False
    - Acknowledgement when no-ack=Tru
    - Acknowledgement with unknown delivery tag
    - Acknowledgement for an already-acknowledged delivery tag
- CHANNEL_ERROR / `connection.close` (504)
    - `channel.open` for an already-open channel
    - Any other method on a closed channel
- NOT_IMPLEMENTED / `connection.close` (540)
    - `basic.publish` with immediate
    - `basic.qos` with prefetch_size

The class_id and method_id fields for situational errors are fixed based on what method caused the error.

There are also situational errors that aren't forced by a client action, but an alternative to the Ok.
For instance, a non-passive `queue.declare` could lead to PRECONDITION_FAILED.
Exhaustively:

- `connection.open`: INVALID_PATH(402), NOT_ALLOWED(530)
- `channel.open`: RESOURCE_ERROR(506)
- `exchange.declare`: NOT_FOUND(404), ACCESS_REFUSED(403) if passive; otherwise ACCESS_REFUSED(403), PRECONDITION_FAILED(406), NOT_ALLOWED(530), RESOURCE_ERROR(506)
- `exchange.bind`: NOT_FOUND(404), ACCESS_REFUSED(403), RESOURCE_ERROR(506)
- `queue.declare`: NOT_FOUND(404), ACCESS_REFUSED(403), RESOURCE_LOCKED(405) if passive; otherwise ACCESS_REFUSED(403), RESOURCE_LOCKED(405), PRECONDITION_FAILED(406), RESOURCE_ERROR(506)
- `queue.bind`, `basic.consume`: NOT_FOUND(404), ACCESS_REFUSED(403), RESOURCE_LOCKED(405), RESOURCE_ERROR(506)
- `basic.get`: NOT_FOUND(404), ACCESS_REFUSED(403), RESOURCE_LOCKED(405)
- `basic.publish`: NOT_FOUND(404) unless to the default exchange, ACCESS_REFUSED(403), CONTENT_TOO_LARGE(311)

An error is available until the model sends any method that would imply it has been accepted.

Forced errors carry the reply text "FORCED" and unforced ones "NOT_FORCED".
That means they can be pattern-matched by the input weights.

The always-on errors are always "NOT_FORCED":
- CONNECTION_FORCED / `connection.close` (320)
- INTERNAL_ERROR / `connection.close` (541)

The class_id and method_id fields for always-on errors are both fixed to 0.

There are also certain conditions on which the model will short-circuit for now (produce Unrepresentable):
- Protocol header is sent twice
- The client's negotiation contradicts the prodived NegotiationOptions
- A second simultaneous connection is opened
- `basic.qos` with global
- A second simultaneous consumer, whether or not it is pipelined behind the first

Known gaps:
- Passive declares with no-wait still offer errors, although the spec says they have no effect
