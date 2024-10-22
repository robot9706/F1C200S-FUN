// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
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
// $Log:$
//
// DESCRIPTION:
//	DOOM graphics stuff for X11, UNIX.
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: i_x.c,v 1.6 1997/02/03 22:45:10 b1 Exp $";

#include "config.h"
#include "v_video.h"
#include "m_argv.h"
#include "w_wad.h"
#include "d_event.h"
#include "d_main.h"
#include "i_video.h"
#include "i_scale.h"
#include "z_zone.h"
#include "deh_str.h"

#include "tables.h"
#include "doomkeys.h"

#include <stdint.h>
#include <stdbool.h>
#include "display.h"
#include "f1c100s_de.h"

// Non aspect ratio-corrected modes (direct multiples of 320x200)

static screen_mode_t *screen_modes[] = {
    &mode_scale_1x,
    &mode_scale_2x,
    &mode_scale_3x,
    &mode_scale_4x,
    &mode_scale_5x,
};

// Aspect ratio corrected modes (4:3 ratio)

static screen_mode_t *screen_modes_corrected[] = {

    // Vertically stretched modes (320x200 -> 320x240 and multiples)

    &mode_stretch_1x,
    &mode_stretch_2x,
    &mode_stretch_3x,
    &mode_stretch_4x,
    &mode_stretch_5x,

    // Horizontally squashed modes (320x200 -> 256x200 and multiples)

    &mode_squash_1x,
    &mode_squash_2x,
    &mode_squash_3x,
    &mode_squash_4x,
};

// The screen mode and scale functions being used

static screen_mode_t *screen_mode;

// The screen buffer; this is modified to draw things to the screen

byte *I_VideoBuffer = NULL;
byte *fb_out = NULL;

// If true, game is running as a screensaver

boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

boolean screenvisible;

// Mouse acceleration
//
// This emulates some of the behavior of DOS mouse drivers by increasing
// the speed when the mouse is moved fast.
//
// The mouse input values are input directly to the game, but when
// the values exceed the value of mouse_threshold, they are multiplied
// by mouse_acceleration to increase the speed.

float mouse_acceleration = 2.0;
int mouse_threshold = 10;

// Gamma correction level to use

int usegamma = 0;

int usemouse = 0;

// If true, keyboard mapping is ignored, like in Vanilla Doom.
// The sensible thing to do is to disable this if you have a non-US
// keyboard.

int vanilla_keyboard_mapping = true;

typedef struct
{
	byte r;
	byte g;
	byte b;
} col_t;

static uint32_t rgb888_palette[256];

// Last touch state

//static touch_state_t last_touch_state;

// Last button state

//static bool last_button_state;

// run state

//static bool run;

void I_InitGraphics (void)
{
	I_VideoBuffer = (byte*)Z_Malloc (SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);

	screen_mode = &mode_stretch_1x;

	byte *doompal = W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE);
    if (screen_mode != NULL && screen_mode->InitMode != NULL)
    {
    	screen_mode->InitMode(doompal);
    }

    fb_out = (byte*)Z_Malloc((screen_mode->width) * screen_mode->height, PU_STATIC, NULL);

	debe_layer_init(1);
	debe_layer_set_size(1, screen_mode->width, screen_mode->height);
	debe_layer_set_pos(1, 0, 0);
	debe_layer_set_addr(1, fb_out);
	debe_layer_set_mode(1, DEBE_MODE_8BPP_PALETTE);
	debe_layer_enable(1);

	screenvisible = true;
}

void I_ShutdownGraphics (void)
{
	debe_layer_disable(1);
	Z_Free (I_VideoBuffer);
}

void I_StartFrame (void)
{

}

void I_StartTic (void)
{
}

void I_UpdateNoBlit (void)
{
}

// Update a small portion of the screen
//
// Does stretching and buffer blitting if neccessary
//
// Return true if blit was successful.

static void BlitArea(int x1, int y1, int x2, int y2)
{
    I_InitScale(I_VideoBuffer, fb_out, screen_mode->width);
    screen_mode->DrawScreen(x1, y1, x2, y2);
}

void I_FinishUpdate (void)
{
	BlitArea(0, 0, SCREENWIDTH, SCREENHEIGHT);
}

//
// I_ReadScreen
//
void I_ReadScreen (byte* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

//
// I_SetPalette
//
void I_SetPalette (byte* palette)
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

int I_GetPaletteIndex (int r, int g, int b)
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

void I_BeginRead (void)
{
}

void I_EndRead (void)
{
}

void I_SetWindowTitle (char *title)
{
}

void I_GraphicsCheckCommandLine (void)
{
}

void I_SetGrabMouseCallback (grabmouse_callback_t func)
{
}

void I_EnableLoadingDisk (void)
{
}

void I_BindVideoVariables (void)
{
}

void I_DisplayFPSDots (boolean dots_on)
{
}

void I_CheckIsScreensaver (void)
{
}
