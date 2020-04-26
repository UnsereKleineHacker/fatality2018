#include "../include_cheat.h"

void autostrafer::strafe()
{
	if (!vars::misc.autostrafe.get<bool>())
		return;

	static bool is_bhopping;
	static float calculated_direction;
	static bool in_transition;
	static float true_direction;
	static float wish_direction;
	static float step;
	static float rough_direction;

	auto local_player = g_pLocalPlayer;
	if (!local_player) return;

	enum directions {
		FORWARDS = 0,
		BACKWARDS = 180,
		LEFT = 90,
		RIGHT = -90
	};

	// Reset direction when player is not strafing
	is_bhopping = g_cmd->buttons & IN_JUMP;
	if (!is_bhopping && local_player->get_flags() & FL_ONGROUND) {
		calculated_direction = directions::FORWARDS;
		in_transition = false;
		return;
	}

	// Get true view angles
	QAngle base{ }; //Vector
	g_pEngine->GetViewAngles(base);

	// Calculate the rough direction closest to the player's true direction
	auto get_rough_direction = [&](float true_direction) -> float {
		// Make array with our four rough directions
		std::array< float, 4 > minimum = { directions::FORWARDS, directions::BACKWARDS, directions::LEFT, directions::RIGHT };
		float best_angle, best_delta = 181.f;

		// Loop through our rough directions and find which one is closest to our true direction
		for (size_t i = 0; i < minimum.size(); ++i) {
			float rough_direction = base.y + minimum.at(i);
			float delta = fabsf(math::NormalizeYaw(true_direction - rough_direction));

			// Only the smallest delta wins out
			if (delta < best_delta) {
				best_angle = rough_direction;
				best_delta = delta;
			}
		}

		return best_angle;
	};

	// Get true direction based on player velocity
	true_direction = local_player->get_velocity().Angle().y;

	// Detect wish direction based on movement keypresses
	if (g_cmd->buttons & IN_FORWARD) {
		wish_direction = base.y + directions::FORWARDS;
	}
	else if (g_cmd->buttons & IN_BACK) {
		wish_direction = base.y + directions::BACKWARDS;
	}
	else if (g_cmd->buttons & IN_MOVELEFT) {
		wish_direction = base.y + directions::LEFT;
	}
	else if (g_cmd->buttons & IN_MOVERIGHT) {
		wish_direction = base.y + directions::RIGHT;
	}
	else {
		// Reset to forward when not pressing a movement key, then fix anti-aim strafing by setting IN_FORWARD
		g_cmd->buttons |= IN_FORWARD;
		wish_direction = base.y + directions::FORWARDS;
	}
	// Calculate the ideal strafe rotation based on player speed (c) navewindre
	float speed_rotation = min(RAD2DEG(std::asinf(30.f / local_player->get_velocity().Length2D())) * 0.5f, 45.f);
	if (in_transition) {
		// Get value to rotate by via calculated speed rotation
		float ideal_step = speed_rotation + calculated_direction;
		step = fabsf(math::NormalizeYaw(calculated_direction - ideal_step)); // 15.f is a good alternative, but here is where you would do your "speed" slider value for the autostrafer

		// Check when the calculated direction arrives close to the wish direction
		if (fabsf(math::NormalizeYaw(wish_direction - calculated_direction)) > step) {
			float add = math::NormalizeYaw(calculated_direction + step);
			float sub = math::NormalizeYaw(calculated_direction - step);

			// Step in direction that gets us closer to our wish direction
			if (fabsf(math::NormalizeYaw(wish_direction - add)) >= fabsf(math::NormalizeYaw(wish_direction - sub))) {
				calculated_direction -= step;
			}
			else {
				calculated_direction += step;
			}
		}
		else {
			// Stop transitioning when we meet our wish direction
			in_transition = false;
		}
	}
	else {
		// Get rough direction and setup calculated direction only when not transitioning
		rough_direction = get_rough_direction(true_direction);
		calculated_direction = rough_direction;

		// When we have a difference between our current (rough) direction and our wish direction, then transition
		if (rough_direction != wish_direction) {
			in_transition = true;
		}
	}
	// Set movement up to be rotated
	g_cmd->forwardmove = 0.f;
	g_cmd->sidemove = g_cmd->command_number % 2 ? 450.f : -450.f;

	// Calculate ideal rotation based on our newly calculated direction
	float direction = (g_cmd->command_number % 2 ? speed_rotation : -speed_rotation) + calculated_direction;

	// Rotate our direction based on our new, defininite direction
	float rotation = DEG2RAD(base.y - direction);

	float cos_rot = cos(rotation);
	float sin_rot = sin(rotation);

	float forwardmove = (cos_rot * g_cmd->forwardmove) - (sin_rot * g_cmd->sidemove);
	float sidemove = (sin_rot * g_cmd->forwardmove) + (cos_rot * g_cmd->sidemove);

	// Apply newly rotated movement
	g_cmd->forwardmove = forwardmove;
	g_cmd->sidemove = sidemove;

}
