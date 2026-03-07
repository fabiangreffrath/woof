//
// Copyright(C)      2023 Andrew Apted
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdlib.h>
#include <string.h>

#include "doomstat.h"
#include "doomtype.h"
#include "hu_crosshair.h"
#include "i_printf.h"
#include "i_video.h"
#include "info.h"
#include "m_fixed.h"
#include "m_misc.h"
#include "mn_menu.h"
#include "p_mobj.h"
#include "r_bmaps.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_state.h"
#include "r_things.h"
#include "tables.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

static boolean voxels_found;
boolean voxels_rendering, default_voxels_rendering;

struct Voxel
{
	int  x_size;
	int  y_size;
	int  z_size;

	fixed_t  x_pivot;
	fixed_t  y_pivot;
	fixed_t  z_pivot;

	int  * offsets;
	byte * data;
};

#define MAX_FRAMES  29

static struct Voxel *** all_voxels;

#define VX_ITEM_ROTATION_ANGLE (4 * ANG1)

static int vx_rotate_items = 1;


// bits in the `face` byte of a slab
enum VoxelFace
{
	F_LEFT   = 0x01,
	F_RIGHT  = 0x02,
	F_BACK   = 0x04,
	F_FRONT  = 0x08,
	F_TOP    = 0x10,
	F_BOTTOM = 0x20,
};


static int VX_PaletteIndex (byte * pal, int r, int g, int b)
{
	int best = 0;
	int best_dist = (1 << 30);

	int i;
	for (i = 0 ; i < 256 ; i++)
	{
		int dr = r - (int)*pal++;
		int dg = g - (int)*pal++;
		int db = b - (int)*pal++;

		int dist = dr * dr + dg * dg + db * db;

		if (dist < best_dist)
		{
			best = i;
			best_dist = dist;
		}
	}

	return best;
}


static void VX_CreateRemapTable (byte * p, byte * table)
{
	byte * pal = W_CacheLumpName ("PLAYPAL", PU_CACHE);

	int c;
	for (c = 0 ; c < 256 ; c++)
	{
		int r = (int)*p++ << 2;
		int g = (int)*p++ << 2;
		int b = (int)*p++ << 2;

		table[c] = VX_PaletteIndex (pal, r, g, b);
	}
}


static void VX_RemapSlabColors (struct Voxel * v, int x, int y, byte * table)
{
	int A = v->offsets[y     * v->x_size + x];
	int B = v->offsets[(y+1) * v->x_size + x];

	if (! (A < B))
		return;

	byte * slab = &v->data[A];
	byte * end  = &v->data[B];

	while (slab < end)
	{
		byte top  = *slab++;
		byte len  = *slab++;
		byte face = *slab++;
		byte i;

		(void) top;
		(void) face;

		for (i = 0 ; i < len ; i++, slab++)
		{
			*slab = table[*slab];
		}
	}
}


static struct Voxel * VX_Decode (byte * p, int length)
{
	// too short?
	if (length < 40 + 768)
		return NULL;

	byte * orig_p = p;

	struct Voxel * v = Z_Malloc (sizeof(struct Voxel), PU_STATIC, NULL);

	// skip num_bytes
	p += 4;

	v->x_size = (int)p[0];  p += 4;
	v->y_size = (int)p[0];  p += 4;
	v->z_size = (int)p[0];  p += 4;

	if (v->x_size == 0 || v->y_size == 0 || v->z_size == 0)
		return NULL;

	v->x_pivot = (p[0] << 8) | (p[1] << 16);  p += 4;
	v->y_pivot = (p[0] << 8) | (p[1] << 16);  p += 4;
	v->z_pivot = (p[0] << 8) | (p[1] << 16);  p += 4;

	// decode the offset values...
	int xoffsets[260];

	int x;
	for (x = 0 ; x <= v->x_size ; x++)
	{
		xoffsets[x] = (int)p[0] | (p[1] << 8) | (p[2] << 16);
		p += 4;
	}

	int num_offsets = v->x_size * (v->y_size + 1);

