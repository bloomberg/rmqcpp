The aim of this model is to generate method sequences for plausible AMQP client-server interactions.

Always keep the logic in this model decoupled from the imposter harness.
This is a pure and reusable AMQP state machine.

To that end server tracks a "bag of methods" that it can send or be controlled to send, updating it on every send/receive event.
If you ask the server to send a method that's illegal in that part of the sequence, it raises Unrepresentable.
If you ask the server to receive a method that's not yet representable by the model, it raises Unrepresentable.
Everything in the bag is what a conforming server is permitted to send.

A good mental model is that the bag is underpinned by a multiset S plus a queue Q, and it contains S ∪ { Q.head }.
In effect:
- S contains any always-on-offer methods (e.g. deliveries)
- Q contains methods that must be ordered (e.g. if we received `queue.declare` -> `queue.bind`, `queue.declare-ok` -> `queue.bind-ok` must also be ordered).

We make some simplifications while also adhering to the spec:

- There's one vhost, one connection and one channel at a time.
- Every method in send/receive is assumed to belong this channel.
- Topology is volatile: declaring and binding never affect routability, and message counts are arbitrary.
- Messages are produced arbitrarily. Exchange, routing key, properties and body are generated (but of course requeues and returns do match the originals).
- The server is constructed with the exact capabilities and limits it'll accept in the handshake.
