#include "opendva/report/AssemblyState.h"

namespace opendva {

StateEffects AssemblyStateMachine::nominalBuild() {
    // Reset to nominal, run the move, never apply tolerances (README §8.1).
    effects_ = {/*reset=*/true, /*runTolerances=*/false, /*leaveTrace=*/false};
    state_ = AssemblyState::Built;
    return effects_;
}

StateEffects AssemblyStateMachine::assemble() {
    // Run the move but keep deviations: no reset, no tolerances (README §8.1).
    effects_ = {/*reset=*/false, /*runTolerances=*/false, /*leaveTrace=*/false};
    state_ = AssemblyState::Built;
    return effects_;
}

StateEffects AssemblyStateMachine::separate() {
    // Tear the assembly apart (undo move); no tolerances (README §8.1).
    effects_ = {/*reset=*/false, /*runTolerances=*/false, /*leaveTrace=*/false};
    state_ = AssemblyState::Separated;
    return effects_;
}

StateEffects AssemblyStateMachine::deviate() {
    // Apply random tolerances. "reset depends on state" (README §8.1): a fresh
    // deviate off a build resets points; continued deviation in the statistical
    // loop keeps stepping without resetting.
    const bool reset = (state_ != AssemblyState::Deviated);
    effects_ = {reset, /*runTolerances=*/true, /*leaveTrace=*/false};
    state_ = AssemblyState::Deviated;
    return effects_;
}

StateEffects AssemblyStateMachine::sweep() {
    // Same as Deviate but keeps the prior result on screen (residue trace).
    const bool reset = (state_ != AssemblyState::Deviated);
    effects_ = {reset, /*runTolerances=*/true, /*leaveTrace=*/true};
    state_ = AssemblyState::Deviated;
    return effects_;
}

}  // namespace opendva
