# Security

Cubey's server holds children's conversations and the keys of paid providers, so security reports are welcome and taken seriously.

## Reporting a vulnerability

Please report it privately through GitHub: on this repository's **Security** tab, choose **Report a vulnerability**. Don't open a public issue.

Include what's affected, how to reproduce it and what it could expose. You'll get a reply, and a fix is coordinated with you before anything is made public.

## Scope

Of most interest:

- the server's device authentication (`CUBEY_DEVICE_TOKEN` and each cube's own key, `server/src/fleet.ts` and `access.ts`)
- the admin page and its session (`server/src/admin.ts`)
- anything that could expose a child's talks, the parents' log or a provider key
- the firmware's handling of messages from the server

## Running your own

- Keep `server/.env` out of version control (it's git-ignored) and provider keys on the server only. Never build them into the firmware.
- Set a long random `CUBEY_DEVICE_TOKEN` (`openssl rand -hex 32`) and a strong `CUBEY_ADMIN_PASSWORD` before exposing the server to a network.
- Serve it over TLS (`wss://`), as the included Fly.io configuration does.
