/**
 * @file cl_input.c
 * @brief Client input handling - bindable commands.
 * @note Continuous button event tracking is complicated by the fact that two different
 * input sources (say, mouse button 1 and the control key) can both press the
 * same button, but the button should only be released when both of the
 * pressing key have been released.
 *
 * When a key event issues a button command (+forward, +attack, etc), it appends
 * its key number as a parameter to the command so it can be matched up with
 * the release.
 *
 * Key_Event(unsigned int key, unsigned short unicode, qboolean down, unsigned time);
 *
 *  +mlook src time
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/cl_input.c
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_keys.h"
#include "cl_input.h"
#include "cl_joystick.h"
#include "cl_actor.h"
#include "cl_view.h"
#include "cl_map.h"
#include "menu/m_input.h"

#include "../renderer/r_sdl.h"
#include "../common/tracing.h"

/** @brief Grid position on the map for pending events */
extern pos3_t mousePendPos;

/* power of two please */
#define MAX_KEYQ 64

static struct {
	unsigned int key;
	unsigned short unicode;
	int down;
} keyq[MAX_KEYQ];

static int keyq_head = 0;
static int keyq_tail = 0;

camera_mode_t camera_mode;

static cvar_t *in_debug;

int mouseSpace;
int mousePosX, mousePosY;
static int oldMousePosX, oldMousePosY;

/**
 * rotate angles for menu models - pointer to menu node angles vec3_t
 * modify the node->angles values to rotate a model
 */
float *rotateAngles;
static qboolean wasCrouched = qfalse, doCrouch = qfalse;
static float crouchHt = 0.;

static qboolean cameraRoute = qfalse;
static vec3_t routeFrom, routeDelta;
static float routeDist;

/*
===============================================================================
KEY BUTTONS
===============================================================================
*/

typedef struct {
	int down[2];				/**< key nums holding it down */
	unsigned downtime;			/**< msec timestamp when the key was pressed down */
	unsigned msec;				/**< downtime for this key in msec (delta between pressed and released) */
	int state;					/**< 1 if down, 0 if not down */
} kbutton_t;

static kbutton_t in_turnleft, in_turnright, in_shiftleft, in_shiftright;
static kbutton_t in_shiftup, in_shiftdown;
static kbutton_t in_zoomin, in_zoomout;
static kbutton_t in_turnup, in_turndown;

/**
 * @brief
 * @sa IN_KeyUp
 * @sa CL_GetKeyMouseState
 * @sa CL_CameraMoveFirstPerson
 */
static void IN_KeyDown (kbutton_t * b)
{
	int k;
	const char *c = Cmd_Argv(1);

	if (c[0])
		k = atoi(c);
	else
		/* typed manually at the console for continuous down */
		k = -1;

	/* repeating key */
	if (k == b->down[0] || k == b->down[1])
		return;

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Printf("Three keys down for a button!\n");
		return;
	}

	/* still down */
	if (b->state)
		return;

	/* save timestamp */
	c = Cmd_Argv(2);
	b->downtime = atoi(c);
	if (!b->downtime)
		b->downtime = cls.realtime - 100;

	/* down */
	b->state = 1;
}

/**
 * @brief
 * @sa IN_KeyDown
 * @sa CL_GetKeyMouseState
 * @sa CL_CameraMoveFirstPerson
 */
static void IN_KeyUp (kbutton_t * b)
{
	int k;
	unsigned uptime;
	const char *c = Cmd_Argv(1);

	if (c[0])
		k = atoi(c);
	/* typed manually at the console, assume for unsticking, so clear all */
	else {
		b->down[0] = b->down[1] = 0;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	/* key up without coresponding down (menu pass through) */
	else
		return;

	/* some other key is still holding it down */
	if (b->down[0] || b->down[1])
		return;

	/* still up (this should not happen) */
	if (!b->state)
		return;

	/* save timestamp */
	c = Cmd_Argv(2);
	uptime = atoi(c);
	if (uptime)
		b->msec += uptime - b->downtime;
	else
		b->msec += 10;

	/* now up */
	b->state = 0;
}

static void IN_TurnLeftDown_f (void)
{
	IN_KeyDown(&in_turnleft);
}
static void IN_TurnLeftUp_f (void)
{
	IN_KeyUp(&in_turnleft);
}
static void IN_TurnRightDown_f (void)
{
	IN_KeyDown(&in_turnright);
}
static void IN_TurnRightUp_f (void)
{
	IN_KeyUp(&in_turnright);
}
static void IN_TurnUpDown_f (void)
{
	IN_KeyDown(&in_turnup);
}
static void IN_TurnUpUp_f (void)
{
	IN_KeyUp(&in_turnup);
}
static void IN_TurnDownDown_f (void)
{
	IN_KeyDown(&in_turndown);
}
static void IN_TurnDownUp_f (void)
{
	IN_KeyUp(&in_turndown);
}
static void IN_ShiftLeftDown_f (void)
{
	IN_KeyDown(&in_shiftleft);
}
static void IN_ShiftLeftUp_f (void)
{
	IN_KeyUp(&in_shiftleft);
}
static void IN_ShiftRightDown_f (void)
{
	IN_KeyDown(&in_shiftright);
}
static void IN_ShiftRightUp_f (void)
{
	IN_KeyUp(&in_shiftright);
}
static void IN_ShiftUpDown_f (void)
{
	IN_KeyDown(&in_shiftup);
}
static void IN_ShiftUpUp_f (void)
{
	IN_KeyUp(&in_shiftup);
}
static void IN_ShiftDownDown_f (void)
{
	IN_KeyDown(&in_shiftdown);
}
static void IN_ShiftDownUp_f (void)
{
	IN_KeyUp(&in_shiftdown);
}
static void IN_ZoomInDown_f (void)
{
	IN_KeyDown(&in_zoomin);
}
static void IN_ZoomInUp_f (void)
{
	IN_KeyUp(&in_zoomin);
}
static void IN_ZoomOutDown_f (void)
{
	IN_KeyDown(&in_zoomout);
}
static void IN_ZoomOutUp_f (void)
{
	IN_KeyUp(&in_zoomout);
}

/**
 * @brief Switches camera mode between remote and firstperson
 */
static void CL_CameraMode_f (void)
{
	if (!selActor)
		return;
	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);
	else
		CL_CameraModeChange(CAMERA_MODE_FIRSTPERSON);
}

void CL_CameraModeChange (camera_mode_t new_camera_mode)
{
	static vec3_t save_camorg, save_camangles;
	static float save_camzoom;
	static int save_level;

	/* no camera change if this is not our round */
	if (cls.team != cl.actTeam)
		return;

	/* save remote camera position, angles, zoom */
	if (camera_mode == CAMERA_MODE_REMOTE) {
		VectorCopy(cl.cam.camorg, save_camorg);
		VectorCopy(cl.cam.angles, save_camangles);
		save_camzoom = cl.cam.zoom;
		save_level = cl_worldlevel->value;
	}

	/* toggle camera mode */
	if (new_camera_mode == CAMERA_MODE_REMOTE) {
		Com_Printf("Changed camera mode to remote.\n");
		camera_mode = CAMERA_MODE_REMOTE;
		VectorCopy(save_camorg, cl.cam.camorg);
		VectorCopy(save_camangles, cl.cam.angles);
		cl.cam.zoom = save_camzoom;
		V_CalcFovX();
		Cvar_SetValue("cl_worldlevel", save_level);
	} else if (!cl_isometric->integer) {
		Com_Printf("Changed camera mode to first-person.\n");
		camera_mode = CAMERA_MODE_FIRSTPERSON;
		VectorCopy(selActor->origin, cl.cam.camorg);
		Cvar_SetValue("cl_worldlevel", cl.map_maxlevel);
		VectorCopy(selActor->angles, cl.cam.angles);
		refdef.fov_x = FOV_FPS;
		cl.cam.zoom = 1.0;
		wasCrouched = selActor->state & STATE_CROUCHED;
		crouchHt = 0.;
	} else {
		Com_Printf("You can't change to first person mode in isometric mode\n");
	}
}

