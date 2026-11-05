# Lexia goes online only when the user asks, and never sends the screen

Lexia works fully offline. The only network use is downloading something the user explicitly asked
for (official Piper Voices from the Hugging Face `piper-voices` catalogue). Nothing about the
screen, Shots, recognised text or Readings ever leaves the PC: Lexia reads whatever is on screen,
including private messages and documents, so this is a promise to users, not a default.

## Consequences

- No telemetry, crash upload or update check without the user's explicit request.
- Recognition and speech must keep working with no network at all.
