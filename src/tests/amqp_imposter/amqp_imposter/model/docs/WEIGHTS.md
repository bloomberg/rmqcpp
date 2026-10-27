I'd like the server to automatically draw from the enumerated sendables.
We could supply it with weights, e.g.:

```
    basic.ack = 8
    basic.cancel = 1
    channel.close = 2
    connection.close = 1
    ...
```

And any omissions are 100 by default, except for server-initiated teardown (`connection.close`/`channel.close`/`basic.cancel`),
which need to be 0; otherwise the model would not progress and using send_gen to autopilot some basic things would become annoying.
100 is chosen because it allows some granularity
(e.g. if we used one but I wanted to override a server-side Close to be a _bit_ likely, I could set it to e.g. 5, and the rest can stay at the default 100).

But this exclusion brings trouble: if we do not somehow exclude the Closes that are genuine method replies, how will the model state progress?
We need some kind of exclusion.


Then say the enumerated sendables are these:

```
    basic.ack(X), basic.ack(Y), channel.close(X), connection.close(X)
```

Considering just the method kinds available, the draw must satisfy:

```
    P(any basic.ack)     / P(any channel.close)    = 8 / 2
    P(any channel.close) / P(any connection.close) = 2 / 1

    P(any basic.ack) + P(any channel.close) + P(any connection.close) = 1

    ->

    P(any basic.ack)        = 8 / 11
    P(any channel.close)    = 2 / 11
    P(any connection.close) = 1 / 11
```

Which gives us the probabilities for the actual draw:

```
    P(basic.ack(X)) + P(basic.ack(Y)) = P(any basic.ack)
    P(channel.close(X))               = P(any channel.close)
    P(connection.close(X))            = P(any connection.close)

    P(basic.ack(X)) = P(basic.ack(Y))

    ->

    P(basic.ack(X))        = 4 / 11
    P(basic.ack(Y))        = 4 / 11
    P(channel.close(X))    = 2 / 11
    P(connection.close(X)) = 1 / 11
```

Something else to think about:
- Should weight 0 actually be allowed?
- Should there also be a max weight?

----------------------------------------------

What if we extended the patterns from names to fields too?

This would also allow us to specify zero default weight to `connection.close`/`channel.close` (class-id=0, method-id=0) for no broker-initated teardown.
For another example, the following would never confirm delivery tag 2 and keep cancelling a particular consumer (more useful once we have multiple channels).

```
    basic.ack(2) = 0
    basic.ack() = 1
    basic.cancel("im-a-consumer") = 8
```

- Question: What is the resolution order?
  Example: say we have specified different weights on
                `basic.return(exchange="ex")`
                `basic.return(routing_key="rk")`
           then which one matches:
                `P(basic.return(exchange="ex", routing_key="rk"))`
  Answer: use the given order for resolution, this is how many (all?) pattern-matchers solve this problem

- Question: What happens if you don't specify a value on a specific field?
  Answer: unify with what's in the bag.

- What happens if you specify a value on an arbitrary field?
  Answer: discussion below

----------------------------------------------

Let's assume we specify values on an arbitrary field - what will that mean?

Consider if this statement even makes sense: "this is the weight of `basic.deliver(message='hello')` compared to all other deliveries" -- but there are ~infinitely many deliveries!
Maybe we could somehow bias towards this string?

Sidestepping that objection for a moment, let's work through an example for a simpler type: bool.

requeue is an arbitrary field on acknowledgments, so suppose we're given weighted patterns in the following order:

```
    basic.nack(1) = 5
    basic.nack(requeue=True) = 4
    basic.nack() = 2
```

Say the enumerated sendables are:

```
    basic.nack(1), basic.nack(2)
```

Matching in order while restricting the domain of requeue:

```
    basic.nack(1)            takes basic.nack(1, requeue=True), basic.nack(1, requeue=False)
    basic.nack(requeue=True) takes basic.nack(2, requeue=True)
    basic.nack()             takes basic.nack(2, requeue=False)
```

then we should have:

```
    P(basic.nack(1)) / P(basic.nack(2, requeue=True)) / P(basic.nack(2, requeue=False)) = 5 / 4 / 2
```

Instead, the resolution order

```
    basic.nack(requeue=True) = 4
    basic.nack(1) = 5
    basic.nack() = 2
```

would've given:

```
    [P(basic.nack(1, requeue=True)) + P(basic.nack(2, requeue=True))] / P(basic.nack(1, requeue=False)) = 4 / 5
```

We're effectively implementing full pattern-matching with knowledge of the domain of each type, and trying to bias towards some values.
This would feel fiddly to use and be way too complex to implement.

We could try to simplify: perhaps the first pattern that matches an arbitrary field should fix it to that value.
But then this would be unintuitive:

```
    basic.nack(requeue=True) = 1
    basic.nack(requeue=False) = 100  # no-op, but you'd think it might weight the False side of basic.nack to 100.
```

TL;DR: only exact fields make sense in patterns.
