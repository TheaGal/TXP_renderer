#include "editor_state.h"


TXP::anim_frame_action::Editor_state TXP::anim_frame_action::s_editor_state;

void TXP::anim_frame_action::reset_editor_state()
{
    s_editor_state = Editor_state{};
}
