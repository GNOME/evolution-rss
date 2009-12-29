
#ifndef __DEBUG_H__
#define __DEBUG_H__ 1

#define d(f, x...) if (rss_verbose_debug) { g_print("%s(%d) %s():", __FILE__, __LINE__, __FUNCTION__);\
                        g_print(f, ## x);\
                        g_print("\n");}

#define dp(f, x...) { g_print("%s(%d) %s():", __FILE__, __LINE__, __FUNCTION__);\
                        g_print(f, ## x);\
                        g_print("\n");}

#endif /*__DEBUG_H__*/

