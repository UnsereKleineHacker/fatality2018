#include "../include_cheat.h"
#include <directxmath.h>

static bool trap = false;
int m_iRotate = 0;
int m_iRotateIteration = 0;

float m_flCurrentFeetYaw = 0.0f;
float m_flPreviousFeetYaw = 0.0f;

bool m_bAutomaticDir = false;
int m_iAutoDirection = 0;

void antiaim::pitch(int type)
{
	enum pitch_type {
		off = 0,
		down,
		up
	};

	if (!type) return;
	
	if (type == down)
	{
		g_cmd->viewangles.x = +89.f;
	}
	if (type == up)
	{
		g_cmd->viewangles.x = -89.f;
	}

}

void antiaim::pitch_setup()
{
	pitch((int)vars::aa.pitch_type.get<int>());
}

void antiaim::manual()
{
	static int side = 0;

}


void antiaim::yaw_desync(int type)
{
	enum yaw_desync_type {
		off = 0,
		backwards,
		staticd,
	};
	
	if (!type) return;
	if (type == backwards)
	{
		g_cmd->viewangles.y += 180.f;
	}
	if (type == staticd)
	{
		auto next_lby_update = []() -> bool
		{
			if (!g_pLocalPlayer)
				return false;


			static float next_lby_update_time = 0;
			float curtime = g_pGlobals->curtime;

			auto animstate = g_pLocalPlayer->get_anim_state();
			if (!animstate)
				return false;

			if (!(g_pLocalPlayer->get_flags() & FL_ONGROUND))
				return false;

			if (animstate->m_flSpeedNormalized > 0.1)
				next_lby_update_time = curtime + 0.22f;

			if (next_lby_update_time < curtime)
			{
				next_lby_update_time = curtime + 1.1f;
				return true;
			}

			return false;
		};

		static bool swap_bind = false;
		static bool init = false;

		if (GetKeyState(vars::aa.desync_swap.get<int>()))
		{
			if (init)
			{
				swap_bind = true;
			}
			init = false;
		}
		else
		{
			if (!init)
			{
				swap_bind = false;

			}
			init = true;

		}

		if (swap_bind)
		{
			g_cmd->viewangles.y -= (vars::aa.body_lean.get<int>() - 100);
			if (next_lby_update()) {
				if (g_send_packet) {
					g_cmd->viewangles.y = math::NormalizeYaw(g_cmd->viewangles.y);
				}
				g_send_packet = false;
				g_cmd->viewangles.y += 119.f;
			}
			else {
				if (g_send_packet) {
					g_cmd->viewangles.y += g_pLocalPlayer->GetDesyncDelta();
				}
				else {
					g_cmd->viewangles.y -= 119.f;
				}
			}
		}
		else
		{
			g_cmd->viewangles.y += (vars::aa.body_inverted.get<int>() - 100);
			if (next_lby_update()) {
				if (g_send_packet) {
					g_cmd->viewangles.y = math::NormalizeYaw(g_cmd->viewangles.y);
				}
				g_send_packet = false;
				g_cmd->viewangles.y -= 119.f;
			}
			else {
				if (g_send_packet) {
					g_cmd->viewangles.y -= g_pLocalPlayer->GetDesyncDelta();
				}
				else {
					g_cmd->viewangles.y += 119.f;
				}
			}
		}
	}
}

void antiaim::yaw_desync_setup()
{
	yaw_desync((int)vars::aa.desync_enable.get<int>());
}

inline float FastSqrt222(float x)
{
	unsigned int i = *(unsigned int*)&x;
	i += 127 << 23;
	i >>= 1;
	return *(float*)&i;
}
#define square( x ) ( x * x )
void ClampMovement2(CUserCmd* pCommand, float fMaxSpeed)
{
	if (fMaxSpeed <= 0.f)
		return;
	float fSpeed = (float)(FastSqrt222(square(pCommand->forwardmove) + square(pCommand->sidemove) + square(pCommand->upmove)));
	if (fSpeed <= 0.f)
		return;
	if (pCommand->buttons & IN_DUCK)
		fMaxSpeed *= 2.94117647f;
	if (fSpeed <= fMaxSpeed)
		return;
	float fRatio = fMaxSpeed / fSpeed;
	pCommand->forwardmove *= fRatio;
	pCommand->sidemove *= fRatio;
	pCommand->upmove *= fRatio;
}