const float MIN_ZOOM = 0.5;
const float MAX_ZOOM = 32.0;

static cvar_t *cl_camrotspeed;
static cvar_t *cl_cammovespeed;
static cvar_t *cl_cammoveaccel;
static cvar_t *cl_campitchmax;
static cvar_t *cl_campitchmin;
static cvar_t *cl_camzoomquant;
static cvar_t *cl_camzoommax;
cvar_t *cl_camzoommin;

cvar_t *cl_mapzoommax;
cvar_t *cl_mapzoommin;

#define MIN_CAMROT_SPEED	50
#define MIN_CAMROT_ACCEL	50
#define MAX_CAMROT_SPEED	1000
#define MAX_CAMROT_ACCEL	1000
#define MIN_CAMMOVE_SPEED	150
#define MIN_CAMMOVE_ACCEL	150
#define MAX_CAMMOVE_SPEED	3000
#define MAX_CAMMOVE_ACCEL	3000
#define ZOOM_SPEED			2.0
#define MIN_CAMZOOM_QUANT	0.05
#define MAX_CAMZOOM_QUANT	1.0
#define LEVEL_SPEED			3.0
#define LEVEL_MIN			0.05

/*========================================================================== */

/**
 * @brief Switch one worldlevel up
 */
static void CL_LevelUp_f (void)
{
	if (!CL_OnBattlescape())
		return;
	if (camera_mode != CAMERA_MODE_FIRSTPERSON)
		Cvar_SetValue("cl_worldlevel", (cl_worldlevel->integer < cl.map_maxlevel - 1) ? cl_worldlevel->integer + 1 : cl.map_maxlevel - 1);
}

/**
 * @brief Switch on worldlevel down
 */
static void CL_LevelDown_f (void)
{
	if (!CL_OnBattlescape())
		return;
	if (camera_mode != CAMERA_MODE_FIRSTPERSON)
		Cvar_SetValue("cl_worldlevel", (cl_worldlevel->integer > 0) ? cl_worldlevel->integer - 1 : 0);
}


static void CL_ZoomInQuant_f (void)
{
	if (mouseSpace == MS_MENU)
		MN_MouseWheel(qfalse, mousePosX, mousePosY);
	else {
		float quant;

		/* no zooming in first person mode */
		if (camera_mode == CAMERA_MODE_FIRSTPERSON)
			return;

		/* check zoom quant */
		if (cl_camzoomquant->value < MIN_CAMZOOM_QUANT)
			quant = 1 + MIN_CAMZOOM_QUANT;
		else if (cl_camzoomquant->value > MAX_CAMZOOM_QUANT)
			quant = 1 + MAX_CAMZOOM_QUANT;
		else
			quant = 1 + cl_camzoomquant->value;

		/* change zoom */
		cl.cam.zoom *= quant;

		/* ensure zoom doesn't exceed either MAX_ZOOM or cl_camzoommax */
		cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
		V_CalcFovX();
	}
}

static void CL_ZoomOutQuant_f (void)
{
	if (mouseSpace == MS_MENU)
		MN_MouseWheel(qtrue, mousePosX, mousePosY);
	else {
		float quant;

		/* no zooming in first person mode */
		if (camera_mode == CAMERA_MODE_FIRSTPERSON)
			return;

		/* check zoom quant */
		if (cl_camzoomquant->value < MIN_CAMZOOM_QUANT)
			quant = 1 + MIN_CAMZOOM_QUANT;
		else if (cl_camzoomquant->value > MAX_CAMZOOM_QUANT)
			quant = 1 + MAX_CAMZOOM_QUANT;
		else
			quant = 1 + cl_camzoomquant->value;

		/* change zoom */
		cl.cam.zoom /= quant;

		/* ensure zoom isnt less than either MIN_ZOOM or cl_camzoommin */
		cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
		V_CalcFovX();
	}
}

/**
 * @brief returns the weapon the actor's left hand is touching
 */
static invList_t* CL_GetLeftHandWeapon (le_t *actor)
{
	invList_t *weapon = LEFT(actor);

	if (!weapon || !weapon->item.m) {
		weapon = RIGHT(actor);
		if (!weapon->item.t->holdTwoHanded)
			weapon = NULL;
	}

	return weapon;
}

/**
 * @brief Executes "pending" actions such as walking and firing.
 * @note Manually triggered by the player when hitting the "confirm" button.
 */
static void CL_ConfirmAction_f (void)
{
	if (!selActor)
		return;

	switch (cl.cmode) {
	case M_PEND_MOVE:
		CL_ActorStartMove(selActor, mousePendPos);
		break;
	case M_PEND_FIRE_R:
	case M_PEND_FIRE_L:
		CL_ActorShoot(selActor, mousePendPos);
		/** @todo this might've broken animation choosing in cl_actor:CL_ActorDoShoot and CL_ActorStartShoot. */
		/* cl.cmode = M_MOVE; */
		break;
	default:
		break;
	}
}


/**
 * @brief Reload left weapon.
 */
static void CL_ReloadLeft_f (void)
{
	if (!selActor || !CL_CheckMenuAction(selActor->TU, CL_GetLeftHandWeapon(selActor), EV_INV_RELOAD))
		return;
	CL_ActorReload(csi.idLeft);
}

/**
 * @brief Reload right weapon.
 */
static void CL_ReloadRight_f (void)
{
	if (!selActor || !CL_CheckMenuAction(selActor->TU, RIGHT(selActor), EV_INV_RELOAD))
		 return;
	CL_ActorReload(csi.idRight);
}


/**
 * @brief Left mouse click
 */
static void CL_SelectDown_f (void)
{
	menu_t* menu;
	if (mouseSpace != MS_WORLD)
		return;
	CL_ActorSelectMouse();
	/* we clicked outside the world but not onto a menu */
	/* get the current menu */
	menu = MN_GetActiveMenu();
	if (menu && menu->leaveNode)
		MN_ExecuteActions(menu, menu->leaveNode->click);
}

static void CL_SelectUp_f (void)
{
	mouseSpace = MS_NULL;
}

/**
 * @brief Mouse click
 */
static void CL_ActionDown_f (void)
{
	if (mouseSpace != MS_WORLD)
		return;
	CL_ActorActionMouse();
}

static void CL_ActionUp_f (void)
{
	mouseSpace = MS_NULL;
}

/**
 * @brief Turn button is hit
 */
static void CL_TurnDown_f (void)
{
	if (mouseSpace == MS_WORLD)
		CL_ActorTurnMouse();
}

static void CL_TurnUp_f (void)
{
	mouseSpace = MS_NULL;
	/* leave the fire mode when turning around - not everybody has a mmb mouse */
	cl.cmode = M_MOVE;
}

static void CL_HudRadarDown_f (void)
{
	const menu_t* menu = MN_GetMenu(mn_hud->string);
	menuNode_t *hudRadarNode;

	if (!menu)
		return;

	hudRadarNode = MN_GetNode(menu, "hudradar");
	if (hudRadarNode)
		MN_UnHideNode(hudRadarNode);
	else
		Com_Printf("Could not get a hudradar node from menu: '%s'\n", menu->name);
}

static void CL_HudRadarUp_f (void)
{
	const menu_t* menu = MN_GetMenu(mn_hud->string);
	menuNode_t *hudRadarNode;

	if (!menu)
		return;

	hudRadarNode = MN_GetNode(menu, "hudradar");
	if (hudRadarNode)
		MN_HideNode(hudRadarNode);
}

/**
 * @brief Right mouse button is hit in menu
 */
static void CL_RightClickDown_f (void)
{
	if (mouseSpace != MS_MENU)
		return;
	MN_RightClick(mousePosX, mousePosY);
}

/**
 * @brief Right mouse button is freed in menu
 */