	int min_offset = (1 << 30);
	int max_offset = 0;

	v->offsets = Z_Malloc (sizeof(int) * num_offsets, PU_STATIC, NULL);

	for (x = 0 ; x < v->x_size ; x++)
	{
		int y;
		for (y = 0 ; y <= v->y_size ; y++)
		{
			int offset = (int)p[0] | (p[1] << 8);
			p += 2;

			offset = offset + xoffsets[x];

			v->offsets[y * v->x_size + x] = offset;

			if (offset < min_offset) min_offset = offset;
			if (offset > max_offset) max_offset = offset;
		}
	}

	int data_size = max_offset - min_offset;
	if (data_size <= 0)
		return NULL;

	for (x = 0 ; x < num_offsets ; x++)
		v->offsets[x] -= min_offset;

	// copy the slab data
	p = orig_p + (7 * 4) + min_offset;

	v->data = Z_Malloc (data_size, PU_STATIC, NULL);

	memcpy (v->data, p, data_size);

	// handle palette: create a mapping table
	byte remap_table[256];

	VX_CreateRemapTable (orig_p + (length - 768), remap_table);

	// remap colors...
	for (x = 0 ; x < v->x_size ; x++)
	{
		int y;
		for (y = 0 ; y < v->y_size ; y++)
		{
			VX_RemapSlabColors (v, x, y, remap_table);
		}
	}

	return v;
}


static boolean VX_Load (int spr, int frame)
{
	char frame_ch = 'A' + frame;

	if (frame_ch == '\\')
		frame_ch = '^';

	char lumpname[9] = {0};

	M_snprintf (lumpname, sizeof(lumpname), "%s%c", sprnames[spr], frame_ch);

	int lumpnum = (W_CheckNumForName)(lumpname, ns_voxels);

	if (lumpnum < 0)
	{
		return false;
	}

	byte *buf = W_CacheLumpNum(lumpnum, PU_STATIC);
	int len   = W_LumpLength(lumpnum);

	// Note: this may return NULL
	struct Voxel * v = VX_Decode (buf, len);

	if (v != NULL)
		voxels_found = true;

	all_voxels[spr][frame] = v;

	Z_Free (buf);

	return true;
}


void VX_Init (void)
{
	int spr, frame;

	all_voxels = Z_Malloc(num_sprites * sizeof(*all_voxels), PU_STATIC, NULL);
	for (spr = 0 ; spr < num_sprites ; spr++)
	{
		all_voxels[spr] = Z_Malloc (MAX_FRAMES * sizeof(**all_voxels),
					    PU_STATIC, NULL);
		for (frame = 0 ; frame < MAX_FRAMES ; frame++)
		{
			all_voxels[spr][frame] = NULL;
		}
	}

	I_Printf(VB_INFO, "Loading voxels... ");

	for (spr = 0 ; spr < num_sprites ; spr++)
	{
		for (frame = 0 ; frame < MAX_FRAMES ; frame++)
		{
			if (! VX_Load (spr, frame))
				break;
		}
	}

	voxels_rendering = default_voxels_rendering;

	if (!voxels_found)
	{
		voxels_rendering = false;
		MN_DisableVoxelsRenderingItem();
		I_Printf(VB_INFO, "not found.");
	}
	else
	{
		I_Printf (VB_INFO, "done.");
	}
}

//------------------------------------------------------------------------

#define VX_MINZ         (   4 * FRACUNIT)
#define VX_MAX_DIST     (2048 * FRACUNIT)
#define VX_MIN_DIST     ( 512 * FRACUNIT)
#define VX_DIST_STEP    ( 512 * FRACUNIT)

static int vx_max_dist = VX_MAX_DIST;

void VX_IncreaseMaxDist (int step_multipler)
{
	vx_max_dist = MIN (VX_MAX_DIST, vx_max_dist + VX_DIST_STEP * step_multipler);
}

void VX_DecreaseMaxDist (int step_multipler)
{
	vx_max_dist = MAX (VX_MIN_DIST, vx_max_dist - VX_DIST_STEP * step_multipler);
}

