#ifndef OCR_APP_APP_WINDOW_H
#define OCR_APP_APP_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OCR_TYPE_APP_WINDOW (ocr_app_window_get_type())
G_DECLARE_FINAL_TYPE(OcrAppWindow, ocr_app_window, OCR, APP_WINDOW, GtkApplicationWindow)

OcrAppWindow *ocr_app_window_new(GtkApplication *application);

G_END_DECLS

#endif
