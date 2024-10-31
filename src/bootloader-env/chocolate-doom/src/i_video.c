//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	DOOM graphics stuff.
//


#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "display.h"
#include "f1c100s_de.h"

#define DISPLAYWIDTH  320
#define DISPLAYHEIGHT 240

static byte *stretch_tables[2] = { NULL, NULL };
static byte *src_buffer;
static byte *dest_buffer;
static int dest_pitch;

static uint32_t rgb888_palette[256];

byte *I_VideoBuffer = NULL;
byte *fb_out = NULL;

boolean screensaver_mode = false;
boolean screenvisible;
float mouse_acceleration = 2.0;
int mouse_threshold = 10;
int usegamma = 0;
int usemouse = 0;

typedef struct
{
	byte r;
	byte g;
	byte b;
} col_t;


// Search through the given palette, finding the nearest color that matches
// the given color.
static int FindNearestColor(byte *palette, int r, int g, int b)
{
    byte *col;
    int best;
    int best_diff;
    int diff;
    int i;

    best = 0;
    best_diff = INT_MAX;

    for (i=0; i<256; ++i)
    {
        col = palette + i * 3;
        diff = (r - col[0]) * (r - col[0])
             + (g - col[1]) * (g - col[1])
             + (b - col[2]) * (b - col[2]);

        if (diff == 0)
        {
            return i;
        }
        else if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }
    }

    return best;
}


// Create a stretch table.  This is a lookup table for blending colors.
// pct specifies the bias between the two colors: 0 = all y, 100 = all x.
// NB: This is identical to the lookup tables used in other ports for
// translucency.
static byte *GenerateStretchTable(byte *palette, int pct)
{
    byte *result;
    int x, y;
    int r, g, b;
    byte *col1;
    byte *col2;

    result = Z_Malloc(256 * 256, PU_STATIC, NULL);

    for (x=0; x<256; ++x)
    {
        for (y=0; y<256; ++y)
        {
            col1 = palette + x * 3;
            col2 = palette + y * 3;
            r = (((int) col1[0]) * pct + ((int) col2[0]) * (100 - pct)) / 100;
            g = (((int) col1[1]) * pct + ((int) col2[1]) * (100 - pct)) / 100;
            b = (((int) col1[2]) * pct + ((int) col2[2]) * (100 - pct)) / 100;
            result[x * 256 + y] = FindNearestColor(palette, r, g, b);
        }
    }

    return result;
}

// Called at startup to generate the lookup tables for aspect ratio
// correcting scale up.
static void I_InitStretchTables(byte *palette)
{
    if (stretch_tables[0] != NULL)
    {
        return;
    }

    // We only actually need two lookup tables:
    //
    // mix 0%   =  just write line 1
    // mix 20%  =  stretch_tables[0]
    // mix 40%  =  stretch_tables[1]
    // mix 60%  =  stretch_tables[1] used backwards
    // mix 80%  =  stretch_tables[0] used backwards
    // mix 100% =  just write line 2

    printf("I_InitStretchTables: Generating lookup tables..");
    fflush(stdout);
    stretch_tables[0] = GenerateStretchTable(palette, 20);
    printf(".."); fflush(stdout);
    stretch_tables[1] = GenerateStretchTable(palette, 40);
    puts("");
}

void I_SetGrabMouseCallback(grabmouse_callback_t func)
{
}

void I_DisplayFPSDots(boolean dots_on)
{
}

void I_ShutdownGraphics(void)
{
    debe_layer_disable(1);
	Z_Free (I_VideoBuffer);
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
}

void I_GetEvent(void)
{
}

//
// I_StartTic
//
void I_StartTic (void)
{
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
}

static void I_InitScale(byte *_src_buffer, byte *_dest_buffer, int _dest_pitch)
{
    src_buffer = _src_buffer;
    dest_buffer = _dest_buffer;
    dest_pitch = _dest_pitch;
}

// 
// Aspect ratio correcting scale up functions.
//
// These double up pixels to stretch the screen when using a 4:3
// screen mode.
//

static inline void WriteBlendedLine1x(byte *dest, byte *src1, byte *src2, 
                               byte *stretch_table)
{
    int x;

    for (x=0; x<SCREENWIDTH; ++x)
    {
        *dest = stretch_table[*src1 * 256 + *src2];
        ++dest;
        ++src1;
        ++src2;
    }
} 