void antiaim::run()
{
	if ( !g_pLocalPlayer->get_alive() || ( *g_ppGameRules )->m_bFreezePeriod() )
	{
		freestanding::get().last_fs_time = 0.f;
		return;
	}

	if ( !vars::aa.enabled.get<bool>()) return;

	
	fake_lag();

	auto anim_state = g_pLocalPlayer->get_anim_state();
	auto velocioty = Vector(anim_state->m_vVelocityX, anim_state->m_vVelocityY, 0);
	bool is_local_moving = velocioty.Length2D() > 0.1;
	bool is_local_standing = velocioty.Length2D() == 0;
	bool is_local_air = !(g_pLocalPlayer->get_flags() & FL_ONGROUND) && !(GetAsyncKeyState(vars::key.slowwalk.get<int>()));
	bool is_local_slow = (g_pLocalPlayer->get_flags() & FL_ONGROUND) && velocioty.Length2D() > 0.1 && (GetAsyncKeyState(vars::key.slowwalk.get<int>()));
	
	auto weapon = get_weapon( g_pLocalPlayer->get_active_weapon() );
	if (!weapon ||
		g_pLocalPlayer->get_move_type() == MOVETYPE_OBSERVER ||
		g_pLocalPlayer->get_move_type() == MOVETYPE_NOCLIP ||
		g_pLocalPlayer->get_move_type() == MOVETYPE_LADDER ||
		g_pLocalPlayer->get_flags() & FL_FROZEN )
	{
		return;
	}

	bool throw_nade = false;

	if (weapon->is_grenade())
		throw_nade = !weapon->get_pin_pulled() && weapon->get_throw_time() > 0.f && weapon->get_throw_time() < g_pGlobals->curtime;

	if (throw_nade || g_cmd->buttons & IN_USE)
	{
		return;
	}

	static bool hold;
	static bool omg;
	static bool inverter;
	if (GetAsyncKeyState(VK_XBUTTON2) && !hold)
	{
		inverter = !inverter;
		hold = true;
		if (!omg)
		{
			omg = true;
		}
	}
	else if (hold && !GetAsyncKeyState(VK_XBUTTON2))
	{
		omg = false;
		hold = false;
	}

	//here calls
	pitch_setup();
	yaw_desync_setup();
	choose_real();
}

