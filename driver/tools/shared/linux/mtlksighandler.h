#ifndef __MTLK_SIGNAL_HANDLER_H__
#define __MTLK_SIGNAL_HANDLER_H__

typedef void (__MTLK_IFUNC *mtlk_shandler_clb_f)(mtlk_handle_t usr_ctx, int signo);

int __MTLK_IFUNC
mtlk_shandler_init(mtlk_shandler_clb_f clb, mtlk_handle_t usr_ctx);
void __MTLK_IFUNC
mtlk_shandler_cleanup(void);

#endif
