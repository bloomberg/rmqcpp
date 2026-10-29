The fields of a method in the bag are in one of two states:

- Exact     (an instantiation of the method is incomplete without a concrete value here; value is determined by the model state)
- Arbitrary (functionally equivalent instantiations of a method could hold different values; value is always compatible with the model state)

For an arbitrary slot, the caller may bring their own value and we generate if they don't.
The types below are inspired by the QuickCheck white paper.

```
    type Gen[T]       = (GenState) -> (T, GenState)
    type Exact[T]     = T | Hole      # the hole should be filled with concrete; until then, the method makes no sense
    type Arbitrary[T] = T | Gen[T]

    BasicDeliver
        consumer_tag: Exact[str]
        delivery_tag: Exact[int]
        redelivered:  Arbitrary[bool] # you can get deliveries that look like they were previously delivered to another consumer
        exchange:     Arbitrary[str]  # N.b. should be bound when it's bagged if it's a requeue!
        routing_key:  Arbitrary[str]
        properties:   Arbitrary[MessageProperties | None]
        body:         Arbitrary[bytes]
```

For example, send(method) tries to unify any arbitrary fields in the bag with those that are already concrete in the input method, and generates the ones that are still Gen.