void antiaim::fake_lag()
{


	unchoke = false;
	static auto onpeek_called = 0.f;

	to_choke = get_antiaim( type )->choke->get<int>();

	auto weapon = get_weapon( g_pLocalPlayer->get_active_weapon() );
	if ( weapon )
	{
		if ( vars::aa.choke_reload.get<bool>() && weapon->in_reload() )
			to_choke = 13;

		else if ( vars::aa.choke_weapon_switch.get<bool>() && g_pLocalPlayer->get_next_attack() > prediction::get_curtime() )
			to_choke = 13;
	}

	if ( onpeek_called >= g_pGlobals->curtime && onpeek_called < g_pGlobals->curtime + ticks_to_time( 15 ) )
		to_choke = 14;

	if ( get_fake_walk_state() )
		to_choke = 13;

	to_choke = std::clamp( to_choke, 1u, 15u );
	if ( ( *g_ppGameRules )->m_bIsValveDS() )
		to_choke = std::clamp( to_choke, 1u, 10u );

	*g_send_packet = g_pClientState->m_nChokedCommands >= to_choke;

	if (GetAsyncKeyState(vars::key.slowwalk.get<int>()))
	{
		if (g_pLocalPlayer->get_flags() & FL_ONGROUND)
			ClampMovement2(g_cmd, vars::aa.slowwalkamount.get<int>() * 2);
	}

	/*if (GetAsyncKeyState(vars::key.slowwalk.get<int>()))
	{
		float amount = 0.0034f * vars::aa.slowwalkamount.get<int>(); // options.misc.slow_walk_amount has 100 max value

		auto velocity = g_pLocalPlayer->get_velocity();
		QAngle direction;

		math::get().vector_angles(velocity, direction);

		float speed = velocity.Length2D();

		direction.y = g_cmd->viewangles.y - direction.y;

		Vector forward;

		math::get().angle_vectors(direction, &forward);

		Vector source = forward * -speed;

		if (speed >= (weapon->get_wpn_data()->flMaxSpeed * amount))
		{
			g_cmd->forwardmove = source.x;
			g_cmd->sidemove = source.y;
		}

	}*/

	

	if ( get_fake_walk_state() )
		return;

	if ( unchoke )
	{
		*g_send_packet = true;
		unchoke = false;
		return;
	}

	if ( *g_send_packet )
		onpeek_called = 0.f;

	if ( on_peek_fakelag() )
	{
		if ( *g_send_packet )
			return;

		if ( onpeek_called < g_pGlobals->curtime )
			onpeek_called = g_pGlobals->curtime + ticks_to_time( 14 );
		else if ( g_pClientState->m_nChokedCommands > 1 && onpeek_called < g_pGlobals->curtime )
			unchoke = true;
	}

	static int cnt = 0;
	static bool do_ = false;
	g_cmd->buttons |= IN_BULLRUSH;
	
	/*if (GetAsyncKeyState(VK_CONTROL))
	{
		*g_send_packet = false;
		if (cnt % 14 == 0)
			do_ = true;
		else if (cnt % 14 == 6)
			*g_send_packet = true;
		else if (cnt % 14 == 7)
			do_ = false;

		if (do_)
			g_cmd->buttons |= IN_DUCK;
		else
			g_cmd->buttons &= ~IN_DUCK;

		cnt++;
	}
	else {
		do_ = false;
		cnt = 0;
	}*/
}

void antiaim::choose_fk()
{
	fake_walk();

	fake_lag();

	if ( *g_send_packet )
		g_cmd->viewangles.y += RandomFlt( 130.f, 260.f );
}

bool antiaim::lby_update()
{
	if ( g_pClientState->m_nChokedCommands || !( g_pLocalPlayer->get_flags() & FL_ONGROUND ) )
		return false;

	const auto updated = update_lby;

	if ( update_lby )
	{
		if ( get_antiaim( type )->lby_random->get<bool>() )
		{
			const auto lby_random_range = get_antiaim( type )->lby_random_range->get<int>() * 0.5f;
			const auto lby_random_add = get_antiaim( type )->lby_random_add->get<int>();
			const auto lby = random_float( -lby_random_range, lby_random_range );
			initial_lby = g_cmd->viewangles.y + lby + lby_random_add;
		}

		auto angles = g_cmd->viewangles.y;

		target_lby = initial_lby;
		g_cmd->viewangles.y = initial_lby;
		g_cmd->viewangles.Clamp();
		update_lby = false;

		if ( secondupdate || !get_antiaim( type )->lby_static->get<bool>() )
		{
			if ( get_antiaim( type )->lby_static->get<bool>() )
			{
				initial_lby += -get_antiaim( type )->lby_delta_first->get<float>() + get_antiaim( type )->lby_delta->get<float>();
			}
			else
			{
				initial_lby = angles + get_antiaim( type )->lby_delta->get<float>();
			}
			secondupdate = false;
		}
	}

	return updated;
}

void antiaim::lby_prediction()
{
	const auto animstate = g_pLocalPlayer->get_anim_state();
	if ( !animstate )
		return;

	if ( g_pClientState->m_nChokedCommands )
		return;

	if ( animstate->m_velocity > 0.1f )
	{
		next_lby_update = prediction::get().get_curtime() + 0.22f;
		firstupdate = true;
	}
	else if ( prediction::get().get_curtime() > next_lby_update )
	{
		update_lby = true;
		next_lby_update = prediction::get().get_curtime() + 1.1f;
	}

	const auto get_add_by_choke = [ & ]() -> float
	{
		static auto max = 137.f;
		static auto min = 100.f;

		auto mult = 1.f / 0.2f * ticks_to_time( to_choke );

		return 100.f + ( max - min ) * mult;
	};

	if ( firstupdate && animstate->m_velocity <= 0.1f )
	{
		initial_lby = g_cmd->viewangles.y + get_antiaim( type )->lby_delta_first->get<float>();
		secondupdate = true;
		firstupdate = false;
	}

	if ( !firstupdate && prediction::get().get_curtime() + ticks_to_time( to_choke + 1 ) > next_lby_update
		 &&  fabsf( math::get().normalize_float( g_cmd->viewangles.y - initial_lby ) ) < get_add_by_choke() )
	{
		g_cmd->viewangles.y = initial_lby + get_add_by_choke();
	}


}