static boolean I_Stretch1x(int x1, int y1, int x2, int y2)
{
    byte *bufp, *screenp;
    int y;

    // Only works with full screen update

    if (x1 != 0 || y1 != 0 || x2 != SCREENWIDTH || y2 != SCREENHEIGHT)
    {
        return false;
    }    

    // Need to byte-copy from buffer into the screen buffer

    bufp = src_buffer + y1 * SCREENWIDTH + x1;
    screenp = (byte *) dest_buffer + y1 * dest_pitch + x1;

    // For every 5 lines of src_buffer, 6 lines are written to dest_buffer
    // (200 -> 240)

    for (y=0; y<SCREENHEIGHT; y += 5)
    {
        // 100% line 0
        memcpy(screenp, bufp, SCREENWIDTH);
        screenp += dest_pitch;

        // 20% line 0, 80% line 1
        WriteBlendedLine1x(screenp, bufp, bufp + SCREENWIDTH, stretch_tables[0]);
        screenp += dest_pitch; bufp += SCREENWIDTH;

        // 40% line 1, 60% line 2
        WriteBlendedLine1x(screenp, bufp, bufp + SCREENWIDTH, stretch_tables[1]);
        screenp += dest_pitch; bufp += SCREENWIDTH;

        // 60% line 2, 40% line 3
        WriteBlendedLine1x(screenp, bufp + SCREENWIDTH, bufp, stretch_tables[1]);
        screenp += dest_pitch; bufp += SCREENWIDTH;

        // 80% line 3, 20% line 4
        WriteBlendedLine1x(screenp, bufp + SCREENWIDTH, bufp, stretch_tables[0]);
        screenp += dest_pitch; bufp += SCREENWIDTH;

        // 100% line 4
        memcpy(screenp, bufp, SCREENWIDTH);
        screenp += dest_pitch; bufp += SCREENWIDTH;
    }

    return true;
}

static void BlitArea(int x1, int y1, int x2, int y2)
{
    I_InitScale(I_VideoBuffer, fb_out, DISPLAYWIDTH);
    I_Stretch1x(x1, y1, x2, y2);
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    BlitArea(0, 0, SCREENWIDTH, SCREENHEIGHT);
}


//
// I_ReadScreen
//
void I_ReadScreen (pixel_t* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}


//
// I_SetPalette
//
void I_SetPalette (byte *palette)
{
    int i;
	col_t* c;

	for (i = 0; i < 256; i++)
	{
		c = (col_t*)palette;

		rgb888_palette[i] = GFX_ARGB8888(gammatable[usegamma][c->r],
									   gammatable[usegamma][c->g],
									   gammatable[usegamma][c->b], 0xFF);

		palette += 3;
	}
	debe_write_palette(rgb888_palette, 256);
}

// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex(int r, int g, int b)
{
    int best, best_diff, diff;
    int i;
    col_t color;

    best = 0;
    best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
    	color.r = GFX_ARGB8888_R(rgb888_palette[i]);
    	color.g = GFX_ARGB8888_G(rgb888_palette[i]);
    	color.b = GFX_ARGB8888_B(rgb888_palette[i]);

        diff = (r - color.r) * (r - color.r)
             + (g - color.g) * (g - color.g)
             + (b - color.b) * (b - color.b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

void I_SetWindowTitle(const char *title)
{
}

void I_InitWindowTitle(void)
{
}

void I_RegisterWindowIcon(const unsigned int *icon, int width, int height)
{
}

void I_InitWindowIcon(void)
{
}

void I_GraphicsCheckCommandLine(void)
{
}

void I_CheckIsScreensaver(void)
{
}

void I_InitGraphics(void)
{
    I_VideoBuffer = (byte*)Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);

	byte *doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    I_InitStretchTables(doompal);

    fb_out = (byte*)Z_Malloc(DISPLAYWIDTH * DISPLAYHEIGHT, PU_STATIC, NULL);

	debe_layer_init(1);
	debe_layer_set_size(1, DISPLAYWIDTH, DISPLAYHEIGHT);
	debe_layer_set_pos(1, 0, 0);
	debe_layer_set_addr(1, fb_out);
	debe_layer_set_mode(1, DEBE_MODE_8BPP_PALETTE);
	debe_layer_enable(1);

	screenvisible = true;
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
}
