The `RevealingLine` data structure is used to enforce ordering constraints, in particular:

- When a pending method becomes observable/sendable
- When an observed or superseded method can be dismissed

Say a client sends a `queue.declare` and then a publish on a confirming channel.
The broker replies in order, so it can't confirm the publish before sending the `queue.declare-ok`.
Thus the `queue.declare-ok` is Opaque, hiding the publish behind it.

One thing that's tricky to model is that a nowait method needs no reply, but the imposter should be able to respond with an error!
Thus we owe an optional reply that's Transparent, which doesn't hide anything behind it.

```
  RevealingLine[T]:
    Revealed[T] = Transparent[T] | Opaque[T]

    push(Revealed[T] revealed) -> RevealingLine[T]                # append to back
    observe() -> tuple[T, ...]                                    # observe up to and including the first opaque item

    split(n) -> (RevealingLine[T], RevealingLine[T])              # split into first n items, rest; observable, unobservable by default
    split_while(p) -> (RevealingLine[T], RevealingLine[T])
```

Used in the model as follows.

```
  KnownReplyState = { expected_methods: tuple[Method], causing_method: Method | None }
  DeferredReplyState = { causing_method: BasicGet }               # expected_methods are implicit and decided at reveal-time
  DeferredPublishState = { publish_state: PublishState }

  ReplyState = KnownReplyState | DeferredReplyState

  line: RevealingLine[DeferredPublishState | ReplyState]

  - Replies are Opaque, except optional replies (to a nowait or a publish), which are Transparent.
  - When a reply is sent, that pops it along with all the items in front.

  - Publishes are Transparent.
  - Publishes are never at the front. (They are always popped/flushed whenever they reach the front of the line.)
  - Publishes that are observable are added to standby.
  - Publishes that are confirmed, returned or nacked on standby cause a pop that takes with it all the items in front.
    (A publish is only popped in this situation or if it's at the front of the line.)
```
