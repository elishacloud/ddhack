#include <Windows.h>

void gdi_write_string(HDC hdc, int nXStart, int nYStart, LPCTSTR lpString, int cchString);
void gdi_clear(const RECT *lpRect);
void gdi_invalidate(const RECT *lpRect);
void gdi_clear_all();
void gdi_run_invalidations();
void gdi_clear_invalidations();
unsigned char *gdi_get_buffer();