void antiaim::fix_movement( const QAngle wish_angle )
{
	if ( aimbot::get().get_quick_stop_state() )
		return;

	Vector view_fwd, view_right, view_up, cmd_fwd, cmd_right, cmd_up;
	auto viewangles = g_cmd->viewangles.Clamp();
	viewangles.Normalize();

	math::get().angle_vectors( wish_angle, &view_fwd, &view_right, &view_up );
	math::get().angle_vectors( viewangles, &cmd_fwd, &cmd_right, &cmd_up );

	const auto v8 = sqrtf( ( view_fwd.x * view_fwd.x ) + ( view_fwd.y * view_fwd.y ) );
	const auto v10 = sqrtf( ( view_right.x * view_right.x ) + ( view_right.y * view_right.y ) );
	const auto v12 = sqrtf( view_up.z * view_up.z );

	const Vector norm_view_fwd( ( 1.f / v8 ) * view_fwd.x, ( 1.f / v8 ) * view_fwd.y, 0.f );
	const Vector norm_view_right( ( 1.f / v10 ) * view_right.x, ( 1.f / v10 ) * view_right.y, 0.f );
	const Vector norm_view_up( 0.f, 0.f, ( 1.f / v12 ) * view_up.z );

	const auto v14 = sqrtf( ( cmd_fwd.x * cmd_fwd.x ) + ( cmd_fwd.y * cmd_fwd.y ) );
	const auto v16 = sqrtf( ( cmd_right.x * cmd_right.x ) + ( cmd_right.y * cmd_right.y ) );
	const auto v18 = sqrtf( cmd_up.z * cmd_up.z );

	const Vector norm_cmd_fwd( ( 1.f / v14 ) * cmd_fwd.x, ( 1.f / v14 ) * cmd_fwd.y, 0.f );
	const Vector norm_cmd_right( ( 1.f / v16 ) * cmd_right.x, ( 1.f / v16 ) * cmd_right.y, 0.f );
	const Vector norm_cmd_up( 0.f, 0.f, ( 1.f / v18 ) * cmd_up.z );

	const auto v22 = norm_view_fwd.x * g_cmd->forwardmove;
	const auto v26 = norm_view_fwd.y * g_cmd->forwardmove;
	const auto v28 = norm_view_fwd.z * g_cmd->forwardmove;
	const auto v24 = norm_view_right.x * g_cmd->sidemove;
	const auto v23 = norm_view_right.y * g_cmd->sidemove;
	const auto v25 = norm_view_right.z * g_cmd->sidemove;
	const auto v30 = norm_view_up.x * g_cmd->upmove;
	const auto v27 = norm_view_up.z * g_cmd->upmove;
	const auto v29 = norm_view_up.y * g_cmd->upmove;

	g_cmd->forwardmove = ( ( ( ( norm_cmd_fwd.x * v24 ) + ( norm_cmd_fwd.y * v23 ) ) + ( norm_cmd_fwd.z * v25 ) )
						   + ( ( ( norm_cmd_fwd.x * v22 ) + ( norm_cmd_fwd.y * v26 ) ) + ( norm_cmd_fwd.z * v28 ) ) )
		+ ( ( ( norm_cmd_fwd.y * v30 ) + ( norm_cmd_fwd.x * v29 ) ) + ( norm_cmd_fwd.z * v27 ) );
	g_cmd->sidemove = ( ( ( ( norm_cmd_right.x * v24 ) + ( norm_cmd_right.y * v23 ) ) + ( norm_cmd_right.z * v25 ) )
						+ ( ( ( norm_cmd_right.x * v22 ) + ( norm_cmd_right.y * v26 ) ) + ( norm_cmd_right.z * v28 ) ) )
		+ ( ( ( norm_cmd_right.x * v29 ) + ( norm_cmd_right.y * v30 ) ) + ( norm_cmd_right.z * v27 ) );
	g_cmd->upmove = ( ( ( ( norm_cmd_up.x * v23 ) + ( norm_cmd_up.y * v24 ) ) + ( norm_cmd_up.z * v25 ) )
					  + ( ( ( norm_cmd_up.x * v26 ) + ( norm_cmd_up.y * v22 ) ) + ( norm_cmd_up.z * v28 ) ) )
		+ ( ( ( norm_cmd_up.x * v30 ) + ( norm_cmd_up.y * v29 ) ) + ( norm_cmd_up.z * v27 ) );
}

