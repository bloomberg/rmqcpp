# Consumer states

The diagram shows the state changes based on the Broker messages we receive.

## `CANCELLING_BUT_RESUMING`

New state `CANCELLING_BUT_RESUMING` was added to support resume after cancellation. 

- calling `resume()` in a `CANCELLING` state sets the state to `CANCELLING_BUT_RESUMING` which will enable consumer restart

- previously cancelled consumer would be restarted after `resume()` call