static void CL_RightClickUp_f (void)
{
	mouseSpace = MS_NULL;
}

/**
 * @brief Middle mouse button is hit in menu
 */
static void CL_MiddleClickDown_f (void)
{
	if (mouseSpace == MS_MENU)
		MN_MiddleClick(mousePosX, mousePosY);
}

/**
 * @brief Middle mouse button is freed in menu
 */
static void CL_MiddleClickUp_f (void)
{
	mouseSpace = MS_NULL;
}

/**
 * @brief Left mouse button is hit in menu
 */
static void CL_LeftClickDown_f (void)
{
	switch (mouseSpace) {
	case MS_MENU:
		MN_LeftClick(mousePosX, mousePosY);
		break;
	default:
		/* we clicked outside the world but not onto a menu */
		if (cls.state == ca_active) {
			/* get the current menu */
			const menu_t *menu = MN_GetActiveMenu();
			if (menu && menu->leaveNode)
				MN_ExecuteActions(menu, menu->leaveNode->click);
		}
		break;
	}
}

/**
 * @brief Left mouse button is freed in menu
 */
static void CL_LeftClickUp_f (void)
{
	if (mouseSpace == MS_DRAGITEM)
		MN_LeftClick(mousePosX, mousePosY);
	mouseSpace = MS_NULL;
}

/**
 * @brief Draws a line to each alien the selected 'watcher' can see.
 * @sa CL_NextAlienVisibleFromActor_f (Shares quite some code)
 * @todo Save info about displayed traces and combine this function with CL_NextAlienActor_f.
 */
static void CL_DrawSpottedLines_f (void)
{
	le_t *watcher; /** @todo make this a parameter for use in other functions? */
	int i;
	trace_t tr;
	vec3_t from, at;

	if (!selActor)
		return;

	watcher = selActor;

	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);

	for (i = 0; i < numLEs; i++) {
		const le_t *le = &LEs[i];
		if (le->inuse && LE_IsLivingActor(le) && le->team != cls.team
		 && le->team != TEAM_CIVILIAN) {
			/* not facing in the direction of the 'target' */
			if (!FrustumVis(watcher->origin, watcher->dir, le->origin))
				continue;
			VectorCopy(watcher->origin, from);
			VectorCopy(le->origin, at);
			/* actor eye height */
			if (watcher->state & STATE_CROUCHED)
				from[2] += EYE_HT_CROUCH;
			else
				from[2] += EYE_HT_STAND;
			/* target height */
			if (le->state & STATE_CROUCHED)
				at[2] += EYE_HT_CROUCH; /** @todo */
			else
				at[2] += UNIT_HEIGHT; /* full unit */
			/** @todo maybe doing more than one trace to different target heights */
			tr = CL_Trace(from, at, vec3_origin, vec3_origin, watcher, NULL, MASK_SOLID);
			/* trace didn't reach the target - something was hit before */
			if (tr.fraction < 1.0)
				continue;
			/* draw line from watcher to le */
			CL_DrawLineOfSight(watcher, le);
		}
	}
}

static int lastAlien = 0;

/**
 * @brief Cycles between visible (to selected actor) aliens.
 * @sa CL_DrawSpottedLines_f (Shares quite some code)
 * @sa CL_NextAlien_f
 */
static void CL_NextAlienVisibleFromActor_f (void)
{
	le_t *watcher; /** @todo make this a parameter for use in other functions? */
	le_t *le;
	int i;
	trace_t tr;
	vec3_t from, at;

	if (!selActor)
		return;

	watcher = selActor;

	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);

	if (lastAlien >= numLEs)
		lastAlien = 0;

	i = lastAlien;
	do {
		if (++i >= numLEs)
			i = 0;
		le = &LEs[i];
		if (le->inuse && LE_IsLivingActor(le) && le->team != cls.team
		 && le->team != TEAM_CIVILIAN) {
			VectorCopy(watcher->origin, from);
			VectorCopy(le->origin, at);
			/* actor eye height */
			if (watcher->state & STATE_CROUCHED)
				from[2] += EYE_HT_CROUCH;
			else
				from[2] += EYE_HT_STAND;
			/* target height */
			if (le->state & STATE_CROUCHED)
				at[2] += EYE_HT_CROUCH; /** @todo */
			else
				at[2] += UNIT_HEIGHT; /* full unit */
			/** @todo check the facing of the actor: watcher->dir
			 * maybe doing more than one trace to different target heights */
			tr = CL_Trace(from, at, vec3_origin, vec3_origin, watcher, NULL, MASK_SOLID);
			/* trace didn't reach the target - something was hit before */
			if (tr.fraction < 1.0)
				continue;

			lastAlien = i;
			V_CenterView(le->pos);
			return;
		}
	} while (i != lastAlien);
}

/**
 * @brief Cycles between visible aliens
 * @sa CL_NextAlienVisibleFromActor_f
 */

static void CL_NextAlien_f (void)
{
	le_t *le;
	int i;

	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraModeChange(CAMERA_MODE_REMOTE);
	if (lastAlien >= numLEs)
		lastAlien = 0;

	i = lastAlien;
	do {
		if (++i >= numLEs)
			i = 0;
		le = &LEs[i];
		if (le->inuse && LE_IsLivingActor(le) && le->team != cls.team
		 && le->team != TEAM_CIVILIAN) {
			lastAlien = i;
			V_CenterView(le->pos);
			return;
		}
	} while (i != lastAlien);
}

/*========================================================================== */

#ifdef DEBUG
/**
 * @brief Prints the current camera angles
 * @note Only available in debug mode
 * Accessable via console command camangles
 */
static void CL_CamPrintAngles_f (void)
{
	Com_Printf("camera angles %0.3f:%0.3f:%0.3f\n", cl.cam.angles[0], cl.cam.angles[1], cl.cam.angles[2]);
}
#endif /* DEBUG */

static void CL_CamSetAngles_f (void)
{
	int c = Cmd_Argc();

	if (c < 3) {
		Com_Printf("Usage %s <value> <value>\n", Cmd_Argv(0));
		return;
	}

	cl.cam.angles[PITCH] = atof(Cmd_Argv(1));
	cl.cam.angles[YAW] = atof(Cmd_Argv(2));
	cl.cam.angles[ROLL] = 0.0f;
}

static void CL_CamSetZoom_f (void)
{
	int c = Cmd_Argc();

	if (c < 2) {
		Com_Printf("Usage %s <value>\n", Cmd_Argv(0));
		return;
	}

	Com_Printf("old zoom value: %.2f\n", cl.cam.zoom);
	cl.cam.zoom = atof(Cmd_Argv(1));
	cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
	cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
}

/**
 * @brief Makes a mapshot - called by basemapshot script command
 * @note Load a basemap and execute 'basemapshot' in console
 */
static void CL_MakeBaseMapShot_f (void)
{
	if (cls.state != ca_active) {
		Com_Printf("Load the base map before you try to use this function\n");
		return;
	}

	cl.cam.angles[0] = 60.0f;
	cl.cam.angles[1] = 90.0f;
	Cvar_SetValue("r_isometric", 1);
	/* we are interested in the second level only */
	Cvar_SetValue("cl_worldlevel", 1);
	MN_PushMenu("nohud");
	/* hide any active console */
	Key_SetDest(key_game);
	Cmd_ExecuteString("r_screenshot tga");
}

#define STATE_FORWARD	1
#define STATE_RIGHT		2
#define STATE_ZOOM		3
#define STATE_ROT		4
#define STATE_TILT		5

#define SCROLL_BORDER	4

/**
 * @note see SCROLL_BORDER define
 */
