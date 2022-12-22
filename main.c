#include <MiniFB.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "crt.h"
#include "machine.h"
#include "fake6502.h"
#include "WindowData.h"

uint32_t  g_width  = 64*MACHINE_SCALE;
uint32_t  g_height = 64*MACHINE_SCALE;
uint32_t *g_buffer = 0x0;
uint32_t *g_crt_buffer = 0x0;
int g_crt_field = 0x0;
uint32_t g_crt_noise = 24;
struct CRT g_crt = {0};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int CalcScale(int bmpW, int bmpH, int areaW, int areaH)
{
	int scale = 0;
	for(;;)
	{
		scale++;
		if (bmpW*scale > areaW || bmpH*scale > areaH)
		{
			scale--;
			break;
		}
	}
	return (scale > 1) ? scale : 1;
}

//	UP 0x109
//	DOWN 0x108
//	LEFT 0x107
//	RIGHT 0x106

mfb_key keys[8] =
{
	0x5a,
	0x58,
	0x43,
	0x101,
	0x109,
	0x108,
	0x107,
	0x106,
};

#define BIT_SET(PIN,N) (PIN |=  (1<<N))
#define BIT_CLR(PIN,N) (PIN &= ~(1<<N))

void
keyboard(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool isPressed) {
    const char *window_title = "";
    if(window) {
        window_title = (const char *) mfb_get_user_data(window);
    }

		uint8_t kb = read6502(IO_INPUT);
		for (int q=0;q<8;q++)
		{
			if (key==keys[q])
			{
				if (isPressed==true)
					BIT_SET(kb,q);
				else
					BIT_CLR(kb,q);

			}
		}
		write6502(IO_INPUT,kb);

		if (key==0x102)
		{
			if (isPressed==false)
			{
				next_view();
			}
		}
    fprintf(stdout, "%s > keyboard: key: %s (pressed: %d) [key_mod: %x] key %x\n", window_title, mfb_get_key_name(key), isPressed, mod, key);
    if(key == KB_KEY_ESCAPE) {
        mfb_close(window);
    }
}

void
resize(struct mfb_window *window, int width, int height) {
    (void) window;
int scale = 1;
int iw,ih;
int ox,oy;

	scale = CalcScale(g_width,g_height,width,height);
	iw = g_width*scale;
	ih = g_height*scale;
	//	center
	ox = (width-iw)/2;
	oy = (height-ih)/2;
	mfb_set_viewport(window, ox, oy, iw,ih );
}

void crt_update() {
	g_crt_field = (g_crt_field+1) & 0x01;

	// fade phosphurs
    for (uint32_t i = 0; i < g_width * g_height; i++) {
        uint32_t c = g_buffer[i] & 0xffffff;
        g_buffer[i] = (c >> 1 & 0x7f7f7f) +
               (c >> 2 & 0x3f3f3f) +
               (c >> 3 & 0x1f1f1f) +
               (c >> 4 & 0x0f0f0f);
    }

	struct NTSC_SETTINGS settings;
	settings.w = g_width;
	settings.h = g_height;
	settings.rgb = g_buffer;
	settings.as_color = 1;
	settings.field = g_crt_field;

	crt_2ntsc(&g_crt, &settings);
	crt_draw(&g_crt, g_crt_noise);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc,char **argv)
{
    struct mfb_window *window = mfb_open_ex("minicube64", g_width, g_height, WF_RESIZABLE);
    if (!window)
        return 0;

    mfb_set_keyboard_callback(window, keyboard);

	g_crt_buffer = (uint32_t *) malloc(g_width * g_height * 4);
    g_buffer = (uint32_t *) malloc(g_width * g_height * 4);
	crt_init(&g_crt, g_width, g_height, g_crt_buffer);

    mfb_set_resize_callback(window, resize);

    resize(window, g_width*3, g_height*3);  // to resize buffer

    reset_machine(argv[1]);

    // Manual assignment of draw buffer on window to avoid compatibility issues
    // with X11.
	SWindowData *window_data = (SWindowData *) window;
	window_data->draw_buffer = g_buffer;

    mfb_update_state state;
    do {
        display_machine();
		crt_update();
		// moved gif update here so it picks up crt
		update_gif();
		
        state = mfb_update_ex(window, g_crt_buffer, g_width, g_height);
        if (state != STATE_OK) {
            window = 0x0;
            break;
        }
    } while(mfb_wait_sync(window));

    kill_machine();

    return 0;
}
