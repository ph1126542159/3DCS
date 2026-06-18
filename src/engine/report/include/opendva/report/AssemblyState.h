// A7 assembly state machine (README §8.1). Pure state logic for the graphical
// analysis actions, decoupled from any GUI so it stays unit-testable.
#pragma once

namespace opendva {

// Discrete assembly states reachable through the graphical analysis actions.
// Nominal     -- freshly reset, no move/tolerances applied yet.
// Built       -- nominal build done (points reset, move applied, no tolerances).
// Separated   -- assembly torn apart (move undone), awaiting a rebuild.
// Deviated    -- tolerances applied on top of a build (statistical state).
enum class AssemblyState { Nominal, Built, Separated, Deviated };

// Effects produced by an action, per the README §8.1 semantics table:
//   reset         -- whether points are reset to nominal before the action,
//   runTolerances -- whether random tolerances are applied this step,
//   leaveTrace    -- whether the prior result is kept on screen (Sweep residue).
struct StateEffects {
    bool reset{false};
    bool runTolerances{false};
    bool leaveTrace{false};
};

// Models the README §8.1 action semantics. Each method mutates the internal
// state and returns the (reset, runTolerances, leaveTrace) effects it implies.
class AssemblyStateMachine {
  public:
    AssemblyStateMachine() = default;

    AssemblyState state() const { return state_; }
    const StateEffects& lastEffects() const { return effects_; }

    // Nominal Build: reset points to nominal, run the move, never run
    // tolerances. Establishes the baseline assembly. -> Built.
    StateEffects nominalBuild();

    // Assemble: run the move but keep existing deviations (no reset, no
    // tolerances). Continues building on an already deviated result. -> Built.
    StateEffects assemble();

    // Separate: tear the assembly apart (undo the move). No tolerances.
    // -> Separated.
    StateEffects separate();

    // Deviate: apply random tolerances on top of a build. Resets only when not
    // already in a statistical (Deviated) state. -> Deviated.
    StateEffects deviate();

    // Sweep: like Deviate but keeps the prior result on screen (residue trace).
    // -> Deviated.
    StateEffects sweep();

  private:
    AssemblyState state_{AssemblyState::Nominal};
    StateEffects effects_{};
};

}  // namespace opendva