void VX_ResetMaxDist (void)
{
	vx_max_dist = VX_MAX_DIST;
}

struct VisVoxel
{
	struct Voxel * model;

	angle_t  angle;

	// coord of top-left corner (transformed)
	fixed_t  TL_x, TL_y;

	// unit vector toward top-right corner (from TL corner).
	// the vector (s, -c) points toward bottom-left corner.
	fixed_t  c, s;
};

static struct VisVoxel * visvoxels;
static int num_visvoxels;

vissprite_t * R_NewVisSprite (void);

static int VX_NewVisVoxel (void)
{
	static int size;

	if (num_visvoxels >= size)
	{
		size = (size ? size * 2 : 128);
		visvoxels = Z_Realloc (visvoxels, size * sizeof(*visvoxels),
				       PU_STATIC, 0);
	}

	return num_visvoxels++;
}


void VX_ClearVoxels (void)
{
	rendered_voxels = num_visvoxels;
	num_visvoxels = 0;
}


static int VX_RotateModeForThing (mobj_t * thing)
{
	// these four spherical items *must* face directly at the camera,
	// otherwise they look very weird.
	switch (thing->sprite)
	{
		case SPR_PINS:
		case SPR_PINV:
		case SPR_SOUL:
		case SPR_MEGA:
			return 2;

		default:
			break;
	}

	if (vx_rotate_items)
	{
		if (thing->flags & MF_DROPPED)
			return 1;

		switch (thing->sprite)
		{
			case SPR_SHOT:
			case SPR_MGUN:
			case SPR_LAUN:
			case SPR_PLAS:
			case SPR_BFUG:
			case SPR_CSAW:
			case SPR_SGN2:
				return 3;

			default:
				break;
		}

		// in most maps, items don't have "nice" facing directions, so this
		// option orientates them in a way similar to sprites.
		if (thing->flags & MF_SPECIAL)
			return 1;
	}

	// use existing angle
	return 0;
}


static angle_t VX_GetItemRotationAngle (void)
{
	static int oldgametic = -1;
	static angle_t oldangle, newangle;

	if (oldgametic < gametic)
	{
		oldangle = newangle;
		newangle = leveltime * VX_ITEM_ROTATION_ANGLE;
		oldgametic = gametic;
	}

	if (uncapped)
	{
		return LerpAngle (oldangle, newangle);
	}
	else
	{
		return newangle;
	}
}


static boolean VX_CheckFrustum (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2,
                         fixed_t x3, fixed_t y3, fixed_t x4, fixed_t y4)
{
	// completely behind the viewplane?
	if (y1 < VX_MINZ && y2 < VX_MINZ && y3 < VX_MINZ && y4 < VX_MINZ)
		return false;

	fixed_t c = finecosine[vx_clipangle >> ANGLETOFINESHIFT];
	fixed_t s = finesine  [vx_clipangle >> ANGLETOFINESHIFT];

	// note the funky shifts here, to counter-balance opposite
	// shifts in the R_PointOnSide() code.  Sigh.
	c = c << 12;
	s = s << 12;

	node_t left;

	left.x  = 0;
	left.y  = 0;
	left.dx = -c;
	left.dy =  s;

	if (R_PointOnSide (x1, y1, &left) &&
	    R_PointOnSide (x2, y2, &left) &&
	    R_PointOnSide (x3, y3, &left) &&
	    R_PointOnSide (x4, y4, &left))
	{
		return false;
	}

	node_t right;

	right.x  = 0;
	right.y  = 0;
	right.dx = -c;
	right.dy = -s;

	if (R_PointOnSide (x1, y1, &right) &&
	    R_PointOnSide (x2, y2, &right) &&
	    R_PointOnSide (x3, y3, &right) &&
	    R_PointOnSide (x4, y4, &right))
	{
		return false;
	}

	// some part is visible
	return true;
}


