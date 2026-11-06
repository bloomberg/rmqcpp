# AMQP Imposter

AMQP clients are difficult to test thoroughly against a real broker like RabbitMQ.
Many bugs lurk in rare method sequences that are difficult to reproduce deterministically or cover exhaustively with hand-written tests.
How does your client juggle multiple `basic.return -> basic.ack` interleaved with broker-initiated `connection.close`?

This imposter lets you ask these questions of your code, discover violations of properties you expect to hold across _every_ interaction, and reliably reproduce known tricky interactions.

It is an AMQP 0-9-1 server with a model of the legal actions available to it.
A small configuration language lets you tune its behaviour with:

- Injected `when-then`-style rules on model state transitions, to construct particular decision sequences, layered on top of...
- Weighted sampling from the legal actions using a seed, to let the imposter make its own decisions.

The imposter fills in the blanks (fields of a method or whole methods) with values it knows are valid in the current state, in pattern-matching style.

It supports one channel over one connection at a time.

## Quick start

### Python

The `amqp_imposter` context manager runs and configures an imposter subprocess on available ports.
An example test written using the imposter:

<!-- fmt:off -->
```py
import pytest
from amqp_imposter.control.client import amqp_imposter, pb


@pytest.mark.parametrize("seed", range(500))
def test_my_client(seed: int) -> None:
    """
    Interact with the imposter, then check a desired property of the interaction.
    """
    config = pb.SessionConfig(seed=seed)

    with amqp_imposter(config) as imposter:
        run_my_client(imposter.amqp_port)

        transcript = imposter.transcript()
        assert check_my_property(transcript), str(transcript)
```

The runs above each spawn their own imposter process, so they can be parallelised, for example using `pytest-xdist`.
If running serially, calling `imposter.configure(config)` will disconnect the client and prepare a new session on the same imposter process.

Optionally tune the behaviour of the imposter by pattern matching.

```py
from amqp_imposter.control.client import (
    pb,
    weighted,
    when_receive,
)

script = (
    when_receive(pb.BasicPublishPattern(), then_send=pb.BasicReturnPattern()),
)
weights = (
    weighted(pb.BasicAckPattern(), pb.BASE_WEIGHT // 2),
    weighted(pb.ConnectionClosePattern(class_id=0, method_id=0), pb.BASE_WEIGHT // 20),
)

config = pb.SessionConfig(script=script, weights=weights, seed=seed)
```
<!-- fmt:on -->

### Other languages

Run the imposter as a foreground process:

```
uvx amqp-imposter@<version> [--socket <path>] [--amqp-port <port>]
```

then read the first line of `stdout` for the control socket and AMQP port.
If the imposter created the socket, it will be cleaned up on `SIGINT`/`SIGTERM`.

```json
{"socket": "/tmp/amqp-imposter-<pid>/control.sock", "amqp_port": 5672}
```

Configure the imposter over gRPC on the socket using the proto schema (see [Control](#control)).

## How does the AMQP server work?

The actual AMQP server is thin and sits atop the more complex logic model.
It frames its output, puts it on the wire, and tracks session state (the model state machine, a transcript of the interaction, etc.).
`pika` is used for framing.
The connection is in plaintext.

If the server detects that the client-server interaction has stalled (timed out idling since the last ingress method), it samples egress methods from the model to jumpstart it.
If there are no samples, then the client must owe us ingress.

## How does the control server work?

The control plane to manage the server speaks gRPC over a Unix domain socket.

Talk to it in gRPC to:

- Configure: set the model parameters, reset the transcript and disconnect the client
- Reset: default the model parameters, reset the transcript and disconnect the client
- Status: see the AMQP port, whether there is an active client, the model parameters
- Stream the transcript: see the events and methods exchanged

This lets us generate typed messages for all of these to use with the language of the client under test.
It also makes it easy to represent some more complex AMQP data types, such as tables.
Generate client stubs from `proto/amqp_imposter/control/v1/imposter.proto`.

Note on development: `tox -e proto` regenerates types from the schema.

## How does the model work?

During a session, what the imposter needs to track is the methods it may send next.
The server model is responsible for tracking these sendables.
It is a pure core and strongly decoupled from the imposter harness.

A good rule of thumb for maintaining this separation is "can I interact/play with the model nicely from a REPL."

### Methods and method patterns

Because the sendable methods are infinite, we have to represent this bag in terms of partially-specified methods.
A method field belongs to one of two categories:

- **Exact**. It is always fixed and determined by the model. If I ask "is this method legal now", the answer partially depends on these fields. Example: `basic.getok(delivery-tag=1)`.
- **Arbitrary**. It can be empty or fixed. If I ask "is this method legal now", the answer ignores any empty fields. When sending a method, empty arbitraries can be filled in with any value. For example, `basic.getok(body="a")` is as legal as `basic.getok(body="bc")`. The field `basic.getok(redelivered=False)` is as legal as `basic.getok(redelivered=True)` on a first-time delivery to a channel, but `True` is fixed in the representation of a known redelivery.

### Model sampling

Sampling is configured with patterns on methods and their exact fields.

For example, you may set a high weight on `basic.return(*)` and some on `connection.close(class-id=0, method-id=0, *)` to see that your producer republishes correctly.
The sampling algorithm collects the matches for all patterns in order, picks the lucky pattern, samples one of its methods uniformly, and fills in the empty arbitraries.
You can weight `channel.close(reply-text="NOT_FORCED", *)` and `connection.close(reply-text="NOT_FORCED", *)` to configure separate probabilities for forced and unforced errors.

There is one special pattern, _stop_.
The model keeps generating methods for a burst until no methods are left or this pattern is sampled.

You do not have to manually weight everything -- there is a set of implicit patterns at the bottom of the order, weighting each method and the stop at 100 and all server-initiated teardown and non-forced errors at 0.

On idle timeout, the imposter will also pass the special _idle weights_ to the model, at the the top of the order.
For example, you can use this to lower the chance of broker-initiated teardown during longer pauses by the client.

### Model scripting

Scripting is configured with patterns that activate after any send or receive transition.

They sit above weighted patterns in the resolution order, including above the stop pattern.
Therefore, you can configure the rules you want and let the imposter fall through to its own judgement when there is no match.

For example, configuring `when=received(basic.publish(*)), then_send=basic.ack(*)` means that after applying a received publish, the model should send an ack.
If the ack is not valid at that point, it will try the next matching pattern, in order.

Use the configurable _max sends_ to bound the bursts, 1,000 by default.
It is always a finite value, as a misconfigured ruleset could produce infinite loops: `when=sent(basic.deliver(*)), then_send=basic.deliver(*)`.

### Representing topology

There is no persistent broker state; anything is fair game on a connection as long as it does not contradict the state of the interaction.
If your client declares an exchange, that does not permanently make the exchange exist inside the AMQP server you are talking to.
For example, in the imposter, publishing to an undeclared exchange will not deterministically produce channel error and publishing to a declared one may still 404.

In practice, vhosts are shared resources and their topologies volatile.
All those states -- existence and non-existence, changed arguments, changed message counts, etc. -- need to be represented by the model, so it does not need to track past declarations and bindings.

## Decision register

See [model/docs](amqp_imposter/model/docs/).