static float CL_GetKeyMouseState (int dir)
{
	float value;

	switch (dir) {
	case STATE_FORWARD:
		value = (in_shiftup.state & 1) + (mousePosY <= SCROLL_BORDER) - (in_shiftdown.state & 1) - (mousePosY >= VID_NORM_HEIGHT - SCROLL_BORDER);
		break;
	case STATE_RIGHT:
		value = (in_shiftright.state & 1) + (mousePosX >= VID_NORM_WIDTH - SCROLL_BORDER) - (in_shiftleft.state & 1) - (mousePosX <= SCROLL_BORDER);
		break;
	case STATE_ZOOM:
		value = (in_zoomin.state & 1) - (in_zoomout.state & 1);
		break;
	case STATE_ROT:
		value = (in_turnleft.state & 1) - (in_turnright.state & 1);
		break;
	case STATE_TILT:
		value = (in_turnup.state & 1) - (in_turndown.state & 1);
		break;
	default:
		value = 0.0;
		break;
	}

	return value;
}

/**
 * @sa CL_CameraMove
 */
static void CL_CameraMoveFirstPerson (void)
{
	const float rotation_speed =
		(cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ((cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED) : MIN_CAMROT_SPEED;

	/* look left */
	if ((in_turnleft.state & 1) && ((cl.cam.angles[YAW] - selActor->angles[YAW]) < 90))
		cl.cam.angles[YAW] += cls.frametime * rotation_speed;

	/* look right */
	if ((in_turnright.state & 1) && ((selActor->angles[YAW] - cl.cam.angles[YAW]) < 90))
		cl.cam.angles[YAW] -= cls.frametime * rotation_speed;

	/* look down */
	if ((in_turndown.state & 1) && (cl.cam.angles[PITCH] < 45))
		cl.cam.angles[PITCH] += cls.frametime * rotation_speed;

	/* look up */
	if ((in_turnup.state & 1) && (cl.cam.angles[PITCH] > -45))
		cl.cam.angles[PITCH] -= cls.frametime * rotation_speed;

	/* ensure camera position matches actors origin */
	VectorCopy(selActor->origin, cl.cam.camorg);

	/* check if actor is starting a crouch/stand action */
	if (wasCrouched != (selActor->state & STATE_CROUCHED)) {
		doCrouch = qtrue;
		crouchHt = 0.;
		wasCrouched = selActor->state & STATE_CROUCHED;
	}

	/* adjust camera if actor is standing vs crouching */
	if (doCrouch) {
		if (selActor->state & STATE_CROUCHED) {
			cl.cam.camorg[2] += EYE_HT_STAND;
			crouchHt += 8. * cls.frametime;
			cl.cam.camorg[2] -= crouchHt;
			if (cl.cam.camorg[2] < selActor->origin[2] + EYE_HT_CROUCH) {
				cl.cam.camorg[2] = selActor->origin[2] + EYE_HT_CROUCH;
				doCrouch = qfalse;
			}
		} else {
			cl.cam.camorg[2] += EYE_HT_CROUCH;
			crouchHt += 8. * cls.frametime;
			cl.cam.camorg[2] += crouchHt;
			if (cl.cam.camorg[2] > selActor->origin[2] + EYE_HT_STAND) {
				cl.cam.camorg[2] = selActor->origin[2] + EYE_HT_STAND;
				doCrouch = qfalse;
			}
		}
	} else {
		if (selActor->state & STATE_CROUCHED)
			cl.cam.camorg[2] += EYE_HT_CROUCH;
		else
			cl.cam.camorg[2] += EYE_HT_STAND;
	}

	/* move camera forward horizontally to where soldier eyes are */
	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);
	cl.cam.axis[0][2] = 0.0;
	VectorMA(cl.cam.camorg, 5.0, cl.cam.axis[0], cl.cam.camorg);
}

/**
 * @brief forces the camera to stay within the horizontal bounds of the
 * map plus some border
 */
static inline void CL_ClampCamToMap (const float border)
{
	if (cl.cam.origin[0] < map_min[0] - border)
		cl.cam.origin[0] = map_min[0] - border;
	else if (cl.cam.origin[0] > map_max[0] + border)
		cl.cam.origin[0] = map_max[0] + border;

	if (cl.cam.origin[1] < map_min[1] - border)
		cl.cam.origin[1] = map_min[1] - border;
	else if (cl.cam.origin[1] > map_max[1] + border)
		cl.cam.origin[1] = map_max[1] + border;
}

/**
 * @sa CL_CameraMove
 */
static void CL_CameraMoveRemote (void)
{
	float frac;
	vec3_t g_forward, g_right, g_up;
	vec3_t delta;
	int i;

	/* get relevant variables */
	const float rotspeed =
		(cl_camrotspeed->value > MIN_CAMROT_SPEED) ? ((cl_camrotspeed->value < MAX_CAMROT_SPEED) ? cl_camrotspeed->value : MAX_CAMROT_SPEED) : MIN_CAMROT_SPEED;
	const float movespeed =
		(cl_cammovespeed->value > MIN_CAMMOVE_SPEED) ?
		((cl_cammovespeed->value < MAX_CAMMOVE_SPEED) ? cl_cammovespeed->value / cl.cam.zoom : MAX_CAMMOVE_SPEED / cl.cam.zoom) : MIN_CAMMOVE_SPEED / cl.cam.zoom;
	const float moveaccel =
		(cl_cammoveaccel->value > MIN_CAMMOVE_ACCEL) ?
		((cl_cammoveaccel->value < MAX_CAMMOVE_ACCEL) ? cl_cammoveaccel->value / cl.cam.zoom : MAX_CAMMOVE_ACCEL / cl.cam.zoom) : MIN_CAMMOVE_ACCEL / cl.cam.zoom;

	/* calculate camera omega */
	/* stop acceleration */
	frac = cls.frametime * moveaccel * 2.5;

	for (i = 0; i < 2; i++) {
		if (fabs(cl.cam.omega[i]) > frac) {
			if (cl.cam.omega[i] > 0)
				cl.cam.omega[i] -= frac;
			else
				cl.cam.omega[i] += frac;
		} else
			cl.cam.omega[i] = 0;

		/* rotational acceleration */
		if (i == YAW)
			cl.cam.omega[i] += CL_GetKeyMouseState(STATE_ROT) * frac * 2;
		else
			cl.cam.omega[i] += CL_GetKeyMouseState(STATE_TILT) * frac * 2;

		if (cl.cam.omega[i] > rotspeed)
			cl.cam.omega[i] = rotspeed;
		if (-cl.cam.omega[i] > rotspeed)
			cl.cam.omega[i] = -rotspeed;
	}

	cl.cam.omega[ROLL] = 0;
	/* calculate new camera angles for this frame */
	VectorMA(cl.cam.angles, cls.frametime, cl.cam.omega, cl.cam.angles);

	if (cl.cam.angles[PITCH] > cl_campitchmax->value)
		cl.cam.angles[PITCH] = cl_campitchmax->value;
	if (cl.cam.angles[PITCH] < cl_campitchmin->value)
		cl.cam.angles[PITCH] = cl_campitchmin->value;

	AngleVectors(cl.cam.angles, cl.cam.axis[0], cl.cam.axis[1], cl.cam.axis[2]);

	/* camera route overrides user input */
	if (cameraRoute) {
		/* camera route */
		frac = cls.frametime * moveaccel * 2;
		if (VectorDist(cl.cam.origin, routeFrom) > routeDist - 200) {
			VectorMA(cl.cam.speed, -frac, routeDelta, cl.cam.speed);
			VectorNormalize2(cl.cam.speed, delta);
			if (DotProduct(delta, routeDelta) < 0.05) {
				CL_UnblockEvents();
				cameraRoute = qfalse;
			}
		} else
			VectorMA(cl.cam.speed, frac, routeDelta, cl.cam.speed);
	} else {
		/* normal camera movement */
		/* calculate ground-based movement vectors */
		const float angle = cl.cam.angles[YAW] * (M_PI * 2 / 360);
		const float sy = sin(angle);
		const float cy = cos(angle);

		VectorSet(g_forward, cy, sy, 0.0);
		VectorSet(g_right, sy, -cy, 0.0);
		VectorSet(g_up, 0.0, 0.0, 1.0);

		/* calculate camera speed */
		/* stop acceleration */
		frac = cls.frametime * moveaccel;
		if (VectorLength(cl.cam.speed) > frac) {
			VectorNormalize2(cl.cam.speed, delta);
			VectorMA(cl.cam.speed, -frac, delta, cl.cam.speed);
		} else
			VectorClear(cl.cam.speed);

		/* acceleration */
		frac = cls.frametime * moveaccel * 3.5;
		VectorClear(delta);
		VectorScale(g_forward, CL_GetKeyMouseState(STATE_FORWARD), delta);
		VectorMA(delta, CL_GetKeyMouseState(STATE_RIGHT), g_right, delta);
		VectorNormalize(delta);
		VectorMA(cl.cam.speed, frac, delta, cl.cam.speed);

		/* lerp the level change */
		if (cl.cam.lerplevel < cl_worldlevel->value) {
			cl.cam.lerplevel += LEVEL_SPEED * (cl_worldlevel->value - cl.cam.lerplevel + LEVEL_MIN) * cls.frametime;
			if (cl.cam.lerplevel > cl_worldlevel->value)
				cl.cam.lerplevel = cl_worldlevel->value;
		} else if (cl.cam.lerplevel > cl_worldlevel->value) {
			cl.cam.lerplevel -= LEVEL_SPEED * (cl.cam.lerplevel - cl_worldlevel->value + LEVEL_MIN) * cls.frametime;
			if (cl.cam.lerplevel < cl_worldlevel->value)
				cl.cam.lerplevel = cl_worldlevel->value;
		}
	}

	/* clamp speed */
	frac = VectorLength(cl.cam.speed) / movespeed;
	if (frac > 1.0)
		VectorScale(cl.cam.speed, 1.0 / frac, cl.cam.speed);

	/* zoom change */
	frac = CL_GetKeyMouseState(STATE_ZOOM);
	if (frac > 0.1) {
		cl.cam.zoom *= 1.0 + cls.frametime * ZOOM_SPEED * frac;
		/* ensure zoom isn't greater than either MAX_ZOOM or cl_camzoommax */
		cl.cam.zoom = min(min(MAX_ZOOM, cl_camzoommax->value), cl.cam.zoom);
	} else if (frac < -0.1) {
		cl.cam.zoom /= 1.0 - cls.frametime * ZOOM_SPEED * frac;
		/* ensure zoom isn't less than either MIN_ZOOM or cl_camzoommin */
		cl.cam.zoom = max(max(MIN_ZOOM, cl_camzoommin->value), cl.cam.zoom);
	}
	V_CalcFovX();

	/* calc new camera reference and new camera real origin */
	VectorMA(cl.cam.origin, cls.frametime, cl.cam.speed, cl.cam.origin);
	cl.cam.origin[2] = 0.;
	if (cl_isometric->integer) {
		CL_ClampCamToMap(72.);
		VectorMA(cl.cam.origin, -CAMERA_START_DIST + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT, cl.cam.axis[0], cl.cam.camorg);
		cl.cam.camorg[2] += CAMERA_START_HEIGHT + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;
	} else {
		CL_ClampCamToMap(min(144. * (cl.cam.zoom - cl_camzoommin->value - 0.4) / cl_camzoommax->value, 86));
		VectorMA(cl.cam.origin, -CAMERA_START_DIST / cl.cam.zoom , cl.cam.axis[0], cl.cam.camorg);
		cl.cam.camorg[2] += CAMERA_START_HEIGHT / cl.cam.zoom + cl.cam.lerplevel * CAMERA_LEVEL_HEIGHT;
	}
}

/**
 * @sa CL_CameraMoveRemote
 * @sa CL_CameraMoveFirstPerson
 */
void CL_CameraMove (void)
{
	if (cls.state != ca_active)
		return;

	if (!viddef.viewWidth || !viddef.viewHeight)
		return;

	if (camera_mode == CAMERA_MODE_FIRSTPERSON)
		CL_CameraMoveFirstPerson();
	else
		CL_CameraMoveRemote();
}

/**
 * @sa CL_BlockEvents
 * @sa CL_CameraMove
 */
void CL_CameraRoute (pos3_t from, pos3_t target)
{
	/* initialize the camera route variables */
	PosToVec(from, routeFrom);
	PosToVec(target, routeDelta);
	VectorSubtract(routeDelta, routeFrom, routeDelta);
	routeDelta[2] = 0;
	routeDist = VectorLength(routeDelta);
	VectorNormalize(routeDelta);

	/* center the camera on the route starting position */
	VectorCopy(routeFrom, cl.cam.origin);
	/* set the world level to the z axis value of the camera target
	 * the camera lerp will do a smooth translate from the old level
	 * to the new one */
	Cvar_SetValue("cl_worldlevel", target[2]);

	VectorClear(cl.cam.speed);
	cameraRoute = qtrue;
	/* no other event should intersect during camera movement */
	CL_BlockEvents();
}

/**
 * @brief Called every frame to parse the input
 * @note The geoscape zooming code is in MN_MouseWheel too
 * @sa CL_Frame
 */
static void IN_Parse (void)
{
	switch (mouseSpace) {
	case MS_ROTATE:
		/* rotate a model */
		rotateAngles[YAW] -= ROTATE_SPEED * (mousePosX - oldMousePosX);
		rotateAngles[ROLL] += ROTATE_SPEED * (mousePosY - oldMousePosY);

		/* clamp the angles */
		while (rotateAngles[YAW] > 360.0)
			rotateAngles[YAW] -= 360.0;
		while (rotateAngles[YAW] < 0.0)
			rotateAngles[YAW] += 360.0;

		if (rotateAngles[ROLL] < 0.0)
			rotateAngles[ROLL] = 0.0;
		else if (rotateAngles[ROLL] > 180.0)
			rotateAngles[ROLL] = 180.0;
		return;

	case MS_SHIFTMAP:
	{
		int i;
		const float zoom = 0.5 / ccs.zoom;
		/* shift the map */
		ccs.center[0] -= (float) (mousePosX - oldMousePosX) / (ccs.mapSize[0] * ccs.zoom);
		ccs.center[1] -= (float) (mousePosY - oldMousePosY) / (ccs.mapSize[1] * ccs.zoom);
		for (i = 0; i < 2; i++) {
			/* clamp to min/max values */
			while (ccs.center[i] < 0.0)
				ccs.center[i] += 1.0;
			while (ccs.center[i] > 1.0)
				ccs.center[i] -= 1.0;
		}
		if (ccs.center[1] < zoom)
			ccs.center[1] = zoom;
		if (ccs.center[1] > 1.0 - zoom)
			ccs.center[1] = 1.0 - zoom;
		return;
	}

	case MS_SHIFT3DMAP:
		/* rotate a model */
		ccs.angles[PITCH] += ROTATE_SPEED * (mousePosX - oldMousePosX) / ccs.zoom;
		ccs.angles[YAW] -= ROTATE_SPEED * (mousePosY - oldMousePosY) / ccs.zoom;

		/* clamp the angles */
		while (ccs.angles[YAW] > 180.0)
			ccs.angles[YAW] -= 360.0;
		while (ccs.angles[YAW] < -180.0)
			ccs.angles[YAW] += 360.0;

		while (ccs.angles[PITCH] > 180.0)
			ccs.angles[PITCH] -= 360.0;
		while (ccs.angles[PITCH] < -180.0)
			ccs.angles[PITCH] += 360.0;
		return;

	case MS_ZOOMMAP:
	{
		const float zoom = 0.5 / ccs.zoom;
		/* zoom the map */
		ccs.zoom *= pow(0.995, mousePosY - oldMousePosY);
		if (ccs.zoom < cl_mapzoommin->value)
			ccs.zoom = cl_mapzoommin->value;
		else if (ccs.zoom > cl_mapzoommax->value)
			ccs.zoom = cl_mapzoommax->value;

		if (ccs.center[1] < zoom)
			ccs.center[1] = zoom;
		if (ccs.center[1] > 1.0 - zoom)
			ccs.center[1] = 1.0 - zoom;
		return;
	}

	case MS_DRAGITEM:
		/* do nothing */
		return;

	case MS_DRAGMENU:
		/* do nothing */
		return;

	/* repeat the mouse button */
	case MS_LHOLD:
		if (cls.realtime >= mn.mouseRepeat.nexttime) {
			/* how many times delay between 2 consecutive actions other
			 * than the 2 first one must be shorter than delay between the two
			 * first actions. (must be > 1) */
			const int DELAY_FACTOR = 3;

			/* delay between 2 actions when mouse button is kept pushed */
			const int repeatDelay = mn.mouseRepeat.clickDelay / DELAY_FACTOR;

			/* number of actions to perform since we last reached this part of the code
			 * (we don't use cls.realtime to make number of actions independant of FPS) */
			const int numClickSinceLastCall = 1 + (cls.realtime - mn.mouseRepeat.nexttime) / repeatDelay;
			int i;

			for (i = 0; i < numClickSinceLastCall; i++) {
				mn.mouseRepeat.numClick++;
				if (mn.mouseRepeat.node->type != MN_TEXT) {
					/* we didn't click on a text node (button,...) */
					MN_ExecuteActions(mn.mouseRepeat.menu, mn.mouseRepeat.action);
				} else {
					if (mn.mouseRepeat.textLine) {
						menuNode_t *node = mn.mouseRepeat.node;
						nodeBehaviourList[node->type].leftClick(node, mousePosX, mousePosY);
					}
				}
			}

			/* next "event" after clickdelay msec - low values (>= 100) would result in executing
			 * repeatable click nodes more than once - only do this for menus you want this behaviour */
			mn.mouseRepeat.nexttime += repeatDelay * numClickSinceLastCall;
		}
		return;
	default:
		mouseSpace = MS_NULL;

		/* standard menu and world mouse handling */
		if (MN_CursorOnMenu(mousePosX, mousePosY)) {
			mouseSpace = MS_MENU;
			return;
		}

		if (cls.state != ca_active)
			return;

		if (!viddef.viewWidth || !viddef.viewHeight)
			return;

		CL_ActorMouseTrace();
		/* set the mousespace to MS_WORLD because we are not in a menu
		 * (MN_CursorOnMenu failed) and we have the cursor in the world */
		if (cl.cmode > M_PEND_MOVE)
			mouseSpace = MS_WORLD;

		/* we are in tactical missions here - no focus needed */
		MN_FocusRemove();
		return;
	}
}

/**
 * @brief Debug function to print sdl key events
 */
static inline void IN_PrintKey (const SDL_Event* event, int down)
{
	if (in_debug->integer) {
		Com_Printf("key name: %s (down: %i)", SDL_GetKeyName(event->key.keysym.sym), down);
		if (event->key.keysym.unicode) {
			Com_Printf(" unicode: %hx", event->key.keysym.unicode);
			if (event->key.keysym.unicode >= '0' && event->key.keysym.unicode <= '~')  /* printable? */
				Com_Printf(" (%c)", (unsigned char)(event->key.keysym.unicode));
		}
		Com_Printf("\n");
	}
}

/**
 * @brief Translate the keys to ufo keys
 */
static void IN_TranslateKey (SDL_keysym *keysym, unsigned int *ascii, unsigned short *unicode)
{
	if (cls.deactivateKeyBindings)
		return;

	switch (keysym->sym) {
	case SDLK_KP9:
		*ascii = K_KP_PGUP;
		break;
	case SDLK_PAGEUP:
		*ascii = K_PGUP;
		break;
	case SDLK_KP3:
		*ascii = K_KP_PGDN;
		break;
	case SDLK_PAGEDOWN:
		*ascii = K_PGDN;
		break;
	case SDLK_KP7:
		*ascii = K_KP_HOME;
		break;
	case SDLK_HOME:
		*ascii = K_HOME;
		break;
	case SDLK_KP1:
		*ascii = K_KP_END;
		break;
	case SDLK_END:
		*ascii = K_END;
		break;
	case SDLK_KP4:
		*ascii = K_KP_LEFTARROW;
		break;
	case SDLK_LEFT:
		*ascii = K_LEFTARROW;
		break;
	case SDLK_KP6:
		*ascii = K_KP_RIGHTARROW;
		break;
	case SDLK_RIGHT:
		*ascii = K_RIGHTARROW;
		break;
	case SDLK_KP2:
		*ascii = K_KP_DOWNARROW;
		break;
	case SDLK_DOWN:
		*ascii = K_DOWNARROW;
		break;
	case SDLK_KP8:
		*ascii = K_KP_UPARROW;
		break;
	case SDLK_UP:
		*ascii = K_UPARROW;
		break;
	case SDLK_ESCAPE:
		*ascii = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		*ascii = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		*ascii = K_ENTER;
		break;
	case SDLK_TAB:
		*ascii = K_TAB;
		break;
	case SDLK_F1:
		*ascii = K_F1;
		break;
	case SDLK_F2:
		*ascii = K_F2;
		break;
	case SDLK_F3:
		*ascii = K_F3;
		break;
	case SDLK_F4:
		*ascii = K_F4;
		break;
	case SDLK_F5:
		*ascii = K_F5;
		break;
	case SDLK_F6:
		*ascii = K_F6;
		break;
	case SDLK_F7:
		*ascii = K_F7;
		break;
	case SDLK_F8:
		*ascii = K_F8;
		break;
	case SDLK_F9:
		*ascii = K_F9;
		break;
	case SDLK_F10:
		*ascii = K_F10;
		break;
	case SDLK_F11:
		*ascii = K_F11;
		break;
	case SDLK_F12:
		*ascii = K_F12;
		break;
	case SDLK_F13:
		*ascii = K_F13;
		break;
	case SDLK_F14:
		*ascii = K_F14;
		break;
	case SDLK_F15:
		*ascii = K_F15;
		break;
	case SDLK_BACKSPACE:
		*ascii = K_BACKSPACE;
		break;
	case SDLK_KP_PERIOD:
		*ascii = K_KP_DEL;
		break;
	case SDLK_DELETE:
		*ascii = K_DEL;
		break;
	case SDLK_PAUSE:
		*ascii = K_PAUSE;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		*ascii = K_SHIFT;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		*ascii = K_CTRL;
		break;
	case SDLK_LMETA:
	case SDLK_RMETA:
	case SDLK_LALT:
	case SDLK_RALT:
		*ascii = K_ALT;
		break;
	case SDLK_LSUPER:
	case SDLK_RSUPER:
		*ascii = K_SUPER;
		break;
	case SDLK_KP5:
		*ascii = K_KP_5;
		break;
	case SDLK_INSERT:
		*ascii = K_INS;
		break;
	case SDLK_KP0:
		*ascii = K_KP_INS;
		break;
	case SDLK_KP_MULTIPLY:
		*ascii = '*';
		break;
	case SDLK_KP_PLUS:
		*ascii = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		*ascii = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		*ascii = K_KP_SLASH;
		break;
	/* suggestions on how to handle this better would be appreciated */
	case SDLK_WORLD_7:
		*ascii = '`';
		break;
	case SDLK_MODE:
		*ascii = K_MODE;
		break;
	case SDLK_COMPOSE:
		*ascii = K_COMPOSE;
		break;
	case SDLK_HELP:
		*ascii = K_HELP;
		break;
	case SDLK_PRINT:
		*ascii = K_PRINT;
		break;
	case SDLK_SYSREQ:
		*ascii = K_SYSREQ;
		break;
	case SDLK_BREAK:
		*ascii = K_BREAK;
		break;
	case SDLK_MENU:
		*ascii = K_MENU;
		break;
	case SDLK_POWER:
		*ascii = K_POWER;
		break;
	case SDLK_EURO:
		*ascii = K_EURO;
		break;
	case SDLK_UNDO:
		*ascii = K_UNDO;
		break;
	case SDLK_SCROLLOCK:
		*ascii = K_SCROLLOCK;
		break;
	case SDLK_NUMLOCK:
		*ascii = K_KP_NUMLOCK;
		break;
	case SDLK_CAPSLOCK:
		*ascii = K_CAPSLOCK;
		break;
	case SDLK_SPACE:
		*ascii = K_SPACE;
		break;
	default:
		*ascii = keysym->sym;
		break;
	}

	*unicode = keysym->unicode;

	if (in_debug->integer)
		Com_Printf("unicode: %hx keycode: %i key: %hx\n", keysym->unicode, *ascii, *ascii);
}

#define EVENT_ENQUEUE(keyNum, keyUnicode, keyDown) \
	if (keyNum > 0) { \
		if (in_debug->integer) \
			Com_Printf("Enqueue: %s (%i) (down: %i)\n", Key_KeynumToString(keyNum), keyNum, keyDown); \
		keyq[keyq_head].down = (keyDown); \
		keyq[keyq_head].unicode = (keyUnicode); \
		keyq[keyq_head].key = (keyNum); \
		keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1); \
	}

