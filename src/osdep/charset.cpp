#include "sysdeps.h"
#include "options.h"

#include <string.h>

// UAE4ARM and fs-uae uses only chars / UTF-8 internally, so TCHAR is typedefed to
// char (WinUAE uses wchar_t internally).

char *ua (const TCHAR *s) 
{
	if (s == NULL) 
	  return NULL;
	return strdup (s);
}

char *au (const TCHAR *s) 
{
	if (s == NULL) 
	  return NULL;
	return strdup (s);
}

TCHAR* utf8u (const char *s)
{
	if (s == NULL) 
	  return NULL;
	return ua (s);
}

char* uutf8 (const TCHAR *s)
{
	if (s == NULL) 
	  return NULL;
	return ua (s);
}

TCHAR *au_copy (TCHAR *dst, int maxlen, const char *src)
{
	// this should match the WinUAE au_copy behavior, where either the
	// entire string is copied (and null-terminated), or the result is
	// an empty string
	if (uae_tcslcpy (dst, src, maxlen) >= maxlen) {
		dst[0] = '\0';
	}
	return dst;
}

char *ua_copy (char *dst, int maxlen, const TCHAR *src)
{
	return au_copy (dst, maxlen, src);
}

TCHAR *my_strdup_ansi (const char *src)
{
	return strdup (src);
}

// File names of folder drives: UTF-8 on the host, a byte a character on the Amiga, in the set the
// configuration chooses (filesys_charset): Latin-1, the Amiga's own, or Windows-1251, for Cyrillic
// with fonts of it.
static const uae_u16 cp1251[128] = {
	0x0402, 0x0403, 0x201a, 0x0453, 0x201e, 0x2026, 0x2020, 0x2021,
	0x20ac, 0x2030, 0x0409, 0x2039, 0x040a, 0x040c, 0x040b, 0x040f,
	0x0452, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
	0x0000, 0x2122, 0x0459, 0x203a, 0x045a, 0x045c, 0x045b, 0x045f,
	0x00a0, 0x040e, 0x045e, 0x0408, 0x00a4, 0x0490, 0x00a6, 0x00a7,
	0x0401, 0x00a9, 0x0404, 0x00ab, 0x00ac, 0x00ad, 0x00ae, 0x0407,
	0x00b0, 0x00b1, 0x0406, 0x0456, 0x0491, 0x00b5, 0x00b6, 0x00b7,
	0x0451, 0x2116, 0x0454, 0x00bb, 0x0458, 0x0405, 0x0455, 0x0457,
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
	0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
	0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f,
};

// The character of a byte of the Amiga; 0 for none
static int fs_char (uae_u8 b)
{
	if (b < 0x80 || currprefs.filesys_charset != 1)
		return b;
	return cp1251[b - 0x80];
}

// The byte of the Amiga for a character; -1 for none
static int fs_byte (int c)
{
	if (c < 0x80)
		return c;
	if (currprefs.filesys_charset != 1)
		return c < 0x100 ? c : -1;
	for (int i = 0; i < 128; i++) {
		if (cp1251[i] == c)
			return 0x80 + i;
	}
	return -1;
}

// The character UTF-8 has at s, and its length; a byte that starts none is taken for itself
static int utf8_char (const uae_u8 *s, int *len)
{
	int c = s[0], n = 0;
	if (c >= 0xf0 && c < 0xf8) {
		c &= 0x07; n = 3;
	} else if (c >= 0xe0 && c < 0xf0) {
		c &= 0x0f; n = 2;
	} else if (c >= 0xc2 && c < 0xe0) {
		c &= 0x1f; n = 1;
	} else if (c >= 0x80) {
		*len = 1;
		return c;
	}
	for (int i = 1; i <= n; i++) {
		if ((s[i] & 0xc0) != 0x80) {
			*len = 1;
			return s[0];
		}
		c = (c << 6) | (s[i] & 0x3f);
	}
	*len = n + 1;
	return c;
}

static int utf8_put (char *d, int c)
{
	if (c < 0x80) {
		d[0] = c;
		return 1;
	}
	if (c < 0x800) {
		d[0] = 0xc0 | (c >> 6);
		d[1] = 0x80 | (c & 0x3f);
		return 2;
	}
	d[0] = 0xe0 | (c >> 12);
	d[1] = 0x80 | ((c >> 6) & 0x3f);
	d[2] = 0x80 | (c & 0x3f);
	return 3;
}

TCHAR *au_fs_copy (TCHAR *dst, int maxlen, const char *src)
{
	int i = 0;
	for (const uae_u8 *s = (const uae_u8 *)src; *s; s++) {
		char tmp[3];
		int c = fs_char (*s);
		int n = utf8_put (tmp, c ? c : '_');
		if (i + n >= maxlen)
			break;
		memcpy (dst + i, tmp, n);
		i += n;
	}
	dst[i] = 0;
	return dst;
}

TCHAR *au_fs (const char *src) 
{
	if (src == NULL) 
		return NULL;
	int size = strlen (src) * 3 + 1;
	TCHAR *dst = xmalloc (TCHAR, size);
	return au_fs_copy (dst, size, src);
}

// Characters the Amiga's set lacks become defchar, or '?' without one
char *ua_fs_copy (char *dst, int maxlen, const TCHAR *src, int defchar)
{
	int i = 0;
	const uae_u8 *s = (const uae_u8 *)src;
	while (*s && i + 1 < maxlen) {
		int len;
		int b = fs_byte (utf8_char (s, &len));
		dst[i++] = b >= 0 ? b : (defchar >= 0 ? defchar : '?');
		s += len;
	}
	dst[i] = 0;
	return dst;
}

char *ua_fs (const TCHAR *s, int defchar) 
{
	if (s == NULL) 
		return NULL;
	int size = strlen (s) + 1;
	char *dst = xmalloc (char, size);
	return ua_fs_copy (dst, size, s, defchar);
}