void antiaim::fake_walk()
{
	static auto wasfakewalking = false;

	if ( !vars::aa.fakewalk.get<bool>() || !GetAsyncKeyState( vars::key.fakewalk.get<int>() ) )
	{
		wasfakewalking = false;
		return;
	}

	if ( !wasfakewalking && prediction::get().get_unpred_vel().Length2D() < 0.1f )
		wasfakewalking = true;
	if ( !wasfakewalking )
	{
		aimbot::get().quick_stop();
		return;
	}

	auto weapon = get_weapon( g_pLocalPlayer->get_active_weapon() );

	fake_walk_called = prediction::get().get_unpred_curtime();

	const auto choked = g_pClientState->m_nChokedCommands;
	auto newmax = ( ( *g_ppGameRules )->m_bIsValveDS() ? 6 : 8 );
	static auto max = newmax;
	if ( ( weapon->get_weapon_id() == WEAPON_SCAR20 || weapon->get_weapon_id() == WEAPON_G3SG1 || weapon->get_weapon_id() == WEAPON_AWP ) && g_pLocalPlayer->get_scoped() && !( g_pLocalPlayer->get_flags() & FL_DUCKING ) )
		newmax += 1;
	if ( g_pLocalPlayer->get_flags() & FL_DUCKING )
		newmax += 2;
	if ( g_pLocalPlayer->get_next_attack() > prediction::get().get_curtime() )
		newmax -= 1;
	if ( choked == to_choke || !choked || choked > max )
		aimbot::get().quick_stop();

	if ( !choked )
		max = newmax;


}

bool antiaim::get_fake_walk_state() const
{
	return fake_walk_called == prediction::get().get_unpred_curtime();
}

bool antiaim::on_peek_fakelag()
{
	if ( !vars::aa.on_peek.get<bool>() )
		return false;

	auto weapon = get_weapon( g_pLocalPlayer->get_active_weapon() );
	if ( weapon && weapon->get_item_definiton_index() == WEAPON_REVOLVER )
		return false;

	if ( g_pLocalPlayer->get_velocity().Length() < 2.f )
		return false;

	const auto time = prediction::get_curtime() + ticks_to_time( 3 );
	if ( !aimbot::get().can_shoot( time ) )
		return false;

	const auto mindmg = get_config( get_weapon( g_pLocalPlayer->get_active_weapon() ) )->mindmg->get<float>();

	auto newpos = g_pLocalPlayer->get_origin() + g_pLocalPlayer->get_velocity() * g_pGlobals->interval_per_tick * 3; //3 == prediction amount
	for ( auto i = 1; i < g_pGlobals->maxClients; i++ )
	{
		auto player = get_entity( i );
		if ( !player || !player->get_alive() || player->IsDormant() || !player->is_enemy() || player == g_pLocalPlayer || player->get_immunity() )
			continue;

		auto& log = player_log::get().get_log( i );
		if ( !log.m_bFilled || log.record.empty() )
			continue;

		auto player_damage = 0.f;
		aimbot::get().get_best_damage( log.record[ 0 ], player_damage, &newpos );

		if ( player_damage > mindmg )
			return true;
	}

	return false;
}