boolean VX_ProjectVoxel(mobj_t *thing, int lightlevel_override)
{
	if (!STRICTMODE(voxels_rendering))
		return false;

	// skip the player thing we are viewing from
	if (thing->player == viewplayer)
		return true;

	// does the voxel model exist?
	int spr   = thing->sprite;
	int frame = thing->frame & FF_FRAMEMASK;

	struct Voxel * v = all_voxels[spr][frame];
	if (v == NULL)
		return false;

	fixed_t gx, gy, gz;
	angle_t angle;

	// [AM] Interpolate between current and last position,
	//      if prudent.
	if (uncapped &&
	    // Don't interpolate if the mobj did something
	    // that would necessitate turning it off for a tic.
	    thing->interp == true &&
	    // Don't interpolate during a paused state.
	    leveltime > oldleveltime)
	{
		gx = LerpFixed (thing->oldx, thing->x);
		gy = LerpFixed (thing->oldy, thing->y);
		gz = LerpFixed (thing->oldz, thing->z);
		angle = LerpAngle (thing->oldangle, thing->angle);
	}
	else
	{
		gx = thing->x;
		gy = thing->y;
		gz = thing->z;
		angle = thing->angle;
	}

	// transform the origin point
	fixed_t tran_x = gx - viewx;
	fixed_t tran_y = gy - viewy;

	// too far away?
	if (abs (tran_x) > vx_max_dist || abs (tran_y) > vx_max_dist)
		return false;

	fixed_t tx = FixedMul (tran_x, viewsin) - FixedMul (tran_y, viewcos);
	fixed_t ty = FixedMul (tran_x, viewcos) + FixedMul (tran_y, viewsin);

	fixed_t xscale;

	// thing is behind view plane?  if so, ignore it
	if (ty < VX_MINZ)
	{
		if (ty < -64 * FRACUNIT)
			return true;

		xscale = (15000 * FRACUNIT) - ty;
	}
	else
	{
		// too far off the side?  if so, ignore it
		if (ty > (64 * FRACUNIT) && abs(tx) / max_project_slope > ty)
			return true;

		xscale = FixedDiv (projection, ty);
	}

	switch (VX_RotateModeForThing (thing))
	{
		case 1:
			angle = viewangle + ANG180;
			break;

		case 2:
			angle = R_PointToAngle (gx, gy) + ANG180;
			break;

		case 3:
			angle = VX_GetItemRotationAngle ();
			break;
	}

	angle_t ang2 = ANG180 - viewangle + angle;

	// unit vector from top-left corner toward top-right corner
	fixed_t c = finecosine[ang2 >> ANGLETOFINESHIFT];
	fixed_t s = finesine  [ang2 >> ANGLETOFINESHIFT];

	// compute coord of top-left corner (transformed)
	fixed_t xp = v->x_pivot;
	fixed_t yp = v->y_pivot;

	fixed_t TL_x = tx - FixedMul (xp, c) - FixedMul (yp, s);
	fixed_t TL_y = ty - FixedMul (xp, s) + FixedMul (yp, c);

	// compute X coords of the other corners
	fixed_t xs = v->x_size;
	fixed_t ys = v->y_size;

	fixed_t BL_x = TL_x + (ys * s);
	fixed_t BL_y = TL_y - (ys * c);

	fixed_t TR_x = TL_x + (xs * c);
	fixed_t TR_y = TL_y + (xs * s);

	fixed_t BR_x = BL_x + (xs * c);
	fixed_t BR_y = BL_y + (xs * s);

	// is it outside the view frustum?
	if (! VX_CheckFrustum (TL_x, TL_y, BL_x, BL_y, BR_x, BR_y, TR_x, TR_y))
		return true;

	// compute minimum and maximum X coord
	int x1 = viewwidth-1;
	int x2 = 0;

	int i;
	for (i = 0 ; i < 4 ; i++)
	{
		fixed_t cx = (i == 0) ? BL_x : (i == 1) ? BR_x : (i == 2) ? TL_x : TR_x;
		fixed_t cy = (i == 0) ? BL_y : (i == 1) ? BR_y : (i == 2) ? TL_y : TR_y;

		// when any corner is behind the viewplane, expand to whole screen
		if (cy < VX_MINZ)
		{
			x1 = 0;
			x2 = viewwidth - 1;
			break;
		}
		else
		{
			fixed_t sx = centerxfrac + FixedMul (cx, FixedDiv (projection, cy));

			int isx = sx >> FRACBITS;

			if (isx < x1) x1 = isx;
			if (isx > x2) x2 = isx;
		}
	}

	// clip X range to the screen
	if (x1 < 0)           x1 = 0;
	if (x2 > viewwidth-1) x2 = viewwidth-1;

	// fully clipped?
	if (x1 > x2)
		return true;

	// create the VisVoxel...
	int voxel_index = VX_NewVisVoxel ();
	struct VisVoxel * vv = &visvoxels[voxel_index];

	vv->model  = v;
	vv->angle  = angle;

	vv->TL_x = TL_x;
	vv->TL_y = TL_y;
	vv->c    = c;
	vv->s    = s;

	// create the vissprite...
	vissprite_t * vis = R_NewVisSprite ();
	vis->voxel_index = voxel_index;

	vis->heightsec = thing->subsector->sector->heightsec;

	vis->mobjflags = thing->flags;
	vis->mobjflags2 = thing->flags2;
	vis->mobjflags_extra = thing->flags_extra;
	vis->scale = xscale;

	vis->gx  = gx;
	vis->gy  = gy;
	vis->gz  = gz;
	vis->gzt = gz + v->z_pivot;

	vis->x1 = x1;
	vis->x2 = x2;

	// get light level...
	const lighttable_t * const thiscolormap = GetThingTint(thing, thing->subsector->sector);

	if (vis->mobjflags & MF_SHADOW)
	{
		vis->colormap[0] = vis->colormap[1] = NULL;
	}
	else if (fixedcolormap != NULL)
	{
		vis->colormap[0] = vis->colormap[1] = thiscolormap + fixedcolormapindex * 256;
	}
	else if (thing->frame & FF_FULLBRIGHT)
	{
		vis->colormap[0] = vis->colormap[1] = thiscolormap;
	}
	else
	{
		// diminished light
		const int index = R_GetLightIndex(xscale);
		int lightnum = (demo_version >= DV_MBF)
				? (lightlevel_override >> LIGHTSEGSHIFT)
				: (thing->subsector->sector->lightlevel >> LIGHTSEGSHIFT);

		lightnum = CLAMP(lightnum, 0, LIGHTLEVELS - 1);
		int* spritelightoffsets = &scalelightoffset[MAXLIGHTSCALE * lightnum];

		vis->colormap[0] = thiscolormap + spritelightoffsets[index];
		vis->colormap[1] = (STRICTMODE(brightmaps) || force_brightmaps)
				? thiscolormap
				: dc_colormap[0];
	}

	// ID24 per-state tranmap
	// tranmaps do not work with Voxels yet
	vis->tranmap = NULL;

	vis->brightmap = R_BrightmapForSprite(thing->sprite);
	vis->color = thing->bloodcolor;

	// [Alaux] Lock crosshair on target
	if (STRICTMODE(hud_crosshair_lockon) && thing == crosshair_target)
	{
		int x = (centerxfrac + FixedMul(tx, xscale)) >> FRACBITS;
		int y = (centeryfrac + FixedMul(viewz - gz - crosshair_target->actualheight / 2, xscale)) >> FRACBITS;
		x = clampi(x, 0, viewwidth - 1);
		y = clampi(y, 0, viewheight - 1);
		HU_UpdateCrosshairLock(x, y);
		crosshair_target = NULL; // Don't update it again until next tic
	}

	return true;
}