void IN_EventEnqueue (unsigned int key, unsigned short unicode, qboolean down)
{
	EVENT_ENQUEUE(key, unicode, down)
}

/**
 * @brief Handle input events like keys presses and joystick movement as well
 * as window events
 * @sa CL_Frame
 * @sa IN_Parse
 * @sa IN_JoystickMove
 */
void IN_Frame (void)
{
	int mouse_buttonstate;
	unsigned short unicode;
	unsigned int key;
	SDL_Event event;

	IN_Parse();

	IN_JoystickMove();

	if (vid_grabmouse->modified) {
		vid_grabmouse->modified = qfalse;

		if (!vid_grabmouse->integer) {
			/* ungrab the pointer */
			Com_Printf("Switch grab input off\n");
			SDL_WM_GrabInput(SDL_GRAB_OFF);
		/* don't allow grabbing the input in fullscreen mode */
		} else if (!vid_fullscreen->integer) {
			/* grab the pointer */
			Com_Printf("Switch grab input on\n");
			SDL_WM_GrabInput(SDL_GRAB_ON);
		} else {
			Com_Printf("No input grabbing in fullscreen mode\n");
			Cvar_SetValue("vid_grabmouse", 0);
		}
	}

	oldMousePosX = mousePosX;
	oldMousePosY = mousePosY;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			switch (event.button.button) {
			case SDL_BUTTON_LEFT:
				mouse_buttonstate = K_MOUSE1;
				break;
			case SDL_BUTTON_MIDDLE:
				mouse_buttonstate = K_MOUSE3;
				break;
			case SDL_BUTTON_RIGHT:
				mouse_buttonstate = K_MOUSE2;
				break;
			case SDL_BUTTON_WHEELUP:
				mouse_buttonstate = K_MWHEELUP;
				break;
			case SDL_BUTTON_WHEELDOWN:
				mouse_buttonstate = K_MWHEELDOWN;
				break;
			case 6:
				mouse_buttonstate = K_MOUSE4;
				break;
			case 7:
				mouse_buttonstate = K_MOUSE5;
				break;
			default:
				mouse_buttonstate = K_AUX1 + (event.button.button - 8) % 16;
				break;
			}
			EVENT_ENQUEUE(mouse_buttonstate, 0, (event.type == SDL_MOUSEBUTTONDOWN))
			break;

		case SDL_MOUSEMOTION:
			SDL_GetMouseState(&mousePosX, &mousePosY);
			mousePosX /= viddef.rx;
			mousePosY /= viddef.ry;
			break;

		case SDL_KEYDOWN:
			IN_PrintKey(&event, 1);
			if (event.key.keysym.mod & KMOD_ALT && event.key.keysym.sym == SDLK_RETURN) {
				SDL_Surface *surface = SDL_GetVideoSurface();
				if (!SDL_WM_ToggleFullScreen(surface))
					Com_Printf("IN_Frame: Could not toggle fullscreen mode\n");

				if (surface->flags & SDL_FULLSCREEN) {
					Cvar_SetValue("vid_fullscreen", 1);
					/* make sure, that input grab is deactivated in fullscreen mode */
					Cvar_SetValue("vid_grabmouse", 0);
				} else {
					Cvar_SetValue("vid_fullscreen", 0);
				}
				vid_fullscreen->modified = qfalse; /* we just changed it with SDL. */
				break; /* ignore this key */
			}

			if (event.key.keysym.mod & KMOD_CTRL && event.key.keysym.sym == SDLK_g) {
				SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
				Cvar_SetValue("vid_grabmouse", (gm == SDL_GRAB_ON) ? 0 : 1);
				break; /* ignore this key */
			}

			/* console key is hardcoded, so the user can never unbind it */
			if (event.key.keysym.mod & KMOD_SHIFT && event.key.keysym.sym == SDLK_ESCAPE) {
				Con_ToggleConsole_f();
				break;
			}

			IN_TranslateKey(&event.key.keysym, &key, &unicode);
			EVENT_ENQUEUE(key, unicode, qtrue)
			break;

		case SDL_VIDEOEXPOSE:
			break;

		case SDL_KEYUP:
			IN_PrintKey(&event, 0);
			IN_TranslateKey(&event.key.keysym, &key, &unicode);
			EVENT_ENQUEUE(key, unicode, qfalse)
			break;

		case SDL_QUIT:
			Cmd_ExecuteString("quit");
			break;
		}
	}
}

