# Player death and restart

## Source of truth

The original WL6 implementation is in `WL_AGENT.C` (`TakeDamage`) and `WL_GAME.C` (`Died` and the game loop). WolfVK keeps the original state transition and restart loadout, while the camera fall and fire-only acknowledgement are the requested presentation/input choices.

## Behaviour

1. Enemy damage that reduces health to zero enters a single death sequence and records the lethal attacker position.
2. The player cannot move, look, fire, use doors, collect bonuses or advance enemy simulation while dying.
3. The camera turns towards the attacker, receives a fading red palette shift, and lowers towards the floor. The first-person weapon is hidden.
4. After the presentation, exactly one life is deducted.
5. If the life counter is at least zero, the game waits for a fresh fire press. A held fire button from before the prompt does not acknowledge it.
6. Restart rebuilds the same decoded level, restoring its original doors, pushwalls, pickups and actors. The player returns to the level's original start marker with 100 health, 8 bullets, the pistol and no keys. Score, extra-life threshold and remaining lives are retained.
7. If the life counter falls below zero, the game remains in a safe no-lives state. The game-over/high-score/menu transition is intentionally deferred.

## Acceptance

- [ ] Windows build and full Unity test suite pass.
- [ ] Tests cover lethal attacker reporting, single life deduction, fire edge handling, restart loadout and terminal no-lives state.
- [ ] Verify death animation, weapon suppression, same-level reset and repeated deaths in a WL6 runtime session.
- [ ] Confirm the deferred no-lives state does not crash or permit an unintended restart.