//------------------------------------------------------------------------

// camera position in model space
static fixed_t  vx_eye_x;
static fixed_t  vx_eye_y;


static void VX_DrawColumn (vissprite_t * spr, int x, int y)
{
	struct VisVoxel * vv = &visvoxels[spr->voxel_index];
	struct Voxel    * v  = vv->model;

	int ofs1 = v->offsets[y     * v->x_size + x];
	int ofs2 = v->offsets[(y+1) * v->x_size + x];

	if (! (ofs1 < ofs2))
		return;

	// determine where camera is in relation to this column
	int qu_x = vx_eye_x < (x << FRACBITS) ? 0 : vx_eye_x < ((x+1) << FRACBITS) ? 1 : 2;
	int qu_y = vx_eye_y < (y << FRACBITS) ? 0 : vx_eye_y < ((y+1) << FRACBITS) ? 1 : 2;

	int quadrant = qu_y * 3 + qu_x;

	if (quadrant == 4)
		return;

	// determine transformed 2D coordinates of the corners.
	// A should be left-most on the screen, B in the middle (at the front),
	// C is the right-most, and D is at the back.
	//
	//       D
	//      / \          D-----C
	//     /   \         |     |
	//    A     C        |     |
	//     \   /         |     |
	//      \ /          A-----B
	//       B
	//
	// ------------------------------- viewplane
	//              ^
	//              |
	//             eye
	//
	// it is possible that B is the right-most corner, and C is at the
	// back and left from B (or has same X coord).  we may also have D
	// with same X coord as A.

	fixed_t c = vv->c;
	fixed_t s = vv->s;

	// the order here is: TL, BL, BR, TR.
	fixed_t tx[4];
	fixed_t ty[4];

	tx[0] = vv->TL_x + x * c + y * s;
	ty[0] = vv->TL_y + x * s - y * c;

	tx[1] = tx[0] + s;
	ty[1] = ty[0] - c;

	tx[2] = tx[1] + c;
	ty[2] = ty[1] + s;

	tx[3] = tx[0] + c;
	ty[3] = ty[0] + s;

	static const int A_corners[9] = { 3, 3, 2, 0, -1, 2, 0, 1, 1 };

	int idx = A_corners[quadrant];

	fixed_t Ax = tx[idx];
	fixed_t Ay = ty[idx];  idx = (idx + 1) & 3;

	fixed_t Bx = tx[idx];
	fixed_t By = ty[idx];  idx = (idx + 1) & 3;

	fixed_t Cx = tx[idx];
	fixed_t Cy = ty[idx];  idx = (idx + 1) & 3;

	fixed_t Dx = tx[idx];
	fixed_t Dy = ty[idx];

	// skip any part which is behind the viewplane
	if (By < VX_MINZ || Ay < VX_MINZ || Cy < VX_MINZ || Dy < VX_MINZ)
		return;

	fixed_t A_xscale = FixedDiv (projection, Ay);
	fixed_t B_xscale = FixedDiv (projection, By);
	fixed_t C_xscale = FixedDiv (projection, Cy);
	fixed_t D_xscale = FixedDiv (projection, Dy);

	Ax = centerxfrac + FixedMul (Ax, A_xscale);
	Bx = centerxfrac + FixedMul (Bx, B_xscale);
	Cx = centerxfrac + FixedMul (Cx, C_xscale);
	Dx = centerxfrac + FixedMul (Dx, D_xscale);

	static const byte A_faces[9] = { F_BACK, F_BACK, F_RIGHT, F_LEFT, 0, F_RIGHT, F_LEFT, F_FRONT, F_FRONT };
	static const byte B_faces[9] = { F_LEFT, 0, F_BACK, 0, 0, 0, F_FRONT, 0, F_RIGHT };

	byte A_face = A_faces[quadrant];
	byte B_face = B_faces[quadrant];

	boolean shadow = ((spr->mobjflags & MF_SHADOW) != 0);

	int linesize = video.width;
	pixel_t * dest = I_VideoBuffer + viewwindowy * linesize + viewwindowx;

	// iterate over screen columns
	fixed_t ux = ((Ax - 1) | FRACMASK) + 1;

	for (; ux < ((Cx > Bx) ? Cx : Bx) ; ux += FRACUNIT)
	{
		// clip horizontally
		if (ux >= ((spr->x2 + 1) << FRACBITS)) break;
		if (ux <  ((spr->x1    ) << FRACBITS)) continue;

		fixed_t clip_y1 =  ((int)mceilingclip[ux >> FRACBITS] + 1) << FRACBITS;
		fixed_t clip_y2 = (((int)mfloorclip  [ux >> FRACBITS]    ) << FRACBITS) - 1;

		fixed_t scale;
		fixed_t iscale;

		if (ux > Bx)
			scale = B_xscale + FixedMul (C_xscale - B_xscale, FixedDiv (ux - Bx, Cx - Bx));
		else
			scale = A_xscale + FixedMul (B_xscale - A_xscale, FixedDiv (ux - Ax, Bx - Ax));

		iscale = FixedDiv (FRACUNIT, scale);

		const byte * slab = &v->data[ofs1];
		const byte * end  = &v->data[ofs2];

		byte top, len, face;

		for (; slab < end ; slab += len)
		{
			top  = *slab++;
			len  = *slab++;
			face = *slab++;

			fixed_t top_z = spr->gzt - viewz - (top << FRACBITS);

			fixed_t uy1 = centeryfrac - FixedMul (top_z, scale);
			fixed_t uy2 = uy1 + (fixed_t) len * scale;
			fixed_t uy0 = uy1;

			// clip the slab vertically
			if (uy1 >= clip_y2) uy1 = clip_y2;
			if (uy2 <= clip_y1) uy2 = clip_y1;

			if (uy1 < clip_y1) uy1 = clip_y1;
			if (uy2 > clip_y2) uy2 = clip_y2;

			boolean has_side = ((face & (ux > Bx ? B_face : A_face)) != 0
                          && uy1 < clip_y2 && uy2 > clip_y1);

			// handle the fuzz effect for Spectres
			if (shadow)
			{
				if (! has_side)
					continue;

				dc_x  = ux  >> FRACBITS;
				dc_yl = uy1 >> FRACBITS;
				dc_yh = uy2 >> FRACBITS;

				if (dc_yl <= dc_yh)
					R_DrawFuzzColumn ();

				continue;
			}

			boolean has_top    = ((face & F_TOP) && top_z < 0);
			boolean has_bottom = ((face & F_BOTTOM) && top_z > ((int)len << FRACBITS));

			fixed_t wscale = 0;

			if (has_top || has_bottom)
			{
				if (ux > Cx)
					wscale = C_xscale + FixedMul (B_xscale - C_xscale, FixedDiv (ux - Cx, Bx - Cx));
				else if (ux > Dx)
					wscale = D_xscale + FixedMul (C_xscale - D_xscale, FixedDiv (ux - Dx, Cx - Dx));
				else
					wscale = A_xscale + FixedMul (D_xscale - A_xscale, FixedDiv (ux - Ax, Dx - Ax));
			}

			if (has_top)
			{
				fixed_t uy = centeryfrac - FixedMul (top_z, wscale);

				uy = ((uy - 1) | FRACMASK) + 1;

				if (uy < clip_y1)
					uy = clip_y1;

				byte src = slab[0];
				byte pix = spr->colormap[spr->brightmap[src]][src];

				for (; uy < uy1 ; uy += FRACUNIT)
				{
					dest[(uy >> FRACBITS) * linesize + (ux >> FRACBITS)] = pix;
				}
			}
			else if (has_bottom)
			{
				fixed_t uy = centeryfrac - FixedMul (top_z - ((int)len << FRACBITS), wscale);

				if (uy > clip_y2)
					uy = clip_y2;

				byte src = slab[len - 1];
				byte pix = spr->colormap[spr->brightmap[src]][src];

				for (; uy > uy2 ; uy -= FRACUNIT)
				{
					dest[(uy >> FRACBITS) * linesize + (ux >> FRACBITS)] = pix;
				}
			}

			if (has_side)
			{
				fixed_t uy = ((uy1 - 1) | FRACMASK) + 1;

				for (; uy <= uy2 ; uy += FRACUNIT)
				{
					int i = (((uy - uy0) >> FRACBITS) * iscale) >> FRACBITS;

					if (i < 0)    i = 0;
					if (i >= len) i = len - 1;

					byte src = slab[i];
					byte pix = spr->colormap[spr->brightmap[src]][src];

					dest[(uy >> FRACBITS) * linesize + (ux >> FRACBITS)] = pix;
				}
			}
		}
	}
}


