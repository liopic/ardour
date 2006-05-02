/*
    Copyright (C) 2003 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sys/stat.h>
#include <libart_lgpl/art_misc.h>
#include <gtkmm/window.h>
#include <gtkmm/combo.h>
#include <gtkmm/label.h>
#include <gtkmm/paned.h>
#include <gtk/gtkpaned.h>

#include <gtkmm2ext/utils.h>
#include <ardour/ardour.h>

#include "ardour_ui.h"
#include "keyboard.h"
#include "utils.h"
#include "i18n.h"
#include "rgb_macros.h"
#include "canvas_impl.h"

using namespace std;
using namespace Gtk;
using namespace sigc;
using namespace Glib;

ustring
fit_to_pixels (const ustring& str, int pixel_width, Pango::FontDescription& font, int& actual_width)
{
	Label foo;
	Glib::RefPtr<Pango::Layout> layout = foo.create_pango_layout ("");
	
	layout->set_font_description (font);

	actual_width = 0;

	ustring ustr = str;
	ustring::iterator last = ustr.end();
	--last; /* now points at final entry */

	while (!ustr.empty()) {

		layout->set_text (ustr);

		int width, height;
		Gtkmm2ext::get_ink_pixel_size (layout, width, height);

		if (width < pixel_width) {
			actual_width = width;
			break;
		}
		
		ustr.erase (last);
		--last;
	}

	return ustr;
}

gint
just_hide_it (GdkEventAny *ev, Gtk::Window *win)
{
	win->hide_all ();
	return TRUE;
}

/* xpm2rgb copied from nixieclock, which bore the legend:

    nixieclock - a nixie desktop timepiece
    Copyright (C) 2000 Greg Ercolano, erco@3dsite.com

    and was released under the GPL.
*/

unsigned char*
xpm2rgb (const char** xpm, uint32_t& w, uint32_t& h)
{
	static long vals[256], val;
	uint32_t t, x, y, colors, cpp;
	unsigned char c;
	unsigned char *savergb, *rgb;
	
	// PARSE HEADER
	
	if ( sscanf(xpm[0], "%u%u%u%u", &w, &h, &colors, &cpp) != 4 ) {
		error << string_compose (_("bad XPM header %1"), xpm[0])
		      << endmsg;
		return 0;
	}

	savergb = rgb = (unsigned char*)art_alloc (h * w * 3);
	
	// LOAD XPM COLORMAP LONG ENOUGH TO DO CONVERSION
	for (t = 0; t < colors; ++t) {
		sscanf (xpm[t+1], "%c c #%lx", &c, &val);
		vals[c] = val;
	}
	
	// COLORMAP -> RGB CONVERSION
	//    Get low 3 bytes from vals[]
	//

	const char *p;
	for (y = h-1; y > 0; --y) {

		for (p = xpm[1+colors+(h-y-1)], x = 0; x < w; x++, rgb += 3) {
			val = vals[(int)*p++];
			*(rgb+2) = val & 0xff; val >>= 8;  // 2:B
			*(rgb+1) = val & 0xff; val >>= 8;  // 1:G
			*(rgb+0) = val & 0xff;             // 0:R
		}
	}

	return (savergb);
}

unsigned char*
xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h)
{
	static long vals[256], val;
	uint32_t t, x, y, colors, cpp;
	unsigned char c;
	unsigned char *savergb, *rgb;
	char transparent;

	// PARSE HEADER

	if ( sscanf(xpm[0], "%u%u%u%u", &w, &h, &colors, &cpp) != 4 ) {
		error << string_compose (_("bad XPM header %1"), xpm[0])
		      << endmsg;
		return 0;
	}

	savergb = rgb = (unsigned char*)art_alloc (h * w * 4);
	
	// LOAD XPM COLORMAP LONG ENOUGH TO DO CONVERSION

	if (strstr (xpm[1], "None")) {
		sscanf (xpm[1], "%c", &transparent);
		t = 1;
	} else {
		transparent = 0;
		t = 0;
	}

	for (; t < colors; ++t) {
		sscanf (xpm[t+1], "%c c #%lx", &c, &val);
		vals[c] = val;
	}
	
	// COLORMAP -> RGB CONVERSION
	//    Get low 3 bytes from vals[]
	//

	const char *p;
	for (y = h-1; y > 0; --y) {

		char alpha;

		for (p = xpm[1+colors+(h-y-1)], x = 0; x < w; x++, rgb += 4) {

			if (transparent && (*p++ == transparent)) {
				alpha = 0;
				val = 0;
			} else {
				alpha = 255;
				val = vals[(int)*p];
			}

			*(rgb+3) = alpha;                  // 3: alpha
			*(rgb+2) = val & 0xff; val >>= 8;  // 2:B
			*(rgb+1) = val & 0xff; val >>= 8;  // 1:G
			*(rgb+0) = val & 0xff;             // 0:R
		}
	}

	return (savergb);
}

