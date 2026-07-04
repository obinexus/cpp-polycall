# cpp-polycall tests

The native test links the real C++ adapter against a mock libpolycall FFI. It
verifies path forwarding, `run=1`, unchanged statuses, and the `polycall::Error`
exception contract. Run it with `make test`, `npm test`, or CTest.
