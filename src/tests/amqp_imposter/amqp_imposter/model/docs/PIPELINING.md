Some invariants in the pipelining model:

- Replies are ordered by receipt irrespective of class (e.g. `channel.open` -> `queue.declare` gives `channel.open-ok` -> `queue.declare-ok`)
- Effects are applied on receipt
- no-wait just means that we owe no reply, but an error response should be made available until the state advances past it (e.g. if we confirm a message that came later)