ArdourCanvas::Points*
get_canvas_points (string who, uint32_t npoints)
{
	// cerr << who << ": wants " << npoints << " canvas points" << endl;
#ifdef TRAP_EXCESSIVE_POINT_REQUESTS
	if (npoints > (uint32_t) gdk_screen_width() + 4) {
		abort ();
	}
#endif
	return new ArdourCanvas::Points (npoints);
}

Pango::FontDescription
get_font_for_style (string widgetname)
{
	Gtk::Window window (WINDOW_TOPLEVEL);
	Gtk::Label foobar;
	Glib::RefPtr<Style> style;

	window.add (foobar);
	foobar.set_name (widgetname);
	foobar.ensure_style();

	style = foobar.get_style ();
	return style->get_font();
}

gint
pane_handler (GdkEventButton* ev, Gtk::Paned* pane)
{
	if (ev->window != Gtkmm2ext::get_paned_handle (*pane)) {
		return FALSE;
	}

	if (Keyboard::is_delete_event (ev)) {

		gint pos;
		gint cmp;
		
		pos = pane->get_position ();

		if (dynamic_cast<VPaned*>(pane)) {
			cmp = pane->get_height();
		} else {
			cmp = pane->get_width();
		}

		/* we have to use approximations here because we can't predict the
		   exact position or sizes of the pane (themes, etc)
		*/

		if (pos < 10 || abs (pos - cmp) < 10) {

			/* already collapsed: restore it (note that this is cast from a pointer value to int, which is tricky on 64bit */
			
			pane->set_position ((intptr_t) pane->get_data ("rpos"));

		} else {	

			int collapse_direction;

			/* store the current position */

			pane->set_data ("rpos", (gpointer) pos);

			/* collapse to show the relevant child in full */
			
			collapse_direction = (intptr_t) pane->get_data ("collapse-direction");

			if (collapse_direction) {
				pane->set_position (1);
			} else {
				if (dynamic_cast<VPaned*>(pane)) {
					pane->set_position (pane->get_height());
				} else {
					pane->set_position (pane->get_width());
				}
			}
		}

		return TRUE;
	} 

	return FALSE;
}
uint32_t
rgba_from_style (string style, uint32_t r, uint32_t g, uint32_t b, uint32_t a, string attr, int state, bool rgba)
{
	/* In GTK+2, styles aren't set up correctly if the widget is not
	   attached to a toplevel window that has a screen pointer.
	*/

	static Gtk::Window* window = 0;

	if (window == 0) {
		window = new Window (WINDOW_TOPLEVEL);
	}

	Gtk::Label foo;
	
	window->add (foo);

	foo.set_name (style);
	foo.ensure_style ();
	
	GtkRcStyle* waverc = foo.get_style()->gobj()->rc_style;

	if (waverc) {
		if (attr == "fg") {
			r = waverc->fg[state].red / 257;
			g = waverc->fg[state].green / 257;
			b = waverc->fg[state].blue / 257;
			/* what a hack ... "a" is for "active" */
			if (state == Gtk::STATE_NORMAL && rgba) {
				a = waverc->fg[GTK_STATE_ACTIVE].red / 257;
			}
		} else if (attr == "bg") {
			r = g = b = 0;
			r = waverc->bg[state].red / 257;
			g = waverc->bg[state].green / 257;
			b = waverc->bg[state].blue / 257;
		} else if (attr == "base") {
			r = waverc->base[state].red / 257;
			g = waverc->base[state].green / 257;
			b = waverc->base[state].blue / 257;
		} else if (attr == "text") {
			r = waverc->text[state].red / 257;
			g = waverc->text[state].green / 257;
			b = waverc->text[state].blue / 257;
		}
	} else {
		warning << string_compose (_("missing RGBA style for \"%1\""), style) << endl;
	}

	window->remove ();
	
	if (state == Gtk::STATE_NORMAL && rgba) {
		return (uint32_t) RGBA_TO_UINT(r,g,b,a);
	} else {
		return (uint32_t) RGB_TO_UINT(r,g,b);
	}
}

