# SCPI And CDC Regressions

Run `python3 run_tests.py` on the host. The tests compile the production
SCPI, CDC, line reader, and character queue sources with ST's USB headers.
Only the hardware and scheduler boundary is modeled by `usb_fixture.c`.
The fixture retains TX pointers until completion, models OUT endpoint
arming, and queues application callbacks instead of invoking them from ISR
callbacks.

Initialization models ST's unconditional first OUT arm after the interface
callback. A separate 64-byte delivery snapshot preserves callback storage
across bus reset, even if the hardware receives a new packet. Lifecycle
tests explicitly reorder queued callbacks: disconnect cleanup must run
once before any new connect, RX, or TX callback, independent of scheduler
ordering.

Coverage includes multi-command packets, bursts spanning many packets,
backpressure while responses are pending, concurrent streaming, split
lines and CRLF, error snapshots, cancellation, and fast reconnects with
old callbacks still queued. CDC tests also exercise pause/resume inside
and outside callbacks, idempotent resume, and zero-length OUT packets.

Command handlers may emit one response line with `scpi_print()`. Commands
pause behind that response, so the transport needs only one reply buffer
and one retained USB packet. Unsolicited producers must retry when
`scpi_print()` returns false. Hosts sending large query bursts should read
responses concurrently so their own receive buffers do not block progress.

DTR close discards partial commands and unsent responses. A transfer
already submitted to USB can still complete; its buffer is retained until
completion or bus deinitialization. An error query clears the latched
error only after completion, and does not clear a newer error reported
while its response was in flight.
