# Network

- Authority: `AFRRolesSubsystem` (SpawnOnServer_Replicate, always relevant). Clients never decide roles, permissions, admin actions, start or exceptions.
- Requests: `UFRRemote` RCO (registered with the game mode at world init, plus for controllers already logged in). The server identifies the caller from the RCO's owning controller; no identity is accepted from the client.
- Identity: `UniqueNetId` type + string (Steam/Epic). Fallback `name:<account name>` only when no online id exists (offline/LAN). Renaming in-game does not change the online id.
- Host/admin: listen host = local controller on the server. Dedicated server: first player to reach an unhosted session is recorded as host (reassignable by editing nothing — documented limitation).
- Idempotence: every mutation validates current state (choose twice → no-op, start twice → no-op, revoke twice → no-op). Simultaneous choices are serialized by the server; capacity check rejects the second.
- Late join / reconnection: `PostLogin` + client `Server_Hello` (retried every 4 s until the record replicates). Saved division restored by id; no re-selection.
- Notifications: server → client `Client_Notify(key, args)`; localized on the receiver; refusal toasts throttled per player/reason.
- Replication volume: one actor, arrays bounded (60 requests, 40 messages, 80 journal lines, grants pruned after 24 h); `ForceNetUpdate` only on change; no tick.