void antiaim::choose_real()
{
	const auto air = vars::aa.air.enabled.get<bool>() && !( g_pLocalPlayer->get_flags() & FL_ONGROUND );
	const auto moving = vars::aa.moving.enabled.get<bool>() && g_pLocalPlayer->get_flags() & FL_ONGROUND && prediction::get().get_unpred_vel().Length() > 0.1f;
	const auto fakewalking = vars::aa.fakewalk.get<bool>() && GetAsyncKeyState( vars::key.fakewalk.get<int>() );
	const auto regular = ( !air && !moving ) || ( fakewalking && !air );

	if ( air ) { type = 2; }
	if ( moving ) { type = 1; }
	if ( regular ) { type = 0; }

	if ( get_antiaim( type )->at_target->get<bool>() )
		do_at_target();

	if ( get_antiaim( type )->spin->get<bool>() )
		do_spin();

	if ( get_antiaim( type )->jitter->get<bool>() )
		do_jitter();

	do_add();

	if ( !get_antiaim( type )->edge->get<bool>() || !do_edge() )
		do_freestand();

	if ( get_antiaim( type )->lby->get<bool>() )
		do_lby();

	g_cmd->viewangles.x = 89.f;
}

void antiaim::do_spin() const
{
	const auto speed = get_antiaim( type )->spin_speed->get<int>();

	static auto add = 0.f;

	if ( add > get_antiaim( type )->spin_range->get<int>() )
	{
		add = 0.f;
	}

	if ( type == 2 && g_pLocalPlayer->get_flags() & FL_ONGROUND )
	{
		add = 0.f;
	}

	add += speed;
	g_cmd->viewangles.y += add;
}

void antiaim::do_add()
{
	const auto add = get_antiaim( type )->add->get<int>();

	g_cmd->viewangles.y += add;
}

void antiaim::do_jitter()
{
	auto jitter_range = get_antiaim( type )->jitter_range->get<int>() * 0.5f;
	const auto jitter_speed = get_antiaim( type )->jitter_speed->get<int>();

	static auto last_set_tick = 0;
	static auto flip = false;

	static auto add = 0.f;

	if ( last_set_tick + jitter_speed < g_pLocalPlayer->get_tickbase() || last_set_tick > g_pLocalPlayer->get_tickbase() )
	{
		last_set_tick = g_pLocalPlayer->get_tickbase();

		if ( get_antiaim( type )->jitter_random->get<int>() )
		{
			jitter_range = random_float( -jitter_range, jitter_range );
			flip = true;
		}

		add = flip ? jitter_range : -jitter_range;

		flip = !flip;
	}

	g_cmd->viewangles.y += add;
}

void antiaim::do_freestand()
{
	static auto yaw = 0.f;
	if ( freestanding::get().get_real( yaw ) )
		g_cmd->viewangles.y = yaw;
}

bool antiaim::do_edge()
{
	return false;
}

void antiaim::do_at_target() const
{
	C_CSPlayer* target = nullptr;
	QAngle target_angle;

	QAngle original_viewangles;
	g_pEngine->GetViewAngles( original_viewangles );

	auto lowest_fov = 90.f;
	for ( auto i = 1; i < g_pGlobals->maxClients; i++ )
	{
		auto player = get_entity( i );
		if ( !player || !player->get_alive() || !player->is_enemy() || player == g_pLocalPlayer )
			continue;

		if ( player->IsDormant() && ( player->get_simtime() > g_pGlobals->curtime || player->get_simtime() + 5.f < g_pGlobals->curtime ) )
			continue;

		auto enemy_pos = player->get_origin();
		enemy_pos.z += 64.f;

		const auto angle = math::get().calc_angle( g_pLocalPlayer->get_eye_pos(), enemy_pos );
		const auto fov = math::get().get_fov( original_viewangles, angle );

		if ( fov < lowest_fov )
		{
			target = player;
			lowest_fov = fov;
			target_angle = angle;
		}
	}

	if ( !target )
		return;

	g_cmd->viewangles.y = target_angle.y;
}

bool antiaim::do_lby()
{
	lby_prediction();

	return lby_update();
}