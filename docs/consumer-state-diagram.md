# Consumer states

The diagram shows the state changes based on the Broker messages we receive.

## `CANCELLING_BUT_RESUMING`

New state `CANCELLING_BUT_RESUMING` was added to support resume after cancellation. 

- calling `resume()` in a `CANCELLING` state sets the state to `CANCELLING_BUT_RESUMING` which will enable consumer restart

- previously cancelled consumer would be restarted after `resume()` call

![image](https://github.com/mvrsss/rmqcpp/assets/60746841/54b025ee-3d7a-48de-bbcf-97ba00abf76b)
