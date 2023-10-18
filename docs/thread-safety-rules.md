## Event Loop only classes - rmqamqp,rmqio,rmqamqpt libraries. 
* these should only run inside the event loop, whilst they could expose an interface callable by an execution context that isn't the event loop, this must purely be a proxy (e.g. post) for the execution to happen on the event loop. 
an example of this would be rmqa creating a new rmqamqp connection, and calling methods on it. 

## rmqa components
* must be called from client code
* should only hold references to lower level (event loop) components
* must never destruct event loop components

## Event loop 
* must never callback into client code, this should be dispatched onto a seperate threadpool. 