/**
 * @brief De-/Activates the executing of key bindings
 */
static void CL_InputBindingsToggle_f (void)
{
	cls.deactivateKeyBindings ^= 1;
}

/**
 * @sa CL_InitLocal
 */
void IN_Init (void)
{
	Com_Printf("\n------- input initialization -------\n");

	/* cvars */
	in_debug = Cvar_Get("in_debug", "0", 0, "Show input key codes on game console");
	cl_camrotspeed = Cvar_Get("cl_camrotspeed", "250", CVAR_ARCHIVE, NULL);
	cl_cammovespeed = Cvar_Get("cl_cammovespeed", "750", CVAR_ARCHIVE, NULL);
	cl_cammoveaccel = Cvar_Get("cl_cammoveaccel", "1250", CVAR_ARCHIVE, NULL);
	cl_campitchmax = Cvar_Get("cl_campitchmax", "90", 0, "Max camera pitch - over 90 presents apparent mouse inversion");
	cl_campitchmin = Cvar_Get("cl_campitchmin", "35", 0, "Min camera pitch - under 35 presents difficulty positioning cursor");
	cl_camzoomquant = Cvar_Get("cl_camzoomquant", "0.16", CVAR_ARCHIVE, NULL);
	cl_camzoommin = Cvar_Get("cl_camzoommin", "0.7", 0, "Minimum zoom value for tactical missions");
	cl_camzoommax = Cvar_Get("cl_camzoommax", "3.4", 0, "Maximum zoom value for tactical missions");
	cl_centerview = Cvar_Get("cl_centerview", "1", CVAR_ARCHIVE, "Center the view when selecting a new soldier");
	cl_mapzoommax = Cvar_Get("cl_mapzoommax", "6.0", CVAR_ARCHIVE, "Maximum geoscape zooming value");
	cl_mapzoommin = Cvar_Get("cl_mapzoommin", "1.0", CVAR_ARCHIVE, "Minimum geoscape zooming value");

	/* commands */
	Cmd_AddCommand("+turnleft", IN_TurnLeftDown_f, _("Rotate battlescape camera anti-clockwise"));
	Cmd_AddCommand("-turnleft", IN_TurnLeftUp_f, NULL);
	Cmd_AddCommand("+turnright", IN_TurnRightDown_f, _("Rotate battlescape camera clockwise"));
	Cmd_AddCommand("-turnright", IN_TurnRightUp_f, NULL);
	Cmd_AddCommand("+turnup", IN_TurnUpDown_f, _("Tilt battlescape camera up"));
	Cmd_AddCommand("-turnup", IN_TurnUpUp_f, NULL);
	Cmd_AddCommand("+turndown", IN_TurnDownDown_f, _("Tilt battlescape camera down"));
	Cmd_AddCommand("-turndown", IN_TurnDownUp_f, NULL);
	Cmd_AddCommand("+shiftleft", IN_ShiftLeftDown_f, _("Move battlescape camera left"));
	Cmd_AddCommand("-shiftleft", IN_ShiftLeftUp_f, NULL);
	Cmd_AddCommand("+shiftright", IN_ShiftRightDown_f, _("Move battlescape camera right"));
	Cmd_AddCommand("-shiftright", IN_ShiftRightUp_f, NULL);
	Cmd_AddCommand("+shiftup", IN_ShiftUpDown_f, _("Move battlescape camera forward"));
	Cmd_AddCommand("-shiftup", IN_ShiftUpUp_f, NULL);
	Cmd_AddCommand("+shiftdown", IN_ShiftDownDown_f, _("Move battlescape camera backward"));
	Cmd_AddCommand("-shiftdown", IN_ShiftDownUp_f, NULL);
	Cmd_AddCommand("+zoomin", IN_ZoomInDown_f, _("Zoom in"));
	Cmd_AddCommand("-zoomin", IN_ZoomInUp_f, NULL);
	Cmd_AddCommand("+zoomout", IN_ZoomOutDown_f, _("Zoom out"));
	Cmd_AddCommand("-zoomout", IN_ZoomOutUp_f, NULL);

	Cmd_AddCommand("+leftmouse", CL_LeftClickDown_f, _("Left mouse button click (menu)"));
	Cmd_AddCommand("-leftmouse", CL_LeftClickUp_f, NULL);
	Cmd_AddCommand("+middlemouse", CL_MiddleClickDown_f, _("Middle mouse button click (menu)"));
	Cmd_AddCommand("-middlemouse", CL_MiddleClickUp_f, NULL);
	Cmd_AddCommand("+rightmouse", CL_RightClickDown_f, _("Right mouse button click (menu)"));
	Cmd_AddCommand("-rightmouse", CL_RightClickUp_f, NULL);
	Cmd_AddCommand("+select", CL_SelectDown_f, _("Select objects/Walk to a square/In fire mode, fire etc"));
	Cmd_AddCommand("-select", CL_SelectUp_f, NULL);
	Cmd_AddCommand("+action", CL_ActionDown_f, _("Walk to a square/In fire mode, cancel action"));
	Cmd_AddCommand("-action", CL_ActionUp_f, NULL);
	Cmd_AddCommand("+turn", CL_TurnDown_f, _("Turn soldier toward mouse pointer"));
	Cmd_AddCommand("-turn", CL_TurnUp_f, NULL);
	Cmd_AddCommand("+hudradar", CL_HudRadarDown_f, _("Toggles the hud radar mode"));
	Cmd_AddCommand("-hudradar", CL_HudRadarUp_f, NULL);

	Cmd_AddCommand("standcrouch", CL_ActorStandCrouch_f, _("Toggle stand/crounch"));
	Cmd_AddCommand("togglereaction", CL_ActorToggleReaction_f, _("Toggle reaction fire"));
	Cmd_AddCommand("togglecrouchreserve", CL_ActorToggleCrouchReservation_f, _("Toggle reservation for crouching."));
	Cmd_AddCommand("useheadgear", CL_ActorUseHeadgear_f, _("Toggle the headgear"));
	Cmd_AddCommand("nextalien", CL_NextAlien_f, _("Toggle to next alien"));
	Cmd_AddCommand("drawspottedlines", CL_DrawSpottedLines_f, _("Draw a line to each alien visible to the current actor."));
	Cmd_AddCommand("nextalienactor", CL_NextAlienVisibleFromActor_f, _("Toggle to next alien visible from selected actor."));

	Cmd_AddCommand("list_firemodes", CL_DisplayFiremodes_f, "Display a list of firemodes for a weapon+ammo.");
	Cmd_AddCommand("switch_firemode_list", CL_SwitchFiremodeList_f, "Switch firemode-list to one for the given hand, but only if the list is visible already.");
	Cmd_AddCommand("fireweap", CL_FireWeapon_f, "Start aiming the weapon.");
	Cmd_AddCommand("sel_reactmode", CL_SelectReactionFiremode_f, "Change/Select firemode used for reaction fire.");
	Cmd_AddCommand("sel_shotreservation", CL_PopupFiremodeReservation_f, "Pop up a list of possible firemodes for reservation in the current turn.");
	Cmd_AddCommand("reserve_shot", CL_ReserveShot_f, "Reserve The TUs for the selected entry in the popup.");
	Cmd_AddCommand("remaining_tus", CL_RemainingTus_f, "Define if remaining TUs should be displayed in the TU-bar for some hovered-over button.");

	Cmd_AddCommand("reloadleft", CL_ReloadLeft_f, _("Reload the weapon in the soldiers left hand"));
	Cmd_AddCommand("reloadright", CL_ReloadRight_f, _("Reload the weapon in the soldiers right hand"));
	Cmd_AddCommand("nextround", CL_NextRound_f, _("Ends current round"));
	Cmd_AddCommand("dooraction", CL_ActorDoorAction_f, _("Opens or closes a door"));
	Cmd_AddCommand("levelup", CL_LevelUp_f, _("Slice through terrain at a higher level"));
	Cmd_AddCommand("leveldown", CL_LevelDown_f, _("Slice through terrain at a lower level"));
	Cmd_AddCommand("zoominquant", CL_ZoomInQuant_f, _("Zoom in"));
	Cmd_AddCommand("zoomoutquant", CL_ZoomOutQuant_f, _("Zoom out"));
	Cmd_AddCommand("confirmaction", CL_ConfirmAction_f, _("Confirm the current action"));
	Cmd_AddCommand("togglecombatzoomlevel", MAP_ToggleCombatZoomLevel_f, _("Toggle the combat zoom level (full zoom or half zoom)."));

#ifdef DEBUG
	Cmd_AddCommand("debug_listreservations", CL_ListReactionAndReservations_f, "Prints all reaction- and reservation-info for the team.");
	Cmd_AddCommand("debug_camangles", CL_CamPrintAngles_f, "Prints current camera angles");
	Cmd_AddCommand("debug_drawblocked", CL_DisplayBlockedPaths_f, "Draws a marker for all blocked map-positions.");
#endif /* DEBUG */
	Cmd_AddCommand("camsetangles", CL_CamSetAngles_f, "Set camera angles to the given values");
	Cmd_AddCommand("camsetzoom", CL_CamSetZoom_f, "Set camera zoom level");
	Cmd_AddCommand("basemapshot", CL_MakeBaseMapShot_f, "Command to make a screenshot for the baseview with the correct angles");

	Cmd_AddCommand("cameramode", CL_CameraMode_f, _("Toggle first-person/third-person camera mode"));
	Cmd_AddCommand("bindingstoggle", CL_InputBindingsToggle_f, "Toggle the use of bind keys - useful for some popup menus");
	Cmd_AddCommand("cameramode", CL_CameraMode_f, _("Toggle first-person/third-person camera mode"));

	mousePosX = mousePosY = 0.0;

	IN_StartupJoystick();
}

/**
 * @sa CL_SendCommand
 */
void IN_SendKeyEvents (void)
{
	while (keyq_head != keyq_tail) {
		Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].unicode, keyq[keyq_tail].down, cls.realtime);
		keyq_tail = (keyq_tail + 1) & (MAX_KEYQ - 1);
	}
}
