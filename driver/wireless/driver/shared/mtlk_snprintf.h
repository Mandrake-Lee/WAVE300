#ifndef  MTLK_SNPRINTF
#define  MTLK_SNPRINTF

int __MTLK_IFUNC mtlk_vsnprintf (char *buffer, size_t buffer_size, const char *format, va_list argv);
int __MTLK_IFUNC mtlk_snprintf (char *buffer, size_t buffer_size, const char *format, ...);

#endif /* MTLK_SNPRINTF */

