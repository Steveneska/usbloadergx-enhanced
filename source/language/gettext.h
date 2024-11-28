#ifndef _GETTEXT_H_
#define _GETTEXT_H_

#ifdef __cplusplus
extern "C"
{
#endif

	bool gettextLoadLanguage(const char* langFile);
	void gettextCleanUp(void);
	/*
	 * Input msg = a text in ASCII
	 * Output = the translated msg in UTF-8
	 */
	const char *get_text(const char *msg);
#define tr(s) get_text(s)
#define trNOOP(s) (s)

#ifdef __cplusplus
}
#endif

#endif
