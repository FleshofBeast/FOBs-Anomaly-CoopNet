# Direct-IP menu connection audit (2026-10-11)

The guest menu normalizes IPv4:port and dispatches the selected character to the same direct UDP transport as the console adapter. A guest console command is not required. The host binds the requested port on all interfaces; this transport has no automatic router traversal or relay. Host acceptance and shared-world readiness are distinct stages.

Fixed queued-menu errors that were silently ignored, unbounded initial retries, hidden admission rejection, and loss of transport termination diagnostics. Initial admission now has a 30-second limit; failure returns to the main menu with an explicit popup. Already admitted connections retain their reconnect behavior. No wire/protocol changes.

Validation: real-process UDP host/client admission, roster, input, snapshot acknowledgement and disconnect passed. Unused-loopback endpoint never admitted and reached Offline/Disconnected. Native fresh-character menu dispatch was accepted, transferred 1234 rubles and selected equipment, loaded/acknowledged the host world, and produced one host join announcement with no fatal/script/update errors. The existing 120-second new-character test's final movement-sample assertion did not pass for its idle actor; the connection assertions were checked separately and passed.

Native unreachable-host run recorded transport reason 5003 (Timed out attempting to connect), bounded initial failure, return to menu, failure popup and screenshot. The screenshot was visually inspected: readable explanation and OK button. Native logs are buffered; the new failure test waits on the screenshot file and checks flushed logs after shutdown. Early harness attempts did not complete because they watched buffered logs or did not stage character loading correctly.

Evidence: _build/join-audit-network2.log; _build/coopnet-new-character-77ee218845564360b0eae0e8004f6048; _build/join-failure-a3830466bb464779a4f07c77c6d1df5e. Build: _build/join-audit-build-final.log. Installed executable and PDB hashes match the tested build. Backup: ../backups/20261011-082228-join-audit.

This audit does not establish public-internet reachability. An outside client and host-side incoming traffic evidence are required to determine why the router-forwarded address fails while the VPN succeeds.

Final automated native failure test passed: _build/join-audit-native-failure-final.log; fixture _build/join-failure-aebc169593ad485ba8c3b2c45fface96. Screenshot-based completion and post-shutdown log assertions both passed.

## Host and join menu update

Host game opens the stock save picker and queues hosting on UDP 27888 (host ID 1, manual game/mod identifiers 1/1) only after selected-world initialization. Canceling the picker clears the request. Join game first shows address/Connect/Cancel; a valid Connect changes the screen to Load save/Create character/Cancel. Choosing a character retains that address and starts joining after loading. A pending start/load event prevents the menu-return cancellation check from dropping the request. Advanced console commands remain available.

Native validation: _build/menu-host-native.log passed queued hosting after saved-world load and guest admission/world acknowledgement. _build/menu-save-flow-native2.log passed actual Host game callback, address/Connect/Load save callbacks, stock picker load action, host admission and guest shared-world acknowledgement. The test invokes stock callbacks with isolated scripts rather than human mouse input. _build/menu-order-visual.log passed initial join/load/create selectors. Host picker, first address screen and second character-choice screen screenshots were inspected. The first script attempt lacked isolated scripts and stopped before client launch; preparation was corrected.

Final build: _build/menu-flow-build-final.log. Installed EXE/PDB hashes match; previous build backup: ../backups/20261011-083903-menu-host-join. Protocol remains 35. GitHub release ZIP has not been republished with these later menu changes.