static void VX_RecursiveDraw (vissprite_t * spr, int x, int y, int w, int h)
{
loop:
	// reached a single column?
	if (w == 1 && h == 1)
	{
		VX_DrawColumn (spr, x, y);
		return;
	}

	// split either horizontally or vertically, and recursively
	// visit the section furthest from the camera, then visit the
	// section nearest the camera.

	if (w >= h)
	{
		if (vx_eye_x < ((x * 2 + w) << (FRACBITS-1)))
		{
			VX_RecursiveDraw (spr, x + w / 2, y, (w+1) / 2, h);
			w = w / 2;
		}
		else
		{
			VX_RecursiveDraw (spr, x, y, w / 2, h);
			x += w / 2;
			w = (w+1) / 2;
		}
	}
	else
	{
		if (vx_eye_y < ((y * 2 + h) << (FRACBITS-1)))
		{
			VX_RecursiveDraw (spr, x, y + h / 2, w, (h+1) / 2);
			h = h / 2;
		}
		else
		{
			VX_RecursiveDraw (spr, x, y, w, h / 2);
			y += h / 2;
			h = (h+1) / 2;
		}
	}

	goto loop;
}


void VX_DrawVoxel (vissprite_t * spr)
{
	struct VisVoxel * vv = &visvoxels[spr->voxel_index];
	struct Voxel    * v  = vv->model;

	// check that some part is visible
	while (spr->x1 <= spr->x2 && mfloorclip[spr->x1] - mceilingclip[spr->x1] < 2)
		spr->x1 += 1;

	while (spr->x2 >= spr->x1 && mfloorclip[spr->x2] - mceilingclip[spr->x2] < 2)
		spr->x2 -= 1;

	if (spr->x1 > spr->x2)
		return;

	// handle translated colors (for players in coop or deathmatch).
	// we build a new map, rather than complicate the slab drawing code.

	if ((spr->mobjflags & MF_TRANSLATION) && (spr->colormap[0] != NULL))
	{
		const byte * trans = translationtables - 256 +
			( (spr->mobjflags & MF_TRANSLATION) >> (MF_TRANSSHIFT-8) );

		static byte new_colormap[256];

		int i;
		for (i = 0 ; i < 256 ; i++)
			new_colormap[i] = spr->colormap[0][trans[i]];

		spr->colormap[0] = new_colormap;
	}

	if ((spr->mobjflags_extra & MFX_COLOREDBLOOD) && (spr->colormap[0] != NULL))
	{
		static const byte * prev_trans = NULL, * prev_map = NULL;
		const byte * trans = red2col[spr->color], * map = spr->colormap[0];

		static byte new_colormap[256];

		if (prev_trans != trans || prev_map != map)
		{
			int i;
			for (i = 0 ; i < 256 ; i++)
				new_colormap[i] = map[trans[i]];

			prev_trans = trans;
			prev_map = map;
		}

		spr->colormap[0] = new_colormap;
	}

	// perform reverse transform, place camera in relation to model

	unsigned int ang = (vv->angle + ANG90) >> ANGLETOFINESHIFT;

	fixed_t c = finecosine[ang];
	fixed_t s = finesine  [ang];

	fixed_t delta_x = viewx - spr->gx;
	fixed_t delta_y = viewy - spr->gy;

	vx_eye_x = v->x_pivot + FixedMul (delta_x, c) + FixedMul (delta_y, s);
	vx_eye_y = v->y_pivot + FixedMul (delta_x, s) - FixedMul (delta_y, c);

	VX_RecursiveDraw (spr, 0, 0, v->x_size, v->y_size);
}