bool 
canvas_item_visible (ArdourCanvas::Item* item)
{
	return (item->gobj()->object.flags & GNOME_CANVAS_ITEM_VISIBLE) ? true : false;
}

void
set_color (Gdk::Color& c, int rgb)
{
	c.set_rgb((rgb >> 16)*256, ((rgb & 0xff00) >> 8)*256, (rgb & 0xff)*256);
}

bool
key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev)
{
	GtkWindow* win = window.gobj();
	GtkWidget* focus = gtk_window_get_focus (win);
	bool special_handling_of_unmodified_accelerators = false;

	if (focus) {
		if (GTK_IS_ENTRY(focus)) {
			special_handling_of_unmodified_accelerators = true;
		}
	} 

	/* This exists to allow us to override the way GTK handles
	   key events. The normal sequence is:

	   a) event is delivered to a GtkWindow
	   b) accelerators/mnemonics are activated
	   c) if (b) didn't handle the event, propagate to
	       the focus widget and/or focus chain

	   The problem with this is that if the accelerators include
	   keys without modifiers, such as the space bar or the 
	   letter "e", then pressing the key while typing into
	   a text entry widget results in the accelerator being
	   activated, instead of the desired letter appearing
	   in the text entry.

	   There is no good way of fixing this, but this
	   represents a compromise. The idea is that 
	   key events involving modifiers (not Shift)
	   get routed into the activation pathway first, then
	   get propagated to the focus widget if necessary.
	   
	   If the key event doesn't involve modifiers,
	   we deliver to the focus widget first, thus allowing
	   it to get "normal text" without interference
	   from acceleration.

	   Of course, this can also be problematic: if there
	   is a widget with focus, then it will swallow
	   all "normal text" accelerators.
	*/


	if (!special_handling_of_unmodified_accelerators) {

		/* pretend that certain key events that GTK does not allow
		   to be used as accelerators are actually something that
		   it does allow.
		*/

		int ret = false;

		switch (ev->keyval) {
		case GDK_Up:
			ret = gtk_accel_groups_activate(G_OBJECT(win), GDK_uparrow, GdkModifierType(ev->state));
			break;

		case GDK_Down:
			ret = gtk_accel_groups_activate(G_OBJECT(win), GDK_downarrow, GdkModifierType(ev->state));
			break;

		case GDK_Right:
			ret = gtk_accel_groups_activate(G_OBJECT(win), GDK_rightarrow, GdkModifierType(ev->state));
			break;

		case GDK_Left:
			ret = gtk_accel_groups_activate(G_OBJECT(win), GDK_leftarrow, GdkModifierType(ev->state));
			break;

		default:
			break;
		}

		if (ret) {
			return true;
		}
	}
		
	if (!special_handling_of_unmodified_accelerators ||
	    ev->state & (Gdk::MOD1_MASK|
			 Gdk::MOD2_MASK|
			 Gdk::MOD3_MASK|
			 Gdk::MOD4_MASK|
			 Gdk::MOD5_MASK|
			 Gdk::CONTROL_MASK|
			 Gdk::LOCK_MASK)) {

		/* no special handling or modifiers in effect: accelerate first */

		if (!gtk_window_activate_key (win, ev)) {
			return gtk_window_propagate_key_event (win, ev);
		} else {
			return true;
		} 
	}
	
	/* no modifiers, propagate first */
	
	if (!gtk_window_propagate_key_event (win, ev)) {
		return gtk_window_activate_key (win, ev);
	} 


	return true;
}

Glib::RefPtr<Gdk::Pixbuf>	
get_xpm (std::string name)
{
	if (!xpm_map[name]) {
		xpm_map[name] = Gdk::Pixbuf::create_from_file (ARDOUR::find_data_file(name, "pixmaps"));
	}
		
	return (xpm_map[name]);
}

