# Consumer Pause and Resume

Consumers created with `consumeOnlyFromHealthyHost` are paused and resumed
by the `HostHealthMonitor`. On every poll that returns a health result
(not a retry) it tells each health-aware consumer to pause (host unhealthy)
or resume (host healthy).

Pausing sends `basic.cancel` and resuming sends `basic.consume` with the
same consumer tag. The consumer object, and its message callback, are kept
while paused.

## Pause and resume set the desired state

`pause()` and `resume()` tell the channel what state the consumer should be
in. The channel makes it so at the next opportunity: straight away if it
can, otherwise once a reply in flight arrives or the channel reopens. The
latest request wins.

A request is only refused if it repeats the state the consumer is already
in, or if it can't be carried out:

- Pausing a paused consumer, or resuming a consuming (or starting) one,
  resolves with an error and changes nothing.
- Resuming or pausing a cancelled consumer, or one being cancelled, resolves
  with an error. Cancel is final.
- Resuming or pausing with no consumer resolves with an error. A consumer
  added later starts in the requested state.
- Resuming or pausing a channel that is permanently closing
  (`gracefulClose()`) resolves with an error, since it won't reopen and the
  broker discards anything sent after `channel.close`.

Repeating a request that is still in flight returns the same future.

## When the future resolves

| Request | Resolves |
|---|---|
| Pause, consumer consuming | On `basic.cancel-ok` |
| Pause, consumer starting | On `basic.cancel-ok` for the `basic.cancel` sent once `basic.consume-ok` arrives |
| Pause, nothing consuming on the broker (e.g. channel reopening) | Immediately; the channel reopens paused |
| Resume, channel ready | On `basic.consume-ok` |
| Resume, channel closed or reopening | On `basic.consume-ok` after the channel reopens |
| Resume, pause in flight | On `basic.consume-ok` for the `basic.consume` sent once the pause's `basic.cancel-ok` arrives |

A request overtaken by the opposite one before it completes resolves with
an error:

- A pending resume fails as soon as a pause or cancel is requested.
- A pending pause fails when its `basic.cancel-ok` arrives, if a resume was
  requested meanwhile; the resume's `basic.consume` follows straight away.

Futures still waiting at a channel reset resolve with an error, but the
latest request stands: the reopened channel carries it out.

## Reopening a channel

A channel that resets reopens in the state the consumer was last asked to
be in:

| Latest request at reset | Reopens |
|---|---|
| Resume (or none since consuming) | Consuming |
| Pause | Paused |
| Cancel | Consumer removed |

## Cancelling

Cancelling a paused consumer, or one waiting for the channel to reopen,
completes immediately, since nothing is consuming on the broker. Cancelling
while a pause is in flight turns the pause into a cancel; the future
resolves when `basic.cancel-ok` arrives.
