#ifndef __ardour_gtk_automation_pan_line_h__
#define __ardour_gtk_automation_pan_line_h__

#include <ardour/ardour.h>
#include <gtk-canvas.h>
#include <gtk--.h>

#include "automation_line.h"

namespace ARDOUR {
	class Session;
}

class TimeAxisView;

class AutomationPanLine : public AutomationLine
{
  public:
	AutomationPanLine (string name, ARDOUR::Session&, TimeAxisView&, GtkCanvasItem* parent,
			   ARDOUR::Curve&, 
			   gint (*point_callback)(GtkCanvasItem*, GdkEvent*, gpointer),
			   gint (*line_callback)(GtkCanvasItem*, GdkEvent*, gpointer));
	
	void view_to_model_y (double&);
	void model_to_view_y (double&);

  private:
	ARDOUR::Session& session;
	vector<GtkCanvasItem*> lines;
};


#endif /* __ardour_gtk_automation_pan_line_h__ */


